#include "tst_application_workflows.h"

#include "Modules/Inference/EditSessionManager.h"
#include "Modules/Inference/InferController.h"
#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/Tasks/InferAcousticCacheProbeTask.h"
#include "Modules/Inference/Tasks/GetPhonemeNameTask.h"
#include "Modules/Inference/Tasks/GetPronunciationTask.h"
#include "../TestSupport/VoicebankFixture.h"
#include "Controller/PlaybackController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Audio/AudioContext.h"
#include "Modules/Inference/Tasks/InferAcousticTask.h"
#include "Modules/Inference/Tasks/InferDurationTask.h"
#include "Modules/Inference/Tasks/InferPitchTask.h"
#include "Modules/Inference/Tasks/InferVarianceTask.h"
#include "Automation/Public/PublicAutomationHostAdapter.h"

#include <lite/History/HistoryManager.h>
#include <lite/PackageManager/PackageManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/InferenceData/InferPiece.h>
#include <lite/Tasking/TaskManager.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>
#include <TalcsCore/AudioBuffer.h>
#include <TalcsCore/MixerAudioSource.h>
#include <TalcsCore/TransportAudioSource.h>
#include <TalcsFormat/AudioFormatIO.h>

#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTimer>
#include <QSemaphore>
#include <QThreadPool>
#include <QFile>
#include <QDir>
#include <QtTest>

#include <algorithm>
#include <atomic>
#include <cmath>

namespace {
    bool inferenceSettled(const SingingClip *clip) {
        return clip && !clip->pieces().isEmpty() &&
               std::all_of(clip->pieces().cbegin(), clip->pieces().cend(),
                           [](const InferPiece *piece) {
                               return piece->state == QStringLiteral("Acoustic.Awaiting") ||
                                      piece->state == QStringLiteral("Ready");
                           }) &&
               taskManager->tasks().isEmpty();
    }
}

void ApplicationWorkflowTests::prepareVoicebankTarget() {
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->languageModuleStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->inferEngineEnvStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->packageModuleStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    SingerInfo singer;
    for (const auto &package : packageManager->installedPackages().successfulPackages) {
        for (const auto &candidate : package.singers()) {
            if (candidate.singerId() == TestSupport::fixtureSingerId())
                singer = candidate;
        }
    }
    QVERIFY2(!singer.isEmpty(), "The configured fixture singer must be installed");
    QVERIFY(!singer.speakers().isEmpty());
    Automation::NoteWordPatchDto word;
    word.noteId = Automation::NoteId(note->id());
    word.lyric = TestSupport::fixtureLyric();
    word.language = TestSupport::fixtureLanguage();
    word.pronunciation = Pronunciation{};
    word.pronunciationCandidates = QStringList{};
    word.phonemes = Phonemes{};
    QVERIFY(runtime().notes().patchWordProperties(commandContext(), Automation::ClipId(clip->id()),
                                                  {word}));
    QVERIFY(runtime().parameters().selectClipSingleSpeaker(
        commandContext(), Automation::ClipId(clip->id()), singer, singer.speakers().first()));
    QTRY_VERIFY_WITH_TIMEOUT(inferenceSettled(clip), 15000);
    QCOMPARE(clip->pieces().size(), 1);
    piece = clip->pieces().first();
}

void ApplicationWorkflowTests::acousticCacheWriteFailureCanBeRetried() {
    using namespace Automation;
    QTemporaryDir materials;
    QVERIFY(materials.isValid());
    const auto previousCache = appOptions->inference()->cacheDirectory;
    appOptions->inference()->cacheDirectory = materials.filePath(QStringLiteral("preparation"));
    TaskId taskId;
    const auto cleanup = qScopeGuard([&] {
        if (!taskId.isNull())
            runtime().automationTasks().requestCancel(runtime().documentVersion().documentId,
                                                      taskId);
        if (piece)
            inferController->cancelPieceInference(piece->id());
        QThreadPool::globalInstance()->waitForDone();
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        appOptions->inference()->cacheDirectory = previousCache;
        if (QTest::currentTestFailed())
            materials.setAutoRemove(false);
    });
    prepareVoicebankTarget();
    if (QTest::currentTestFailed())
        return;
    const QPointer<InferPiece> target(piece);
    QCOMPARE(target->state.get(), QStringLiteral("Acoustic.Awaiting"));
    const auto beforeNote = note->serialize();
    const auto *beforeUndo = historyManager->nextUndoEntry();
    const auto blockedCache = materials.filePath(QStringLiteral("blocked-cache"));
    QFile blocker(blockedCache);
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    QCOMPARE(blocker.write("unavailable cache directory"), qint64{27});
    blocker.close();
    appOptions->inference()->cacheDirectory = blockedCache;
    const auto services = createPublicAutomationHostServices(runtime(), context->m_appModel,
                                                             &SynthrtEngine::instance());
    PublicInferenceStartRequest request{
        .command = commandContext(),
        .scope = {{QStringLiteral("kind"), QStringLiteral("clip")},
                  {QStringLiteral("clip_ids"), QJsonArray{clip->id()}}},
        .stages = {QStringLiteral("acoustic")}
    };
    const auto accepted = services.startInference(request);
    QVERIFY2(accepted, qPrintable(accepted ? QString{} : accepted.getError().message));
    taskId = accepted.get().taskId;
    const auto currentTask = [&] {
        return runtime().tasks().getTask(runtime().documentVersion().documentId, taskId);
    };
    QTRY_VERIFY_WITH_TIMEOUT(
        currentTask() && currentTask().get().state == AutomationTaskState::Failed, 15000);
    QVERIFY(currentTask().get().error);
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    QVERIFY(target);
    QCOMPARE(target->state.get(), QStringLiteral("Acoustic.Error"));
    QVERIFY(target->audioPath.isEmpty());
    QCOMPARE(note->serialize(), beforeNote);
    QCOMPARE(historyManager->nextUndoEntry(), beforeUndo);

    QVERIFY(QFile::remove(blockedCache));
    QVERIFY(QDir().mkpath(blockedCache));
    request.command = commandContext();
    const auto retried = services.startInference(request);
    QVERIFY2(retried, qPrintable(retried ? QString{} : retried.getError().message));
    QVERIFY(retried.get().taskId != taskId);
    taskId = retried.get().taskId;
    QTRY_VERIFY_WITH_TIMEOUT(
        currentTask() && currentTask().get().state == AutomationTaskState::Succeeded, 15000);
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    QVERIFY(target);
    QCOMPARE(target->state.get(), QStringLiteral("Ready"));
    QVERIFY(QFileInfo(target->audioPath).isFile());
    QCOMPARE(note->serialize(), beforeNote);
    QCOMPARE(historyManager->nextUndoEntry(), beforeUndo);
}

void ApplicationWorkflowTests::unsupportedInferencePhonemeAllowsRetry_data() {
    QTest::addColumn<QString>("stage");
    QTest::newRow("duration") << QStringLiteral("duration");
    QTest::newRow("pitch") << QStringLiteral("pitch");
    QTest::newRow("variance") << QStringLiteral("variance");
    QTest::newRow("acoustic") << QStringLiteral("acoustic");
}

void ApplicationWorkflowTests::unsupportedInferencePhonemeAllowsRetry() {
    QFETCH(QString, stage);
    prepareVoicebankTarget();
    if (QTest::currentTestFailed())
        return;
    QVERIFY(!piece->notes.isEmpty());
    QVERIFY(!piece->notes.first()->phonemeNameSeq().result().isEmpty());
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const auto previousCache = appOptions->inference()->cacheDirectory;
    appOptions->inference()->cacheDirectory = cache.path();
    const auto restoreCache = qScopeGuard([&] {
        appOptions->inference()->cacheDirectory = previousCache;
        if (QTest::currentTestFailed())
            cache.setAutoRemove(false);
    });
    const auto before = context->m_appModel->serialize();
    const auto version = runtime().documentVersion();
    const auto *undo = historyManager->nextUndoEntry();
    const auto singer = clip->singerIdentifier();
    const auto createTask = [&](const bool unsupported) -> std::unique_ptr<IInferTask> {
        const auto prepareInput = [&](auto input) {
            if (unsupported)
                input.notes.first().phonemeNames.first().name = QStringLiteral("unmapped-phoneme");
            return input;
        };
        if (stage == QStringLiteral("duration"))
            return std::make_unique<InferDurationTask>(
                prepareInput(InferControllerHelper::buildInferDurInput(*piece, singer)));
        if (stage == QStringLiteral("pitch"))
            return std::make_unique<InferPitchTask>(
                prepareInput(InferControllerHelper::buildInferPitchInput(*piece, singer)));
        if (stage == QStringLiteral("variance"))
            return std::make_unique<InferVarianceTask>(
                prepareInput(InferControllerHelper::buildInferVarianceInput(*piece, singer)));
        return std::make_unique<InferAcousticTask>(
            prepareInput(InferControllerHelper::buildInferAcousticInput(*piece, singer)));
    };
    auto failed = createTask(true);
    auto retried = createTask(false);
    QThreadPool workers;
    QSignalSpy failureFinished(failed.get(), &Task::finished);
    workers.start(failed.get());
    QTRY_COMPARE_WITH_TIMEOUT(failureFinished.count(), 1, 15000);
    QVERIFY(workers.waitForDone(5000));
    QVERIFY(failed->stopped());
    QVERIFY(!failed->success());
    QVERIFY(!failed->terminated());
    QVERIFY(
        QDir(cache.path()).entryList({QStringLiteral("infer-*-output-*")}, QDir::Files).isEmpty());
    QSignalSpy retryFinished(retried.get(), &Task::finished);
    workers.start(retried.get());
    QTRY_COMPARE_WITH_TIMEOUT(retryFinished.count(), 1, 15000);
    QVERIFY(workers.waitForDone(5000));
    QVERIFY(retried->success());
    QVERIFY(
        !QDir(cache.path()).entryList({QStringLiteral("infer-*-output-*")}, QDir::Files).isEmpty());
    QCOMPARE(context->m_appModel->serialize(), before);
    QCOMPARE(runtime().documentVersion(), version);
    QCOMPARE(historyManager->nextUndoEntry(), undo);
}

void ApplicationWorkflowTests::languageTasksKeepMixedResultsAligned() {
    prepareVoicebankTarget();
    if (QTest::currentTestFailed())
        return;
    const auto language = TestSupport::fixtureLanguage();
    const auto lyric = TestSupport::fixtureLyric();
    const auto singer = clip->singerInfo();
    const auto before = context->m_appModel->serialize();
    const auto version = runtime().documentVersion();
    QList<NoteInferenceSnapshot> inputs{
        {11, lyric,                         language, {}, 0,    480, 60},
        {17, QStringLiteral("SP"),          language, {}, 480,  480, 60},
        {23,
         QStringLiteral("preserve first"),
         QStringLiteral("unavailable-language"),
         {},
         960,                                                   480,
         60                                                            },
        {29, lyric + QLatin1Char('+'),      language, {}, 1440, 480, 60},
        {31,
         QStringLiteral("preserve second"),
         QStringLiteral("unavailable-language"),
         {},
         1920,                                                  480,
         60                                                            },
        {37, QStringLiteral("-"),           language, {}, 2400, 480, 60},
    };
    const auto execute = [](Task &task) {
        QThreadPool workers;
        QSignalSpy finished(&task, &Task::finished);
        workers.start(&task);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);
        QVERIFY(workers.waitForDone(5000));
    };
    GetPronunciationTask pronunciations(version, clip->id(), clip->inferenceRevision(), inputs,
                                        singer);
    execute(pronunciations);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(pronunciations.result.size(), inputs.size());
    QVERIFY(!pronunciations.result.first().pronunciation.isEmpty());
    QCOMPARE(pronunciations.result.at(3).pronunciation,
             pronunciations.result.first().pronunciation);
    for (const auto index : {1, 2, 4, 5}) {
        QCOMPARE(pronunciations.result.at(index).pronunciation, inputs.at(index).lyric);
        QCOMPARE(pronunciations.result.at(index).candidates, QStringList{inputs.at(index).lyric});
    }
    for (qsizetype index = 0; index < inputs.size(); ++index)
        inputs[index].pronunciation = pronunciations.result.at(index).pronunciation;
    GetPhonemeNameTask phonemes(version, clip->id(), clip->inferenceRevision(), inputs, singer);
    execute(phonemes);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(phonemes.result.size(), inputs.size());
    QVERIFY(!phonemes.success());
    QVERIFY(phonemes.result.first().success);
    QVERIFY(!phonemes.result.first().phonemeNames.isEmpty());
    QVERIFY(phonemes.result.at(1).success);
    QCOMPARE(phonemes.result.at(1).phonemeNames.size(), 1);
    QCOMPARE(phonemes.result.at(1).phonemeNames.first().name, QStringLiteral("SP"));
    for (const auto index : {2, 4}) {
        QVERIFY(!phonemes.result.at(index).success);
        QVERIFY(phonemes.result.at(index).phonemeNames.isEmpty());
    }
    QVERIFY(phonemes.result.at(5).success);
    QVERIFY(phonemes.result.at(5).phonemeNames.isEmpty());

    auto missingSinger = singer;
    missingSinger.setResolutionState(ResolutionState::Missing);
    GetPronunciationTask unresolved(version, clip->id(), clip->inferenceRevision(), inputs,
                                    missingSinger);
    GetPhonemeNameTask unresolvedPhonemes(version, clip->id(), clip->inferenceRevision(), inputs,
                                          missingSinger);
    execute(unresolved);
    execute(unresolvedPhonemes);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(unresolved.result.size(), inputs.size());
    QCOMPARE(unresolvedPhonemes.result.size(), inputs.size());
    QVERIFY(!unresolvedPhonemes.success());
    for (qsizetype index = 0; index < inputs.size(); ++index) {
        QCOMPARE(unresolved.result.at(index).pronunciation, inputs.at(index).lyric);
        QVERIFY(unresolvedPhonemes.result.at(index).phonemeNames.isEmpty());
    }
    QCOMPARE(context->m_appModel->serialize(), before);
    QCOMPARE(runtime().documentVersion(), version);
}

void ApplicationWorkflowTests::editingParametersRestartsOnlyDependentInference_data() {
    QTest::addColumn<ParamInfo::Name>("name");
    QTest::addColumn<int>("value");
    QTest::newRow("expressiveness-recomputes-pitch-and-variance")
        << ParamInfo::Expressiveness << 500;
    QTest::newRow("pitch-recomputes-variance") << ParamInfo::Pitch << 6300;
    QTest::newRow("gender-preserves-pitch-and-variance") << ParamInfo::Gender << 500;
}

void ApplicationWorkflowTests::cancelingVoiceExportDuringPreparationAllowsAnotherExport() {
    QTemporaryDir materials;
    QVERIFY(materials.isValid());
    const auto previousCache = appOptions->inference()->cacheDirectory;
    appOptions->inference()->cacheDirectory = materials.filePath(QStringLiteral("cache"));
    const auto restore = qScopeGuard([&] {
        QVERIFY(runtime().documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 15000);
        appOptions->inference()->cacheDirectory = previousCache;
    });
    prepareVoicebankTarget();
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(piece->state.get(), QStringLiteral("Acoustic.Awaiting"));
    const auto target = materials.filePath(QStringLiteral("voice.wav"));
    QFile file(target);
    const QByteArray original("previous export");
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(original), original.size());
    file.close();
    Automation::AudioExportConfigDto config;
    config.fileName = QStringLiteral("voice.wav");
    config.fileDirectory = materials.path();
    config.sourceOption = 2;
    config.sources = {0};
    const Automation::AudioExportPolicyDto policy{.allowOverwrite = true};
    Automation::TaskId exportId;
    bool canceledDuringInference = false;
    bool renderedBeforeCancellation = false;
    Automation::AudioExportObserver observer;
    observer.progress = [&](double, int) { renderedBeforeCancellation = true; };
    observer.inferenceProgress = [&](double progress) {
        if (canceledDuringInference || progress >= 1.0)
            return;
        const auto active =
            runtime().tasks().getTask(runtime().documentVersion().documentId, exportId);
        QVERIFY(active);
        QCOMPARE(active.get().state, Automation::AutomationTaskState::Running);
        QVERIFY(active.get().progress.indeterminate);
        canceledDuringInference = true;
        QVERIFY(runtime().tasks().cancelTask(commandContext(), exportId));
    };
    const auto accepted =
        runtime().audioExports().start(commandContext(), config, policy, observer);
    QVERIFY2(accepted, qPrintable(accepted ? QString() : accepted.getError().message));
    exportId = accepted.get().taskId;
    const auto terminalState = [&] {
        const auto result =
            runtime().tasks().getTask(runtime().documentVersion().documentId, exportId);
        return result ? result.get().state : Automation::AutomationTaskState::Failed;
    };
    QTRY_COMPARE_WITH_TIMEOUT(terminalState(), Automation::AutomationTaskState::Canceled, 15000);
    QVERIFY(canceledDuringInference);
    QVERIFY(!renderedBeforeCancellation);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), original);
    file.close();
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 15000);
    const auto retry = runtime().audioExports().start(commandContext(), config, policy);
    QVERIFY2(retry, qPrintable(retry ? QString() : retry.getError().message));
    exportId = retry.get().taskId;
    QTRY_COMPARE_WITH_TIMEOUT(terminalState(), Automation::AutomationTaskState::Succeeded, 15000);
    QVERIFY(file.open(QIODevice::ReadOnly));
    talcs::AudioFormatIO reader(&file);
    QVERIFY(reader.open(talcs::AbstractAudioFormatIO::Read));
    QCOMPARE(reader.sampleRate(), config.sampleRate);
    QVERIFY(reader.length() > 0);
    QVERIFY(reader.channelCount() > 0);
    QVector<float> samples(static_cast<qsizetype>(reader.length()) * reader.channelCount());
    QCOMPARE(reader.read(samples.data(), reader.length()), reader.length());
    QVERIFY(std::all_of(samples.cbegin(), samples.cend(),
                        [](float value) { return std::isfinite(value); }));
    QVERIFY(std::any_of(samples.cbegin(), samples.cend(),
                        [](float value) { return std::abs(value) > 1.0e-5f; }));
    reader.close();
    file.close();
    QCOMPARE(QDir(materials.path()).entryList(QDir::Files | QDir::Hidden),
             QStringList{QStringLiteral("voice.wav")});
}

void ApplicationWorkflowTests::editingParametersRestartsOnlyDependentInference() {
    QFETCH(ParamInfo::Name, name);
    QFETCH(int, value);
    prepareVoicebankTarget();
    if (QTest::currentTestFailed())
        return;
    const auto releaseResults = qScopeGuard([&] {
        QVERIFY(runtime().documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    });
    const QPointer<InferPiece> target(piece);
    inferController->startPendingAcousticInference();
    QTRY_VERIFY_WITH_TIMEOUT(target && target->state == QStringLiteral("Ready") &&
                                 taskManager->tasks().isEmpty(),
                             15000);
    const auto inputBefore = *target->getInputCurve(name);
    const auto samplesBefore = inputBefore.mid(720);
    QVERIFY(!samplesBefore.isEmpty());
    if (samplesBefore.first() == value)
        value += 100;
    const auto offsetsBefore = note->phonemes().offsetSeq.original;
    const auto otherBefore = otherClip->params.getParamByName(name)->curves(Param::Edited);
    HistoryManager::instance()->reset();
    QSet<QString> stages;
    QObject observations;
    connect(taskManager, &TaskManager::taskChanged, &observations,
            [&](TaskManager::TaskChangeType change, Task *task, qsizetype) {
                if (change != TaskManager::Added || !target)
                    return;
                if (const auto *item = qobject_cast<InferDurationTask *>(task);
                    item && item->pieceId() == target->id())
                    stages.insert(QStringLiteral("duration"));
                if (const auto *item = qobject_cast<InferPitchTask *>(task);
                    item && item->pieceId() == target->id())
                    stages.insert(QStringLiteral("pitch"));
                if (const auto *item = qobject_cast<InferVarianceTask *>(task);
                    item && item->pieceId() == target->id())
                    stages.insert(QStringLiteral("variance"));
                if (const auto *item = qobject_cast<InferAcousticCacheProbeTask *>(task);
                    item && item->pieceId() == target->id())
                    stages.insert(QStringLiteral("cache"));
            });
    const auto changed =
        runtime().parameters().drawParameter(commandContext(), Automation::ClipId(clip->id()), name,
                                             Param::Edited, 480, 5, QList<int>(97, value), false);
    QVERIFY(changed && changed.get().changed);
    QTRY_VERIFY_WITH_TIMEOUT(stages.contains(QStringLiteral("cache")) && inferenceSettled(clip),
                             15000);
    QCOMPARE(clip->pieces().first(), target.data());
    QCOMPARE(target->getInputCurve(name)->mid(720).first(), value);
    QCOMPARE(note->phonemes().offsetSeq.original, offsetsBefore);
    QCOMPARE(otherClip->params.getParamByName(name)->curves(Param::Edited), otherBefore);
    QVERIFY(!stages.contains(QStringLiteral("duration")));
    QCOMPARE(stages.contains(QStringLiteral("pitch")), name == ParamInfo::Expressiveness);
    QCOMPARE(stages.contains(QStringLiteral("variance")), name != ParamInfo::Gender);
    stages.clear();
    QVERIFY(runtime().history().undo(commandContext()));
    QTRY_VERIFY_WITH_TIMEOUT(stages.contains(QStringLiteral("cache")) && inferenceSettled(clip),
                             15000);
    QCOMPARE(*target->getInputCurve(name), inputBefore);
    QVERIFY(!HistoryManager::instance()->canUndo());
}

void ApplicationWorkflowTests::playbackWindowPrioritizesAndSuspendsAcousticInference() {
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    auto *audio = AudioContext::instance();
    const auto previousCache = appOptions->inference()->cacheDirectory;
    const auto previousLookahead = appOptions->inference()->playbackLookaheadSeconds;
    const auto previousReadAhead = audio->bufferingReadAheadSize();
    const auto restore = qScopeGuard([&] {
        audio->preMixer()->close();
        audio->setBufferingReadAheadSize(previousReadAhead);
        playbackController->setPlaybackStartGuard([] { return false; });
        appOptions->inference()->cacheDirectory = previousCache;
        appOptions->inference()->playbackLookaheadSeconds = previousLookahead;
    });
    appOptions->inference()->cacheDirectory = cache.path();
    appOptions->inference()->playbackLookaheadSeconds = 1.0;
    audio->setBufferingReadAheadSize(0);
    QSemaphore entered;
    QSemaphore release;
    std::atomic_bool blocked = false;
    const auto drain = qScopeGuard([&] {
        release.release();
        runtime().playback().stop(commandContext());
        QVERIFY(runtime().documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
        QThreadPool::globalInstance()->waitForDone();
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    });
    prepareVoicebankTarget();
    if (QTest::currentTestFailed())
        return;
    const auto singer = clip->singerInfo();
    const auto speaker = clip->speakerInfo();
    QVERIFY(runtime().project().patchClipProperties(
        commandContext(), {.id = Automation::ClipId(clip->id()), .start = 2400}));
    QVERIFY(runtime().project().patchClipProperties(
        commandContext(), {.id = Automation::ClipId(otherClip->id()), .start = 3360}));
    const auto otherNote = *otherClip->notes().begin();
    QVERIFY(
        runtime().notes().patchWordProperties(commandContext(), Automation::ClipId(otherClip->id()),
                                              {
                                                  {.noteId = Automation::NoteId(otherNote->id()),
                                                   .lyric = TestSupport::fixtureLyric(),
                                                   .language = TestSupport::fixtureLanguage(),
                                                   .pronunciation = Pronunciation{},
                                                   .pronunciationCandidates = QStringList{},
                                                   .phonemes = Phonemes{}}
    }));
    QVERIFY(runtime().notes().moveNotes(commandContext(), Automation::ClipId(otherClip->id()),
                                        {Automation::NoteId(otherNote->id())}, 0, 4));
    QVERIFY(runtime().parameters().selectClipSingleSpeaker(
        commandContext(), Automation::ClipId(otherClip->id()), singer, speaker));
    QList<QPointer<SingingClip>> outside;
    for (const auto [start, key] : {qMakePair(0, 55), qMakePair(9600, 67)}) {
        Automation::NoteDraftDto word;
        word.localStart = 480;
        word.length = 480;
        word.keyIndex = key;
        word.lyric = TestSupport::fixtureLyric();
        word.language = TestSupport::fixtureLanguage();
        Automation::ClipDraftDto draft;
        draft.type = Automation::ClipDraftDto::Type::Singing;
        draft.properties.start = start;
        draft.properties.length = 1920;
        draft.properties.clipLen = 1920;
        draft.notes = {word};
        const auto inserted = runtime().project().insertClips(
            commandContext(), {
                                  {.trackId = trackId, .clip = draft}
        });
        QVERIFY(inserted && !inserted.get().affectedObjects.isEmpty());
        auto *added = dynamic_cast<SingingClip *>(
            context->m_appModel->findClipById(inserted.get().affectedObjects.first().value));
        QVERIFY(added);
        outside.append(added);
        QVERIFY(runtime().parameters().selectClipSingleSpeaker(
            commandContext(), Automation::ClipId(added->id()), singer, speaker));
    }
    const QList<QPointer<SingingClip>> targets{clip, otherClip, outside.first(), outside.last()};
    QTRY_VERIFY_WITH_TIMEOUT(std::all_of(targets.cbegin(), targets.cend(),
                                         [](const auto &target) {
                                             return target && target->pieces().size() == 1 &&
                                                    target->pieces().first()->state ==
                                                        QStringLiteral("Acoustic.Awaiting");
                                         }) &&
                                 taskManager->tasks().isEmpty(),
                             15000);
    const QPointer<InferPiece> currentPiece(clip->pieces().first());
    const QPointer<InferPiece> nextPiece(otherClip->pieces().first());
    const QPointer<InferPiece> pastPiece(outside.first()->pieces().first());
    const QPointer<InferPiece> farPiece(outside.last()->pieces().first());
    QPointer<InferAcousticTask> currentWorker;
    QPointer<InferAcousticTask> queuedWorker;
    QSet<int> registeredPieces;
    QObject observations;
    connect(taskManager, &TaskManager::taskChanged, &observations,
            [&](TaskManager::TaskChangeType change, Task *task, qsizetype) {
                auto *acoustic = qobject_cast<InferAcousticTask *>(task);
                if (change != TaskManager::Added || !acoustic)
                    return;
                registeredPieces.insert(acoustic->pieceId());
                if (acoustic->pieceId() == nextPiece->id())
                    queuedWorker = acoustic;
                if (acoustic->pieceId() != currentPiece->id())
                    return;
                currentWorker = acoustic;
                connect(
                    acoustic, &Task::statusUpdated, &observations,
                    [&](const TaskStatus &) {
                        if (!blocked.exchange(true)) {
                            entered.release();
                            release.acquire();
                        }
                    },
                    Qt::DirectConnection);
            });
    QVERIFY(audio->preMixer()->open(256, 48000));
    // The test supplies audio callbacks while retaining the production playback scheduler.
    playbackController->setPlaybackStartGuard([] { return true; });
    QVERIFY(runtime().playback().setPosition(commandContext(), 3000));
    QVERIFY(runtime().playback().play(commandContext()));
    QTRY_COMPARE_WITH_TIMEOUT(entered.available(), 1, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(queuedWorker, 5000);
    QVERIFY(currentWorker && currentWorker->started());
    QVERIFY(!queuedWorker->started());
    QVERIFY(!registeredPieces.contains(pastPiece->id()));
    QVERIFY(!registeredPieces.contains(farPiece->id()));
    QVERIFY(runtime().playback().pause(commandContext()));
    QTRY_VERIFY(nextPiece->state == QStringLiteral("Acoustic.Awaiting") && !queuedWorker);
    QVERIFY(currentWorker && !currentWorker->terminated());
    release.release();
    QTRY_VERIFY_WITH_TIMEOUT(
        currentPiece->state == QStringLiteral("Ready") && taskManager->tasks().isEmpty(), 15000);
    QCOMPARE(nextPiece->state.get(), QStringLiteral("Acoustic.Awaiting"));
    QCOMPARE(pastPiece->state.get(), QStringLiteral("Acoustic.Awaiting"));
    QCOMPARE(farPiece->state.get(), QStringLiteral("Acoustic.Awaiting"));
    QVERIFY(runtime().playback().setPosition(commandContext(), 3840));
    QVERIFY(runtime().playback().play(commandContext()));
    QTRY_VERIFY_WITH_TIMEOUT(
        nextPiece->state == QStringLiteral("Ready") && taskManager->tasks().isEmpty(), 15000);
    QCOMPARE(farPiece->state.get(), QStringLiteral("Acoustic.Awaiting"));
    QTRY_COMPARE(audio->transport()->bufferingCounter(), 0);
    talcs::AudioBuffer buffer(2, 256);
    const auto position = audio->transport()->position();
    QCOMPARE(audio->preMixer()->read(&buffer), qint64{256});
    QVERIFY(audio->transport()->position() > position);
    QVERIFY(runtime().playback().setPosition(commandContext(), 10080));
    QTRY_VERIFY_WITH_TIMEOUT(
        farPiece->state == QStringLiteral("Ready") && taskManager->tasks().isEmpty(), 15000);
    QCOMPARE(pastPiece->state.get(), QStringLiteral("Acoustic.Awaiting"));
    QVERIFY(!registeredPieces.contains(pastPiece->id()));
}

void ApplicationWorkflowTests::changingSpeakerMixRefreshesExistingInference() {
    prepareVoicebankTarget();
    if (QTest::currentTestFailed())
        return;
    const auto singer = clip->singerInfo();
    if (singer.speakers().size() < 2)
        QSKIP("The configured voicebank needs two speakers for mixing");
    const QPointer<InferPiece> target(piece);
    const auto originalMix = target->speakerMix;
    const auto offsets = note->phonemes().offsetSeq.original;
    SpeakerMixModel::SpeakerMixData mix;
    mix.mode = SpeakerMixModel::SingerSourceMode::FixedMix;
    mix.sources = {{singer.speakers().at(0)}, {singer.speakers().at(1)}};
    mix.fixedWeights = {0.25};
    HistoryManager::instance()->reset();
    QSignalSpy states(target, &InferPiece::stateChanged);
    const auto changed = runtime().parameters().replaceClipSpeakerMix(
        commandContext(), Automation::ClipId(clip->id()), mix);
    QVERIFY(changed && changed.get().changed);
    QTRY_VERIFY_WITH_TIMEOUT(!states.isEmpty() && inferenceSettled(clip), 15000);
    QCOMPARE(clip->pieces().first(), target.data());
    QCOMPARE(target->speakerMix.sources.size(), 2);
    QCOMPARE(target->speakerMix.sources.first().speaker, singer.speakers().first().id());
    QCOMPARE(target->speakerMix.sources.first().proportions.first(), 0.25);
    QCOMPARE(target->speakerMix.sources.last().proportions.first(), 0.75);
    QCOMPARE(note->phonemes().offsetSeq.original, offsets);
    states.clear();
    QVERIFY(runtime().history().undo(commandContext()));
    QTRY_VERIFY_WITH_TIMEOUT(!states.isEmpty() && inferenceSettled(clip), 15000);
    QCOMPARE(clip->pieces().first(), target.data());
    QCOMPARE(target->speakerMix, originalMix);
    QVERIFY(!HistoryManager::instance()->canUndo());
}

void ApplicationWorkflowTests::movingInheritedVoiceReusesOrRebuildsInference_data() {
    QTest::addColumn<bool>("differentSpeaker");
    QTest::newRow("same-voice-preserves-existing-piece") << false;
    QTest::newRow("different-voice-rebuilds-piece") << true;
}

void ApplicationWorkflowTests::movingInheritedVoiceReusesOrRebuildsInference() {
    QFETCH(bool, differentSpeaker);
    prepareVoicebankTarget();
    if (QTest::currentTestFailed())
        return;
    const auto singer = clip->singerInfo();
    if (differentSpeaker && singer.speakers().size() < 2)
        QSKIP("The configured voicebank needs two speakers for voice changes");
    const auto firstSpeaker = singer.speakers().first();
    const auto nextSpeaker = singer.speakers().at(differentSpeaker ? 1 : 0);
    const auto secondTrack = Automation::TrackId(context->m_appModel->tracks().last()->id());
    QVERIFY(
        runtime().project().removeClips(commandContext(), {Automation::ClipId(otherClip->id())}));
    otherClip = nullptr;
    QVERIFY(runtime().parameters().selectTrackSingleSpeaker(commandContext(), trackId, singer,
                                                            firstSpeaker));
    QVERIFY(runtime().parameters().selectTrackSingleSpeaker(commandContext(), secondTrack, singer,
                                                            nextSpeaker));
    QVERIFY(runtime().parameters().useTrackVoiceContext(commandContext(),
                                                        Automation::ClipId(clip->id())));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QTRY_VERIFY_WITH_TIMEOUT(inferenceSettled(clip), 15000);
    const QPointer<InferPiece> before(clip->pieces().first());
    const auto oldStart = clip->start();
    HistoryManager::instance()->reset();
    QVERIFY(runtime().project().moveClips(
        commandContext(), {
                              {Automation::ClipId(clip->id()), secondTrack, oldStart}
    }));
    QTRY_VERIFY_WITH_TIMEOUT(inferenceSettled(clip), 15000);
    QCOMPARE(clip->speakerInfo(), nextSpeaker);
    QCOMPARE(clip->pieces().first()->speaker, nextSpeaker.id());
    if (differentSpeaker)
        QVERIFY(!before);
    else
        QCOMPARE(clip->pieces().first(), before.data());
    const QPointer<InferPiece> moved(clip->pieces().first());
    QVERIFY(runtime().history().undo(commandContext()));
    QTRY_VERIFY_WITH_TIMEOUT(inferenceSettled(clip), 15000);
    QCOMPARE(clip->speakerInfo(), firstSpeaker);
    QCOMPARE(clip->pieces().first()->speaker, firstSpeaker.id());
    if (differentSpeaker)
        QVERIFY(!moved);
    else
        QCOMPARE(clip->pieces().first(), before.data());
    QVERIFY(!HistoryManager::instance()->canUndo());
}

void ApplicationWorkflowTests::clipInferenceResultsRespectEditSession_data() {
    QTest::addColumn<bool>("phonemeStage");
    QTest::addColumn<QString>("completion");
    QTest::newRow("pronunciation-defer-then-apply") << false << QStringLiteral("apply");
    QTest::newRow("phoneme-defer-then-apply") << true << QStringLiteral("apply");
    QTest::newRow("pronunciation-document-replaced") << false << QStringLiteral("replace-document");
    QTest::newRow("phoneme-clip-removed") << true << QStringLiteral("remove-clip");
}

void ApplicationWorkflowTests::clipInferenceResultsRespectEditSession() {
    QFETCH(bool, phonemeStage);
    QFETCH(QString, completion);
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->languageModuleStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->inferEngineEnvStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->packageModuleStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    SingerInfo singer;
    for (const auto &package : packageManager->installedPackages().successfulPackages) {
        for (const auto &candidate : package.singers()) {
            if (candidate.singerId() == TestSupport::fixtureSingerId())
                singer = candidate;
        }
    }
    QVERIFY2(!singer.isEmpty(), "The configured fixture singer must be installed");
    QVERIFY(!singer.speakers().isEmpty());
    QVERIFY(!TestSupport::fixtureLyric().isEmpty());
    QVERIFY(!TestSupport::fixtureLanguage().isEmpty());

    Automation::NoteDraftDto draftNote;
    draftNote.length = 480;
    draftNote.lyric = TestSupport::fixtureLyric();
    draftNote.language = TestSupport::fixtureLanguage();
    Automation::ClipDraftDto draftClip;
    draftClip.properties.name = QStringLiteral("Deferred words");
    draftClip.properties.length = 1920;
    draftClip.properties.clipLen = 1920;
    draftClip.defaultLanguage = TestSupport::fixtureLanguage();
    draftClip.notes = {draftNote};
    Automation::TrackDraftDto draftTrack;
    draftTrack.name = QStringLiteral("Inference target");
    draftTrack.defaultLanguage = TestSupport::fixtureLanguage();
    draftTrack.clips = {draftClip};
    auto document = Automation::DocumentAutomationFacade::newDocumentDraft(false);
    document.tracks = {draftTrack};
    QVERIFY(runtime().documents().commitNewDocument(commandContext(), document));
    const QPointer<SingingClip> targetClip =
        dynamic_cast<SingingClip *>(*context->m_appModel->tracks().first()->clips().begin());
    QVERIFY(targetClip);
    const QPointer<Note> targetNote = *targetClip->notes().begin();
    QVERIFY(targetNote);
    const auto targetClipId = targetClip->id();
    const auto targetNoteId = targetNote->id();
    QVERIFY(targetNote->pronunciation().original.isEmpty());
    QVERIFY(targetNote->phonemes().nameSeq.original.isEmpty());
    // Drain insertion-triggered startup while the clip still has no singer.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);

    quint64 editSessionId = 0;
    int observedTaskId = -1;
    bool resultReceived = false;
    bool validResult = false;
    QString expectedPronunciation;
    QList<PhonemeName> expectedPhonemes;
    Automation::DocumentVersion stageBase;
    const ActionSequence *stageUndo = nullptr;
    QObject observations;
    const auto finishSession =
        qScopeGuard([] { editSessionManager->endActiveTransaction(EditSessionEndReason::Cancel); });
    connect(taskManager, &TaskManager::taskChanged, &observations,
            [&](TaskManager::TaskChangeType change, Task *task, qsizetype) {
                auto *pronunciation = qobject_cast<GetPronunciationTask *>(task);
                auto *phonemes = qobject_cast<GetPhonemeNameTask *>(task);
                const bool selected =
                    phonemeStage ? phonemes && phonemes->clipId() == targetClipId
                                 : pronunciation && pronunciation->clipId() == targetClipId;
                if (!selected)
                    return;
                if (change == TaskManager::Added && observedTaskId < 0) {
                    observedTaskId = task->id();
                    stageBase = runtime().documentVersion();
                    stageUndo = HistoryManager::instance()->nextUndoEntry();
                    // TaskManager announces the task before its worker starts.
                    editSessionId = editSessionManager->beginTransaction(
                        phonemeStage ? AppStatus::EditObjectType::Phoneme
                                     : AppStatus::EditObjectType::Note,
                        targetClipId, {}, {targetNoteId});
                } else if (change == TaskManager::Removed && task->id() == observedTaskId) {
                    // The controller has resolved and stored the completed result before queue
                    // removal.
                    resultReceived = true;
                    if (phonemeStage) {
                        validResult = !task->terminated() && phonemes->success() &&
                                      phonemes->result.size() == 1 &&
                                      phonemes->result.first().success;
                        if (validResult)
                            expectedPhonemes = phonemes->result.first().phonemeNames;
                    } else {
                        validResult = !task->terminated() && pronunciation->result.size() == 1;
                        if (validResult)
                            expectedPronunciation = pronunciation->result.first().pronunciation;
                    }
                }
            });
    QVERIFY(runtime().parameters().selectClipSingleSpeaker(
        commandContext(), Automation::ClipId(targetClipId), singer, singer.speakers().first()));
    QTRY_VERIFY_WITH_TIMEOUT(resultReceived, 15000);
    QVERIFY(validResult);
    QVERIFY(editSessionId != 0);
    QVERIFY(editSessionManager->hasActiveTransaction());
    QCOMPARE(runtime().documentVersion(), stageBase);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), stageUndo);
    QVERIFY(targetNote->phonemes().nameSeq.original.isEmpty());
    if (phonemeStage) {
        QVERIFY(!expectedPhonemes.isEmpty());
        QVERIFY(!targetNote->pronunciation().original.isEmpty());
    } else {
        QVERIFY(!expectedPronunciation.isEmpty());
        QVERIFY(targetNote->pronunciation().original.isEmpty());
    }

    if (completion == QStringLiteral("apply")) {
        editSessionManager->endTransaction(editSessionId, EditSessionEndReason::Discard);
        QTRY_VERIFY_WITH_TIMEOUT(!targetNote->phonemes().nameSeq.original.isEmpty(), 15000);
        if (phonemeStage)
            QCOMPARE(targetNote->phonemes().nameSeq.original, expectedPhonemes);
        else
            QCOMPARE(targetNote->pronunciation().original, expectedPronunciation);
        QTRY_VERIFY_WITH_TIMEOUT(
            !targetClip->pieces().isEmpty() &&
                std::all_of(targetClip->pieces().cbegin(), targetClip->pieces().cend(),
                            [](const InferPiece *piece) {
                                return piece->state == QStringLiteral("Acoustic.Awaiting") ||
                                       piece->state == QStringLiteral("Ready");
                            }) &&
                taskManager->tasks().isEmpty(),
            15000);
        QVERIFY(runtime().documentVersion().revision > stageBase.revision);
        QCOMPARE(HistoryManager::instance()->nextUndoEntry(), stageUndo);
        return;
    }

    if (completion == QStringLiteral("replace-document")) {
        QVERIFY(runtime().documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
        QVERIFY(runtime().documentVersion().documentId != stageBase.documentId);
        QVERIFY(!targetClip);
        QVERIFY(!targetNote);
    } else {
        QVERIFY(
            runtime().project().removeClips(commandContext(), {Automation::ClipId(targetClipId)}));
        QVERIFY(!context->m_appModel->findClipById(targetClipId));
        editSessionManager->endTransaction(editSessionId, EditSessionEndReason::Commit);
    }
    const auto afterRemoval = runtime().documentVersion();
    const auto *afterUndo = HistoryManager::instance()->nextUndoEntry();
    bool flushDispatched = false;
    QTimer::singleShot(0, &observations, [&] { flushDispatched = true; });
    QTRY_VERIFY(flushDispatched);
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    QCOMPARE(runtime().documentVersion(), afterRemoval);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), afterUndo);
    QVERIFY(!context->m_appModel->findClipById(targetClipId));
    if (targetNote)
        QVERIFY(targetNote->phonemes().nameSeq.original.isEmpty());
}
