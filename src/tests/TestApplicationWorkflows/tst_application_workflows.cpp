#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Bootstrap/AppDataPaths.h"
#include "Bootstrap/AppEnvironment.h"
#include "Model/AppOptions/AppOptions.h"
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
#include <lite/PackageManager/PackageManager.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>
#include "../TestSupport/ProcessFixture.h"

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

class ApplicationWorkflowTests final : public QObject {
    Q_OBJECT

private slots:

    void initTestCase() {
        QVERIFY(dataRoot.isValid());
        previousDataRoot = qgetenv("DSEL_TEST_DATA_ROOT");
        qputenv("DSEL_TEST_DATA_ROOT", dataRoot.path().toUtf8());
        dataRootInstalled = true;
        QCOMPARE(AppDataPaths::testRoot(), QDir::cleanPath(dataRoot.path()));
        AppEnvironment::postInit(AppHostMode::Headless);

        auto options = std::make_unique<AppOptions>();
        QVERIFY(QDir::cleanPath(options->configPath()).startsWith(dataRoot.path() + '/'));
        options->general()->packageSearchPaths.clear();
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
    }

    void init() {
        auto document = Automation::DocumentAutomationFacade::newDocumentDraft(false);
        document.tracks = {trackDraft(QStringLiteral("Target")),
                           trackDraft(QStringLiteral("Other track"))};
        QVERIFY(runtime().documents().commitNewDocument(commandContext(), document));
        QCOMPARE(context->m_appModel->tracks().size(), 2);
        const auto *track = context->m_appModel->tracks().first();
        trackId = Automation::TrackId(track->id());
        clip = dynamic_cast<SingingClip *>(*track->clips().begin());
        otherClip =
            dynamic_cast<SingingClip *>(*context->m_appModel->tracks().last()->clips().begin());
        QVERIFY(clip);
        QVERIFY(otherClip);
        clip->reSegment(context->m_appModel->timeline());
        QCOMPARE(clip->pieces().size(), 1);
        piece = clip->pieces().first();
        QCOMPARE(piece->notes.size(), 1);
        note = piece->notes.first();
        QVERIFY(!editSessionManager->hasActiveTransaction());
    }

    void changedTargetInputDropsResult_data() {
        addInferenceStages();
    }

    void failedInferenceInitializationReleasesPackageWaiters() {
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

    void changedTargetInputDropsResult() {
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

    void removedTargetDropsResult_data() {
        QTest::addColumn<bool>("replaceDocument");
        QTest::newRow("replace-document") << true;
        QTest::newRow("remove-clip") << false;
    }

    void removedTargetDropsResult() {
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

    void unchangedInputSurvivesRevisionDrift_data() {
        addInferenceStages();
    }

    void unchangedInputSurvivesRevisionDrift() {
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

    void editSessionControlsResultDeferral_data() {
        using Domain = AppStatus::EditObjectType;
        QTest::addColumn<QString>("stage");
        QTest::addColumn<int>("domain");
        QTest::addColumn<QString>("scope");
        QTest::addColumn<bool>("deferred");
        QTest::newRow("same-note")
            << QStringLiteral("acoustic") << int(Domain::Note) << QStringLiteral("note") << true;
        QTest::newRow("other-clip") << QStringLiteral("acoustic") << int(Domain::Note)
                                    << QStringLiteral("other-clip") << false;
        QTest::newRow("piece-phonemes") << QStringLiteral("duration") << int(Domain::Phoneme)
                                        << QStringLiteral("piece") << true;
        QTest::newRow("whole-clip")
            << QStringLiteral("acoustic") << int(Domain::Clip) << QStringLiteral("clip") << true;
        QTest::newRow("pitch-affects-acoustic")
            << QStringLiteral("acoustic") << int(Domain::Param) << QStringLiteral("pitch") << true;
        QTest::newRow("pitch-does-not-affect-duration")
            << QStringLiteral("duration") << int(Domain::Param) << QStringLiteral("pitch") << false;
    }

    void editSessionControlsResultDeferral() {
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
        QCOMPARE(resolution.dropReason,
                 deferred ? QStringLiteral("edit-session-conflict") : QString());
        QCOMPARE(runtime().documentVersion(), snapshot.documentVersion);

        editSessionManager->endTransaction(sessionId, EditSessionEndReason::Discard);
        QVERIFY(!editSessionManager->hasActiveTransaction());
        QCOMPARE(InferenceApplyGate::resolve(snapshot, resolution, options), Decision::Apply);
        QVERIFY(resolution.dropReason.isEmpty());
    }

    void restartInferenceReleasesReplacedTask() {
        const QPointer<SingingClip> targetClip(clip);
        QTRY_COMPARE_WITH_TIMEOUT(appStatus->languageModuleStatus.get(),
                                  AppStatus::ModuleStatus::Ready, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(appStatus->inferEngineEnvStatus.get(),
                                  AppStatus::ModuleStatus::Ready, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(
            appStatus->packageModuleStatus.get() != AppStatus::ModuleStatus::Loading, 10000);
        const auto packageStatus = appStatus->packageModuleStatus.get();
        const auto restorePackageStatus =
            qScopeGuard([packageStatus] { appStatus->packageModuleStatus = packageStatus; });
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
        const QPointer<InferPiece> targetPiece(targetClip->pieces().first());
        const auto targetPieceId = targetPiece->id();
        piece = targetPiece.data();

        QSemaphore workerEntered;
        QSemaphore releaseWorker;
        std::atomic_bool paused = false;
        QObject observations;
        QPointer<InferDurationTask> firstTask;
        int firstTaskId = -1;
        int replacementTaskId = -1;
        bool replacementFinished = false;
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
                    } else {
                        replacementTaskId = duration->id();
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

        inferController->restartPieceInference(*targetPiece);
        QTRY_VERIFY_WITH_TIMEOUT(workerEntered.available() == 1, 5000);
        QVERIFY(firstTask);
        QVERIFY(firstTask->started());
        QVERIFY(!firstTask->stopped());
        QVERIFY(targetPiece);
        const QPointer<InferPipeline> firstPipeline =
            targetPiece->findChild<InferPipeline *>(Qt::FindDirectChildrenOnly);
        QVERIFY(firstPipeline);

        inferController->restartPieceInference(*targetPiece);
        QTRY_VERIFY_WITH_TIMEOUT(firstPipeline.isNull() && replacementTaskId >= 0, 5000);
        QVERIFY(targetPiece);
        const QPointer<InferPipeline> replacementPipeline =
            targetPiece->findChild<InferPipeline *>(Qt::FindDirectChildrenOnly);
        QVERIFY(replacementPipeline);
        QPointer<InferPipeline> backgroundPipeline = new InferPipeline(*targetPiece);
        const auto removeBackgroundPipeline =
            qScopeGuard([&] { delete backgroundPipeline.data(); });
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
    }

    void offlineExportRestoresMixerState_data() {
        QTest::addColumn<bool>("initiallyOpen");
        QTest::newRow("closed-mixer") << false;
        QTest::newRow("open-mixer") << true;
    }

    void offlineExportRestoresMixerState() {
        QFETCH(bool, initiallyOpen);
        QTemporaryDir files;
        QVERIFY(files.isValid());
        const auto source = files.filePath(QStringLiteral("source.wav"));
        QVERIFY(TestSupport::ProcessFixture::writeWaveFixture(source));
        auto document = Automation::DocumentAutomationFacade::newDocumentDraft(false);
        Automation::ClipDraftDto audio;
        audio.type = Automation::ClipDraftDto::Type::Audio;
        audio.properties.name = QStringLiteral("Audio");
        audio.properties.length = 96;
        audio.properties.clipLen = 96;
        audio.audioPath = source;
        audio.audioInfo.sampleRate = 8000;
        audio.audioInfo.channels = 1;
        audio.audioInfo.frames = 800;
        Automation::TrackDraftDto track;
        track.name = QStringLiteral("Audio");
        track.clips = {audio};
        document.tracks = {track};
        QVERIFY(runtime().documents().commitNewDocument(commandContext(), document));
        auto *mixer = AudioContext::instance()->preMixer();
        mixer->close();
        const auto closeMixer = qScopeGuard([mixer] { mixer->close(); });
        if (initiallyOpen)
            QVERIFY(mixer->open(512, 48000));
        QCOMPARE(mixer->isOpen(), initiallyOpen);

        Automation::AudioExportConfigDto config;
        config.fileName = QStringLiteral("render.wav");
        config.fileDirectory = files.path();
        config.sampleRate = 44100;
        config.mono = true;
        const auto accepted = runtime().audioExports().start(commandContext(), config, {});
        QVERIFY2(accepted, qPrintable(accepted ? QString{} : accepted.getError().message));
        const auto terminal = [&] {
            const auto task = runtime().tasks().getTask(accepted.get().document.documentId,
                                                        accepted.get().taskId);
            if (!task)
                return false;
            const auto state = task.get().state;
            return state == Automation::AutomationTaskState::Succeeded ||
                   state == Automation::AutomationTaskState::Failed ||
                   state == Automation::AutomationTaskState::Canceled;
        };
        QTRY_VERIFY_WITH_TIMEOUT(terminal(), 10000);
        const auto task =
            runtime().tasks().getTask(accepted.get().document.documentId, accepted.get().taskId);
        QVERIFY(task);
        QVERIFY2(task.get().state == Automation::AutomationTaskState::Succeeded,
                 qPrintable(task.get().error ? task.get().error->message
                                             : QStringLiteral("Audio export did not succeed")));
        QCOMPARE(mixer->isOpen(), initiallyOpen);
        QCOMPARE(mixer->bufferSize(), initiallyOpen ? qint64{512} : qint64{0});
        QCOMPARE(mixer->sampleRate(), initiallyOpen ? 48000.0 : 0.0);
        QFile output(files.filePath(QStringLiteral("render.wav")));
        QVERIFY(output.open(QIODevice::ReadOnly));
        talcs::AudioFormatIO decoder(&output);
        QVERIFY2(decoder.open(talcs::AbstractAudioFormatIO::Read),
                 qPrintable(decoder.errorString()));
        QCOMPARE(decoder.sampleRate(), 44100.0);
        QCOMPARE(decoder.channelCount(), 1);
        QVERIFY(decoder.length() > 0);
    }

    void cleanup() {
        if (context)
            editSessionManager->clear();
        clip = nullptr;
        otherClip = nullptr;
        piece = nullptr;
        note = nullptr;
    }

    void cleanupTestCase() {
        context.reset();
        if (dataRootInstalled) {
            if (previousDataRoot.isNull())
                qunsetenv("DSEL_TEST_DATA_ROOT");
            else
                qputenv("DSEL_TEST_DATA_ROOT", previousDataRoot);
        }
    }

private:
    void verifyAcousticGate(InferPipeline &pipeline, bool immediateExpected, bool completeFirst) {
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

    Automation::CoreRuntime &runtime() {
        return *context->m_coreRuntime;
    }

    Automation::CommandContext commandContext() {
        return {.expected = runtime().documentVersion(),
                .source = Automation::InvocationSource::InternalAutomation};
    }

    QTemporaryDir dataRoot;
    QByteArray previousDataRoot;
    bool dataRootInstalled = false;
    std::unique_ptr<AppContext> context;
    Automation::TrackId trackId;
    SingingClip *clip = nullptr;
    SingingClip *otherClip = nullptr;
    InferPiece *piece = nullptr;
    Note *note = nullptr;
};

int main(int argc, char **argv) {
    if (argc > 1 &&
        QString::fromLocal8Bit(argv[1]) == QStringLiteral("--unavailable-inference-provider"))
        return unavailableInferenceProvider(argc, argv);
    QCoreApplication application(argc, argv);
    ApplicationWorkflowTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "tst_application_workflows.moc"
