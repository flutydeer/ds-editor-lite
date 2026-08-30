#include "AudioFilePublisher.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

#include <optional>
#include <utility>

#ifdef Q_OS_WIN
#  include <windows.h>
#elif defined(Q_OS_UNIX)
#  include <sys/stat.h>
#endif

namespace Audio::Internal {
    namespace {
        struct FileIdentity {
            quint64 device = 0;
            quint64 object = 0;
            quint64 size = 0;
            quint64 modified = 0;

            bool operator==(const FileIdentity &) const = default;
        };

        std::optional<FileIdentity> fileIdentity(const QString &path) {
#ifdef Q_OS_WIN
            const auto handle =
                CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), FILE_READ_ATTRIBUTES,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (handle == INVALID_HANDLE_VALUE)
                return std::nullopt;
            BY_HANDLE_FILE_INFORMATION information{};
            const auto succeeded = GetFileInformationByHandle(handle, &information);
            CloseHandle(handle);
            if (!succeeded)
                return std::nullopt;
            return FileIdentity{
                information.dwVolumeSerialNumber,
                (quint64(information.nFileIndexHigh) << 32) | information.nFileIndexLow,
                (quint64(information.nFileSizeHigh) << 32) | information.nFileSizeLow,
                (quint64(information.ftLastWriteTime.dwHighDateTime) << 32) |
                    information.ftLastWriteTime.dwLowDateTime,
            };
#elif defined(Q_OS_UNIX)
            struct stat information{};
            const auto encodedPath = QFile::encodeName(path);
            if (::stat(encodedPath.constData(), &information) != 0)
                return std::nullopt;
#  if defined(Q_OS_DARWIN)
            const auto modified = quint64(information.st_mtimespec.tv_sec) * 1000000000ULL +
                                  quint64(information.st_mtimespec.tv_nsec);
#  else
            const auto modified = quint64(information.st_mtim.tv_sec) * 1000000000ULL +
                                  quint64(information.st_mtim.tv_nsec);
#  endif
            return FileIdentity{
                quint64(information.st_dev),
                quint64(information.st_ino),
                quint64(information.st_size),
                modified,
            };
#else
            Q_UNUSED(path)
            return std::nullopt;
#endif
        }

        struct PublicationEntry {
            QString target;
            QString temporary;
            QString backup;
            std::optional<FileIdentity> publishedIdentity;
            bool published = false;
        };
    }

    AudioFilePublicationResult publishAudioFiles(const QHash<QString, QString> &pendingFiles,
                                                 const bool allowOverwrite) {
        QStringList targets = pendingFiles.keys();
        targets.sort(Qt::CaseSensitive);
        QList<PublicationEntry> entries;
        entries.reserve(targets.size());
        for (const auto &target : std::as_const(targets))
            entries.append({target, pendingFiles.value(target), {}, {}, false});

        const auto transactionToken = QUuid::createUuid().toString(QUuid::WithoutBraces);

        AudioFilePublicationResult result;
        const auto fail = [&](const QString &failedTarget) {
            result.failedTarget = failedTarget;
            for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
                if (!it->published || !QFileInfo::exists(it->target))
                    continue;
                // Move the current path out of the way before comparing identity so a replacement
                // cannot be deleted between verification and cleanup.
                const auto recovery =
                    QFileInfo(it->target)
                        .dir()
                        .filePath(QStringLiteral(".ds$") + transactionToken +
                                  QFileInfo(it->target).fileName() + QStringLiteral(".rollback"));
                if (!QFile(it->target).rename(recovery)) {
                    result.recoveryFailures.append(it->target);
                    continue;
                }
                if (!it->publishedIdentity || fileIdentity(recovery) != it->publishedIdentity) {
                    if (!QFile(recovery).rename(it->target))
                        result.recoveryFailures.append(recovery);
                    continue;
                }
                if (!QFile::remove(recovery))
                    result.recoveryFailures.append(recovery);
            }
            for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
                if (it->backup.isEmpty())
                    continue;
                if (QFileInfo::exists(it->target) || !QFile(it->backup).rename(it->target))
                    result.recoveryFailures.append(it->backup);
            }
            for (const auto &entry : std::as_const(entries)) {
                if (QFileInfo::exists(entry.temporary))
                    result.remainingTemporaryFiles.append(entry.temporary);
            }
            return result;
        };

        if (!allowOverwrite) {
            for (auto &entry : entries) {
                // QFile::rename refuses an existing destination, so reject mode has no
                // check-then-replace window at the final publication point.
                entry.publishedIdentity = fileIdentity(entry.temporary);
                if (!entry.publishedIdentity)
                    return fail(entry.target);
                if (!QFile(entry.temporary).rename(entry.target))
                    return fail(entry.target);
                entry.published = true;
            }
            return result;
        }

        for (auto &entry : entries) {
            if (!QFileInfo::exists(entry.target))
                continue;
            const auto backup =
                QFileInfo(entry.target)
                    .dir()
                    .filePath(QStringLiteral(".ds$") + transactionToken +
                              QFileInfo(entry.target).fileName() + QStringLiteral(".backup"));
            if (!QFile(entry.target).rename(backup))
                return fail(entry.target);
            entry.backup = backup;
        }

        for (auto &entry : entries) {
            entry.publishedIdentity = fileIdentity(entry.temporary);
            if (!entry.publishedIdentity)
                return fail(entry.target);
            if (!QFile(entry.temporary).rename(entry.target))
                return fail(entry.target);
            entry.published = true;
        }

        for (const auto &entry : std::as_const(entries)) {
            if (!entry.backup.isEmpty() && !QFile::remove(entry.backup))
                result.backupCleanupFailures.append(entry.backup);
        }
        return result;
    }
}
