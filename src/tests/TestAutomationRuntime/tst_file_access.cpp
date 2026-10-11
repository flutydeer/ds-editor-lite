#include "tst_automation_runtime.h"

#include "Automation/Public/AutomationFileGuard.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

namespace {
    struct FileAccessFixture {
        QTemporaryDir temporary;
        Automation::AutomationFileGuard guard;
        QString readRoot = temporary.filePath("read");
        QString writeRoot = temporary.filePath("write");
        QString outsideRoot = temporary.filePath("read-other");

        static bool createFile(const QString &path) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write("fixture") == 7;
        }

        bool initialize() {
            return temporary.isValid() && QDir().mkpath(readRoot) && QDir().mkpath(writeRoot) &&
                   QDir().mkpath(outsideRoot) && createFile(readRoot + "/song.dspx") &&
                   createFile(outsideRoot + "/outside.dspx") &&
                   createFile(outsideRoot + "/neighbor.dspx") &&
                   bool(guard.setConfiguredRoots({readRoot, writeRoot}));
        }
    };
}

void AutomationRuntimeTests::existingReadUsesCanonicalPath() {
    FileAccessFixture fixture;
    QVERIFY(fixture.initialize());
    const auto path = fixture.readRoot + "/song.dspx";
    const auto result = fixture.guard.authorize(path, Automation::FileAccessPurpose::Read);
    QVERIFY(result);
    QCOMPARE(QFileInfo(result.get().canonicalPath).canonicalFilePath(),
             QFileInfo(path).canonicalFilePath());
#ifdef Q_OS_WIN
    QVERIFY(fixture.guard.authorize(path.toUpper(), Automation::FileAccessPurpose::Read));
#endif
}

void AutomationRuntimeTests::rejectedRead_data() {
    QTest::addColumn<QString>("relativePath");
    QTest::addColumn<int>("error");
    QTest::newRow("missing") << QString("read/missing.dspx")
                             << int(Automation::AutomationErrorCode::FileNotFound);
    QTest::newRow("same-prefix-sibling") << QString("read-other/outside.dspx")
                                         << int(Automation::AutomationErrorCode::PermissionDenied);
    QTest::newRow("parent-traversal") << QString("read/../read-other/outside.dspx")
                                      << int(Automation::AutomationErrorCode::PermissionDenied);
}

void AutomationRuntimeTests::rejectedRead() {
    FileAccessFixture fixture;
    QVERIFY(fixture.initialize());
    QFETCH(QString, relativePath);
    QFETCH(int, error);
    const auto result = fixture.guard.authorize(fixture.temporary.filePath(relativePath),
                                                Automation::FileAccessPurpose::Read);
    QVERIFY(!result);
    QCOMPARE(int(result.getError().code), error);
}

void AutomationRuntimeTests::nonexistentWriteUsesExistingParent() {
    FileAccessFixture fixture;
    QVERIFY(fixture.initialize());
    const auto result = fixture.guard.authorize(fixture.writeRoot + "/nested/render.wav",
                                                Automation::FileAccessPurpose::Write);
    QVERIFY(result);
    QVERIFY(result.get().canonicalPath.endsWith("/write/nested/render.wav", Qt::CaseInsensitive));
    QVERIFY(fixture.guard.authorize(fixture.readRoot + "/render.wav",
                                    Automation::FileAccessPurpose::Write));
}

void AutomationRuntimeTests::revocationIsObservedAtReauthorization() {
    FileAccessFixture fixture;
    QVERIFY(fixture.initialize());
    const auto result = fixture.guard.authorize(fixture.writeRoot + "/render.wav",
                                                Automation::FileAccessPurpose::Write);
    QVERIFY(result);
    QVERIFY(fixture.guard.reauthorize(result.get()));
    QVERIFY(fixture.guard.setConfiguredRoots({fixture.readRoot, fixture.outsideRoot}));
    const auto revoked = fixture.guard.reauthorize(result.get());
    QVERIFY(!revoked);
    QCOMPARE(revoked.getError().code, Automation::AutomationErrorCode::PermissionDenied);
}

void AutomationRuntimeTests::fileGrantDoesNotGrantItsDirectory() {
    FileAccessFixture fixture;
    QVERIFY(fixture.initialize());
    const auto path = fixture.outsideRoot + "/outside.dspx";
    QVERIFY(fixture.guard.addSessionGrant(path, Automation::FileAccessPurpose::Read));
    QVERIFY(fixture.guard.authorize(path, Automation::FileAccessPurpose::Read));
    const auto neighbor = fixture.guard.authorize(fixture.outsideRoot + "/neighbor.dspx",
                                                  Automation::FileAccessPurpose::Read);
    QVERIFY(!neighbor);
    QCOMPARE(neighbor.getError().code, Automation::AutomationErrorCode::PermissionDenied);
    const auto snapshot = fixture.guard.snapshot();
    QCOMPARE(snapshot.accessRoots.size(), 2);
    QCOMPARE(snapshot.sessionReadGrants.size(), 1);
    QVERIFY(snapshot.sessionWriteGrants.isEmpty());
}

void AutomationRuntimeTests::relativePathIsInvalid() {
    FileAccessFixture fixture;
    QVERIFY(fixture.initialize());
    const auto result =
        fixture.guard.authorize("relative.dspx", Automation::FileAccessPurpose::Read);
    QVERIFY(!result);
    QCOMPARE(result.getError().code, Automation::AutomationErrorCode::InvalidArgument);
}
