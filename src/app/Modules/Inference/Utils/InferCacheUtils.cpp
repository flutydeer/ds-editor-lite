#include "InferCacheUtils.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/InferenceData/InferPiece.h>
#include <lite/Support/Log.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace InferCacheUtils {

    namespace {

        const QRegularExpression kCacheFilePattern(
            QStringLiteral("^infer-(acoustic|duration|pitch|variance)-(input|output)-"
                           "[0-9a-f]{40}\\.(json|wav)$"));

        QHash<AppModel *, QSet<QString>> &registrations() {
            static QHash<AppModel *, QSet<QString>> files;
            return files;
        }

        QString normalizePath(const QString &path) {
            return QFileInfo(path).absoluteFilePath().toLower();
        }

    } // namespace

    void observeModel(AppModel *model) {
        if (registrations().contains(model))
            return;
        registrations().insert(model, {});
        QObject::connect(model, &QObject::destroyed, QCoreApplication::instance(),
                         [model] { registrations().remove(model); });
    }

    CacheStats scanCache(const QString &cacheDir) {
        CacheStats stats;
        const QDir dir(cacheDir);
        if (!dir.exists())
            return stats;
        const auto entries = dir.entryInfoList(QDir::Files | QDir::NoSymLinks);
        stats.files.reserve(entries.size());
        for (const auto &entry : entries) {
            const auto match = kCacheFilePattern.match(entry.fileName());
            if (!match.hasMatch())
                continue;
            CacheFileInfo info;
            info.fileName = entry.fileName();
            info.category = match.captured(1);
            info.isInput = match.captured(2) == QStringLiteral("input");
            info.size = entry.size();
            stats.files.append(info);
            stats.totalBytes += info.size;
        }
        return stats;
    }

    void registerCacheFile(const QString &absolutePath) {
        if (absolutePath.isEmpty())
            return;
        observeModel(appModel);
        registrations()[appModel].insert(normalizePath(absolutePath));
    }

    QSet<QString> registeredCacheFiles() {
        QSet<QString> files;
        for (const auto &entries : std::as_const(registrations()))
            files.unite(entries);
        return files;
    }

    void clearRegisteredCacheFiles() {
        observeModel(appModel);
        registrations()[appModel].clear();
    }

    static void collectModelFiles(QSet<QString> &active, const AppModel &model) {
        for (const auto *track : model.tracks()) {
            for (const auto *clip : track->clips()) {
                const auto *singingClip = dynamic_cast<const SingingClip *>(clip);
                if (!singingClip)
                    continue;
                for (const auto *piece : singingClip->pieces()) {
                    if (piece->acousticInferStatus != Success)
                        continue;
                    const auto &audioPath = piece->audioPath;
                    if (audioPath.isEmpty())
                        continue;
                    active.insert(normalizePath(audioPath));
                    // 配套 input json：infer-acoustic-output-<hash>.wav ->
                    // infer-acoustic-input-<hash>.json
                    const auto fileInfo = QFileInfo(audioPath);
                    const auto &fileName = fileInfo.fileName();
                    const QString kOutputPrefix = QStringLiteral("infer-acoustic-output-");
                    if (fileName.startsWith(kOutputPrefix) &&
                        fileName.endsWith(QStringLiteral(".wav"))) {
                        const auto hash = fileName.mid(kOutputPrefix.size(),
                                                       fileName.size() - kOutputPrefix.size() - 4);
                        active.insert(normalizePath(fileInfo.dir().filePath(
                            QStringLiteral("infer-acoustic-input-%1.json").arg(hash))));
                    }
                }
            }
        }
    }

    QSet<QString> collectActiveCacheFiles() {
        observeModel(appModel);
        auto active = registeredCacheFiles();
        for (const auto *model : registrations().keys())
            collectModelFiles(active, *model);
        return active;
    }

    CleanResult cleanCache(const QString &cacheDir, const QSet<QString> &activeFiles) {
        CleanResult result;
        const auto stats = scanCache(cacheDir);
        for (const auto &info : stats.files) {
            const auto path = QDir(cacheDir).filePath(info.fileName);
            if (activeFiles.contains(normalizePath(path))) {
                ++result.retainedActiveCount;
                continue;
            }
            if (QFile::remove(path)) {
                ++result.deletedCount;
                result.deletedBytes += info.size;
            } else {
                ++result.retainedLockedCount;
                qWarning().noquote() << "Failed to remove cache file (in use?):" << path;
            }
        }
        return result;
    }

} // namespace InferCacheUtils
