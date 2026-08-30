#include "AudioFilePublisher.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

#include <utility>

namespace Audio::Internal {
    namespace {
        struct PublicationEntry {
            QString target;
            QString temporary;
            QString backup;
            bool published = false;
        };
    }

    AudioFilePublicationResult publishAudioFiles(const QHash<QString, QString> &pendingFiles) {
        QStringList targets = pendingFiles.keys();
        targets.sort(Qt::CaseSensitive);
        QList<PublicationEntry> entries;
        entries.reserve(targets.size());
        for (const auto &target : std::as_const(targets))
            entries.append({target, pendingFiles.value(target), {}, false});

        AudioFilePublicationResult result;
        const auto fail = [&](const QString &failedTarget) {
            result.failedTarget = failedTarget;
            for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
                if (it->published && QFileInfo::exists(it->target) &&
                    !QFile::remove(it->target)) {
                    result.recoveryFailures.append(it->target);
                }
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

        const auto backupToken =
            QByteArray::fromHex(QUuid::createUuid().toByteArray(QUuid::Id128))
                .toBase64(QByteArray::Base64UrlEncoding)
                .mid(0, 8);
        for (auto &entry : entries) {
            if (!QFileInfo::exists(entry.target))
                continue;
            const auto backup = QFileInfo(entry.target).dir().filePath(
                QStringLiteral(".ds$") + QString::fromLatin1(backupToken) +
                QFileInfo(entry.target).fileName() + QStringLiteral(".backup"));
            if (!QFile(entry.target).rename(backup))
                return fail(entry.target);
            entry.backup = backup;
        }

        for (auto &entry : entries) {
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
