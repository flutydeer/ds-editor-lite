#include "tst_application_workflows.h"

#include "Model/AppOptions/AppOptions.h"

#include <lite/History/HistoryManager.h>
#include <lite/PackageManager/PackageManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>
#include <lite/Tasking/TaskManager.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QtTest/QTest>

#include <algorithm>
#include <filesystem>

void ApplicationWorkflowTests::packageRefreshPreservesCatalogAndReportsInvalidRoots() {
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->packageModuleStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    const auto originalPaths = context->m_appOptions->general()->packageSearchPaths;
    const auto original = packageManager->installedPackages();
    QVERIFY(!original.successfulPackages.isEmpty());
    SingerInfo singer;
    for (const auto &package : original.successfulPackages) {
        if (!package.singers().isEmpty()) {
            singer = package.singers().first();
            break;
        }
    }
    QVERIFY(!singer.isEmpty());
    QSignalSpy refreshed(packageManager, &PackageManager::packagesRefreshed);
    QTemporaryDir emptyDirectory;
    QVERIFY(emptyDirectory.isValid());
    {
        const auto restore = qScopeGuard([&] {
            const auto result = packageManager->refreshInstalledPackages(originalPaths);
            QVERIFY2(result, qPrintable(result ? QString{} : result.getError().message));
        });
        const auto missingRoot = emptyDirectory.filePath(QStringLiteral("missing"));
        const auto invalidPackage = emptyDirectory.filePath(QStringLiteral("broken@1.0.0"));
        QVERIFY(QDir().mkpath(invalidPackage));
        {
            QFile manifest(QDir(invalidPackage).filePath(QStringLiteral("desc.json")));
            QVERIFY(manifest.open(QIODevice::WriteOnly));
            QCOMPARE(manifest.write("invalid manifest"), qint64(16));
        }
        const auto before = runtime().documentVersion();
        const auto *undo = historyManager->nextUndoEntry();
        auto searchPaths = originalPaths;
        searchPaths.append(missingRoot);
        searchPaths.append(emptyDirectory.path());
        const auto partial = packageManager->refreshInstalledPackages(searchPaths);
        QVERIFY2(partial, qPrintable(partial ? QString{} : partial.getError().message));
        QCOMPARE(partial.get().successfulPackages, original.successfulPackages);
        const auto &failures = partial.get().failedPackages;
        for (const auto &path : {missingRoot, invalidPackage}) {
            const auto failed =
                std::find_if(failures.cbegin(), failures.cend(), [&](const auto &failure) {
                    return QDir::fromNativeSeparators(failure.path) == path ||
                           QDir::fromNativeSeparators(failure.reason).contains(path);
                });
            QVERIFY2(failed != failures.cend(), qPrintable(path));
            QVERIFY(!failed->reason.isEmpty());
        }
        QCOMPARE(packageManager->findSingerByIdentifier(singer.identifier()), singer);
        QCOMPARE(runtime().documentVersion(), before);
        QCOMPARE(historyManager->nextUndoEntry(), undo);
        QCOMPARE(refreshed.size(), 1);
        refreshed.clear();
        QVERIFY(QDir().mkdir(missingRoot));
        const auto sessionBefore = SynthrtEngine::instance().session().snapshot();
        QVERIFY(sessionBefore);
        const auto canceled =
            packageManager->refreshInstalledPackages({missingRoot}, [] { return false; });
        QVERIFY2(canceled, qPrintable(canceled ? QString{} : canceled.getError().message));
        QVERIFY(canceled.get().successfulPackages.isEmpty());
        QCOMPARE(packageManager->installedPackages().successfulPackages,
                 original.successfulPackages);
        QCOMPARE(packageManager->findSingerByIdentifier(singer.identifier()), singer);
        QVERIFY(refreshed.isEmpty());
        const auto sessionAfter = SynthrtEngine::instance().session().snapshot();
        QVERIFY(sessionAfter);
        QCOMPARE(sessionAfter->generation, sessionBefore->generation);
        QCOMPARE(sessionAfter->catalogFingerprint, sessionBefore->catalogFingerprint);
    }
    QCOMPARE(refreshed.size(), 1);
    QCOMPARE(packageManager->installedPackages().successfulPackages, original.successfulPackages);
    QCOMPARE(packageManager->installedPackages().failedPackages.size(),
             original.failedPackages.size());
    QCOMPARE(packageManager->findSingerByIdentifier(singer.identifier()), singer);
    QTRY_VERIFY(taskManager->tasks().isEmpty());
}

void ApplicationWorkflowTests::localizedPackageMetadataLoadsAndUpdatesWithTheVersion() {
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->packageModuleStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    const auto originalPaths = context->m_appOptions->general()->packageSearchPaths;
    const auto original = packageManager->installedPackages().successfulPackages;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = directory.filePath(QStringLiteral("ci-fixture@1.0.0"));
    std::error_code error;
    std::filesystem::copy(std::filesystem::u8path(LITE_TEST_VOICEBANK_ROOT),
                          std::filesystem::u8path(root.toUtf8().constData()),
                          std::filesystem::copy_options::recursive, error);
    QVERIFY2(!error, qPrintable(QString::fromStdString(error.message())));
    const auto restore = qScopeGuard([&] {
        const auto result = packageManager->refreshInstalledPackages(originalPaths);
        QVERIFY2(result, qPrintable(result ? QString{} : result.getError().message));
        QCOMPARE(packageManager->installedPackages().successfulPackages, original);
    });
    const auto rewrite = [&](const QString &relativePath, auto amend) {
        QFile file(QDir(root).filePath(relativePath));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto document = QJsonDocument::fromJson(file.readAll());
        QVERIFY(document.isObject());
        auto object = document.object();
        amend(object);
        file.close();
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const auto bytes = QJsonDocument(object).toJson();
        QCOMPARE(file.write(bytes), qint64(bytes.size()));
    };
    const auto localized = [](const QString &fallback, const QString &chinese) {
        return QJsonObject{
            {QStringLiteral("_"),     fallback},
            {QStringLiteral("zh-CN"), chinese }
        };
    };
    rewrite(QStringLiteral("desc.json"), [&](QJsonObject &object) {
        object.insert(QStringLiteral("vendor"),
                      localized(QStringLiteral("Fixture vendor"), QStringLiteral("测试制作方")));
        object.insert(QStringLiteral("description"),
                      localized(QStringLiteral("Catalog fixture"), QStringLiteral("目录测试资源")));
    });
    if (QTest::currentTestFailed())
        return;
    rewrite(QStringLiteral("characters/fixture/config.json"), [&](QJsonObject &object) {
        object.insert(QStringLiteral("name"),
                      localized(QStringLiteral("Fixture singer"), QStringLiteral("测试歌手")));
        auto configuration = object.value(QStringLiteral("configuration")).toObject();
        auto speakers = configuration.value(QStringLiteral("speakers")).toArray();
        QVERIFY(!speakers.isEmpty());
        auto speaker = speakers.first().toObject();
        speaker.insert(QStringLiteral("name"),
                       localized(QStringLiteral("Clear"), QStringLiteral("清亮声线")));
        speakers[0] = speaker;
        configuration.insert(QStringLiteral("speakers"), speakers);
        auto languages = configuration.value(QStringLiteral("languages")).toArray();
        QVERIFY(!languages.isEmpty());
        auto language = languages.first().toObject();
        language.insert(QStringLiteral("name"),
                        localized(QStringLiteral("Mandarin"), QStringLiteral("普通话")));
        languages[0] = language;
        configuration.insert(QStringLiteral("languages"), languages);
        object.insert(QStringLiteral("configuration"), configuration);
    });
    if (QTest::currentTestFailed())
        return;
    const auto before = runtime().documentVersion();
    const auto project = TestSupport::projectSnapshot(*context->m_appModel);
    const auto *undo = historyManager->nextUndoEntry();
    const QList<QPair<QString, QString>> releases{
        {QStringLiteral("1.0.0"), QStringLiteral("测试制作方")    },
        {QStringLiteral("1.0.1"), QStringLiteral("更新后的制作方")},
    };
    for (const auto &[version, vendor] : releases) {
        rewrite(QStringLiteral("desc.json"), [&](QJsonObject &object) {
            object.insert(QStringLiteral("version"), version);
            object.insert(QStringLiteral("vendor"),
                          localized(QStringLiteral("Fixture vendor"), vendor));
        });
        if (QTest::currentTestFailed())
            return;
        const auto result = packageManager->refreshInstalledPackages({root});
        QVERIFY2(result, qPrintable(result ? QString{} : result.getError().message));
        const auto &packages = result.get().successfulPackages;
        const auto package = std::find_if(packages.cbegin(), packages.cend(), [](const auto &item) {
            return item.id() == QStringLiteral("ci-fixture");
        });
        QVERIFY(package != packages.cend());
        QCOMPARE(package->version().normalized(), QVersionNumber::fromString(version).normalized());
        QCOMPARE(package->displayVendor(QStringLiteral("zh-CN")), vendor);
        QCOMPARE(package->displayDescription(QStringLiteral("zh-CN")),
                 QStringLiteral("目录测试资源"));
        QVERIFY(!package->singers().isEmpty());
        const auto singer = package->singers().first();
        QCOMPARE(singer.displayName(QStringLiteral("zh-CN")), QStringLiteral("测试歌手"));
        QCOMPARE(singer.displayName(QStringLiteral("de")), QStringLiteral("Fixture singer"));
        QVERIFY(!singer.speakers().isEmpty() && !singer.languages().isEmpty());
        QCOMPARE(singer.speakers().first().displayName(QStringLiteral("zh-CN")),
                 QStringLiteral("清亮声线"));
        QCOMPARE(singer.languages().first().displayName(QStringLiteral("zh-CN")),
                 QStringLiteral("普通话"));
        QCOMPARE(packageManager->findSingerByIdentifier(singer.identifier()), singer);
        QCOMPARE(runtime().documentVersion(), before);
        QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), project);
        QCOMPARE(historyManager->nextUndoEntry(), undo);
    }
}
