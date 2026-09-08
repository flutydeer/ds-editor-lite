#include "Automation/Public/AutomationFileGuard.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

class TestAutomationFileGuard final : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> temporary;
    std::unique_ptr<Automation::AutomationFileGuard> guard;
    QString readRoot;
    QString writeRoot;
    QString outsideRoot;

    static bool createFile(const QString &path) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write("fixture") == 7;
    }

private slots:

    void init() {
        temporary = std::make_unique<QTemporaryDir>();
        QVERIFY(temporary->isValid());
        readRoot = temporary->filePath("read");
        writeRoot = temporary->filePath("write");
        outsideRoot = temporary->filePath("read-other");
        QVERIFY(QDir().mkpath(readRoot));
        QVERIFY(QDir().mkpath(writeRoot));
        QVERIFY(QDir().mkpath(outsideRoot));
        QVERIFY(createFile(readRoot + "/song.dspx"));
        QVERIFY(createFile(outsideRoot + "/outside.dspx"));
        QVERIFY(createFile(outsideRoot + "/neighbor.dspx"));
        guard = std::make_unique<Automation::AutomationFileGuard>();
        QVERIFY(guard->setConfiguredRoots({readRoot, writeRoot}));
    }

    void cleanup() {
        guard.reset();
        temporary.reset();
    }

    void existingReadUsesCanonicalPath() {
        const auto path = readRoot + "/song.dspx";
        const auto result = guard->authorize(path, Automation::FileAccessPurpose::Read);
        QVERIFY(result);
        QCOMPARE(QFileInfo(result.get().canonicalPath).canonicalFilePath(),
                 QFileInfo(path).canonicalFilePath());
#ifdef Q_OS_WIN
        QVERIFY(guard->authorize(path.toUpper(), Automation::FileAccessPurpose::Read));
#endif
    }

    void rejectedRead_data() {
        QTest::addColumn<QString>("relativePath");
        QTest::addColumn<int>("error");
        QTest::newRow("missing") << QString("read/missing.dspx")
                                 << int(Automation::AutomationErrorCode::FileNotFound);
        QTest::newRow("same-prefix-sibling")
            << QString("read-other/outside.dspx")
            << int(Automation::AutomationErrorCode::PermissionDenied);
        QTest::newRow("parent-traversal") << QString("read/../read-other/outside.dspx")
                                          << int(Automation::AutomationErrorCode::PermissionDenied);
    }

    void rejectedRead() {
        QFETCH(QString, relativePath);
        QFETCH(int, error);
        const auto result = guard->authorize(temporary->filePath(relativePath),
                                             Automation::FileAccessPurpose::Read);
        QVERIFY(!result);
        QCOMPARE(int(result.getError().code), error);
    }

    void nonexistentWriteUsesExistingParent() {
        const auto result = guard->authorize(writeRoot + "/nested/render.wav",
                                             Automation::FileAccessPurpose::Write);
        QVERIFY(result);
        QVERIFY(
            result.get().canonicalPath.endsWith("/write/nested/render.wav", Qt::CaseInsensitive));
        QVERIFY(guard->authorize(readRoot + "/render.wav", Automation::FileAccessPurpose::Write));
    }

    void revocationIsObservedAtReauthorization() {
        const auto result =
            guard->authorize(writeRoot + "/render.wav", Automation::FileAccessPurpose::Write);
        QVERIFY(result);
        QVERIFY(guard->reauthorize(result.get()));
        QVERIFY(guard->setConfiguredRoots({readRoot, outsideRoot}));
        const auto revoked = guard->reauthorize(result.get());
        QVERIFY(!revoked);
        QCOMPARE(revoked.getError().code, Automation::AutomationErrorCode::PermissionDenied);
    }

    void fileGrantDoesNotGrantItsDirectory() {
        const auto path = outsideRoot + "/outside.dspx";
        QVERIFY(guard->addSessionGrant(path, Automation::FileAccessPurpose::Read));
        QVERIFY(guard->authorize(path, Automation::FileAccessPurpose::Read));
        const auto neighbor =
            guard->authorize(outsideRoot + "/neighbor.dspx", Automation::FileAccessPurpose::Read);
        QVERIFY(!neighbor);
        QCOMPARE(neighbor.getError().code, Automation::AutomationErrorCode::PermissionDenied);
        const auto snapshot = guard->snapshot();
        QCOMPARE(snapshot.accessRoots.size(), 2);
        QCOMPARE(snapshot.sessionReadGrants.size(), 1);
        QVERIFY(snapshot.sessionWriteGrants.isEmpty());
    }

    void relativePathIsInvalid() {
        const auto result = guard->authorize("relative.dspx", Automation::FileAccessPurpose::Read);
        QVERIFY(!result);
        QCOMPARE(result.getError().code, Automation::AutomationErrorCode::InvalidArgument);
    }
};

QTEST_GUILESS_MAIN(TestAutomationFileGuard)
#include "main.moc"
