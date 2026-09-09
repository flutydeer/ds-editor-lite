#include "Modules/Inference/Utils/InferCacheUtils.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QtTest/QTest>
#include <QFile>
#include <QTemporaryDir>

class InferCacheTests final : public QObject {
    Q_OBJECT

private slots:

    void scanAndClean() {
        QTemporaryDir dir;
        QVERIFY2(dir.isValid(), "temporary dir is valid");

        const auto writeFile = [&dir](const char *name, int size) {
            QFile f(dir.filePath(name));
            return f.open(QIODevice::WriteOnly) && f.write(QByteArray(size, 'x')) == size;
        };
        QVERIFY(
            writeFile("infer-acoustic-output-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.wav", 1000));
        QVERIFY(
            writeFile("infer-acoustic-input-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.json", 200));
        QVERIFY(
            writeFile("infer-duration-output-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb.json", 300));
        QVERIFY(writeFile("unrelated.txt", 999));

        const auto stats = InferCacheUtils::scanCache(dir.path());
        QCOMPARE(stats.files.size(), 3);
        QCOMPARE(stats.totalBytes, 1500);

        InferCacheUtils::registerCacheFile(
            dir.filePath("infer-acoustic-output-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.wav"));
        InferCacheUtils::registerCacheFile(
            dir.filePath("infer-acoustic-input-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.json"));

        const auto result =
            InferCacheUtils::cleanCache(dir.path(), InferCacheUtils::registeredCacheFiles());
        QCOMPARE(result.deletedCount, 1);
        QCOMPARE(result.deletedBytes, 300);
        QCOMPARE(result.retainedActiveCount, 2);
        QVERIFY2(!QFile::exists(dir.filePath(
                     "infer-duration-output-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb.json")),
                 "duration output removed");
        QVERIFY2(QFile::exists(dir.filePath(
                     "infer-acoustic-output-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.wav")),
                 "registered wav kept");
        QVERIFY2(QFile::exists(dir.filePath("unrelated.txt")), "unrelated file untouched");

        // Active registrations protect the remaining files during subsequent cleanup.
        const auto second =
            InferCacheUtils::cleanCache(dir.path(), InferCacheUtils::registeredCacheFiles());
        QCOMPARE(second.deletedCount, 0);
    }

    void clearRegisteredFiles() {
        // The registry is process-global; start from a clean state regardless of
        // what previous test cases registered
        InferCacheUtils::clearRegisteredCacheFiles();

        QTemporaryDir dir;
        QVERIFY2(dir.isValid(), "temporary dir is valid");
        const char *fileName =
            "infer-variance-output-dddddddddddddddddddddddddddddddddddddddd.json";
        QFile f(dir.filePath(fileName));
        QVERIFY2(f.open(QIODevice::WriteOnly), "cache file created");
        QCOMPARE(f.write(QByteArray(120, 'x')), qint64{120});
        f.close();

        InferCacheUtils::registerCacheFile(dir.filePath(fileName));
        const auto before =
            InferCacheUtils::cleanCache(dir.path(), InferCacheUtils::registeredCacheFiles());
        QCOMPARE(before.retainedActiveCount, 1);
        QCOMPARE(before.deletedCount, 0);

        InferCacheUtils::clearRegisteredCacheFiles();
        QVERIFY2(InferCacheUtils::registeredCacheFiles().isEmpty(),
                 "registry empty after clearRegisteredCacheFiles");

        // Same scenario as replacing the document: stale registrations must no
        // longer keep the previous project's cache file alive
        const auto after =
            InferCacheUtils::cleanCache(dir.path(), InferCacheUtils::registeredCacheFiles());
        QCOMPARE(after.deletedCount, 1);
        QVERIFY2(!QFile::exists(dir.filePath(fileName)), "file removed from disk after clear");
    }
};

QTEST_GUILESS_MAIN(InferCacheTests)
#include "main.moc"
