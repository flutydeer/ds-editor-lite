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
} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    bool ok = true;
    ok &= testScanAndClean();
    return ok ? 0 : 1;
}
