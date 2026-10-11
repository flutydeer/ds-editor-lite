#include "InferCacheUtils.h"

#include "Bootstrap/AppDataPaths.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/InferenceData/InferPiece.h>
#include <lite/Support/Log.h>
#include <lite/Support/JsonUtils.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QMutexLocker>
#include <QSaveFile>
#include <QUuid>

namespace InferCacheUtils {

    namespace {

        QMutex revisionMutex;
        QHash<QString, QByteArray> unpersistedRevisions;

        QString revisionPath() {
            return QDir(AppDataPaths::applicationData()).filePath(
                QStringLiteral("inference-model-revision"));
        }

        QByteArray newRevision(const QString &path) {
            const auto revision = QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
            QDir().mkpath(QFileInfo(path).absolutePath());
            QSaveFile file(path);
            if (file.open(QIODevice::WriteOnly) && file.write(revision) == revision.size() &&
                file.commit()) {
                unpersistedRevisions.remove(path);
            } else {
                // Keep the current session safe even if the profile cannot be written. Removing
                // the previous marker also prevents an old revision being reused after restart.
                QFile::remove(path);
                unpersistedRevisions.insert(path, revision);
                qWarning() << "Could not persist inference cache revision:" << path;
            }
            return revision;
        }

        QByteArray currentRevision(const QString &path) {
            if (unpersistedRevisions.contains(path))
                return unpersistedRevisions.value(path);
            QFile file(path);
            if (file.open(QIODevice::ReadOnly)) {
                const auto revision = file.readAll();
                if (!QUuid::fromString(QString::fromUtf8(revision)).isNull())
                    return revision;
            }
            return newRevision(path);
        }

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

    QString inputHashForCurrentModels(const QString &inputHash) {
        QMutexLocker lock(&revisionMutex);
        return QCryptographicHash::hash(inputHash.toUtf8() + ':' + currentRevision(revisionPath()),
                                        QCryptographicHash::Sha1).toHex();
    }

    void invalidateModelCaches() {
        QMutexLocker lock(&revisionMutex);
        newRevision(revisionPath());
    }

    CacheFileNames cacheFileNames(const QString &category, const QString &hash) {
        // Only the acoustic task renders audio; every other task stores its result as JSON.
        const auto outputSuffix =
            category == QStringLiteral("acoustic") ? QStringLiteral("wav") : QStringLiteral("json");
        return {
            QStringLiteral("infer-%1-input-%2.json").arg(category, hash),
            QStringLiteral("infer-%1-output-%2.%3").arg(category, hash, outputSuffix),
        };
    }

    JsonCacheLookup lookupJsonCache(const QString &category, const QString &cacheDirectory,
                                   const std::function<GenericInferModel()> &buildInput,
                                   const QString &keySuffix) {
        const QDir directory(cacheDirectory);
        if (!directory.exists())
            directory.mkpath(".");
        for (;;) {
            const auto generation = SynthrtEngine::instance().catalogGeneration();
            JsonCacheLookup lookup;
            lookup.input = buildInput();
            auto modelHash = lookup.input.hashData();
            if (!keySuffix.isEmpty())
                modelHash = QCryptographicHash::hash(
                                modelHash.toUtf8() + keySuffix.toUtf8(), QCryptographicHash::Sha1)
                                .toHex();
            lookup.inputHash = inputHashForCurrentModels(modelHash);
            const auto names = cacheFileNames(category, lookup.inputHash);
            const auto inputPath = directory.filePath(names.input);
            if (!QFile::exists(inputPath))
                JsonUtils::save(inputPath, lookup.input.serialize());
            lookup.outputCachePath = directory.filePath(names.output);
            if (QFile::exists(lookup.outputCachePath)) {
                QJsonObject object;
                lookup.hit = JsonUtils::load(lookup.outputCachePath, object) &&
                             lookup.output.deserialize(object);
            }
            if (lookup.inputHash == inputHashForCurrentModels(modelHash) &&
                generation == SynthrtEngine::instance().catalogGeneration())
                return lookup;
        }
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
