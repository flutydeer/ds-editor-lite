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

#include <lite/History/HistoryManager.h>
#include <lite/PackageManager/PackageManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/InferenceData/InferPiece.h>
#include <lite/Tasking/TaskManager.h>
#include <TalcsCore/AudioBuffer.h>
#include <TalcsCore/MixerAudioSource.h>
#include <TalcsCore/TransportAudioSource.h>

#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTimer>
#include <QSemaphore>
#include <QThreadPool>
#include <QtTest>

#include <algorithm>
#include <atomic>

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

void ApplicationWorkflowTests::editingParametersRestartsOnlyDependentInference_data() {
    QTest::addColumn<ParamInfo::Name>("name");
    QTest::addColumn<int>("value");
    QTest::newRow("expressiveness-recomputes-pitch-and-variance")
        << ParamInfo::Expressiveness << 500;
    QTest::newRow("pitch-recomputes-variance") << ParamInfo::Pitch << 6300;
    QTest::newRow("gender-preserves-pitch-and-variance") << ParamInfo::Gender << 500;
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
    const QPointer<SingingClip> targetClip = dynamic_cast<SingingClip *>(
        *context->m_appModel->tracks().first()->clips().begin());
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
    const auto finishSession = qScopeGuard([] {
        editSessionManager->endActiveTransaction(EditSessionEndReason::Cancel);
    });
    connect(taskManager, &TaskManager::taskChanged, &observations,
            [&](TaskManager::TaskChangeType change, Task *task, qsizetype) {
        auto *pronunciation = qobject_cast<GetPronunciationTask *>(task);
        auto *phonemes = qobject_cast<GetPhonemeNameTask *>(task);
        const bool selected = phonemeStage ? phonemes && phonemes->clipId() == targetClipId
                                          : pronunciation &&
                                                pronunciation->clipId() == targetClipId;
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
            // The controller has resolved and stored the completed result before queue removal.
            resultReceived = true;
            if (phonemeStage) {
                validResult = !task->terminated() && phonemes->success() &&
                              phonemes->result.size() == 1 && phonemes->result.first().success;
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
                }) && taskManager->tasks().isEmpty(),
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
        QVERIFY(runtime().project().removeClips(commandContext(),
                                                {Automation::ClipId(targetClipId)}));
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
