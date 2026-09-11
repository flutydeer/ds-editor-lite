#include "tst_application_workflows.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Automation/AppOptionsAutomationAdapter.h"
#include "Automation/Public/PublicAutomationHostAdapter.h"
#include "Bootstrap/AppDataPaths.h"
#include "Bootstrap/AppEnvironment.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/SpeakerMixPreset/SpeakerMixPresetStore.h"
#include "Modules/Audio/AudioContext.h"
#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/subsystem/OutputSystem.h"
#include "Modules/Inference/EditSessionManager.h"
#include "Modules/Inference/InferController.h"
#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/InferPipeline.h"
#include "Modules/Inference/States/UpdateVarianceState.h"
#include "Modules/Inference/States/PlaybackReadyState.h"
#include "Modules/Inference/Utils/InferenceApplyGate.h"
#include "Modules/Inference/Utils/CudaGpuUtils.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/InferenceData/InferPiece.h>
#include <lite/Tasking/TaskManager.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectConverters/DspxProjectConverter.h>
#include <lite/ProjectConverters/MidiConverter.h>
#include <lite/PackageManager/PackageManager.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>
#include "../TestSupport/ProcessFixture.h"
#include "../TestSupport/RuntimeResourcesFixture.h"
#include "../TestSupport/VoicebankFixture.h"

#include <TalcsDevice/AbstractOutputContext.h>
#include <TalcsDevice/AudioDevice.h>
#include <TalcsFormat/AudioFormatIO.h>
#include <TalcsCore/MixerAudioSource.h>

#include <QtTest/QTest>
#include <QSignalSpy>
#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QFile>
#include <QScopeGuard>
#include <QSemaphore>
#include <QSignalBlocker>
#include <QPointer>
#include <QTimer>
#include <QJsonArray>

#include <memory>
#include <atomic>

namespace {
    using InferenceApplyGate::Decision;

    int unavailableInferenceProvider(int argc, char **argv) {
        QCoreApplication application(argc, argv);
        AppEnvironment::postInit(AppHostMode::Headless);
        auto options = std::make_unique<AppOptions>();
        options->general()->packageSearchPaths.clear();
        options->inference()->autoStartInfer = false;
        // Reject the device prerequisite before the shared inference runtime starts.
        options->inference()->executionProvider = QStringLiteral("CUDA");
        CudaGpuUtils::setNvidiaSmiPath(
            QDir(AppDataPaths::testRoot()).filePath(QStringLiteral("missing-nvidia-smi")));
        AppContext context(std::move(options), AppHostMode::Headless);
        packageManager->initialize({});
        if (!TestSupport::waitUntil(
                [] { return appStatus->inferEngineEnvStatus == AppStatus::ModuleStatus::Error; },
                5000)) {
            qCritical("The unavailable provider did not report initialization failure");
            return 1;
        }
        if (SynthrtEngine::instance().runtimeInitialized() ||
            !SynthrtEngine::instance().initializationDone()) {
            qCritical("Rejected device prerequisites must finish the initialization attempt");
            return 2;
        }
        if (!TestSupport::waitUntil(
                [] {
                    return taskManager->tasks().isEmpty() &&
                           appStatus->packageModuleStatus == AppStatus::ModuleStatus::Error;
                },
                5000)) {
            qCritical("Package discovery must fail without waiting for an unavailable runtime");
            return 3;
        }
        return 0;
    }

    void addInferenceStages() {
        QTest::addColumn<QString>("stage");
        QTest::newRow("duration") << QStringLiteral("duration");
        QTest::newRow("pitch") << QStringLiteral("pitch");
        QTest::newRow("variance") << QStringLiteral("variance");
        QTest::newRow("acoustic") << QStringLiteral("acoustic");
    }

    InferenceTaskContext captureTask(const QString &stage, const InferPiece &piece) {
        const auto singer = piece.clip->singerIdentifier();
        // Construct the same task snapshot used by completion handlers without scheduling it.
        if (stage == QStringLiteral("duration")) {
            const InferDurationTask task(InferControllerHelper::buildInferDurInput(piece, singer));
            return task.inferenceContext();
        }
        if (stage == QStringLiteral("pitch")) {
            const InferPitchTask task(InferControllerHelper::buildInferPitchInput(piece, singer));
            return task.inferenceContext();
        }
        if (stage == QStringLiteral("variance")) {
            const InferVarianceTask task(
                InferControllerHelper::buildInferVarianceInput(piece, singer));
            return task.inferenceContext();
        }
        const InferAcousticTask task(InferControllerHelper::buildInferAcousticInput(piece, singer));
        return task.inferenceContext();
    }

    Automation::TrackDraftDto trackDraft(const QString &name) {
        Automation::NoteDraftDto note;
        note.localStart = 480;
        note.length = 480;
        note.keyIndex = 60;
        note.lyric = QStringLiteral("a");
        note.language = QStringLiteral("eng");
        PhonemeName phoneme;
        phoneme.language = QStringLiteral("eng");
        phoneme.name = QStringLiteral("a");
        note.phonemes.nameSeq.original = {phoneme};
        note.phonemes.offsetSeq.original = {0};

        Automation::ClipDraftDto clip;
        clip.properties.name = name;
        clip.properties.length = 1920;
        clip.properties.clipLen = 1920;
        clip.defaultLanguage = QStringLiteral("eng");
        clip.notes = {note};

        Automation::TrackDraftDto track;
        track.name = name;
        track.defaultLanguage = QStringLiteral("eng");
        track.clips = {clip};
        return track;
    }
}

void ApplicationWorkflowTests::initTestCase() {
    QVERIFY(dataRoot.isValid());
    previousDataRoot = qgetenv("DSEL_TEST_DATA_ROOT");
    qputenv("DSEL_TEST_DATA_ROOT", dataRoot.path().toUtf8());
    dataRootInstalled = true;
    QVERIFY(TestSupport::initializeApplicationResources());
    QCOMPARE(AppDataPaths::testRoot(), QDir::cleanPath(dataRoot.path()));
    AppEnvironment::postInit(AppHostMode::Headless);

    auto options = std::make_unique<AppOptions>();
    QVERIFY(QDir::cleanPath(options->configPath()).startsWith(dataRoot.path() + '/'));
    const auto voicebankRoot = TestSupport::voicebankRoot();
    QVERIFY2(QDir::isAbsolutePath(voicebankRoot) && QFileInfo(voicebankRoot).isDir(),
             qPrintable(QStringLiteral("Voicebank fixture is unavailable: %1").arg(voicebankRoot)));
    options->general()->packageSearchPaths = {voicebankRoot};
    options->general()->defaultSingingLanguage = QStringLiteral("eng");
    options->inference()->autoStartInfer = false;
    options->inference()->executionProvider = QStringLiteral("CPU");
    options->inference()->cacheDirectory = dataRoot.filePath(QStringLiteral("cache"));
    context = std::make_unique<AppContext>(std::move(options), AppHostMode::Headless);
    if (auto *device = AudioSystem::outputSystem()->context()->device()) {
        device->stop();
        device->close();
        QVERIFY(!device->isOpen());
    }
    AudioContext::instance()->preMixer()->close();
    packageManager->initialize(context->m_appOptions->general()->packageSearchPaths);
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->packageModuleStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
}

void ApplicationWorkflowTests::init() {
    auto document = Automation::DocumentAutomationFacade::newDocumentDraft(false);
    document.tracks = {trackDraft(QStringLiteral("Target")),
                       trackDraft(QStringLiteral("Other track"))};
    QVERIFY(runtime().documents().commitNewDocument(commandContext(), document));
    QCOMPARE(context->m_appModel->tracks().size(), 2);
    const auto *track = context->m_appModel->tracks().first();
    trackId = Automation::TrackId(track->id());
    clip = dynamic_cast<SingingClip *>(*track->clips().begin());
    otherClip = dynamic_cast<SingingClip *>(*context->m_appModel->tracks().last()->clips().begin());
    QVERIFY(clip);
    QVERIFY(otherClip);
    clip->reSegment(context->m_appModel->timeline());
    QCOMPARE(clip->pieces().size(), 1);
    piece = clip->pieces().first();
    QCOMPARE(piece->notes.size(), 1);
    note = piece->notes.first();
    QVERIFY(!editSessionManager->hasActiveTransaction());
}

void ApplicationWorkflowTests::changedTargetInputDropsResult_data() {
    addInferenceStages();
}

void ApplicationWorkflowTests::speakerMixPresetPersistsThroughTheProductionStore() {
    using Store = SpeakerMixPresetStore;
    const SpeakerInfo clear("clear", "Clear");
    const SpeakerInfo soft("soft", "Soft");
    const SingerInfo singer({"fixture", "ci-fixture", QVersionNumber(1, 0, 0)}, "Fixture",
                            {clear, soft});
    SpeakerMixPreset draft;
    draft.name = QStringLiteral("Mixed voice");
    draft.packageId = singer.identifier().packageId;
    draft.singerId = singer.identifier().singerId;
    draft.packageVersion = singer.identifier().packageVersion;
    draft.sources = {{clear}, {soft}};
    draft.fixedWeights = {0.3};
    const auto saved = Store::savePreset(draft);
    QVERIFY(saved);
    const auto cleanup = qScopeGuard([&] { Store::deletePreset(saved->id); });
    QVERIFY(!saved->id.isEmpty());
    QVERIFY(Store::findPreset(saved->id));
    QVERIFY(Store::presetNameExists(singer, draft.name));
    QVERIFY(!Store::presetNameExists(singer, draft.name, saved->id));
    const auto data = Store::speakerMixDataFromPreset(*saved, singer);
    QCOMPARE(data.sources.size(), 2);
    QCOMPARE(data.fixedWeights, QVector<double>{0.3});
    QVERIFY(Store::speakerMixDataMatchesPreset(*saved, singer, data));
    QVERIFY(Store::sourcePresetForData(singer, data));
    auto edited = data;
    edited.fixedWeights = {0.7};
    QVERIFY(!Store::speakerMixDataMatchesPreset(*saved, singer, edited));

    auto renamed = *saved;
    renamed.name = QStringLiteral("Soft blend");
    renamed.fixedWeights = {0.1};
    QVERIFY(Store::savePreset(renamed));
    QVERIFY(!Store::findPresetByName(singer, draft.name));
    QVERIFY(Store::findPresetByName(singer, renamed.name));
    AppOptions reopened;
    const auto services = Automation::createAppOptionsPresetAutomationServices(&reopened);
    const auto persisted = services.speakerMixPresets();
    const auto it = std::find_if(persisted.cbegin(), persisted.cend(),
                                 [&](const auto &preset) { return preset.id == saved->id; });
    QVERIFY(it != persisted.cend());
    QCOMPARE(it->name, renamed.name);
    QCOMPARE(it->fixedWeights, QVector<double>{0.1});
    QVERIFY(Store::deletePreset(saved->id));
    QVERIFY(!Store::findPreset(saved->id));
    AppOptions afterDeletion;
    const auto remaining =
        Automation::createAppOptionsPresetAutomationServices(&afterDeletion).speakerMixPresets();
    QVERIFY(std::none_of(remaining.cbegin(), remaining.cend(),
                         [&](const auto &preset) { return preset.id == saved->id; }));
}

void ApplicationWorkflowTests::publicSpeakerMixPresetsResolveAndPreserveAppliedVoices() {
    prepareVoicebankTarget();
    if (QTest::currentTestFailed())
        return;
    const auto singer = clip->singerInfo();
    if (singer.speakers().size() < 2)
        QSKIP("The configured voicebank needs two speakers for preset blending");
    const auto first = singer.speakers().at(0).id();
    const auto second = singer.speakers().at(1).id();
    const QJsonObject singerRef{
        {"package_id",      singer.packageId()                },
        {"package_version", singer.packageVersion().toString()},
        {"singer_id",       singer.singerId()                 }
    };
    const auto sources = [&](double weight) {
        return QJsonArray{
            QJsonObject{{"speaker", QJsonObject{{"speaker_id", first}}},  {"weight", weight}},
            QJsonObject{{"speaker", QJsonObject{{"speaker_id", second}}},
                        {"weight", 1.0 - weight}                                            }
        };
    };
    Automation::AutomationAccessPolicy access(AutomationWire::ControlLevel::L2);
    Automation::AutomationFileGuard fileGuard;
    Automation::AdmissionController admission;
    Automation::PublicAutomationRegistry registry(
        runtime(), access, fileGuard, admission,
        Automation::createPublicAutomationHostServices(runtime(), context->m_appModel,
                                                       &SynthrtEngine::instance()));
    const auto invoke = [&](const QString &name, const QJsonObject &arguments) {
        return registry.invoke(name, arguments,
                               {.clientId = QStringLiteral("voice-preset-client"),
                                .source = Automation::InvocationSource::PublicJsonRpc});
    };
    const auto beforeCatalog = runtime().documentVersion();
    QJsonObject preset{
        {"name",    "Wire blend" },
        {"singer",  singerRef    },
        {"sources", sources(0.25)}
    };
    const auto saved = invoke(QStringLiteral("speaker_mix.presets.save"), {
                                                                              {"preset", preset}
    });
    QVERIFY2(saved, qPrintable(saved ? QString() : saved.getError().message));
    const auto id = saved.get().value("preset").toObject().value("preset_id").toString();
    QVERIFY(!id.isEmpty());
    const auto cleanup = qScopeGuard([&] {
        if (SpeakerMixPresetStore::findPreset(id))
            SpeakerMixPresetStore::deletePreset(id);
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    });
    const auto listed =
        invoke(QStringLiteral("speaker_mix.presets.list"), {
                                                               {"singer", singerRef}
    });
    QVERIFY2(listed, qPrintable(listed ? QString() : listed.getError().message));
    QJsonObject returned;
    for (const auto value : listed.get().value("presets").toArray()) {
        if (value.toObject().value("preset_id").toString() == id)
            returned = value.toObject();
    }
    QCOMPARE(returned.value("name").toString(), QStringLiteral("Wire blend"));
    const auto listedSources = returned.value("sources").toArray();
    QCOMPARE(listedSources.size(), 2);
    QCOMPARE(listedSources.first().toObject().value("weight").toDouble(), 0.25);
    preset.insert(QStringLiteral("preset_id"), id);
    preset.insert(QStringLiteral("name"), QStringLiteral("Updated wire blend"));
    preset.insert(QStringLiteral("sources"), sources(0.7));
    const auto updated =
        invoke(QStringLiteral("speaker_mix.presets.save"), {
                                                               {"preset", preset}
    });
    QVERIFY2(updated, qPrintable(updated ? QString() : updated.getError().message));
    QCOMPARE(updated.get().value("preset").toObject().value("preset_id").toString(), id);
    const auto stored = SpeakerMixPresetStore::findPreset(id);
    QVERIFY(stored);
    QCOMPARE(stored->name, QStringLiteral("Updated wire blend"));
    QCOMPARE(stored->fixedWeights, QVector<double>{0.7});
    QCOMPARE(runtime().documentVersion(), beforeCatalog);
    historyManager->reset();
    const Automation::SpeakerMixTargetDto target{Automation::SpeakerMixTargetKind::Clip,
                                                 clip->id()};
    const auto baseline = runtime().parameters().getSpeakerMix(beforeCatalog.documentId, target);
    QVERIFY(baseline);
    const auto apply = [&] {
        const auto version = runtime().documentVersion();
        return invoke(QStringLiteral("speaker_mix.presets.apply"),
                      {
                          {"document_id",       version.documentId.toString()                    },
                          {"expected_revision", static_cast<qint64>(version.revision)            },
                          {"preset_id",         id                                               },
                          {"target",            QJsonObject{{"type", "clip"}, {"id", clip->id()}}}
        });
    };
    const auto applied = apply();
    QVERIFY2(applied, qPrintable(applied ? QString() : applied.getError().message));
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    const auto mix =
        runtime().parameters().getSpeakerMix(runtime().documentVersion().documentId, target);
    QVERIFY(mix);
    QCOMPARE(mix.get().mix.mode, SpeakerMixModel::SingerSourceMode::FixedMix);
    QCOMPARE(mix.get().mix.sources.size(), 2);
    QCOMPARE(mix.get().mix.sources.first().speaker.id(), first);
    QCOMPARE(mix.get().mix.sources.last().speaker.id(), second);
    QCOMPARE(mix.get().mix.fixedWeights, QVector<double>{0.7});
    QCOMPARE(mix.get().mix.sourcePresetId, id);
    const auto appliedVersion = runtime().documentVersion();
    const auto *undo = historyManager->nextUndoEntry();
    const auto removed =
        invoke(QStringLiteral("speaker_mix.presets.delete"), {
                                                                 {"preset_id", id}
    });
    QVERIFY2(removed, qPrintable(removed ? QString() : removed.getError().message));
    QVERIFY(!SpeakerMixPresetStore::findPreset(id));
    const auto stale = apply();
    QVERIFY(!stale);
    QCOMPARE(stale.getError().code, Automation::AutomationErrorCode::NotFound);
    QCOMPARE(runtime().documentVersion(), appliedVersion);
    QCOMPARE(historyManager->nextUndoEntry(), undo);
    const auto retained = runtime().parameters().getSpeakerMix(appliedVersion.documentId, target);
    QVERIFY(retained);
    QCOMPARE(retained.get().mix.fixedWeights, mix.get().mix.fixedWeights);
    QVERIFY(runtime().history().undo(commandContext()));
    const auto undone =
        runtime().parameters().getSpeakerMix(runtime().documentVersion().documentId, target);
    QVERIFY(undone);
    QCOMPARE(undone.get().mix.mode, baseline.get().mix.mode);
    QCOMPARE(undone.get().speaker.id(), baseline.get().speaker.id());
    QVERIFY(!historyManager->canUndo());
}

void ApplicationWorkflowTests::lyricRulesUseTheProductionRuntimeAndPersistence() {
    auto &settings = runtime().settings();
    Automation::LyricRuleDraftDto draft;
    draft.kind = Automation::LyricRuleKind::Tagger;
    draft.name = QStringLiteral("Fixture language override");
    draft.language = QStringLiteral("cmn");
    draft.position = 0;
    draft.entries = {
        {.type = QStringLiteral("array"),
         .value = {QStringLiteral("fixtureword")},
         .tag = QStringLiteral("word")}
    };
    const auto created = settings.createLyricRule({}, draft);
    QVERIFY2(created, qPrintable(created ? QString{} : created.getError().message));
    const auto id = created.get().rule.ruleId;
    const auto cleanup = qScopeGuard([&] { settings.deleteLyricRule({}, id); });
    const auto preview = settings.testLyricRules(QStringLiteral("fixtureword"));
    QVERIFY(preview);
    QCOMPARE(preview.get().taggedTokens.size(), 1);
    QCOMPARE(preview.get().taggedTokens.first().lyric, QStringLiteral("fixtureword"));
    QCOMPARE(preview.get().taggedTokens.first().language, QStringLiteral("cmn"));

    QVERIFY(settings.updateLyricRule({}, id, {.name = QStringLiteral("Renamed rule")}));
    QVERIFY(settings.setLyricRuleEnabled({}, id, false));
    const auto disabledPreview = settings.testLyricRules(QStringLiteral("fixtureword"));
    QVERIFY(disabledPreview);
    QCOMPARE(disabledPreview.get().taggedTokens.size(), 1);
    QCOMPARE(disabledPreview.get().taggedTokens.first().language, QStringLiteral("eng"));
    AppOptions reopened;
    const auto rules = Automation::createAppOptionsAutomationServices(&reopened).lyricRules();
    const auto it = std::find_if(rules.cbegin(), rules.cend(),
                                 [&](const auto &rule) { return rule.ruleId == id; });
    QVERIFY(it != rules.cend());
    QCOMPARE(it->name, QStringLiteral("Renamed rule"));
    QVERIFY(!it->enabled);
    QVERIFY(settings.deleteLyricRule({}, id));
    const auto remaining = settings.listLyricRules();
    QVERIFY(remaining);
    QVERIFY(std::none_of(remaining.get().cbegin(), remaining.get().cend(),
                         [&](const auto &rule) { return rule.ruleId == id; }));
}

void ApplicationWorkflowTests::projectBatchImportUsesRealLoaders_data() {
    QTest::addColumn<bool>("invalidItem");
    QTest::addColumn<bool>("bestEffort");
    QTest::newRow("midi-and-dspx") << false << false;
    QTest::newRow("atomic-failure") << true << false;
    QTest::newRow("best-effort") << true << true;
}

void ApplicationWorkflowTests::projectBatchImportUsesRealLoaders() {
    QFETCH(bool, invalidItem);
    QFETCH(bool, bestEffort);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto dspx = files.filePath(QStringLiteral("source.dspx"));
    const auto midi = files.filePath(QStringLiteral("source.mid"));
    QString error;
    DspxProjectConverter dspxConverter;
    MidiConverter midiConverter;
    QVERIFY2(dspxConverter.save(dspx, context->m_appModel, error), qPrintable(error));
    QVERIFY2(midiConverter.save(midi, context->m_appModel, error), qPrintable(error));
    if (invalidItem) {
        QFile broken(midi);
        QVERIFY(broken.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(broken.write("invalid midi"), qint64(12));
    }
    const auto before = runtime().documentVersion();
    const auto initialTrackCount = context->m_appModel->tracks().size();
    Automation::PublicDocumentBatchImportRequest request;
    request.command = commandContext();
    request.failurePolicy = bestEffort ? Automation::PublicBatchFailurePolicy::BestEffort
                                       : Automation::PublicBatchFailurePolicy::Atomic;
    request.items = {
        {.canonicalPath = dspx, .formatId = QStringLiteral("dspx")},
        {.canonicalPath = midi, .formatId = QStringLiteral("midi")}
    };
    const auto services = Automation::createPublicAutomationHostServices(
        runtime(), context->m_appModel, &SynthrtEngine::instance());
    const auto accepted = services.importDocuments(request);
    QVERIFY2(accepted, qPrintable(accepted ? QString{} : accepted.getError().message));
    const auto task = [&] {
        return runtime().tasks().getTask(before.documentId, accepted.get().taskId);
    };
    QTRY_VERIFY_WITH_TIMEOUT(
        task() && (task().get().state == Automation::AutomationTaskState::Succeeded ||
                   task().get().state == Automation::AutomationTaskState::Failed),
        10000);
    if (invalidItem && !bestEffort) {
        QCOMPARE(task().get().state, Automation::AutomationTaskState::Failed);
        QCOMPARE(runtime().documentVersion(), before);
        QCOMPARE(context->m_appModel->tracks().size(), initialTrackCount);
    } else {
        const auto terminal = task().get();
        QVERIFY2(terminal.state == Automation::AutomationTaskState::Succeeded,
                 qPrintable(terminal.error ? terminal.error->message : QString{}));
        QVERIFY(context->m_appModel->tracks().size() > initialTrackCount);
        QCOMPARE(runtime().documentVersion().revision, before.revision + 1);
        QVERIFY(runtime().history().undo(commandContext()));
        QCOMPARE(context->m_appModel->tracks().size(), initialTrackCount);
        QCOMPARE(context->m_appModel->tracks().first()->name(), QStringLiteral("Target"));
    }
}

void ApplicationWorkflowTests::publicSingleProjectImportUsesThePreparedPlanAndKeepsTheDocument() {
    QTemporaryDir files;
    QVERIFY(files.isValid());
    AppModel source;
    source.setTimeline(Timeline(
        {
            {0, 87.0}
    },
        {{0, 6, 8}}));
    auto *sourceTrack = new Track;
    sourceTrack->setName(QStringLiteral("Imported lead"));
    auto *sourceClip = new SingingClip;
    sourceClip->setStart(1920);
    sourceClip->setLength(1920);
    sourceClip->setClipLen(1920);
    auto *sourceNote = new Note(sourceClip);
    sourceNote->setLocalStart(240);
    sourceNote->setLength(120);
    sourceNote->setKeyIndex(72);
    sourceNote->setLyric(QStringLiteral("你好"));
    sourceNote->setLanguage(QStringLiteral("cmn"));
    sourceClip->insertNote(sourceNote);
    sourceTrack->insertClip(sourceClip);
    QVERIFY(source.appendTrack(sourceTrack));
    const auto path = files.filePath(QStringLiteral("待导入.dspx"));
    DspxProjectConverter converter;
    QString error;
    QVERIFY2(converter.save(path, &source, error), qPrintable(error));
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    historyManager->reset();
    const auto before = runtime().documentVersion();
    const auto beforeModel = context->m_appModel->serialize();
    const auto originalTracks = context->m_appModel->tracks();
    Automation::AutomationAccessPolicy access(AutomationWire::ControlLevel::L3);
    Automation::AutomationFileGuard fileGuard;
    Automation::AdmissionController admission;
    QVERIFY(fileGuard.setConfiguredRoots({files.path()}));
    Automation::PublicAutomationRegistry registry(
        runtime(), access, fileGuard, admission,
        Automation::createPublicAutomationHostServices(runtime(), context->m_appModel,
                                                       &SynthrtEngine::instance()));
    const auto inspected = registry.invoke(
        QStringLiteral("formats.inspect"),
        {
            {QStringLiteral("path"),    path                    },
            {QStringLiteral("purpose"), QStringLiteral("import")}
    });
    QVERIFY2(inspected, qPrintable(inspected ? QString() : inspected.getError().message));
    const auto digest = inspected.get().value(QStringLiteral("plan_digest")).toString();
    QVERIFY(!digest.isEmpty());
    QCOMPARE(runtime().documentVersion(), before);
    QCOMPARE(context->m_appModel->serialize(), beforeModel);
    const auto accepted = registry.invoke(
        QStringLiteral("documents.import"),
        {
            {QStringLiteral("document_id"),       before.documentId.toString()        },
            {QStringLiteral("expected_revision"), static_cast<qint64>(before.revision)},
            {QStringLiteral("path"),              path                                },
            {QStringLiteral("options"),           QJsonObject{}                       },
            {QStringLiteral("plan_digest"),       digest                              }
    },
        {.clientId = QStringLiteral("project-import-client"),
         .source = Automation::InvocationSource::PublicJsonRpc});
    QVERIFY2(accepted, qPrintable(accepted ? QString() : accepted.getError().message));
    const auto id =
        Automation::TaskId::fromString(accepted.get().value(QStringLiteral("task_id")).toString());
    QVERIFY(!id.isNull());
    const auto task = [&] { return runtime().tasks().getTask(before.documentId, id); };
    QTRY_VERIFY_WITH_TIMEOUT(
        task() && (task().get().state == Automation::AutomationTaskState::Succeeded ||
                   task().get().state == Automation::AutomationTaskState::Failed),
        10000);
    const auto terminal = task().get();
    QVERIFY2(terminal.state == Automation::AutomationTaskState::Succeeded,
             qPrintable(terminal.error ? terminal.error->message : QString()));
    QCOMPARE(runtime().documentVersion().documentId, before.documentId);
    const auto tracks = context->m_appModel->tracks();
    QCOMPARE(tracks.size(), originalTracks.size() + 1);
    for (int index = 0; index < originalTracks.size(); ++index)
        QCOMPARE(tracks.at(index), originalTracks.at(index));
    const auto *importedTrack = tracks.last();
    QCOMPARE(importedTrack->name(), sourceTrack->name());
    QCOMPARE(importedTrack->clips().count(), 1);
    const auto *importedClip = qobject_cast<SingingClip *>(*importedTrack->clips().begin());
    QVERIFY(importedClip);
    QCOMPARE(importedClip->start(), sourceClip->start());
    QCOMPARE(importedClip->notes().count(), 1);
    const auto *importedNote = *importedClip->notes().begin();
    QCOMPARE(importedNote->localStart(), sourceNote->localStart());
    QCOMPARE(importedNote->keyIndex(), sourceNote->keyIndex());
    QCOMPARE(importedNote->lyric(), sourceNote->lyric());
    QVERIFY(runtime().history().undo(commandContext()));
    QCOMPARE(context->m_appModel->serialize(), beforeModel);
    QVERIFY(!historyManager->canUndo());
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
}

void ApplicationWorkflowTests::failedInferenceInitializationReleasesPackageWaiters() {
    TestSupport::ProcessFixture fixture(QStringLiteral("unavailable-inference-provider"));
    QVERIFY(fixture.isValid());
    auto &process = fixture.process(QStringLiteral("application"));
    process.start(QCoreApplication::applicationFilePath(),
                  {QStringLiteral("--unavailable-inference-provider")});
    QVERIFY2(process.waitForStarted(5000), qPrintable(process.errorString()));
    const auto finished = process.waitForFinished(10000);
    const auto diagnostics = QString::fromUtf8(TestSupport::readProcessStdout(process)) +
                             QString::fromUtf8(TestSupport::readProcessStderr(process));
    QVERIFY2(finished, qPrintable(diagnostics));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QVERIFY2(process.exitCode() == 0, qPrintable(diagnostics));
}

void ApplicationWorkflowTests::changedTargetInputDropsResult() {
    QFETCH(QString, stage);
    const auto snapshot = captureTask(stage, *piece);
    QVERIFY(!snapshot.documentVersion.documentId.isNull());
    QVERIFY(!snapshot.inputSignature.isEmpty());
    QVERIFY(snapshot.taskId >= 0);
    InferenceTaskResolution resolution;
    QCOMPARE(InferenceApplyGate::resolve(snapshot, resolution), Decision::Apply);
    QCOMPARE(resolution.clip, clip);
    QCOMPARE(resolution.piece, piece);
    QCOMPARE(resolution.notes, QList<Note *>({note}));

    // The semantic check must reject changed input even before revision bookkeeping advances.
    note->setKeyIndex(note->keyIndex() + 1);
    QCOMPARE(clip->inferenceRevision(), snapshot.clipRevision);
    QCOMPARE(InferenceApplyGate::resolve(snapshot, resolution), Decision::Drop);
    QCOMPARE(resolution.dropReason, QStringLiteral("input-signature-mismatch"));
    QVERIFY(!resolution.clip);
    QVERIFY(!resolution.piece);
    QVERIFY(resolution.notes.isEmpty());
}

void ApplicationWorkflowTests::removedTargetDropsResult_data() {
    QTest::addColumn<bool>("replaceDocument");
    QTest::newRow("replace-document") << true;
    QTest::newRow("remove-clip") << false;
}

void ApplicationWorkflowTests::removedTargetDropsResult() {
    QFETCH(bool, replaceDocument);
    const auto snapshot = captureTask(QStringLiteral("acoustic"), *piece);
    InferenceTaskResolution resolution;
    QCOMPARE(InferenceApplyGate::resolve(snapshot, resolution), Decision::Apply);
    if (replaceDocument) {
        QVERIFY(runtime().documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
        QVERIFY(runtime().documentVersion().documentId != snapshot.documentVersion.documentId);
    } else {
        QVERIFY(runtime().project().removeClips(commandContext(),
                                                {Automation::ClipId(snapshot.clipId)}));
        QVERIFY(!context->m_appModel->findClipById(snapshot.clipId));
    }
    QCOMPARE(InferenceApplyGate::resolve(snapshot, resolution), Decision::Drop);
    QCOMPARE(resolution.dropReason, replaceDocument ? QStringLiteral("document-changed")
                                                    : QStringLiteral("clip-not-found"));
    QVERIFY(!resolution.clip);
    QVERIFY(!resolution.piece);
    QVERIFY(resolution.notes.isEmpty());
}

void ApplicationWorkflowTests::unchangedInputSurvivesRevisionDrift_data() {
    addInferenceStages();
}

void ApplicationWorkflowTests::unchangedInputSurvivesRevisionDrift() {
    QFETCH(QString, stage);
    const auto snapshot = captureTask(stage, *piece);
    QVERIFY(runtime().project().renameTrack(commandContext(), trackId,
                                            QStringLiteral("Renamed while inference runs")));
    QVERIFY(runtime().documentVersion().revision > snapshot.documentVersion.revision);
    QCOMPARE(runtime().documentVersion().documentId, snapshot.documentVersion.documentId);

    // Re-segmenting unchanged notes advances the clip revision while preserving this piece.
    clip->reSegment(context->m_appModel->timeline());
    QCOMPARE(clip->findPieceById(snapshot.pieceId), piece);
    QVERIFY(clip->inferenceRevision() > snapshot.clipRevision);
    QCOMPARE(InferControllerHelper::buildSemanticSignature(stage, *piece, snapshot.singer),
             snapshot.inputSignature);
    InferenceTaskResolution resolution;
    QCOMPARE(InferenceApplyGate::resolve(snapshot, resolution), Decision::Apply);
    QCOMPARE(resolution.clip, clip);
    QCOMPARE(resolution.piece, piece);
    QCOMPARE(resolution.notes, QList<Note *>({note}));
    QVERIFY(resolution.dropReason.isEmpty());
}

void ApplicationWorkflowTests::editSessionControlsResultDeferral_data() {
    using Domain = AppStatus::EditObjectType;
    QTest::addColumn<QString>("stage");
    QTest::addColumn<int>("domain");
    QTest::addColumn<QString>("scope");
    QTest::addColumn<bool>("deferred");
    QTest::newRow("same-note") << QStringLiteral("acoustic") << int(Domain::Note)
                               << QStringLiteral("note") << true;
    QTest::newRow("other-clip") << QStringLiteral("acoustic") << int(Domain::Note)
                                << QStringLiteral("other-clip") << false;
    QTest::newRow("piece-phonemes")
        << QStringLiteral("duration") << int(Domain::Phoneme) << QStringLiteral("piece") << true;
    QTest::newRow("whole-clip") << QStringLiteral("acoustic") << int(Domain::Clip)
                                << QStringLiteral("clip") << true;
    QTest::newRow("pitch-affects-acoustic")
        << QStringLiteral("acoustic") << int(Domain::Param) << QStringLiteral("pitch") << true;
    QTest::newRow("pitch-does-not-affect-duration")
        << QStringLiteral("duration") << int(Domain::Param) << QStringLiteral("pitch") << false;
}

void ApplicationWorkflowTests::editSessionControlsResultDeferral() {
    QFETCH(QString, stage);
    QFETCH(int, domain);
    QFETCH(QString, scope);
    QFETCH(bool, deferred);
    const auto snapshot = captureTask(stage, *piece);
    EditSession session;
    session.domain = static_cast<AppStatus::EditObjectType>(domain);
    session.clipId = scope == QStringLiteral("other-clip") ? otherClip->id() : clip->id();
    if (scope == QStringLiteral("note"))
        session.noteIds = {note->id()};
    else if (scope == QStringLiteral("other-clip"))
        session.noteIds = {(*otherClip->notes().begin())->id()};
    else if (scope == QStringLiteral("piece"))
        session.pieceIds = {piece->id()};
    else if (scope == QStringLiteral("pitch"))
        session.params = {ParamInfo::Pitch};
    const auto sessionId = editSessionManager->beginTransaction(session);
    QVERIFY(sessionId != 0);

    InferenceApplyGate::Options options;
    options.checkEditSession = true;
    options.expectedNoteCount = snapshot.noteIds.size();
    InferenceTaskResolution resolution;
    QCOMPARE(InferenceApplyGate::resolve(snapshot, resolution, options),
             deferred ? Decision::Defer : Decision::Apply);
    QCOMPARE(resolution.clip, clip);
    QCOMPARE(resolution.piece, piece);
    QCOMPARE(resolution.notes, QList<Note *>({note}));
    QCOMPARE(resolution.dropReason, deferred ? QStringLiteral("edit-session-conflict") : QString());
    QCOMPARE(runtime().documentVersion(), snapshot.documentVersion);

    editSessionManager->endTransaction(sessionId, EditSessionEndReason::Discard);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QCOMPARE(InferenceApplyGate::resolve(snapshot, resolution, options), Decision::Apply);
    QVERIFY(resolution.dropReason.isEmpty());
}

void ApplicationWorkflowTests::restartInferenceReleasesReplacedTask_data() {
    QTest::addColumn<bool>("completionQueued");
    QTest::newRow("running-worker") << false;
    QTest::newRow("completion-queued") << true;
}

void ApplicationWorkflowTests::prepareInferenceTarget(
    AppStatus::ModuleStatus &previousPackageStatus) {
    const QPointer<SingingClip> targetClip(clip);
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->languageModuleStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->inferEngineEnvStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    QTRY_VERIFY_WITH_TIMEOUT(
        appStatus->packageModuleStatus.get() != AppStatus::ModuleStatus::Loading, 10000);
    previousPackageStatus = appStatus->packageModuleStatus.get();
    // Supply package availability without installing a singer or loading its models.
    appStatus->packageModuleStatus = AppStatus::ModuleStatus::Ready;
    // Finish startup retries while the document still has no selected singer.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    QVERIFY(targetClip);
    {
        const QSignalBlocker blockVoiceNotification(targetClip);
        const SingerInfo singer(SingerIdentifier{QStringLiteral("missing-singer"),
                                                 QStringLiteral("workflow-test"),
                                                 QVersionNumber(1, 0, 0)});
        targetClip->setOwnSingerAndSpeaker(singer, {});
        targetClip->removeAllPieces();
        targetClip->reSegment(context->m_appModel->timeline());
    }
    QCOMPARE(targetClip->pieces().size(), 1);
    piece = targetClip->pieces().first();
}

void ApplicationWorkflowTests::restartInferenceReleasesReplacedTask() {
    QFETCH(bool, completionQueued);
    auto packageStatus = appStatus->packageModuleStatus.get();
    const auto restorePackageStatus =
        qScopeGuard([&packageStatus] { appStatus->packageModuleStatus = packageStatus; });
    prepareInferenceTarget(packageStatus);
    if (QTest::currentTestFailed())
        return;
    const QPointer<InferPiece> targetPiece(piece);
    const auto targetPieceId = targetPiece->id();

    QSemaphore workerEntered;
    QSemaphore releaseWorker;
    std::atomic_bool paused = false;
    std::atomic_bool replacementPaused = false;
    QObject observations;
    QPointer<InferDurationTask> firstTask;
    int firstTaskId = -1;
    int replacementTaskId = -1;
    bool replacementFinished = false;
    bool replacementRequested = false;
    bool staleError = false;
    connect(targetPiece, &InferPiece::stateChanged, &observations, [&](const QString &state) {
        if (replacementRequested && !replacementFinished &&
            state.endsWith(QStringLiteral(".Error")))
            staleError = true;
    });
    connect(taskManager, &TaskManager::taskChanged, &observations,
            [&](TaskManager::TaskChangeType change, Task *task, qsizetype) {
                auto *duration = qobject_cast<InferDurationTask *>(task);
                if (change != TaskManager::Added || !duration ||
                    duration->pieceId() != targetPieceId)
                    return;
                if (firstTaskId < 0) {
                    firstTask = duration;
                    firstTaskId = duration->id();
                    // Pause the real worker before it requests the external singer session.
                    connect(
                        duration, &Task::statusUpdated, &observations,
                        [&](const TaskStatus &) {
                            if (!paused.exchange(true)) {
                                workerEntered.release();
                                releaseWorker.acquire();
                            }
                        },
                        Qt::DirectConnection);
                    if (completionQueued) {
                        // The state receiver queues its transition before this observer restarts
                        // it.
                        connect(duration, &Task::finished, &observations, [&] {
                            replacementRequested = true;
                            inferController->restartPieceInference(*targetPiece);
                        });
                    }
                } else {
                    replacementTaskId = duration->id();
                    if (completionQueued) {
                        connect(
                            duration, &Task::statusUpdated, &observations,
                            [&](const TaskStatus &) {
                                if (!replacementPaused.exchange(true))
                                    releaseWorker.acquire();
                            },
                            Qt::DirectConnection);
                    }
                    connect(duration, &Task::finished, &observations,
                            [&] { replacementFinished = true; });
                }
            });
    const auto drainTasks = qScopeGuard([&] {
        releaseWorker.release();
        inferController->cancelPieceInference(targetPieceId);
        QThreadPool::globalInstance()->waitForDone();
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    });

    targetPiece->state = QStringLiteral("Ready");
    inferController->restartPieceInference(*targetPiece);
    QCOMPARE(targetPiece->state.get(), QStringLiteral("Duration.Pending"));
    QCOMPARE(targetPiece->acousticInferStatus.get(), Pending);
    QTRY_VERIFY_WITH_TIMEOUT(workerEntered.available() == 1, 5000);
    QVERIFY(firstTask);
    QVERIFY(firstTask->started());
    QVERIFY(!firstTask->stopped());
    QVERIFY(targetPiece);
    const QPointer<InferPipeline> firstPipeline =
        targetPiece->findChild<InferPipeline *>(Qt::FindDirectChildrenOnly);
    QVERIFY(firstPipeline);

    if (completionQueued) {
        releaseWorker.release();
    } else {
        replacementRequested = true;
        inferController->restartPieceInference(*targetPiece);
    }
    QTRY_VERIFY_WITH_TIMEOUT(firstPipeline.isNull() && replacementTaskId >= 0, 5000);
    QVERIFY2(!staleError, "A replaced pipeline must not publish its queued failure to the piece");
    QVERIFY(targetPiece);
    const QPointer<InferPipeline> replacementPipeline =
        targetPiece->findChild<InferPipeline *>(Qt::FindDirectChildrenOnly);
    QVERIFY(replacementPipeline);
    QPointer<InferPipeline> backgroundPipeline = new InferPipeline(*targetPiece);
    const auto removeBackgroundPipeline = qScopeGuard([&] { delete backgroundPipeline.data(); });
    enum class AcousticPermitPhase { Background, Requested, Completed };
    for (const auto phase : {AcousticPermitPhase::Background, AcousticPermitPhase::Requested,
                             AcousticPermitPhase::Completed}) {
        auto *subject = phase == AcousticPermitPhase::Background ? backgroundPipeline.data()
                                                                 : replacementPipeline.data();
        QVERIFY(targetPiece);
        QVERIFY(subject);
        verifyAcousticGate(*subject, phase == AcousticPermitPhase::Requested,
                           phase == AcousticPermitPhase::Completed);
        if (QTest::currentTestFailed())
            return;
    }
    releaseWorker.release();
    QTRY_VERIFY_WITH_TIMEOUT(replacementFinished, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!taskManager->findTaskById(firstTaskId) &&
                                 !taskManager->findTaskById(replacementTaskId),
                             5000);
    // An unavailable singer must fail the replacement normally instead of leaving it queued.
    QTRY_VERIFY_WITH_TIMEOUT(
        targetPiece && targetPiece->state.get() == QStringLiteral("Duration.Error"), 5000);
    QCOMPARE(targetPiece->acousticInferStatus.get(), Failed);
    QVERIFY2(!staleError, "Only the replacement task may publish its terminal state");
}

void ApplicationWorkflowTests::publicInferenceStartsBeforeQueuedDocumentChanges() {
    auto packageStatus = appStatus->packageModuleStatus.get();
    const auto restorePackageStatus =
        qScopeGuard([&packageStatus] { appStatus->packageModuleStatus = packageStatus; });
    prepareInferenceTarget(packageStatus);
    if (QTest::currentTestFailed())
        return;
    const auto targetPieceId = piece->id();
    const auto services = Automation::createPublicAutomationHostServices(
        runtime(), context->m_appModel, &SynthrtEngine::instance());
    Automation::PublicInferenceStartRequest request;
    request.command = commandContext();
    request.command.source = Automation::InvocationSource::PublicJsonRpc;
    request.scope = {
        {QStringLiteral("kind"),     QStringLiteral("clip")},
        {QStringLiteral("clip_ids"), QJsonArray{clip->id()}}
    };

    QVERIFY(runtime().project().renameTrack(commandContext(), trackId,
                                            QStringLiteral("Edited before admission")));
    const auto rejected = services.startInference(request);
    QVERIFY(!rejected);
    QCOMPARE(rejected.getError().code, Automation::AutomationErrorCode::RevisionConflict);

    QObject observations;
    QSemaphore workerEntered;
    QSemaphore releaseWorker;
    std::atomic_bool paused = false;
    connect(taskManager, &TaskManager::taskChanged, &observations,
            [&](TaskManager::TaskChangeType change, Task *task, qsizetype) {
                auto *duration = qobject_cast<InferDurationTask *>(task);
                if (change != TaskManager::Added || !duration ||
                    duration->pieceId() != targetPieceId)
                    return;
                connect(
                    duration, &Task::statusUpdated, &observations,
                    [&](const TaskStatus &) {
                        if (!paused.exchange(true)) {
                            workerEntered.release();
                            releaseWorker.acquire();
                        }
                    },
                    Qt::DirectConnection);
            });
    const auto drainTasks = qScopeGuard([&] {
        releaseWorker.release();
        inferController->cancelPieceInference(targetPieceId);
        QThreadPool::globalInstance()->waitForDone();
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    });

    bool queuedEditApplied = false;
    QTimer::singleShot(0, &observations, [&] {
        queuedEditApplied = bool(runtime().project().renameTrack(
            commandContext(), trackId, QStringLiteral("Edited while inference starts")));
    });
    request.command.expected = runtime().documentVersion();
    const auto accepted = services.startInference(request);
    QVERIFY2(accepted, qPrintable(accepted ? QString{} : accepted.getError().message));
    const auto snapshot = [&] {
        return runtime().tasks().getTask(accepted.get().document.documentId, accepted.get().taskId);
    };
    const auto taskFailed = [&] {
        const auto task = snapshot();
        return !task || task.get().state == Automation::AutomationTaskState::Failed;
    };
    QTRY_VERIFY_WITH_TIMEOUT(queuedEditApplied && (workerEntered.available() == 1 || taskFailed()),
                             5000);
    const auto running = snapshot();
    QVERIFY(running);
    QCOMPARE(running.get().state, Automation::AutomationTaskState::Running);
    QCOMPARE(workerEntered.available(), 1);
    QVERIFY(runtime().documentVersion().revision > accepted.get().document.revision);

    const auto canceled = runtime().automationTasks().requestCancel(
        accepted.get().document.documentId, accepted.get().taskId);
    QVERIFY(canceled);
    const auto terminal = snapshot();
    QVERIFY(terminal);
    QCOMPARE(terminal.get().state, Automation::AutomationTaskState::Canceled);
}

void ApplicationWorkflowTests::cleanup() {
    if (context)
        editSessionManager->clear();
    clip = nullptr;
    otherClip = nullptr;
    piece = nullptr;
    note = nullptr;
}

void ApplicationWorkflowTests::cleanupTestCase() {
    context.reset();
    if (dataRootInstalled) {
        if (previousDataRoot.isNull())
            qunsetenv("DSEL_TEST_DATA_ROOT");
        else
            qputenv("DSEL_TEST_DATA_ROOT", previousDataRoot);
    }
}

void ApplicationWorkflowTests::verifyAcousticGate(InferPipeline &pipeline, bool immediateExpected,
                                                  bool completeFirst) {
    const QPointer<InferPiece> targetPiece(&pipeline.piece());
    const QPointer<InferPipeline> targetPipeline(&pipeline);
    if (completeFirst) {
        QStateMachine readyMachine;
        auto *ready = new PlaybackReadyState(pipeline);
        readyMachine.addState(ready);
        readyMachine.setInitialState(ready);
        readyMachine.start();
        QTRY_VERIFY(targetPiece && targetPiece->state.get() == QStringLiteral("Ready"));
    }
    QVERIFY(targetPiece);
    QVERIFY(targetPipeline);
    // Exercise the production acoustic gate with a completed variance snapshot.
    pipeline.setApplyContext(captureTask(QStringLiteral("variance"), *targetPiece));
    QStateMachine gateMachine;
    auto *variance = new UpdateVarianceState(pipeline);
    gateMachine.addState(variance);
    gateMachine.setInitialState(variance);
    QSignalSpy immediate(variance, &UpdateVarianceState::updateSuccessWithImmediateInference);
    QSignalSpy lazy(variance, &UpdateVarianceState::updateSuccessWithLazyInference);
    gateMachine.start();
    QTRY_VERIFY(immediate.count() + lazy.count() == 1);
    QCOMPARE(immediate.count(), immediateExpected ? 1 : 0);
    QCOMPARE(lazy.count(), immediateExpected ? 0 : 1);
    QVERIFY(!context->m_appOptions->inference()->autoStartInfer);
}

Automation::CoreRuntime &ApplicationWorkflowTests::runtime() {
    return *context->m_coreRuntime;
}

Automation::CommandContext ApplicationWorkflowTests::commandContext() {
    return {.expected = runtime().documentVersion(),
            .source = Automation::InvocationSource::InternalAutomation};
}

int main(int argc, char **argv) {
    if (argc > 1 &&
        QString::fromLocal8Bit(argv[1]) == QStringLiteral("--unavailable-inference-provider"))
        return unavailableInferenceProvider(argc, argv);
    QCoreApplication application(argc, argv);
    ApplicationWorkflowTests tests;
    return QTest::qExec(&tests, argc, argv);
}
