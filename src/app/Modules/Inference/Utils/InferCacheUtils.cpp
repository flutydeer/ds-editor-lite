#include "InferCacheUtils.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/InferenceData/InferPiece.h>
#include <lite/Support/Log.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace InferCacheUtils {

    namespace {

        const QRegularExpression kCacheFilePattern(
            QStringLiteral("^infer-(acoustic|duration|pitch|variance)-(input|output)-"
                           "([0-9a-f]{40})\\.(json|wav)$"));

        // 进程级登记集合：主线程专用（任务完成回调 + 清理确认时收集），无需锁
        QSet<QString> &registeredFiles() {
            static QSet<QString> files;
            return files;
        }

        QString normalizePath(const QString &path) {
            return QFileInfo(path).absoluteFilePath().toLower();
        }

    } // namespace

    CacheFileNames cacheFileNames(const QString &category, const QString &hash) {
        // Only the acoustic task renders audio; every other task stores its result as JSON.
        const auto outputSuffix =
            category == QStringLiteral("acoustic") ? QStringLiteral("wav") : QStringLiteral("json");
        return {
            QStringLiteral("infer-%1-input-%2.json").arg(category, hash),
            QStringLiteral("infer-%1-output-%2.%3").arg(category, hash, outputSuffix),
        };
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
        registeredFiles().insert(normalizePath(absolutePath));
    }

    QSet<QString> registeredCacheFiles() {
        return registeredFiles();
    }

    void clearRegisteredCacheFiles() {
        registeredFiles().clear();
    }

    QSet<QString> collectActiveCacheFiles() {
        QSet<QString> active = registeredCacheFiles();
        for (const auto *track : appModel->tracks()) {
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
                    const auto match = kCacheFilePattern.match(fileInfo.fileName());
                    if (match.hasMatch() && match.captured(1) == QStringLiteral("acoustic") &&
                        match.captured(2) == QStringLiteral("output")) {
                        const auto names =
                            cacheFileNames(QStringLiteral("acoustic"), match.captured(3));
                        active.insert(normalizePath(fileInfo.dir().filePath(names.input)));
                    }
                }
            }
        }
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
