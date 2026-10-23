#include "tst_application_workflows.h"
#include "../TestSupport/VoicebankFixture.h"

#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/Utils/InferCacheUtils.h"
#include "Modules/FillLyric/Utils/G2pService.h"
#include "Automation/PackageAutomationAdapter.h"

#include <lite/History/HistoryManager.h>
#include <lite/PackageManager/PackageManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>
#include <lite/SynthrtEngine/SingerPipeline.h>
#include <lite/Tasking/TaskManager.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSemaphore>
#include <QtTest/QTest>

#include <algorithm>
#include <filesystem>
#include <atomic>
#include <future>
#include <chrono>

namespace {
    bool copyTree(const QString &from, const QString &to) {
        if (!QDir().mkpath(to)) {
            return false;
        }
        const QDir source(from);
        for (const auto &entry : source.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
            if (!QFile::copy(entry.absoluteFilePath(), QDir(to).filePath(entry.fileName()))) {
                return false;
            }
        }
        for (const auto &entry : source.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (!copyTree(entry.absoluteFilePath(), QDir(to).filePath(entry.fileName()))) {
                return false;
            }
        }
        return true;
    }

    QJsonObject readJsonObject(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QJsonDocument::fromJson(file.readAll()).object();
    }

    bool writeJsonObject(const QString &path, const QJsonObject &value) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        return file.write(QJsonDocument(value).toJson()) > 0;
    }
}

void ApplicationWorkflowTests::defaultLyricComesFromTheVoicebank_data() {
    QTest::addColumn<bool>("inferenceLanguageTarget");
    QTest::newRow("linguist-metadata") << false;
    QTest::newRow("non-linguist-language-target") << true;
}

void ApplicationWorkflowTests::defaultLyricComesFromTheVoicebank() {
    QFETCH(bool, inferenceLanguageTarget);
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->packageModuleStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    const auto originalPaths = context->m_appOptions->general()->packageSearchPaths;
    const auto restore = qScopeGuard([&] {
        const auto result = packageManager->refreshInstalledPackages(originalPaths);
        QVERIFY2(result, qPrintable(result ? QString{} : result.getError().message));
    });

    // A default lyric is stated by the voicebank, so the measurement needs one that declares it. The
    // fixture in the tree is copied and doctored rather than changed, and the copy receives a
    // different package id so it cannot be mistaken for the fixture itself.
    QTemporaryDir staging;
    QVERIFY(staging.isValid());
    const auto doctored = staging.filePath(QStringLiteral("default-lyric-fixture@1.0.0"));
    QVERIFY2(copyTree(TestSupport::voicebankRoot(), doctored),
             qPrintable(QStringLiteral("The fixture voicebank must be copyable to %1").arg(doctored)));
    auto description = readJsonObject(doctored + QStringLiteral("/desc.json"));
    QVERIFY(!description.isEmpty());
    description.insert(QStringLiteral("id"), QStringLiteral("default-lyric-fixture"));
    QVERIFY(writeJsonObject(doctored + QStringLiteral("/desc.json"), description));
    for (const auto *module : {"cmn-pinyin", "eng-arpabet"}) {
        const auto path =
            QStringLiteral("%1/linguists/%2/linguist.json").arg(doctored, QString::fromLatin1(module));
        auto declaration = readJsonObject(path);
        QVERIFY2(!declaration.isEmpty(), qPrintable(QStringLiteral("Cannot read %1").arg(path)));
        // Two different values, so the measurement cannot pass by reading one language twice.
        declaration.insert(QStringLiteral("defaultLyric"),
                           QString::fromLatin1(module) == QStringLiteral("eng-arpabet")
                               ? QJsonValue(QStringLiteral("laa"))
                               : QJsonValue(QStringLiteral("la")));
        QVERIFY(writeJsonObject(path, declaration));
    }

    if (inferenceLanguageTarget) {
        const auto path = doctored + QStringLiteral("/characters/fixture/config.json");
        auto declaration = readJsonObject(path);
        auto imports = declaration.value(QStringLiteral("imports")).toArray();
        bool changed = false;
        for (qsizetype index = 0; index < imports.size(); ++index) {
            auto entry = imports[index].toObject();
            if (entry.value(QStringLiteral("role")) == QStringLiteral("lang/eng")) {
                entry.insert(QStringLiteral("ref"), QStringLiteral(":inference/acoustic"));
                imports[index] = entry;
                changed = true;
            }
        }
        QVERIFY(changed);
        declaration.insert(QStringLiteral("imports"), imports);
        QVERIFY(writeJsonObject(path, declaration));
    }

    const auto refreshed = packageManager->refreshInstalledPackages(
        {staging.path(), TestSupport::voicebankRoot()});
    QVERIFY2(refreshed, qPrintable(refreshed ? QString{} : refreshed.getError().message));

    SingerInfo declared;
    SingerInfo fixture;
    for (const auto &package : packageManager->installedPackages().successfulPackages) {
        for (const auto &candidate : package.singers()) {
            if (package.id() == QStringLiteral("default-lyric-fixture")) {
                declared = candidate;
            } else if (package.id().startsWith(QStringLiteral("ci-fixture"))) {
                fixture = candidate;
            }
        }
    }
    QVERIFY(!fixture.isEmpty());
    if (inferenceLanguageTarget) {
        QVERIFY(declared.isEmpty());
        const auto &failures = refreshed.get().failedPackages;
        QVERIFY(std::any_of(failures.cbegin(), failures.cend(), [&](const auto &failure) {
            return QDir::fromNativeSeparators(failure.path) == doctored &&
                   failure.reason.contains(QStringLiteral("not a linguist contribution"));
        }));
        return;
    }
    QVERIFY2(!declared.isEmpty(), "The doctored package must be installed and contribute a singer");

    // The editor reads the default lyric each language of the voicebank declares. This is the whole
    // read path of the editor, and nothing else consumes it yet.
    QCOMPARE(declared.defaultLyricForLanguage(QStringLiteral("cmn")), QStringLiteral("la"));
    QCOMPARE(declared.defaultLyricForLanguage(QStringLiteral("eng")), QStringLiteral("laa"));
    // A language the modules do not declare stays empty, and so does a voicebank that declares none.
    QCOMPARE(declared.defaultLyricForLanguage(QStringLiteral("jpn")), QString());
    QCOMPARE(fixture.defaultLyricForLanguage(QStringLiteral("cmn")), QString());
    QCOMPARE(fixture.defaultLyricForLanguage(QStringLiteral("eng")), QString());
}

void ApplicationWorkflowTests::packageRefreshPreservesCatalogAndReportsInvalidRoots() {
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->packageModuleStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    const auto originalPaths = context->m_appOptions->general()->packageSearchPaths;
    const auto original = packageManager->installedPackages();
    const auto inputHash = QStringLiteral("same singer, version and notes");
    const auto cacheHash = InferCacheUtils::inputHashForCurrentModels(inputHash);
    QCOMPARE(InferCacheUtils::inputHashForCurrentModels(inputHash), cacheHash);
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
        const auto refreshedHash = InferCacheUtils::inputHashForCurrentModels(inputHash);
        QVERIFY(refreshedHash != cacheHash);
        QCOMPARE(InferCacheUtils::inputHashForCurrentModels(inputHash), refreshedHash);
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
        QTRY_COMPARE(refreshed.size(), 1);
        refreshed.clear();
        QVERIFY(QDir().mkdir(missingRoot));
        const auto generationBefore = SynthrtEngine::instance().catalogGeneration();
        const auto canceled =
            packageManager->refreshInstalledPackages({missingRoot}, [] { return false; });
        QVERIFY(!canceled);
        QCOMPARE(packageManager->installedPackages().successfulPackages,
                 original.successfulPackages);
        QCOMPARE(packageManager->findSingerByIdentifier(singer.identifier()), singer);
        QVERIFY(refreshed.isEmpty());
        QCOMPARE(SynthrtEngine::instance().catalogGeneration(), generationBefore);
        QCOMPARE(InferCacheUtils::inputHashForCurrentModels(inputHash), refreshedHash);
    }
    QTRY_COMPARE(refreshed.size(), 1);
    QCOMPARE(packageManager->installedPackages().successfulPackages, original.successfulPackages);
    QCOMPARE(packageManager->installedPackages().failedPackages.size(),
             original.failedPackages.size());
    QCOMPARE(packageManager->findSingerByIdentifier(singer.identifier()), singer);
    QTRY_VERIFY(taskManager->tasks().isEmpty());

    // Matching requests share a scan; a rejected leader, a different mode or different roots
    // must allow the follower to perform its own requested scan.
    for (const int scenario : {0, 1, 2, 3}) {
        const bool commitLeading = scenario != 1;
        const auto leadingMode = scenario == 2 ? PackageManager::ScanMode::ReuseLoaded
                                              : PackageManager::ScanMode::Reload;
        auto followerPaths = originalPaths;
        const auto followerOnlyRoot = emptyDirectory.filePath(QStringLiteral("follower-missing"));
        if (scenario == 3)
            followerPaths.append(followerOnlyRoot);
        refreshed.clear();
        const auto before = runtime().documentVersion();
        const auto contentBefore = TestSupport::projectSnapshot(*context->m_appModel);
        const auto *undoBefore = historyManager->nextUndoEntry();
        const auto generationBefore = SynthrtEngine::instance().catalogGeneration();
        QSemaphore leadingAdmitted;
        QSemaphore releaseLeading;
        QSemaphore followerStarted;
        std::atomic_int followerCommits = 0;
        using RefreshResult = decltype(packageManager->refreshInstalledPackages(originalPaths));
        std::future<RefreshResult> leading;
        std::future<RefreshResult> following;
        const auto releaseWorkers = qScopeGuard([&] {
            releaseLeading.release();
            if (leading.valid())
                leading.wait();
            if (following.valid())
                following.wait();
        });
        leading = std::async(std::launch::async, [&] {
            return packageManager->refreshInstalledPackages(
                originalPaths,
                [&] {
                    leadingAdmitted.release();
                    releaseLeading.acquire();
                    return commitLeading;
                },
                leadingMode);
        });
        QVERIFY(leadingAdmitted.tryAcquire(1, 5000));
        following = std::async(std::launch::async, [&] {
            followerStarted.release();
            return packageManager->refreshInstalledPackages(followerPaths, [&] {
                ++followerCommits;
                return true;
            });
        });
        QVERIFY(followerStarted.tryAcquire(1, 5000));
        QVERIFY(following.wait_for(std::chrono::milliseconds(50)) == std::future_status::timeout);
        QCOMPARE(followerCommits.load(), 0);
        releaseLeading.release();
        QVERIFY(leading.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
        QVERIFY(following.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
        const auto first = leading.get();
        const auto second = following.get();
        QCOMPARE(bool(first), commitLeading);
        QVERIFY2(second, qPrintable(second ? QString{} : second.getError().message));
        if (commitLeading)
            QCOMPARE(first.get().successfulPackages, original.successfulPackages);
        else
            QVERIFY(!first);
        QCOMPARE(second.get().successfulPackages, original.successfulPackages);
        QCOMPARE(followerCommits.load(), 1);
        QTRY_COMPARE(refreshed.size(), scenario >= 2 ? 2 : 1);
        if (scenario == 3) {
            const auto &failures = second.get().failedPackages;
            QVERIFY(std::any_of(failures.cbegin(), failures.cend(), [&](const auto &failure) {
                return failure.path == followerOnlyRoot;
            }));
        }
        QCOMPARE(packageManager->installedPackages().successfulPackages,
                 original.successfulPackages);
        QCOMPARE(packageManager->findSingerByIdentifier(singer.identifier()), singer);
        QVERIFY(SynthrtEngine::instance().catalogGeneration() > generationBefore);
        QVERIFY(SynthrtEngine::instance().singer(singer.identifier()));
        QCOMPARE(runtime().documentVersion(), before);
        QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), contentBefore);
        QCOMPARE(historyManager->nextUndoEntry(), undoBefore);
        QTRY_VERIFY(taskManager->tasks().isEmpty());
    }
}

void ApplicationWorkflowTests::packageReloadReadsUpdatesWhilePipelineIsLeased() {
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->packageModuleStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = directory.filePath(QStringLiteral("ci-fixture"));
    std::error_code error;
    std::filesystem::copy(std::filesystem::u8path(LITE_TEST_VOICEBANK_ROOT),
                          std::filesystem::u8path(root.toUtf8().constData()),
                          std::filesystem::copy_options::recursive, error);
    QVERIFY(!error);
    const auto originalPaths = context->m_appOptions->general()->packageSearchPaths;
    const auto restore = qScopeGuard([&] {
        QVERIFY(packageManager->refreshInstalledPackages(originalPaths));
    });
    const auto first = packageManager->refreshInstalledPackages({root});
    QVERIFY(first);
    const auto identity = first.get().successfulPackages.first().singers().first().identifier();
    auto acquired = SynthrtEngine::instance().pipelineFor(identity);
    QVERIFY(acquired);
    auto active = acquired.take();
    auto oldModel = active->acoustic();
    QVERIFY(oldModel);
    QFile declaration(QDir(root).filePath(QStringLiteral("characters/fixture/config.json")));
    QVERIFY(declaration.open(QIODevice::ReadOnly));
    auto object = QJsonDocument::fromJson(declaration.readAll()).object();
    declaration.close();
    object.insert(QStringLiteral("name"), QStringLiteral("Updated while rendering"));
    QVERIFY(declaration.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const auto bytes = QJsonDocument(object).toJson();
    QCOMPARE(declaration.write(bytes), bytes.size());
    declaration.close();
    const auto refreshed = packageManager->refreshInstalledPackages({root});
    QVERIFY(refreshed);
    QCOMPARE(packageManager->findSingerByIdentifier(identity).name(),
             QStringLiteral("Updated while rendering"));
    auto replacement = SynthrtEngine::instance().pipelineFor(identity);
    QVERIFY(replacement);
    QVERIFY(replacement.get() != active);
    const auto newModel = replacement.get()->acoustic();
    QVERIFY(newModel);
    QVERIFY(newModel.get() != oldModel.get());
    QCOMPARE(active->acoustic().get(), oldModel.get());
}

void ApplicationWorkflowTests::fillLyricPreservesSingerReservedMarkers() {
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->packageModuleStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = directory.filePath(QStringLiteral("ci-fixture"));
    std::error_code error;
    std::filesystem::copy(std::filesystem::u8path(LITE_TEST_VOICEBANK_ROOT),
                          std::filesystem::u8path(root.toUtf8().constData()),
                          std::filesystem::copy_options::recursive, error);
    QVERIFY(!error);
    QFile declaration(QDir(root).filePath(QStringLiteral("characters/fixture/config.json")));
    QVERIFY(declaration.open(QIODevice::ReadOnly));
    auto object = QJsonDocument::fromJson(declaration.readAll()).object();
    declaration.close();
    object.insert(QStringLiteral("reservedPhonemes"), QJsonArray{"AP", "SP", "br"});
    QVERIFY(declaration.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const auto bytes = QJsonDocument(object).toJson();
    QCOMPARE(declaration.write(bytes), bytes.size());
    declaration.close();
    for (const auto *stage : {"duration", "pitch", "variance", "acoustic"}) {
        QFile phonemes(QDir(root).filePath(
            QStringLiteral("inferences/%1/phonemes.json").arg(QString::fromLatin1(stage))));
        QVERIFY(phonemes.open(QIODevice::ReadOnly));
        auto table = QJsonDocument::fromJson(phonemes.readAll()).object();
        phonemes.close();
        table.insert(QStringLiteral("br"), 6);
        QVERIFY(phonemes.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const auto tableBytes = QJsonDocument(table).toJson();
        QCOMPARE(phonemes.write(tableBytes), tableBytes.size());
    }
    QFile dictionary(QDir(root).filePath(
        QStringLiteral("inferences/fixture-g2p-cmn/dictionary.txt")));
    QVERIFY(dictionary.open(QIODevice::Append));
    const QByteArray entries("\nAP\tla\nSP\tla\nbr\tla\n");
    QCOMPARE(dictionary.write(entries), entries.size());
    dictionary.close();
    const auto originalPaths = context->m_appOptions->general()->packageSearchPaths;
    const auto restore = qScopeGuard([&] {
        QVERIFY(packageManager->refreshInstalledPackages(originalPaths));
    });
    const auto loaded = packageManager->refreshInstalledPackages({root});
    QVERIFY(loaded);
    QVERIFY(!loaded.get().successfulPackages.isEmpty());
    const auto identity = loaded.get().successfulPackages.first().singers().first().identifier();
    FillLyric::G2pService g2p(identity);
    const auto results = g2p.convert(
        {LangNote("AP", "cmn", kUnknownG2pId),
         LangNote(QStringLiteral("啦"), "cmn", kUnknownG2pId),
         LangNote("SP", "cmn", kUnknownG2pId), LangNote("br", "cmn", kUnknownG2pId)},
        {"cmn"});
    QCOMPARE(results.size(), 4);
    const QStringList expected{"AP", "la", "SP", "br"};
    for (int i = 0; i < results.size(); ++i) {
        QCOMPARE(results[i].pronunciation, expected[i]);
        QCOMPARE(results[i].candidates, QStringList{expected[i]});
    }
}

void ApplicationWorkflowTests::disjointModelPhonemesDisableLanguageCoverage() {
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->packageModuleStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = directory.filePath(QStringLiteral("ci-fixture"));
    std::error_code error;
    std::filesystem::copy(std::filesystem::u8path(LITE_TEST_VOICEBANK_ROOT),
                          std::filesystem::u8path(root.toUtf8().constData()),
                          std::filesystem::copy_options::recursive, error);
    QVERIFY(!error);
    QFile declaration(QDir(root).filePath(QStringLiteral("characters/fixture/config.json")));
    QVERIFY(declaration.open(QIODevice::ReadOnly));
    auto object = QJsonDocument::fromJson(declaration.readAll()).object();
    declaration.close();
    object.insert(QStringLiteral("reservedPhonemes"), QJsonArray{});
    QVERIFY(declaration.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const auto bytes = QJsonDocument(object).toJson();
    QCOMPARE(declaration.write(bytes), bytes.size());
    declaration.close();
    for (const auto *stage : {"duration", "pitch", "variance", "acoustic"}) {
        QFile phonemes(QDir(root).filePath(
            QStringLiteral("inferences/%1/phonemes.json").arg(QString::fromLatin1(stage))));
        QVERIFY(phonemes.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const QJsonObject table{{QStringLiteral("cmn/%1-only").arg(QString::fromLatin1(stage)), 0}};
        const auto tableBytes = QJsonDocument(table).toJson();
        QCOMPARE(phonemes.write(tableBytes), tableBytes.size());
    }
    const auto originalPaths = context->m_appOptions->general()->packageSearchPaths;
    const auto restore = qScopeGuard([&] {
        QVERIFY(packageManager->refreshInstalledPackages(originalPaths));
    });
    const auto loaded = packageManager->refreshInstalledPackages({root});
    QVERIFY(loaded);
    QVERIFY(!loaded.get().successfulPackages.isEmpty());
    const auto identity = loaded.get().successfulPackages.first().singers().first().identifier();
    const auto singer = SynthrtEngine::instance().singer(identity);
    QVERIFY(singer);
    QVERIFY(singer->capabilities.singablePhonemes.has_value());
    QVERIFY(singer->capabilities.singablePhonemes->empty());
    QVERIFY(!SynthrtEngine::instance().canConvert(identity, QStringLiteral("cmn")));
    QVERIFY(!SynthrtEngine::instance()
                 .languageUnavailableReason(identity, QStringLiteral("cmn")).isEmpty());
}

void ApplicationWorkflowTests::packageLanguageReadinessUsesTheCatalogSnapshot() {
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->packageModuleStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    const auto originalPaths = context->m_appOptions->general()->packageSearchPaths;
    const auto restore = qScopeGuard([&] {
        QVERIFY(packageManager->refreshInstalledPackages(originalPaths));
    });
    const auto services = Automation::createPackageAutomationServices(packageManager);
    const auto before = services.installedPackages();
    const auto fixture = std::find_if(before.cbegin(), before.cend(), [](const auto &package) {
        return package.id == QStringLiteral("ci-fixture");
    });
    QVERIFY(fixture != before.cend());
    QVERIFY(!fixture->singers.isEmpty());
    const auto identity = fixture->singers.first().info.identifier();
    QVERIFY(fixture->singers.first().convertibleLanguages.contains(QStringLiteral("cmn")));
    QTemporaryDir emptyRoot;
    QVERIFY(emptyRoot.isValid());
    // Reproduce the interval after the engine publishes a scan and before PackageManager does.
    auto scanned = SynthrtEngine::instance().refreshVoicebanks(
        {std::filesystem::u8path(emptyRoot.path().toUtf8().constData())});
    QVERIFY(scanned);
    QVERIFY(scanned.take().empty());
    QVERIFY(!SynthrtEngine::instance().canConvert(identity, QStringLiteral("cmn")));
    QCOMPARE(services.installedPackages(), before);
    QVERIFY(packageManager->refreshInstalledPackages({emptyRoot.path()}));
    QVERIFY(services.installedPackages().isEmpty());
}

void ApplicationWorkflowTests::singerSpeakerCatalogPreservesOrderAndAvailability_data() {
    QTest::addColumn<QString>("scenario");
    QTest::newRow("declaration-order") << QStringLiteral("ordered");
    QTest::newRow("tone-octave-overflow") << QStringLiteral("tone-overflow");
    QTest::newRow("tone-midi-boundaries") << QStringLiteral("tone-bounds");
    QTest::newRow("missing-mapping-keys") << QStringLiteral("missing-keys");
    QTest::newRow("missing-model-targets") << QStringLiteral("missing-targets");
    QTest::newRow("speakerless-model") << QStringLiteral("speakerless");
}

void ApplicationWorkflowTests::singerSpeakerCatalogPreservesOrderAndAvailability() {
    QFETCH(QString, scenario);
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->packageModuleStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto root = directory.filePath(QStringLiteral("ci-fixture"));
    std::error_code error;
    std::filesystem::copy(std::filesystem::u8path(LITE_TEST_VOICEBANK_ROOT),
                          std::filesystem::u8path(root.toUtf8().constData()),
                          std::filesystem::copy_options::recursive, error);
    QVERIFY(!error);
    const auto rewrite = [&](const QString &relativePath, auto amend) {
        QFile file(QDir(root).filePath(relativePath));
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto object = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        amend(object);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const auto bytes = QJsonDocument(object).toJson();
        QCOMPARE(file.write(bytes), bytes.size());
    };
    rewrite(QStringLiteral("characters/fixture/config.json"), [&](QJsonObject &object) {
        auto configuration = object.value(QStringLiteral("configuration")).toObject();
        auto speakers = configuration.value(QStringLiteral("speakers")).toArray();
        QCOMPARE(speakers.size(), 2);
        if (scenario.startsWith(QStringLiteral("tone-"))) {
            for (qsizetype i = 0; i < speakers.size(); ++i) {
                auto speaker = speakers[i].toObject();
                const auto low = scenario == QStringLiteral("tone-overflow")
                                     ? (i == 0 ? QStringLiteral("C2147483647")
                                               : QStringLiteral("C-2147483648"))
                                     : QStringLiteral("C-1");
                speaker.insert(QStringLiteral("toneRanges"),
                               QJsonObject{{QStringLiteral("min"), low},
                                           {QStringLiteral("max"), QStringLiteral("G9")}});
                speakers[i] = speaker;
            }
        }
        configuration.insert(QStringLiteral("speakers"),
                             scenario == QStringLiteral("speakerless")
                                 ? QJsonArray{} : QJsonArray{speakers[1], speakers[0]});
        object.insert(QStringLiteral("configuration"), configuration);
        auto imports = object.value(QStringLiteral("imports")).toArray();
        for (qsizetype i = 0; i < imports.size(); ++i) {
            auto entry = imports[i].toObject();
            if (entry.value(QStringLiteral("role")).toString() !=
                QStringLiteral("singer/acoustic"))
                continue;
            auto options = entry.value(QStringLiteral("options")).toObject();
            if (scenario == QStringLiteral("missing-keys"))
                options.insert(QStringLiteral("speakerMapping"),
                               QJsonObject{{QStringLiteral("unlisted"), QStringLiteral("clear")}});
            else if (scenario == QStringLiteral("missing-targets"))
                options.insert(QStringLiteral("speakerMapping"),
                               QJsonObject{{QStringLiteral("clear"), QStringLiteral("missing")},
                                           {QStringLiteral("soft"), QStringLiteral("missing")}});
            else if (scenario == QStringLiteral("speakerless"))
                options.remove(QStringLiteral("speakerMapping"));
            entry.insert(QStringLiteral("options"), options);
            imports[i] = entry;
        }
        object.insert(QStringLiteral("imports"), imports);
    });
    if (scenario == QStringLiteral("speakerless")) {
        rewrite(QStringLiteral("inferences/acoustic/config.json"), [](QJsonObject &object) {
            auto exports = object.value(QStringLiteral("exports")).toObject();
            exports.insert(QStringLiteral("speakers"), QJsonArray{});
            object.insert(QStringLiteral("exports"), exports);
            auto configuration = object.value(QStringLiteral("configuration")).toObject();
            configuration.insert(QStringLiteral("useSpeakerEmbedding"), false);
            configuration.remove(QStringLiteral("speakers"));
            object.insert(QStringLiteral("configuration"), configuration);
        });
    }
    if (QTest::currentTestFailed())
        return;
    const auto originalPaths = context->m_appOptions->general()->packageSearchPaths;
    const auto restore = qScopeGuard([&] {
        QVERIFY(packageManager->refreshInstalledPackages(originalPaths));
    });
    const auto loaded = packageManager->refreshInstalledPackages({root});
    QVERIFY(loaded);
    QVERIFY(!loaded.get().successfulPackages.isEmpty());
    const auto singers = loaded.get().successfulPackages.first().singers();
    QVERIFY(!singers.isEmpty());
    const auto &singer = singers.first();
    if (scenario == QStringLiteral("ordered") || scenario.startsWith(QStringLiteral("tone-"))) {
        QCOMPARE(singer.resolutionState(), ResolutionState::Resolved);
        QCOMPARE(singer.speakers().size(), 2);
        QCOMPARE(singer.speakers().first().id(), QStringLiteral("soft"));
        QCOMPARE(singer.speakers().last().id(), QStringLiteral("clear"));
        for (const auto &speaker : singer.speakers()) {
            if (scenario == QStringLiteral("tone-overflow"))
                QVERIFY(!speaker.toneRange());
            else if (scenario == QStringLiteral("tone-bounds"))
                QCOMPARE(speaker.toneRange(), std::optional(std::make_pair(0, 127)));
        }
    } else {
        QVERIFY(singer.speakers().isEmpty());
        QCOMPARE(singer.resolutionState(), scenario == QStringLiteral("speakerless")
                                              ? ResolutionState::Resolved
                                              : ResolutionState::Missing);
    }
}

void ApplicationWorkflowTests::localizedPackageMetadataLoadsAndUpdatesWithTheVersion_data() {
    QTest::addColumn<bool>("sharedStages");
    QTest::addColumn<QString>("missingStage");
    QTest::newRow("stages-in-singer-package") << false << QString();
    QTest::newRow("vocoder-in-dependent-package") << true << QString();
    for (const auto &stage : {QStringLiteral("duration"), QStringLiteral("pitch"),
                              QStringLiteral("variance")})
        QTest::newRow(qPrintable(QStringLiteral("missing-%1").arg(stage))) << false << stage;
}

void ApplicationWorkflowTests::localizedPackageMetadataLoadsAndUpdatesWithTheVersion() {
    QFETCH(bool, sharedStages);
    QFETCH(QString, missingStage);
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->packageModuleStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    const auto originalPaths = context->m_appOptions->general()->packageSearchPaths;
    const auto original = packageManager->installedPackages().successfulPackages;
    const auto originalPackage =
        std::find_if(original.cbegin(), original.cend(),
                     [](const auto &item) { return item.id() == QStringLiteral("ci-fixture"); });
    QVERIFY(originalPackage != original.cend() && !originalPackage->singers().isEmpty());
    const auto baseline = originalPackage->singers().first();
    QCOMPARE(baseline.resolutionState(), ResolutionState::Resolved);
    QVERIFY(baseline.capability() && baseline.capability()->acousticParameters);
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
    const auto rewrite = [&](const QString &packageRoot, const QString &relativePath, auto amend) {
        QFile file(QDir(packageRoot).filePath(relativePath));
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
    QStringList searchPaths{root};
    if (!missingStage.isEmpty()) {
        rewrite(root, QStringLiteral("characters/fixture/config.json"), [&](QJsonObject &object) {
            QJsonArray imports;
            for (const auto &value : object.value(QStringLiteral("imports")).toArray()) {
                if (value.toObject().value(QStringLiteral("role")).toString() !=
                    QStringLiteral("singer/") + missingStage)
                    imports.append(value);
            }
            object.insert(QStringLiteral("imports"), imports);
        });
    }
    if (sharedStages) {
        const auto stageRoot = directory.filePath(QStringLiteral("ci-fixture-stages@1.0.0"));
        std::filesystem::copy(std::filesystem::u8path(LITE_TEST_VOICEBANK_ROOT),
                              std::filesystem::u8path(stageRoot.toUtf8().constData()),
                              std::filesystem::copy_options::recursive, error);
        QVERIFY2(!error, qPrintable(QString::fromStdString(error.message())));
        rewrite(stageRoot, QStringLiteral("desc.json"), [](QJsonObject &object) {
            object.insert(QStringLiteral("id"), QStringLiteral("ci-fixture-stages"));
            object.insert(QStringLiteral("contributions"), QJsonObject{
                {QStringLiteral("inference"), QJsonArray{QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("vocoder")},
                    {QStringLiteral("path"), QStringLiteral("./inferences/vocoder/config.json")}
                }}}
            });
        });
        rewrite(root, QStringLiteral("desc.json"), [](QJsonObject &object) {
            auto dependencies = object.value(QStringLiteral("dependencies")).toArray();
            dependencies.append(QJsonObject{
                {QStringLiteral("id"), QStringLiteral("ci-fixture-stages")},
                {QStringLiteral("version"), QStringLiteral("1.0.0.0")}
            });
            object.insert(QStringLiteral("dependencies"), dependencies);
            auto contributes = object.value(QStringLiteral("contributions")).toObject();
            QJsonArray inferences;
            for (const auto &inference : contributes.value(QStringLiteral("inference")).toArray()) {
                if (inference.toObject().value(QStringLiteral("id")) != QStringLiteral("vocoder"))
                    inferences.append(inference);
            }
            contributes.insert(QStringLiteral("inference"), inferences);
            object.insert(QStringLiteral("contributions"), contributes);
        });
        rewrite(root, QStringLiteral("characters/fixture/config.json"), [](QJsonObject &object) {
            auto imports = object.value(QStringLiteral("imports")).toArray();
            for (qsizetype index = 0; index < imports.size(); ++index) {
                auto imported = imports[index].toObject();
                if (imported.value(QStringLiteral("role")) == QStringLiteral("singer/vocoder")) {
                    imported.insert(QStringLiteral("ref"),
                                    QStringLiteral("ci-fixture-stages:inference/vocoder"));
                    imports[index] = imported;
                }
            }
            object.insert(QStringLiteral("imports"), imports);
        });
        if (QTest::currentTestFailed())
            return;
        searchPaths.append(stageRoot);
    }
    rewrite(root, QStringLiteral("desc.json"), [&](QJsonObject &object) {
        object.insert(QStringLiteral("url"), QStringLiteral("https://example.org/voicebank"));
        object.insert(QStringLiteral("vendor"),
                      localized(QStringLiteral("Fixture vendor"), QStringLiteral("测试制作方")));
        object.insert(QStringLiteral("description"),
                      localized(QStringLiteral("Catalog fixture"), QStringLiteral("目录测试资源")));
    });
    if (QTest::currentTestFailed())
        return;
    rewrite(root, QStringLiteral("characters/fixture/config.json"), [&](QJsonObject &object) {
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
        object.insert(QStringLiteral("configuration"), configuration);
    });
    if (QTest::currentTestFailed())
        return;
    rewrite(root, QStringLiteral("linguists/cmn-pinyin/linguist.json"), [&](QJsonObject &object) {
        object.insert(QStringLiteral("name"),
                      localized(QStringLiteral("Mandarin"), QStringLiteral("\u666e\u901a\u8bdd")));
    });
    const auto before = runtime().documentVersion();
    const auto project = TestSupport::projectSnapshot(*context->m_appModel);
    const auto *undo = historyManager->nextUndoEntry();
    const QList<QPair<QString, QString>> releases{
        {QStringLiteral("1.0.0.0"), QStringLiteral("测试制作方")    },
        {QStringLiteral("1.0.1.0"), QStringLiteral("更新后的制作方")},
    };
    for (const auto &[version, vendor] : releases) {
        rewrite(root, QStringLiteral("desc.json"), [&](QJsonObject &object) {
            object.insert(QStringLiteral("version"), version);
            object.insert(QStringLiteral("vendor"),
                          localized(QStringLiteral("Fixture vendor"), vendor));
        });
        if (QTest::currentTestFailed())
            return;
        const auto result = packageManager->refreshInstalledPackages(searchPaths);
        QVERIFY2(result, qPrintable(result ? QString{} : result.getError().message));
        const auto &packages = result.get().successfulPackages;
        const auto package = std::find_if(packages.cbegin(), packages.cend(), [](const auto &item) {
            return item.id() == QStringLiteral("ci-fixture");
        });
        QVERIFY(package != packages.cend());
        QCOMPARE(package->url(), QStringLiteral("https://example.org/voicebank"));
        QCOMPARE(package->version().normalized(), QVersionNumber::fromString(version).normalized());
        QCOMPARE(package->displayVendor(QStringLiteral("zh-CN")), vendor);
        QCOMPARE(package->displayDescription(QStringLiteral("zh-CN")),
                 QStringLiteral("目录测试资源"));
        QVERIFY(!package->singers().isEmpty());
        const auto singer = package->singers().first();
        QCOMPARE(singer.resolutionState(), missingStage.isEmpty() ? ResolutionState::Resolved
                                                                 : ResolutionState::Missing);
        QVERIFY(singer.capability());
        if (sharedStages) {
            QCOMPARE(singer.capability()->vocoderPitchControllable,
                     baseline.capability()->vocoderPitchControllable);
            const auto stages = std::find_if(packages.cbegin(), packages.cend(), [](const auto &item) {
                return item.id() == QStringLiteral("ci-fixture-stages");
            });
            QVERIFY(stages != packages.cend());
            QVERIFY(stages->singers().isEmpty());
        } else if (missingStage.isEmpty()) {
            QCOMPARE(singer.capability(), baseline.capability());
        } else if (missingStage == QStringLiteral("pitch")) {
            QCOMPARE(singer.capability()->pitchUsesExpressiveness, std::optional<bool>(false));
        }
        QCOMPARE(singer.displayName(QStringLiteral("zh-CN")), QStringLiteral("测试歌手"));
        QCOMPARE(singer.displayName(QStringLiteral("de")), QStringLiteral("Fixture singer"));
        QVERIFY(!singer.speakers().isEmpty() && !singer.languages().isEmpty());
        QCOMPARE(singer.speakers().first().displayName(QStringLiteral("zh-CN")),
                 QStringLiteral("清亮声线"));
        QCOMPARE(singer.languages().first().displayName(QStringLiteral("zh-CN")),
                 QStringLiteral("普通话"));
        QCOMPARE(singer.languages().first().name(), QStringLiteral("Mandarin"));
        QCOMPARE(singer.languages().first().displayName(QStringLiteral("de")),
                 QStringLiteral("Mandarin"));
        QCOMPARE(packageManager->findSingerByIdentifier(singer.identifier()), singer);
        QCOMPARE(runtime().documentVersion(), before);
        QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), project);
        QCOMPARE(historyManager->nextUndoEntry(), undo);
    }
}
