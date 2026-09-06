#include "Modules/Inference/Utils/InferCacheUtils.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QTemporaryDir>

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

    bool testClearRegisteredFiles() {
        bool ok = true;
        // The registry is process-global; start from a clean state regardless of
        // what previous test cases registered
        InferCacheUtils::clearRegisteredCacheFiles();

        QTemporaryDir dir;
        ok &= expect(dir.isValid(), "temporary dir is valid");
        const char *fileName = "infer-variance-output-dddddddddddddddddddddddddddddddddddddddd.json";
        QFile f(dir.filePath(fileName));
        ok &= expect(f.open(QIODevice::WriteOnly), "cache file created");
        f.write(QByteArray(120, 'x'));
        f.close();

        InferCacheUtils::registerCacheFile(dir.filePath(fileName));
        const auto before =
            InferCacheUtils::cleanCache(dir.path(), InferCacheUtils::registeredCacheFiles());
        ok &= expect(before.retainedActiveCount == 1 && before.deletedCount == 0,
                     "registered file retained while the registry holds it");

        InferCacheUtils::clearRegisteredCacheFiles();
        ok &= expect(InferCacheUtils::registeredCacheFiles().isEmpty(),
                     "registry empty after clearRegisteredCacheFiles");

        // Same scenario as replacing the document: stale registrations must no
        // longer keep the previous project's cache file alive
        const auto after =
            InferCacheUtils::cleanCache(dir.path(), InferCacheUtils::registeredCacheFiles());
        ok &= expect(after.deletedCount == 1, "file deleted after registry clear");
        ok &= expect(!QFile::exists(dir.filePath(fileName)), "file removed from disk after clear");
        return ok;
    }
} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    bool ok = true;
    ok &= testScanAndClean();
    ok &= testClearRegisteredFiles();
    return ok ? 0 : 1;
}
