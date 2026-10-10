#include "tst_application_workflows.h"

#include "Modules/Inference/EditSessionManager.h"
#include "Modules/Inference/InferController.h"
#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/Tasks/InferAcousticCacheProbeTask.h"
#include "Modules/Inference/Tasks/GetPhonemeNameTask.h"
#include "Modules/Inference/Tasks/GetPronunciationTask.h"
#include "Modules/Inference/Utils/InferCacheUtils.h"
#include "../TestSupport/VoicebankFixture.h"
#include "Controller/PlaybackController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Audio/AudioContext.h"
#include "Modules/Inference/Tasks/InferAcousticTask.h"
#include "Modules/Inference/Tasks/InferDurationTask.h"
#include "Modules/Inference/Tasks/InferPitchTask.h"
#include "Modules/Inference/Tasks/InferVarianceTask.h"
#include "Automation/Public/PublicAutomationHostAdapter.h"
#include "Automation/Public/PublicAutomationRegistry.h"

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
#include <synthrt/G2P/LanguageService.h>

#include <QPointer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
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
#include <filesystem>
#include <utility>

namespace {
    void pauseAtFirstTaskStatus(Task *task, QObject *observer, std::atomic_bool &paused,
                                QSemaphore &entered, QSemaphore &resume) {
        // The caller keeps the synchronization objects alive until the resumed task finishes.
        QObject::connect(
            task, &Task::statusUpdated, observer,
            [&paused, &entered, &resume](const TaskStatus &) {
                if (!paused.exchange(true)) {
                    entered.release();
                    resume.acquire();
                }
            },
            Qt::DirectConnection);
    }
}

bool ApplicationWorkflowTests::inferenceSettled(const SingingClip *clip) {
    return clip && !clip->pieces().isEmpty() &&
           std::all_of(clip->pieces().cbegin(), clip->pieces().cend(),
                       [](const InferPiece *piece) {
                           return piece->state == QStringLiteral("Acoustic.Awaiting") ||
                                  piece->state == QStringLiteral("Ready");
                       }) &&
           taskManager->tasks().isEmpty();
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

void ApplicationWorkflowTests::modelInferenceWaitsForEditingBeforeApplying_data() {
    QTest::addColumn<QString>("stage");
    QTest::addColumn<bool>("replaceDocument");
    QTest::addColumn<bool>("changeInput");
    QTest::newRow("duration") << QStringLiteral("duration") << false << false;
    QTest::newRow("pitch") << QStringLiteral("pitch") << false << false;
    QTest::newRow("variance") << QStringLiteral("variance") << false << false;
    QTest::newRow("acoustic") << QStringLiteral("acoustic") << false << false;
    QTest::newRow("replace-document") << QStringLiteral("acoustic") << true << false;
    QTest::newRow("stale-pitch-input") << QStringLiteral("pitch") << false << true;
}

void ApplicationWorkflowTests::modelInferenceWaitsForEditingBeforeApplying() {
    QFETCH(QString, stage);
    QFETCH(bool, replaceDocument);
    QFETCH(bool, changeInput);
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const auto previousCache = appOptions->inference()->cacheDirectory;
    appOptions->inference()->cacheDirectory = cache.path();
    const auto restoreCache = qScopeGuard([&] {
        appOptions->inference()->cacheDirectory = previousCache;
        if (QTest::currentTestFailed())
            cache.setAutoRemove(false);
    });
    QSemaphore workerEntered;
    QSemaphore releaseWorker;
    std::atomic_bool paused = false;
    int stageTasks = 0;
    QObject observations;
    const auto cleanup = qScopeGuard([&] {
        releaseWorker.release();
        editSessionManager->endActiveTransaction(EditSessionEndReason::Discard);
        QVERIFY(runtime().documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 15000);
    });
    prepareVoicebankTarget();
    if (QTest::currentTestFailed())
        return;
    const QPointer<InferPiece> target(piece);
    const QPointer<Note> targetNote(note);
    connect(taskManager, &TaskManager::taskChanged, &observations,
            [&](TaskManager::TaskChangeType change, Task *task, qsizetype) {
                auto *inference = dynamic_cast<IInferTask *>(task);
                if (change != TaskManager::Added || !inference || !target ||
                    inference->pieceId() != target->id() ||
                    inference->inferenceContext().taskType != stage)
                    return;
                ++stageTasks;
                pauseAtFirstTaskStatus(task, &observations, paused, workerEntered, releaseWorker);
            });
    if (stage == QStringLiteral("acoustic"))
        inferController->startPendingAcousticInference();
    else
        inferController->restartPieceInference(*target);
    QTRY_VERIFY_WITH_TIMEOUT(workerEntered.available() == 1, 10000);
    QVERIFY(target && targetNote);
    const auto hasResult = [&] {
        if (stage == QStringLiteral("duration"))
            return !targetNote->phonemeOffsetSeq().original.isEmpty();
        if (stage == QStringLiteral("pitch"))
            return !target->originalPitch.isEmpty();
        if (stage == QStringLiteral("variance"))
            return !target->originalBreathiness.isEmpty();
        return !target->audioPath.isEmpty();
    };
    QVERIFY(!hasResult());
    const auto session =
        editSessionManager->beginTransaction({.domain = AppStatus::EditObjectType::Note,
                                              .clipId = clip->id(),
                                              .noteIds = {targetNote->id()}});
    QVERIFY(session != 0);
    const auto document = runtime().documentVersion();
    const auto model = TestSupport::projectSnapshot(*context->m_appModel);
    const auto *undo = historyManager->nextUndoEntry();
    releaseWorker.release();
    auto waitingState = stage;
    waitingState[0] = waitingState.at(0).toUpper();
    waitingState += QStringLiteral(".AwaitingEditSessionApply");
    QTRY_COMPARE_WITH_TIMEOUT(target->state.get(), waitingState, 15000);
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    QVERIFY(!hasResult());
    QCOMPARE(runtime().documentVersion(), document);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), model);
    QCOMPARE(historyManager->nextUndoEntry(), undo);

    if (replaceDocument) {
        QVERIFY(runtime().documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
        QVERIFY(target.isNull());
        const auto replacement = runtime().documentVersion();
        QVERIFY(replacement.documentId != document.documentId);
        editSessionManager->endTransaction(session, EditSessionEndReason::Discard);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        QCoreApplication::processEvents();
        QVERIFY(context->m_appModel->tracks().isEmpty());
        QCOMPARE(runtime().documentVersion(), replacement);
        QVERIFY(!historyManager->canUndo());
        return;
    }
    const auto originalKey = targetNote->keyIndex();
    const auto originalClipRevision = clip->inferenceRevision();
    const auto tasksBeforeInputChange = stageTasks;
    if (changeInput) {
        // Queued results must be revalidated when inputs change before revision bookkeeping.
        targetNote->setKeyIndex(originalKey + 1);
        QCOMPARE(runtime().documentVersion(), document);
        QCOMPARE(clip->inferenceRevision(), originalClipRevision);
    }
    editSessionManager->endTransaction(session, EditSessionEndReason::Discard);
    if (changeInput)
        QTRY_VERIFY_WITH_TIMEOUT(stageTasks > tasksBeforeInputChange, 15000);
    QTRY_VERIFY_WITH_TIMEOUT(inferenceSettled(clip), 15000);
    QVERIFY(hasResult());
    QCOMPARE(targetNote->keyIndex(), originalKey + (changeInput ? 1 : 0));
    QCOMPARE(runtime().documentVersion().documentId, document.documentId);
    QCOMPARE(historyManager->nextUndoEntry(), undo);
    if (stage == QStringLiteral("acoustic")) {
        QCOMPARE(target->acousticInferStatus.get(), Success);
        QVERIFY(QFileInfo::exists(target->audioPath));
    }
}

void ApplicationWorkflowTests::cacheCleanupProtectsRestoredInference() {
    QTemporaryDir materials;
    QVERIFY(materials.isValid());
    const auto previousCache = appOptions->inference()->cacheDirectory;
    const auto cacheDirectory = materials.filePath(QStringLiteral("cache"));
    appOptions->inference()->cacheDirectory = cacheDirectory;
    const auto restoreCache = qScopeGuard([&] {
        appOptions->inference()->cacheDirectory = previousCache;
        if (QTest::currentTestFailed())
            materials.setAutoRemove(false);
    });
    const auto releaseProject = qScopeGuard([&] {
        QVERIFY(runtime().documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 15000);
    });
    prepareVoicebankTarget();
    if (QTest::currentTestFailed())
        return;
    inferController->startPendingAcousticInference();
    QTRY_VERIFY_WITH_TIMEOUT(
        piece->state == QStringLiteral("Ready") && taskManager->tasks().isEmpty(), 15000);
    const auto cached = InferAcousticTask::lookupCache(
        InferControllerHelper::buildInferAcousticInput(*piece, clip->singerIdentifier()));
    QVERIFY(cached.hit);
    QCOMPARE(piece->audioPath, cached.outputCachePath);
    QMap<QString, QByteArray> protectedFiles;
    for (const auto &path : {cached.inputCachePath, cached.outputCachePath}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        protectedFiles.insert(path, file.readAll());
        QVERIFY(!protectedFiles.value(path).isEmpty());
    }

    auto restored = Automation::DocumentAutomationFacade::newDocumentDraft(false);
    restored.timeline = context->m_appModel->timeline();
    restored.masterControl = context->m_appModel->masterControl();
    restored.tracks = {Automation::trackDraftDto(*context->m_appModel->tracks().first())};
    const auto oldDocument = runtime().documentVersion().documentId;
    QVERIFY(runtime().documents().commitNewDocument(commandContext(), restored));
    QVERIFY(runtime().documentVersion().documentId != oldDocument);
    clip = dynamic_cast<SingingClip *>(*context->m_appModel->tracks().first()->clips().begin());
    QVERIFY(clip);
    QTRY_VERIFY_WITH_TIMEOUT(inferenceSettled(clip), 15000);
    QCOMPARE(clip->pieces().size(), 1);
    piece = clip->pieces().first();
    QCOMPARE(piece->state.get(), QStringLiteral("Ready"));
    QCOMPARE(piece->audioPath, cached.outputCachePath);
    // A cache probe restores playback without registering a new acoustic task.
    QVERIFY(!InferCacheUtils::registeredCacheFiles().contains(
        QFileInfo(cached.outputCachePath).absoluteFilePath().toLower()));

    const auto unused =
        QDir(cacheDirectory)
            .filePath(
                QStringLiteral("infer-acoustic-output-%1.wav").arg(QString(40, QLatin1Char('0'))));
    const auto unrelated = QDir(cacheDirectory).filePath(QStringLiteral("user-note.txt"));
    for (const auto &path : {unused, unrelated}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("unrelated data"), qint64{14});
    }
    const auto document = runtime().documentVersion();
    const auto model = TestSupport::projectSnapshot(*context->m_appModel);
    const auto result =
        InferCacheUtils::cleanCache(cacheDirectory, InferCacheUtils::collectActiveCacheFiles());
    QVERIFY(result.retainedActiveCount >= protectedFiles.size());
    QVERIFY(result.deletedCount > 0);
    QVERIFY(!QFileInfo::exists(unused));
    QVERIFY(QFileInfo::exists(unrelated));
    for (auto it = protectedFiles.cbegin(); it != protectedFiles.cend(); ++it) {
        QFile file(it.key());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), it.value());
    }
    QCOMPARE(runtime().documentVersion(), document);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), model);
    QVERIFY(!historyManager->canUndo());

    QVERIFY(runtime().documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 15000);
    const auto released =
        InferCacheUtils::cleanCache(cacheDirectory, InferCacheUtils::collectActiveCacheFiles());
    QCOMPARE(released.retainedActiveCount, 0);
    QCOMPARE(released.retainedLockedCount, 0);
    for (auto it = protectedFiles.cbegin(); it != protectedFiles.cend(); ++it)
        QVERIFY(!QFileInfo::exists(it.key()));
    QVERIFY(QFileInfo::exists(unrelated));
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

void ApplicationWorkflowTests::inferenceFailureAndCancellationAllowRetry_data() {
    QTest::addColumn<QString>("stage");
    QTest::newRow("duration") << QStringLiteral("duration");
    QTest::newRow("pitch") << QStringLiteral("pitch");
    QTest::newRow("variance") << QStringLiteral("variance");
    QTest::newRow("acoustic") << QStringLiteral("acoustic");
    QTest::newRow("vocoder-runtime-error") << QStringLiteral("vocoder-runtime-error");
}

void ApplicationWorkflowTests::inferenceFailureAndCancellationAllowRetry() {
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
    const auto before = TestSupport::projectSnapshot(*context->m_appModel);
    const auto version = runtime().documentVersion();
    const auto *undo = historyManager->nextUndoEntry();
    const auto singer = clip->singerIdentifier();
    const bool runtimeFailure = stage == QStringLiteral("vocoder-runtime-error");
    auto failureSinger = singer;
    const auto originalRoots = appOptions->general()->packageSearchPaths;
    const auto restorePackages = qScopeGuard([&] {
        if (!runtimeFailure)
            return;
        const auto restored = packageManager->refreshInstalledPackages(originalRoots);
        QVERIFY2(restored, qPrintable(restored ? QString{} : restored.getError().message));
    });
    if (runtimeFailure) {
        const auto root = cache.filePath(QStringLiteral("voicebanks/ci-fixture@1.0.101"));
        QVERIFY(QDir().mkpath(QFileInfo(root).absolutePath()));
        std::error_code copyError;
        std::filesystem::copy(std::filesystem::u8path(LITE_TEST_VOICEBANK_ROOT),
                              std::filesystem::u8path(root.toUtf8().constData()),
                              std::filesystem::copy_options::recursive, copyError);
        QVERIFY2(!copyError, qPrintable(QString::fromStdString(copyError.message())));
        QFile manifest(QDir(root).filePath(QStringLiteral("desc.json")));
        QVERIFY(manifest.open(QIODevice::ReadOnly));
        auto description = QJsonDocument::fromJson(manifest.readAll()).object();
        manifest.close();
        description.insert(QStringLiteral("version"), QStringLiteral("1.0.101"));
        QVERIFY(manifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const auto manifestBytes = QJsonDocument(description).toJson();
        QCOMPARE(manifest.write(manifestBytes), qint64(manifestBytes.size()));
        manifest.close();
        const auto modelPath = QFINDTESTDATA("../resources/inference-vocoder-runtime-error.onnx");
        QVERIFY(!modelPath.isEmpty());
        QFile model(modelPath);
        QVERIFY(model.open(QIODevice::ReadOnly));
        const auto modelBytes = model.readAll();
        QFile target(QDir(root).filePath(QStringLiteral("inferences/vocoder/vocoder.onnx")));
        QVERIFY(target.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(target.write(modelBytes), qint64(modelBytes.size()));
        target.close();
        auto roots = originalRoots;
        roots.append(root);
        const auto refreshed = packageManager->refreshInstalledPackages(roots);
        QVERIFY2(refreshed, qPrintable(refreshed ? QString{} : refreshed.getError().message));
        failureSinger.packageVersion = QVersionNumber(1, 0, 101);
        QVERIFY(!packageManager->findSingerByIdentifier(failureSinger).isEmpty());
        QTest::ignoreMessage(QtCriticalMsg,
                             QRegularExpression(QStringLiteral(
                                 "inferAcoustic: Failed to (start|run) vocoder inference for")));
    }
    const auto createTask = [&](const bool unsupported) -> std::unique_ptr<IInferTask> {
        const auto prepareInput = [&](auto input) {
            if (unsupported && !runtimeFailure)
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
            prepareInput(InferControllerHelper::buildInferAcousticInput(
                *piece, unsupported && runtimeFailure ? failureSinger : singer)));
    };
    auto failed = createTask(true);
    auto retried = createTask(false);
    auto canceled = createTask(false);
    auto cachedRetry = createTask(false);
    QThreadPool workers;
    const auto execute = [&](IInferTask *task, const bool expectedSuccess) {
        QSignalSpy finished(task, &Task::finished);
        workers.start(task);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 15000);
        QVERIFY(workers.waitForDone(5000));
        QVERIFY(task->stopped());
        QVERIFY(!task->terminated());
        QCOMPARE(task->success(), expectedSuccess);
        QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), before);
        QCOMPARE(runtime().documentVersion(), version);
        QCOMPARE(historyManager->nextUndoEntry(), undo);
    };
    execute(failed.get(), false);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(
        QDir(cache.path()).entryList({QStringLiteral("infer-*-output-*")}, QDir::Files).isEmpty());
    execute(retried.get(), true);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(
        !QDir(cache.path()).entryList({QStringLiteral("infer-*-output-*")}, QDir::Files).isEmpty());
    const auto cachedOutputs =
        QDir(cache.path()).entryList({QStringLiteral("infer-*-output-*")}, QDir::Files);
    QSemaphore workerEntered;
    QSemaphore releaseWorker;
    std::atomic_bool paused = false;
    connect(
        canceled.get(), &Task::statusUpdated, canceled.get(),
        [&](const TaskStatus &) {
            if (!paused.exchange(true)) {
                workerEntered.release();
                releaseWorker.acquire();
            }
        },
        Qt::DirectConnection);
    const auto drainWorker = qScopeGuard([&] {
        releaseWorker.release();
        workers.waitForDone();
    });
    QSignalSpy canceledFinished(canceled.get(), &Task::finished);
    workers.start(canceled.get());
    QTRY_VERIFY_WITH_TIMEOUT(workerEntered.available() == 1, 5000);
    QVERIFY(canceled->started());
    QVERIFY(!canceled->stopped());
    canceled->terminate();
    releaseWorker.release();
    QTRY_COMPARE_WITH_TIMEOUT(canceledFinished.count(), 1, 15000);
    QVERIFY(workers.waitForDone(5000));
    QVERIFY(canceled->terminated());
    QVERIFY(canceled->stopped());
    QVERIFY(!canceled->success());
    QCOMPARE(QDir(cache.path()).entryList({QStringLiteral("infer-*-output-*")}, QDir::Files),
             cachedOutputs);
    execute(cachedRetry.get(), true);
    if (QTest::currentTestFailed())
        return;
    if (stage == QStringLiteral("duration") || stage == QStringLiteral("pitch") ||
        stage == QStringLiteral("variance")) {
        const auto cacheFiles =
            QDir(cache.path())
                .entryList({QStringLiteral("infer-%1-output-*.json").arg(stage)}, QDir::Files);
        QCOMPARE(cacheFiles.size(), 1);
        const auto outputPath = cache.filePath(cacheFiles.first());
        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        const auto validBytes = output.readAll();
        output.close();
        const auto writeOutput = [&](const QByteArray &bytes) {
            QVERIFY(output.open(QIODevice::WriteOnly | QIODevice::Truncate));
            QCOMPARE(output.write(bytes), qint64(bytes.size()));
            output.close();
        };
        writeOutput(QByteArrayLiteral("{\"words\":"));
        if (QTest::currentTestFailed())
            return;
        auto regenerated = createTask(false);
        execute(regenerated.get(), true);
        if (QTest::currentTestFailed())
            return;
        QVERIFY(output.open(QIODevice::ReadOnly));
        QJsonParseError parseError;
        const auto regeneratedJson = QJsonDocument::fromJson(output.readAll(), &parseError);
        output.close();
        QCOMPARE(parseError.error, QJsonParseError::NoError);
        QVERIFY(regeneratedJson.isObject());
        if (stage == QStringLiteral("duration")) {
            GenericInferModel corrupted;
            QVERIFY(corrupted.deserialize(QJsonDocument::fromJson(validBytes).object()));
            bool changed = false;
            for (auto &word : corrupted.words) {
                for (auto &phone : word.phones) {
                    if (phone.token != QStringLiteral("SP") &&
                        phone.token != QStringLiteral("AP")) {
                        phone.token = QStringLiteral("cached-phoneme-mismatch");
                        changed = true;
                        break;
                    }
                }
                if (changed)
                    break;
            }
            QVERIFY(changed);
            writeOutput(QJsonDocument(corrupted.serialize()).toJson());
            if (QTest::currentTestFailed())
                return;
            auto mismatched = createTask(false);
            execute(mismatched.get(), false);
            if (QTest::currentTestFailed())
                return;
            writeOutput(validBytes);
            if (QTest::currentTestFailed())
                return;
            auto repaired = createTask(false);
            execute(repaired.get(), true);
            if (QTest::currentTestFailed())
                return;
            QVERIFY(qobject_cast<InferDurationTask *>(repaired.get())->result() ==
                    qobject_cast<InferDurationTask *>(cachedRetry.get())->result());
        }
    }
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), before);
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
    const QList<QPair<QString, QString>> words{
        {QStringLiteral("SP"),              language                              },
        {QStringLiteral("preserve first"),  QStringLiteral("unavailable-language")},
        {lyric + QLatin1Char('+'),          language                              },
        {QStringLiteral("+"),               language                              },
        {QStringLiteral("preserve second"), QStringLiteral("unavailable-language")},
        {QStringLiteral("-"),               language                              },
    };
    QList<Automation::NoteDraftDto> addedNotes;
    auto nextStart = note->localStart() + note->length();
    for (const auto &[word, wordLanguage] : words) {
        Automation::NoteDraftDto draft;
        draft.localStart = nextStart;
        draft.length = 120;
        draft.keyIndex = 60;
        draft.lyric = word;
        draft.language = wordLanguage;
        addedNotes.append(draft);
        nextStart += draft.length;
    }
    const auto inserted =
        runtime().notes().insertNotes(commandContext(), Automation::ClipId(clip->id()), addedNotes);
    QVERIFY2(inserted, qPrintable(inserted ? QString{} : inserted.getError().message));
    auto inputs = buildNoteInferenceSnapshots(*clip);
    QCOMPARE(inputs.size(), addedNotes.size() + 1);
    QTRY_VERIFY_WITH_TIMEOUT(
        !clip->noteInferenceErrors().value(inputs.at(2).noteId).detail.isEmpty() &&
            !clip->noteInferenceErrors().value(inputs.at(5).noteId).detail.isEmpty(),
        10000);
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    const auto before = TestSupport::projectSnapshot(*context->m_appModel);
    const auto version = runtime().documentVersion();
    const auto *undo = HistoryManager::instance()->nextUndoEntry();
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
    for (const auto index : {1, 2, 4, 5, 6}) {
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
    QVERIFY(phonemes.result.at(3).success);
    QCOMPARE(phonemes.result.at(3).phonemeNames, phonemes.result.first().phonemeNames);
    for (const auto index : {2, 5}) {
        QVERIFY(!phonemes.result.at(index).success);
        QVERIFY(phonemes.result.at(index).phonemeNames.isEmpty());
    }
    for (const auto index : {4, 6}) {
        QVERIFY(phonemes.result.at(index).success);
        QVERIFY(phonemes.result.at(index).phonemeNames.isEmpty());
    }

    Automation::InferenceMutationRequest writeback;
    writeback.kind = Automation::InferenceMutationKind::ApplyPhonemeNames;
    writeback.clipId = Automation::ClipId(clip->id());
    for (qsizetype index = 0; index < inputs.size(); ++index) {
        const auto &result = phonemes.result.at(index);
        writeback.phonemeNames.append({
            .noteId = Automation::NoteId(inputs.at(index).noteId),
            .phonemeNames = result.phonemeNames,
            .success = result.success,
            .errorMessage = result.errorMessage,
        });
    }
    auto previewContext = commandContext();
    previewContext.validateOnly = true;
    const auto preview = runtime().inference().applyMutation(previewContext, writeback);
    QVERIFY2(preview, qPrintable(preview ? QString{} : preview.getError().message));
    QVERIFY(preview.get().mutation.validatedOnly);
    QCOMPARE(runtime().documentVersion(), version);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), before);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), undo);
    const auto applied = runtime().inference().applyMutation(commandContext(), writeback);
    QVERIFY2(applied, qPrintable(applied ? QString{} : applied.getError().message));
    for (qsizetype index = 0; index < inputs.size(); ++index) {
        const auto *currentNote = clip->findNoteById(inputs.at(index).noteId);
        QVERIFY(currentNote);
        QCOMPARE(currentNote->phonemeNameSeq().original, phonemes.result.at(index).phonemeNames);
        if (index == 4 || index == 6)
            QVERIFY(currentNote->phonemeOffsetSeq().original.isEmpty());
    }
    for (const auto index : {2, 5})
        QCOMPARE(clip->noteInferenceErrors().value(inputs.at(index).noteId).detail,
                 phonemes.result.at(index).errorMessage);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), undo);

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
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), before);
    QCOMPARE(runtime().documentVersion(), version);
}

void ApplicationWorkflowTests::builtInG2pConvertsDictionaryAndUnlistedWords() {
    prepareVoicebankTarget();
    if (QTest::currentTestFailed())
        return;
    const auto &service = SynthrtEngine::instance().languageService();
    QVERIFY(service.modelsReady());
    const auto before = TestSupport::projectSnapshot(*context->m_appModel);
    const auto version = runtime().documentVersion();
    const auto *undo = HistoryManager::instance()->nextUndoEntry();
    const std::vector<srt::g2p::G2pInput> inputs{
        {"hello",      "g2p-eng-official"},
        {"codexarium", "g2p-eng-official"},
    };
    const auto converted = service.convertLyric(inputs);
    QCOMPARE(converted.size(), inputs.size());
    for (size_t index = 0; index < converted.size(); ++index) {
        const auto &result = converted[index];
        QCOMPARE(result.lyric, inputs[index].lyric);
        QCOMPARE(result.errorType, srt::g2p::NoError);
        QCOMPARE(result.mode, std::string(srt::g2p::kG2pModeConvert));
        QVERIFY(!result.pronunciation.empty());
        QVERIFY(result.pronunciation != inputs[index].lyric);
        QVERIFY(!result.candidates.empty());
    }
    QVERIFY(converted.front().pronunciation != converted.back().pronunciation);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), before);
    QCOMPARE(runtime().documentVersion(), version);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), undo);
}

void ApplicationWorkflowTests::editingParametersRestartsOnlyDependentInference_data() {
    QTest::addColumn<ParamInfo::Name>("name");
    QTest::addColumn<int>("value");
    QTest::addColumn<bool>("cancelBeforeEdit");
    QTest::newRow("expressiveness-recomputes-pitch-and-variance")
        << ParamInfo::Expressiveness << 500 << false;
    QTest::newRow("pitch-recomputes-variance") << ParamInfo::Pitch << 6300 << false;
    QTest::newRow("gender-preserves-pitch-and-variance") << ParamInfo::Gender << 500 << false;
    QTest::newRow("cancel-then-expressiveness") << ParamInfo::Expressiveness << 500 << true;
    QTest::newRow("cancel-then-pitch") << ParamInfo::Pitch << 6300 << true;
    QTest::newRow("cancel-then-gender") << ParamInfo::Gender << 500 << true;
}

void ApplicationWorkflowTests::voiceExportPreparationInterruptionsAllowRetry_data() {
    QTest::addColumn<QString>("interruption");
    QTest::newRow("cancel-export") << QStringLiteral("cancel");
    QTest::newRow("replace-document") << QStringLiteral("replace-document");
    QTest::newRow("remove-source-track") << QStringLiteral("remove-source-track");
}

void ApplicationWorkflowTests::voiceExportPreparationInterruptionsAllowRetry() {
    QFETCH(QString, interruption);
    const bool replaceDocument = interruption == QStringLiteral("replace-document");
    const bool removeSourceTrack = interruption == QStringLiteral("remove-source-track");
    const bool checkOtherTrack = interruption == QStringLiteral("cancel");
    QSemaphore workerEntered;
    QSemaphore releaseWorker;
    std::atomic_bool paused = false;
    QObject observations;
    QPointer<InferPiece> blockedBackground;
    QPointer<InferPiece> queuedBackground;
    QString queuedBackgroundState;
    bool gateAttached = false;
    QTemporaryDir materials;
    QVERIFY(materials.isValid());
    const auto previousCache = appOptions->inference()->cacheDirectory;
    appOptions->inference()->cacheDirectory = materials.filePath(QStringLiteral("cache"));
    const auto restore = qScopeGuard([&] {
        releaseWorker.release();
        QVERIFY(runtime().documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 15000);
        appOptions->inference()->cacheDirectory = previousCache;
    });
    prepareVoicebankTarget();
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(piece->state.get(), QStringLiteral("Acoustic.Awaiting"));
    if (checkOtherTrack) {
        auto *backgroundTrack = context->m_appModel->tracks().last();
        const auto backgroundId = Automation::TrackId(backgroundTrack->id());
        QVERIFY(backgroundId != trackId);
        const auto duplicateBackground = [&](int targetStart) -> SingingClip * {
            const auto copied = runtime().project().duplicateClips(
                commandContext(), {Automation::ClipId(clip->id())},
                {.targetTrackId = backgroundId, .targetStart = targetStart});
            if (!copied)
                return nullptr;
            for (const auto &created : copied.get().createdObjects) {
                if (created.object.kind == Automation::ObjectKind::Clip)
                    return qobject_cast<SingingClip *>(context->m_appModel->findClipById(
                        Automation::ClipId(created.object.value).value()));
            }
            return nullptr;
        };
        const QPointer<SingingClip> firstBackground(duplicateBackground(clip->start()));
        QVERIFY(firstBackground && firstBackground != clip);
        const QPointer<SingingClip> secondBackground(
            duplicateBackground(clip->start() + clip->length() + 480));
        QVERIFY(secondBackground && secondBackground != firstBackground);
        QTRY_VERIFY_WITH_TIMEOUT(
            inferenceSettled(firstBackground) && inferenceSettled(secondBackground), 15000);
        const QPointer<InferPiece> firstPiece(firstBackground->pieces().first());
        const QPointer<InferPiece> secondPiece(secondBackground->pieces().first());
        connect(taskManager, &TaskManager::taskChanged, &observations,
                [&, firstPiece, secondPiece](TaskManager::TaskChangeType change, Task *task,
                                             qsizetype) {
                    auto *inference = dynamic_cast<IInferTask *>(task);
                    if (gateAttached || change != TaskManager::Added || !inference || !firstPiece ||
                        !secondPiece ||
                        inference->inferenceContext().taskType != QStringLiteral("acoustic") ||
                        (inference->pieceId() != firstPiece->id() &&
                         inference->pieceId() != secondPiece->id()))
                        return;
                    gateAttached = true;
                    const bool first = inference->pieceId() == firstPiece->id();
                    blockedBackground = first ? firstPiece : secondPiece;
                    queuedBackground = first ? secondPiece : firstPiece;
                    pauseAtFirstTaskStatus(task, &observations, paused, workerEntered,
                                           releaseWorker);
                });
        inferController->startPendingAcousticInference({backgroundTrack});
        QTRY_VERIFY_WITH_TIMEOUT(workerEntered.available() == 1, 10000);
        QVERIFY(blockedBackground && queuedBackground);
        QTRY_VERIFY_WITH_TIMEOUT(queuedBackground->acousticInferStatus.get() == Running, 10000);
        queuedBackgroundState = queuedBackground->state.get();
        QVERIFY(queuedBackgroundState != QStringLiteral("Acoustic.Awaiting"));
    }
    const auto previousDocument = runtime().documentVersion().documentId;
    const auto *previousUndo = HistoryManager::instance()->nextUndoEntry();
    const auto previousContent = TestSupport::projectSnapshot(*context->m_appModel);
    auto interruptedVersion = runtime().documentVersion();
    auto interruptedContent = previousContent;
    auto replacement = Automation::DocumentAutomationFacade::newDocumentDraft(false);
    replacement.timeline = context->m_appModel->timeline();
    for (const auto *track : context->m_appModel->tracks())
        replacement.tracks.append(Automation::trackDraftDto(*track));
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
        if (replaceDocument) {
            QVERIFY(runtime().documents().commitNewDocument(commandContext(), replacement));
        } else if (removeSourceTrack) {
            QVERIFY(runtime().project().removeTracks(commandContext(), {trackId}));
            interruptedVersion = runtime().documentVersion();
            interruptedContent = TestSupport::projectSnapshot(*context->m_appModel);
        } else {
            QVERIFY(runtime().tasks().cancelTask(commandContext(), exportId));
        }
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
    if (replaceDocument) {
        QTRY_VERIFY_WITH_TIMEOUT(runtime().documentVersion().documentId != previousDocument, 15000);
        QVERIFY(!runtime().tasks().getTask(previousDocument, exportId));
        QVERIFY(!HistoryManager::instance()->canUndo());
    } else if (removeSourceTrack) {
        QTRY_COMPARE_WITH_TIMEOUT(terminalState(), Automation::AutomationTaskState::Failed, 15000);
        const auto failed = runtime().tasks().getTask(previousDocument, exportId);
        QVERIFY(failed && failed.get().error);
        QCOMPARE(failed.get().error->code, Automation::AutomationErrorCode::IoError);
        QCOMPARE(failed.get().error->taskId, exportId);
        QVERIFY(!failed.get().error->message.isEmpty());
        QCOMPARE(runtime().documentVersion(), interruptedVersion);
        QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), interruptedContent);
        QVERIFY(runtime().history().undo(commandContext()));
        QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), previousContent);
        QCOMPARE(HistoryManager::instance()->nextUndoEntry(), previousUndo);
        QTRY_VERIFY_WITH_TIMEOUT(inferenceSettled(clip), 15000);
    } else {
        QTRY_COMPARE_WITH_TIMEOUT(terminalState(), Automation::AutomationTaskState::Canceled,
                                  15000);
        QCOMPARE(runtime().documentVersion().documentId, previousDocument);
        QCOMPARE(HistoryManager::instance()->nextUndoEntry(), previousUndo);
    }
    QVERIFY(canceledDuringInference);
    QVERIFY(!renderedBeforeCancellation);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), original);
    file.close();
    if (checkOtherTrack) {
        QVERIFY(blockedBackground && queuedBackground);
        QTRY_COMPARE_WITH_TIMEOUT(piece->state.get(), QStringLiteral("Acoustic.Awaiting"), 10000);
        QCOMPARE(queuedBackground->state.get(), queuedBackgroundState);
        QCOMPARE(queuedBackground->acousticInferStatus.get(), Running);
        QVERIFY(blockedBackground->audioPath.isEmpty());
        releaseWorker.release();
    }
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 15000);
    if (checkOtherTrack) {
        // Tasks leave the queue before the state machine applies their results.
        QTRY_VERIFY_WITH_TIMEOUT(blockedBackground && queuedBackground &&
                                     blockedBackground->state.get() == QStringLiteral("Ready") &&
                                     queuedBackground->state.get() == QStringLiteral("Ready"),
                                 15000);
        QVERIFY(QFileInfo::exists(blockedBackground->audioPath));
        QVERIFY(QFileInfo::exists(queuedBackground->audioPath));
    }
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
    QFETCH(bool, cancelBeforeEdit);
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
    if (cancelBeforeEdit) {
        const auto services = Automation::createPublicAutomationHostServices(
            runtime(), context->m_appModel, &SynthrtEngine::instance());
        const auto accepted = services.startInference({
            .command = commandContext(),
            .scope = {{"kind", "clip"}, {"clip_ids", QJsonArray{clip->id()}}},
            .stages = {QStringLiteral("acoustic")},
        });
        QVERIFY2(accepted, qPrintable(accepted ? QString() : accepted.getError().message));
        QVERIFY(!accepted.get().taskId.isNull());
        QVERIFY(runtime().tasks().cancelTask(commandContext(), accepted.get().taskId));
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
        const auto canceled =
            runtime().tasks().getTask(accepted.get().document.documentId, accepted.get().taskId);
        QVERIFY(canceled);
        QCOMPARE(canceled.get().state, Automation::AutomationTaskState::Canceled);
        QVERIFY(target);
    }
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

void ApplicationWorkflowTests::queuedCacheProbeCannotRestoreAudioAfterAnEdit() {
    prepareVoicebankTarget();
    if (QTest::currentTestFailed())
        return;
    const QPointer<InferPiece> target(piece);
    QSemaphore entered;
    QSemaphore release;
    const auto cleanup = qScopeGuard([&] {
        release.release();
        QVERIFY(runtime().documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 15000);
    });
    inferController->startPendingAcousticInference();
    QTRY_VERIFY_WITH_TIMEOUT(target && target->state == QStringLiteral("Ready") &&
                                 taskManager->tasks().isEmpty(),
                             15000);
    const auto originalAudio = target->audioPath;
    QVERIFY(QFile::exists(originalAudio));
    const auto originalInput = target->getInputCurve(ParamInfo::Gender)->mid(720);
    QVERIFY(!originalInput.isEmpty());
    const auto value = originalInput.first() == 500 ? 750 : 500;
    const auto editGender = [&] {
        return runtime().parameters().drawParameter(
            commandContext(), Automation::ClipId(clip->id()), ParamInfo::Gender, Param::Edited, 480,
            5, QList<int>(97, value), false);
    };
    QVERIFY(editGender());
    QTRY_VERIFY_WITH_TIMEOUT(inferenceSettled(clip), 15000);
    QPointer<InferAcousticCacheProbeTask> probe;
    bool captured = false;
    std::atomic_bool paused = false;
    QObject observations;
    connect(taskManager, &TaskManager::taskChanged, &observations,
            [&](TaskManager::TaskChangeType change, Task *task, qsizetype) {
                auto *candidate = qobject_cast<InferAcousticCacheProbeTask *>(task);
                if (change != TaskManager::Added || !candidate || captured ||
                    candidate->pieceId() != target->id())
                    return;
                captured = true;
                probe = candidate;
                connect(
                    candidate, &Task::statusUpdated, &observations,
                    [&](const TaskStatus &) {
                        if (!paused.exchange(true)) {
                            entered.release();
                            release.acquire();
                        }
                    },
                    Qt::DirectConnection);
            });
    QVERIFY(runtime().history().undo(commandContext()));
    QTRY_COMPARE_WITH_TIMEOUT(entered.available(), 1, 15000);
    QVERIFY(probe);
    release.release();
    // Keep the completion queued until the edit has changed the actual model inputs.
    QVERIFY(QThreadPool::globalInstance()->waitForDone(10000));
    QVERIFY(probe->success());
    QVERIFY(probe->cacheHit());
    const auto changed = editGender();
    QVERIFY(changed && changed.get().changed);
    const auto afterEdit = runtime().documentVersion();
    const auto *undo = HistoryManager::instance()->nextUndoEntry();
    QTRY_VERIFY_WITH_TIMEOUT(inferenceSettled(clip), 15000);
    QVERIFY(target);
    QCOMPARE(target->getInputCurve(ParamInfo::Gender)->mid(720).first(), value);
    QVERIFY(target->audioPath != originalAudio);
    inferController->startPendingAcousticInference();
    QTRY_VERIFY_WITH_TIMEOUT(target && target->state == QStringLiteral("Ready") &&
                                 taskManager->tasks().isEmpty(),
                             15000);
    QVERIFY(QFile::exists(target->audioPath));
    QVERIFY(target->audioPath != originalAudio);
    QCOMPARE(runtime().documentVersion(), afterEdit);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), undo);
}

void ApplicationWorkflowTests::changingSamplingSettingsRestartsRunningInference_data() {
    QTest::addColumn<bool>("pauseDuration");
    QTest::newRow("duration-worker") << true;
    QTest::newRow("pitch-worker") << false;
}

void ApplicationWorkflowTests::changingSamplingSettingsRestartsRunningInference() {
    QFETCH(bool, pauseDuration);
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const auto previousCache = appOptions->inference()->cacheDirectory;
    const auto previousSteps = appOptions->inference()->samplingSteps;
    const auto previousKernel = appOptions->inference()->pitch_smooth_kernel_size;
    const auto changedSteps = previousSteps == 20 ? 21 : 20;
    const auto changedKernel = previousKernel == 3 ? 5 : 3;
    appOptions->inference()->cacheDirectory = cache.path();
    QSemaphore entered;
    QSemaphore release;
    QSemaphore completed;
    std::atomic_bool paused = false;
    std::atomic_bool originalCanceled = false;
    QObject observations;
    const auto cleanup = qScopeGuard([&] {
        disconnect(taskManager, nullptr, &observations, nullptr);
        release.release();
        if (QTest::currentTestFailed())
            cache.setAutoRemove(false);
        QVERIFY(runtime().documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
        QThreadPool::globalInstance()->waitForDone();
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 15000);
        appOptions->inference()->cacheDirectory = previousCache;
        const auto restored = runtime().settings().updateRender(
            {.source = Automation::InvocationSource::Test},
            {.samplingSteps = previousSteps, .pitchSmoothKernelSize = previousKernel});
        QVERIFY(restored);
    });
    prepareVoicebankTarget();
    if (QTest::currentTestFailed())
        return;
    const QPointer<InferPiece> target(piece);
    QPointer<IInferTask> originalTask;
    bool captured = false;
    QList<std::pair<int, int>> submittedSettings;
    QStringList submittedSignatures;
    QList<int> submittedTaskIds;
    connect(taskManager, &TaskManager::taskChanged, &observations,
            [&](TaskManager::TaskChangeType change, Task *task, qsizetype) {
                if (change != TaskManager::Added || !target)
                    return;
                auto *pitchTask = qobject_cast<InferPitchTask *>(task);
                if (pitchTask && pitchTask->pieceId() == target->id()) {
                    const auto input = pitchTask->input();
                    submittedSettings.emplaceBack(input.steps, input.pitchSmoothKernelSize);
                    submittedSignatures.append(pitchTask->inferenceContext().inputSignature);
                }
                auto *candidate =
                    pauseDuration
                        ? static_cast<IInferTask *>(qobject_cast<InferDurationTask *>(task))
                        : static_cast<IInferTask *>(pitchTask);
                if (!candidate || candidate->pieceId() != target->id())
                    return;
                submittedTaskIds.append(candidate->id());
                if (captured)
                    return;
                captured = true;
                originalTask = candidate;
                connect(
                    candidate, &Task::statusUpdated, &observations,
                    [&](const TaskStatus &) {
                        if (!paused.exchange(true)) {
                            entered.release();
                            release.acquire();
                        }
                    },
                    Qt::DirectConnection);
                connect(
                    candidate, &Task::finished, &observations,
                    [&, candidate] {
                        originalCanceled.store(candidate->terminated());
                        completed.release();
                    },
                    Qt::DirectConnection);
            });
    inferController->restartPieceInference(*target);
    QTRY_COMPARE_WITH_TIMEOUT(entered.available(), 1, 10000);
    QVERIFY(originalTask && target);
    const auto originalSignature = originalTask->inferenceContext().inputSignature;
    const auto *undo = HistoryManager::instance()->nextUndoEntry();
    const auto lyric = note->lyric();
    const auto key = note->keyIndex();
    const auto length = note->length();
    const auto beforeSettings = runtime().settings().getSettings();
    QVERIFY(beforeSettings);
    Automation::AutomationAccessPolicy access(AutomationWire::ControlLevel::L3);
    Automation::AutomationFileGuard fileGuard;
    Automation::AdmissionController admission;
    Automation::PublicAutomationRegistry registry(runtime(), access, fileGuard, admission);
    const auto changed =
        registry.invoke(QStringLiteral("settings.render.update"),
                        {
                            {QStringLiteral("sampling_steps"),           changedSteps },
                            {QStringLiteral("pitch_smooth_kernel_size"), changedKernel}
    },
                        {.clientId = QStringLiteral("render-settings-client"),
                         .source = Automation::InvocationSource::PublicJsonRpc});
    QVERIFY2(changed, qPrintable(changed ? QString() : changed.getError().message));
    QCOMPARE(appOptions->inference()->samplingSteps, changedSteps);
    QCOMPARE(appOptions->inference()->pitch_smooth_kernel_size, changedKernel);
    auto expectedSettings = beforeSettings.get();
    expectedSettings.inference.samplingSteps = changedSteps;
    expectedSettings.inference.pitchSmoothKernelSize = changedKernel;
    const auto afterSettings = runtime().settings().getSettings();
    QVERIFY(afterSettings);
    QCOMPARE(afterSettings.get(), expectedSettings);
    AppOptions reopened;
    QCOMPARE(reopened.inference()->samplingSteps, changedSteps);
    QCOMPARE(reopened.inference()->pitch_smooth_kernel_size, changedKernel);
    release.release();
    QTRY_VERIFY_WITH_TIMEOUT(completed.available() > 0, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(
        submittedSettings.contains(std::make_pair(changedSteps, changedKernel)) &&
            inferenceSettled(clip),
        15000);
    QVERIFY(originalCanceled.load());
    QVERIFY(submittedTaskIds.size() > 1);
    QVERIFY(submittedTaskIds.last() != submittedTaskIds.first());
    if (!pauseDuration) {
        QVERIFY(submittedSignatures.size() > 1);
        QVERIFY(submittedSignatures.last() != originalSignature);
    }
    QVERIFY(target && !target->originalPitch.isEmpty());
    QCOMPARE(note->id(), target->notes.first()->id());
    QCOMPARE(note->lyric(), lyric);
    QCOMPARE(note->keyIndex(), key);
    QCOMPARE(note->length(), length);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), undo);
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

void ApplicationWorkflowTests::playbackRecoversAfterPublicInferenceCancellation_data() {
    QTest::addColumn<bool>("autoStartInference");
    QTest::addColumn<bool>("seekIntoWindow");
    QTest::addColumn<bool>("cancelWhilePlaying");
    QTest::newRow("playback-window") << false << false << false;
    QTest::newRow("automatic-inference") << true << false << false;
    QTest::newRow("seek-into-recovery-window") << false << true << false;
    QTest::newRow("cancel-during-playback") << false << false << true;
}

void ApplicationWorkflowTests::playbackRecoversAfterPublicInferenceCancellation() {
    QFETCH(bool, autoStartInference);
    QFETCH(bool, seekIntoWindow);
    QFETCH(bool, cancelWhilePlaying);
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    auto *audio = AudioContext::instance();
    const auto previousCache = appOptions->inference()->cacheDirectory;
    const auto previousReadAhead = audio->bufferingReadAheadSize();
    const auto previousAutoStart = appOptions->inference()->autoStartInfer;
    const auto cleanup = qScopeGuard([&] {
        runtime().playback().stop(commandContext());
        QVERIFY(runtime().documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 15000);
        audio->preMixer()->close();
        audio->setBufferingReadAheadSize(previousReadAhead);
        playbackController->setPlaybackStartGuard([] { return false; });
        appOptions->inference()->cacheDirectory = previousCache;
        appOptions->inference()->autoStartInfer = previousAutoStart;
        appOptions->notifyOptionsChanged(AppOptionsGlobal::Inference);
    });
    appOptions->inference()->cacheDirectory = cache.path();
    audio->setBufferingReadAheadSize(0);
    prepareVoicebankTarget();
    if (QTest::currentTestFailed())
        return;
    const QPointer<InferPiece> target(piece);
    inferController->startPendingAcousticInference();
    QTRY_VERIFY_WITH_TIMEOUT(target && target->state == QStringLiteral("Ready") &&
                                 QFile::exists(target->audioPath) && taskManager->tasks().isEmpty(),
                             15000);
    const auto beforePlayback = runtime().documentVersion();
    const auto beforeModel = TestSupport::projectSnapshot(*context->m_appModel);
    const auto *undoBefore = HistoryManager::instance()->nextUndoEntry();
    QVERIFY(audio->preMixer()->open(256, 48000));
    playbackController->setPlaybackStartGuard([] { return true; });
    const auto targetPosition = clip->start() + note->localStart();
    if (cancelWhilePlaying) {
        QVERIFY(runtime().playback().setPosition(commandContext(), targetPosition));
        QVERIFY(runtime().playback().play(commandContext()));
    }
    const auto services = Automation::createPublicAutomationHostServices(
        runtime(), context->m_appModel, &SynthrtEngine::instance());
    const auto accepted = services.startInference({
        .command = commandContext(),
        .scope = {{"kind", "clip"}, {"clip_ids", QJsonArray{clip->id()}}},
        .stages = {QStringLiteral("acoustic")},
    });
    QVERIFY2(accepted, qPrintable(accepted ? QString{} : accepted.getError().message));
    QVERIFY(!accepted.get().taskId.isNull());
    QVERIFY(target && target->audioPath.isEmpty());
    QVERIFY(runtime().tasks().cancelTask(commandContext(), accepted.get().taskId));
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    const auto canceled =
        runtime().tasks().getTask(accepted.get().document.documentId, accepted.get().taskId);
    QVERIFY(canceled);
    QCOMPARE(canceled.get().state, Automation::AutomationTaskState::Canceled);
    if (!cancelWhilePlaying) {
        QVERIFY(target && target->audioPath.isEmpty());
        appOptions->inference()->autoStartInfer = autoStartInference;
        appOptions->notifyOptionsChanged(AppOptionsGlobal::Inference);
        QVERIFY(runtime().playback().setPosition(
            commandContext(),
            seekIntoWindow ? targetPosition + note->length() + 4800 : targetPosition));
        QVERIFY(runtime().playback().play(commandContext()));
        if (seekIntoWindow) {
            QCoreApplication::processEvents();
            QVERIFY(target && target->audioPath.isEmpty());
            QVERIFY(runtime().playback().setPosition(commandContext(), targetPosition));
        }
    }
    QTRY_VERIFY_WITH_TIMEOUT(target && target->state == QStringLiteral("Ready") &&
                                 QFile::exists(target->audioPath) && taskManager->tasks().isEmpty(),
                             15000);
    QCOMPARE(runtime().documentVersion().documentId, beforePlayback.documentId);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), undoBefore);
    const auto canceledAfterPlayback =
        runtime().tasks().getTask(accepted.get().document.documentId, accepted.get().taskId);
    QVERIFY(canceledAfterPlayback);
    QCOMPARE(canceledAfterPlayback.get().state, Automation::AutomationTaskState::Canceled);
    QCOMPARE(playbackController->playbackStatus(), PlaybackGlobal::Playing);
    QTRY_COMPARE(audio->transport()->bufferingCounter(), 0);
    talcs::AudioBuffer buffer(2, 256);
    const auto position = audio->transport()->position();
    QCOMPARE(audio->preMixer()->read(&buffer), qint64{256});
    QVERIFY(audio->transport()->position() > position);
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
    QTest::newRow("pronunciation-replaced-by-lyric-edit") << false << QStringLiteral("edit-lyric");
    QTest::newRow("phoneme-replaced-by-lyric-edit") << true << QStringLiteral("edit-lyric");
    QTest::newRow("pronunciation-document-replaced") << false << QStringLiteral("replace-document");
    QTest::newRow("phoneme-clip-removed") << true << QStringLiteral("remove-clip");
    QTest::newRow("pronunciation-language-service-failed")
        << false << QStringLiteral("module-error");
    QTest::newRow("phoneme-language-service-failed") << true << QStringLiteral("module-error");
    QTest::newRow("pronunciation-language-service-failed-before-delivery")
        << false << QStringLiteral("module-error-before-delivery");
    QTest::newRow("phoneme-language-service-failed-before-delivery")
        << true << QStringLiteral("module-error-before-delivery");
}

void ApplicationWorkflowTests::clipInferenceResultsRespectEditSession() {
    QFETCH(bool, phonemeStage);
    QFETCH(QString, completion);
    const bool failsBeforeDelivery = completion == QStringLiteral("module-error-before-delivery");
    const bool failsLanguage = failsBeforeDelivery || completion == QStringLiteral("module-error");
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

    const auto settingsBeforeTask = runtime().settings().getSettings();
    QVERIFY(settingsBeforeTask);
    const auto previousLanguage = settingsBeforeTask.get().g2pLanguage;
    const auto previousStatus = appStatus->languageModuleStatus.get();
    bool failureBeforeResult = false;
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
    const auto restoreLanguage = qScopeGuard([&] {
        if (failsLanguage) {
            QVERIFY(runtime().settings().updateG2pLanguage({}, previousLanguage));
            appStatus->languageModuleStatus = previousStatus;
        }
    });
    connect(
        taskManager, &TaskManager::taskChanged, &observations,
        [&](TaskManager::TaskChangeType change, Task *task, qsizetype) {
            auto *pronunciation = qobject_cast<GetPronunciationTask *>(task);
            auto *phonemes = qobject_cast<GetPhonemeNameTask *>(task);
            const bool selected = phonemeStage
                                      ? phonemes && phonemes->clipId() == targetClipId
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
                if (failsBeforeDelivery) {
                    // Queue before the worker starts so failure precedes its result delivery.
                    QMetaObject::invokeMethod(
                        &observations,
                        [&] {
                            if (!QThreadPool::globalInstance()->waitForDone(10000)) {
                                QTest::qFail("The language worker did not complete", __FILE__,
                                             __LINE__);
                                return;
                            }
                            QTest::ignoreMessage(
                                QtCriticalMsg,
                                "Failed to start the language module; tasks have been canceled.");
                            appStatus->languageModuleStatus = AppStatus::ModuleStatus::Error;
                            failureBeforeResult = true;
                        },
                        Qt::QueuedConnection);
                }
            } else if (change == TaskManager::Removed && task->id() == observedTaskId) {
                // Read completed output before queue cleanup deletes the task.
                resultReceived = true;
                if (phonemeStage) {
                    validResult = (failsBeforeDelivery || !task->terminated()) &&
                                  phonemes->success() && phonemes->result.size() == 1 &&
                                  phonemes->result.first().success;
                    if (validResult)
                        expectedPhonemes = phonemes->result.first().phonemeNames;
                } else {
                    validResult = (failsBeforeDelivery || !task->terminated()) &&
                                  pronunciation->result.size() == 1;
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

    if (failsLanguage) {
        const auto beforeFailure = TestSupport::projectSnapshot(*context->m_appModel);
        if (failsBeforeDelivery) {
            QVERIFY(failureBeforeResult);
        } else {
            QTest::ignoreMessage(QtCriticalMsg,
                                 "Failed to start the language module; tasks have been canceled.");
            appStatus->languageModuleStatus = AppStatus::ModuleStatus::Error;
        }
        QCOMPARE(runtime().documentVersion(), stageBase);
        QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeFailure);
        QVERIFY(editSessionManager->hasActiveTransaction());
        editSessionManager->endTransaction(editSessionId, EditSessionEndReason::Discard);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        QCoreApplication::processEvents();
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
        QVERIFY(targetNote->phonemes().nameSeq.original.isEmpty());
        if (!phonemeStage)
            QVERIFY(targetNote->pronunciation().original.isEmpty());
        QCOMPARE(runtime().documentVersion(), stageBase);
        QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeFailure);
        QCOMPARE(HistoryManager::instance()->nextUndoEntry(), stageUndo);
        QVERIFY(!editSessionManager->hasActiveTransaction());

        QVERIFY(runtime().settings().updateG2pLanguage({}, previousLanguage));
        appStatus->languageModuleStatus = AppStatus::ModuleStatus::Ready;
        QTRY_VERIFY_WITH_TIMEOUT(inferenceSettled(targetClip), 15000);
        QVERIFY(!targetNote->phonemes().nameSeq.original.isEmpty());
        if (phonemeStage)
            QCOMPARE(targetNote->phonemes().nameSeq.original, expectedPhonemes);
        else
            QCOMPARE(targetNote->pronunciation().original, expectedPronunciation);
        QCOMPARE(targetNote->lyric(), draftNote.lyric);
        QCOMPARE(targetClip->findNoteById(targetNoteId), targetNote.data());
        QCOMPARE(HistoryManager::instance()->nextUndoEntry(), stageUndo);
        return;
    }

    if (completion == QStringLiteral("edit-lyric")) {
        const auto originalPronunciation =
            phonemeStage ? targetNote->pronunciation().original : expectedPronunciation;
        const auto originalStart = targetNote->localStart();
        const auto originalLength = targetNote->length();
        auto edit = commandContext();
        edit.source = Automation::InvocationSource::TrustedGui;
        const auto changed =
            runtime().notes().setLyric(edit, Automation::ClipId(targetClipId),
                                       Automation::NoteId(targetNoteId), QStringLiteral("SP"));
        QVERIFY2(changed, qPrintable(changed ? QString{} : changed.getError().message));
        QCOMPARE(runtime().documentVersion().revision, stageBase.revision + 1);
        const auto *editedUndo = HistoryManager::instance()->nextUndoEntry();
        QVERIFY(editedUndo != stageUndo);
        editSessionManager->endTransaction(editSessionId, EditSessionEndReason::Commit);
        QTRY_VERIFY_WITH_TIMEOUT(targetNote->pronunciation().original == QStringLiteral("SP") &&
                                     targetNote->phonemes().nameSeq.original.size() == 1 &&
                                     targetNote->phonemes().nameSeq.original.first().name ==
                                         QStringLiteral("SP") &&
                                     taskManager->tasks().isEmpty(),
                                 15000);
        QCOMPARE(targetNote->lyric(), QStringLiteral("SP"));
        QCOMPARE(targetNote->localStart(), originalStart);
        QCOMPARE(targetNote->length(), originalLength);
        QCOMPARE(targetClip->findNoteById(targetNoteId), targetNote.data());
        QCOMPARE(runtime().documentVersion().documentId, stageBase.documentId);
        QCOMPARE(HistoryManager::instance()->nextUndoEntry(), editedUndo);
        auto undo = commandContext();
        undo.source = Automation::InvocationSource::TrustedGui;
        QVERIFY(runtime().history().undo(undo));
        QTRY_VERIFY_WITH_TIMEOUT(targetNote->pronunciation().original == originalPronunciation &&
                                    inferenceSettled(targetClip),
                                15000);
        QCOMPARE(targetNote->lyric(), draftNote.lyric);
        QCOMPARE(targetNote->pronunciation().original, originalPronunciation);
        QVERIFY(!targetNote->phonemes().nameSeq.original.isEmpty());
        if (phonemeStage)
            QCOMPARE(targetNote->phonemes().nameSeq.original, expectedPhonemes);
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
