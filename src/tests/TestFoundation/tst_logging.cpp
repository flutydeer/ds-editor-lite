#include "tst_foundation.h"

#include <lite/Support/Log.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include <iostream>

namespace {
    QByteArray logContents(const QString &directory) {
        QByteArray result;
        const QDir root(directory);
        for (const auto &name : root.entryList({QStringLiteral("*.log")}, QDir::Files)) {
            QFile file(root.filePath(name));
            if (file.open(QIODevice::ReadOnly))
                result += file.readAll();
        }
        return result;
    }
}

int runLogFixture(const QString &directory) {
    const QDir root(directory);
    const auto first = root.filePath(QStringLiteral("first"));
    const auto recovered = root.filePath(QStringLiteral("recovered"));
    Log::setLogDirectory(first);
    Log::i(QStringLiteral("fixture"), QStringLiteral("before-directory-change"));
    if (Log::logDirectory() != first)
        return 2;

    QFile blocker(root.filePath(QStringLiteral("not-a-directory")));
    if (!blocker.open(QIODevice::WriteOnly))
        return 3;
    blocker.close();
    Log::setLogDirectory(blocker.fileName());
    if (Log::logDirectory() != first)
        return 4;
    Log::w(QStringLiteral("fixture"), QStringLiteral("after-rejected-directory"));

    const auto currentFiles = QDir(first).entryList({QStringLiteral("*.log")}, QDir::Files);
    if (currentFiles.size() != 1)
        return 5;
    const auto unwritable = root.filePath(QStringLiteral("unwritable"));
    if (!QDir().mkpath(QDir(unwritable).filePath(currentFiles.first())))
        return 6;
    Log::setLogDirectory(unwritable);
    Log::e(QStringLiteral("fixture"), QStringLiteral("console-survives-file-open-failure"));
    Log::setLogDirectory(recovered);
    Log::i(QStringLiteral("fixture"), QStringLiteral("after-directory-recovery"));
    std::cerr << "captured-third-party-message" << std::endl;
    return Log::logDirectory() == recovered ? 0 : 7;
}

void FoundationTests::fileLoggingChangesDirectoriesAndRecoversFromWriteFailure() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto recovered = directory.filePath(QStringLiteral("recovered"));
    QVERIFY(QDir().mkpath(recovered));
    for (int index = 0; index < 13; ++index) {
        QFile oldLog(QDir(recovered).filePath(QStringLiteral("old-%1.log").arg(index)));
        QVERIFY(oldLog.open(QIODevice::WriteOnly));
        QVERIFY(oldLog.write("old log\n") > 0);
        QVERIFY(oldLog.setFileTime(QDateTime::fromSecsSinceEpoch(946684800 + index),
                                   QFileDevice::FileModificationTime));
    }
    QFile unrelated(QDir(recovered).filePath(QStringLiteral("notes.txt")));
    QVERIFY(unrelated.open(QIODevice::WriteOnly));
    QVERIFY(unrelated.write("retained note\n") > 0);
    unrelated.close();

    QProcess child;
    child.start(QCoreApplication::applicationFilePath(),
                {QStringLiteral("--log-fixture"), directory.path()});
    QVERIFY2(child.waitForStarted(), qPrintable(child.errorString()));
    QVERIFY2(child.waitForFinished(10000), qPrintable(child.errorString()));
    QCOMPARE(child.exitStatus(), QProcess::NormalExit);
    const auto output = child.readAllStandardOutput();
    QCOMPARE(child.exitCode(), 0);
    QVERIFY(output.contains("console-survives-file-open-failure"));
    const auto firstLog = logContents(directory.filePath(QStringLiteral("first")));
    QVERIFY(firstLog.contains("before-directory-change"));
    QVERIFY(firstLog.contains("after-rejected-directory"));
    QVERIFY(!firstLog.contains("after-directory-recovery"));
    const auto recoveredLog = logContents(recovered);
    QVERIFY(recoveredLog.contains("after-directory-recovery"));
    QVERIFY(recoveredLog.contains("captured-third-party-message"));
    QVERIFY(!recoveredLog.contains("before-directory-change"));
    QVERIFY(!QFileInfo::exists(QDir(recovered).filePath(QStringLiteral("old-0.log"))));
    QVERIFY(QFileInfo::exists(QDir(recovered).filePath(QStringLiteral("old-12.log"))));
    QVERIFY(QFileInfo::exists(unrelated.fileName()));
}
