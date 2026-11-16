#ifndef INFERCACHEUTILS_H
#define INFERCACHEUTILS_H

#include <QSet>
#include <QString>

#include <QList>

namespace InferCacheUtils {

    struct CacheFileInfo {
        QString fileName;
        QString category; // "acoustic" | "duration" | "pitch" | "variance"
        bool isInput = false;
        qint64 size = 0;
    };

    struct CacheStats {
        QList<CacheFileInfo> files;
        qint64 totalBytes = 0;
    };

    struct CleanResult {
        int deletedCount = 0;
        qint64 deletedBytes = 0;
        int retainedActiveCount = 0; // Registered or referenced by the current project.
        int retainedLockedCount = 0; // Removal failed, for example for an in-use or read-only file.
    };

    // Worker-safe. A missing cache directory returns empty statistics.
    CacheStats scanCache(const QString &cacheDir);

    // Register the absolute path of a cache file on the main thread when its task completes.
    void registerCacheFile(const QString &absolutePath);

    // Process-wide set of registered absolute paths. Access from the main thread.
    QSet<QString> registeredCacheFiles();

    // Clear the registered set (main thread only). Called when the document is
    // replaced: the old document's undo history is reset and its infer pipelines
    // destroyed, so every pre-replace registration is stale and must be dropped,
    // otherwise the previous project's cache files would be retained forever by
    // later cleanups. Files still referenced by the new project stay protected
    // via the model scan in collectActiveCacheFiles().
    void clearRegisteredCacheFiles();

    // Main-thread snapshot of registered paths and paths used by successful project pieces.
    QSet<QString> collectActiveCacheFiles();

    // Worker-safe. Remove cache files outside the supplied active set.
    CleanResult cleanCache(const QString &cacheDir, const QSet<QString> &activeFiles);

} // namespace InferCacheUtils

#endif // INFERCACHEUTILS_H
