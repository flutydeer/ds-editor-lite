#include "tst_application_workflows.h"

#include "Modules/Inference/EditSessionManager.h"
#include "Modules/Inference/Tasks/GetPhonemeNameTask.h"
#include "Modules/Inference/Tasks/GetPronunciationTask.h"
#include "../TestSupport/VoicebankFixture.h"

#include <lite/History/HistoryManager.h>
#include <lite/PackageManager/PackageManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/InferenceData/InferPiece.h>
#include <lite/Tasking/TaskManager.h>

#include <QPointer>
#include <QScopeGuard>
#include <QTimer>
#include <QtTest>

#include <algorithm>

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
