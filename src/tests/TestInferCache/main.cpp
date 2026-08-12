#include "Modules/Inference/Utils/InferCacheUtils.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QSemaphore>
#include <QTextStream>
#include <QTemporaryDir>

#include <thread>

namespace {
    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }

    bool testScanAndClean() {
        bool ok = true;
        QTemporaryDir dir;
        ok &= expect(dir.isValid(), "temporary dir is valid");

        const auto writeFile = [&dir](const char *name, int size) {
            QFile f(dir.filePath(name));
            if (!f.open(QIODevice::WriteOnly))
                return;
            f.write(QByteArray(size, 'x'));
        };
        writeFile("infer-acoustic-output-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.wav", 1000);
        writeFile("infer-acoustic-input-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.json", 200);
        writeFile("infer-duration-output-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb.json", 300);
        writeFile("unrelated.txt", 999);

        const auto stats = InferCacheUtils::scanCache(dir.path());
        ok &= expect(stats.files.size() == 3, "scan finds 3 cache files");
        ok &= expect(stats.totalBytes == 1500, "scan total bytes");

        InferCacheUtils::registerCacheFile(
            dir.filePath("infer-acoustic-output-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.wav"));
        InferCacheUtils::registerCacheFile(
            dir.filePath("infer-acoustic-input-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.json"));

        const auto result =
            InferCacheUtils::cleanCache(dir.path(), InferCacheUtils::registeredCacheFiles());
        ok &= expect(result.deletedCount == 1, "one file deleted");
        ok &= expect(result.deletedBytes == 300, "deleted bytes");
        ok &= expect(result.retainedActiveCount == 2, "two files retained as active");
        ok &= expect(!QFile::exists(dir.filePath(
                         "infer-duration-output-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb.json")),
                     "duration output removed");
        ok &= expect(QFile::exists(dir.filePath(
                         "infer-acoustic-output-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.wav")),
                     "registered wav kept");
        ok &= expect(QFile::exists(dir.filePath("unrelated.txt")), "unrelated file untouched");

        // 第二次清理：登记集合仍在 → 不再有可删文件
        const auto second =
            InferCacheUtils::cleanCache(dir.path(), InferCacheUtils::registeredCacheFiles());
        ok &= expect(second.deletedCount == 0, "second clean deletes nothing");
        return ok;
    }

    bool testKeyedWriteLocks() {
        bool ok = true;
        QSemaphore firstEntered;
        QSemaphore releaseFirst;
        QSemaphore secondEntered;
        std::thread first([&] {
            InferCacheUtils::CacheWriteGuard guard(QStringLiteral("shared"));
            firstEntered.release(2);
            releaseFirst.acquire();
        });
        std::thread second([&] {
            firstEntered.acquire();
            InferCacheUtils::CacheWriteGuard guard(QStringLiteral("shared"));
            secondEntered.release();
        });
        ok &= expect(firstEntered.tryAcquire(1, 1000), "first shared-key writer must enter");
        ok &= expect(!secondEntered.tryAcquire(1, 100),
                     "same cache key must serialize concurrent writers");
        releaseFirst.release();
        first.join();
        second.join();
        ok &= expect(secondEntered.tryAcquire(1), "second shared-key writer must eventually enter");

        QSemaphore distinctFirstEntered;
        QSemaphore releaseDistinctFirst;
        QSemaphore distinctSecondEntered;
        std::thread distinctFirst([&] {
            InferCacheUtils::CacheWriteGuard guard(QStringLiteral("first"));
            distinctFirstEntered.release(2);
            releaseDistinctFirst.acquire();
        });
        std::thread distinctSecond([&] {
            distinctFirstEntered.acquire();
            InferCacheUtils::CacheWriteGuard guard(QStringLiteral("second"));
            distinctSecondEntered.release();
        });
        ok &= expect(distinctFirstEntered.tryAcquire(1, 1000),
                     "first distinct-key writer must enter");
        ok &= expect(distinctSecondEntered.tryAcquire(1, 1000),
                     "different cache keys must remain concurrent");
        releaseDistinctFirst.release();
        distinctFirst.join();
        distinctSecond.join();
        return ok;
    }
} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    bool ok = true;
    ok &= testScanAndClean();
    ok &= testKeyedWriteLocks();
    return ok ? 0 : 1;
}
