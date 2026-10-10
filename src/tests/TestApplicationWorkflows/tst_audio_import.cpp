#include "tst_application_workflows.h"

#include "Automation/Public/PublicAutomationHostAdapter.h"
#include "Automation/Public/PublicAutomationRegistry.h"
#include "Automation/ExtractionAutomationAdapter.h"
#include "../TestSupport/ThreadPoolBarrier.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Extractors/ExtractPitchTask.h"
#include "Modules/Extractors/ExtractMidiTask.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>
#include <lite/Tasking/TaskManager.h>

#include <TalcsFormat/AudioFormatIO.h>

#include <QFile>
#include <QCryptographicHash>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

using namespace Automation;

namespace {
    bool writeAudio(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return false;
        talcs::AudioFormatIO writer(&file);
        writer.setSampleRate(48000);
        writer.setChannelCount(1);
        writer.setFormat(talcs::AudioFormatIO::WAV | talcs::AudioFormatIO::FLOAT);
        const QVector<float> samples(4800, 0.125f);
        return writer.open(talcs::AbstractAudioFormatIO::Write) &&
               writer.write(samples.constData(), samples.size()) == samples.size();
    }

    bool terminal(CoreRuntime &runtime, const TaskAcceptedResult &accepted) {
        const auto task = runtime.tasks().getTask(accepted.document.documentId, accepted.taskId);
        return task && (task.get().state == AutomationTaskState::Succeeded ||
                        task.get().state == AutomationTaskState::Failed ||
                        task.get().state == AutomationTaskState::Canceled);
    }

    PublicAudioClipBatchItem audioItem(const TrackId trackId, const QString &path,
                                       const QString &clientRef, const int start) {
        return {
            .trackId = trackId,
            .canonicalPath = path,
            .properties =
                PublicAudioClipProperties{
                                          .name = clientRef, .start = start, .gain = 0.5, .mute = true},
            .clientRef = clientRef
        };
    }
}

void ApplicationWorkflowTests::
    extractionPreparationRejectsMissingResourcesAndChangedSources_data() {
    QTest::addColumn<bool>("pitch");
    QTest::newRow("pitch-preparation") << true;
    QTest::newRow("midi-preparation") << false;
}

void ApplicationWorkflowTests::extractionPreparationRejectsMissingResourcesAndChangedSources() {
    QFETCH(bool, pitch);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto sourcePath = files.filePath(QStringLiteral("source.wav"));
    QVERIFY(writeAudio(sourcePath));
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const auto originalBytes = source.readAll();
    source.close();
    const auto digest = QString::fromLatin1(
        QCryptographicHash::hash(originalBytes, QCryptographicHash::Sha512).toHex());
    const auto restoreSource = [&] {
        QFile restored(sourcePath);
        return restored.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
               restored.write(originalBytes) == originalBytes.size();
    };
    const auto previousPitch = context->m_appOptions->general()->rmvpePath;
    const auto previousMidi = context->m_appOptions->general()->gameDir;
    std::unique_ptr<TestSupport::ThreadPoolBarrier> workers;
    QList<TaskId> attempts;
    const auto cleanup = qScopeGuard([&] {
        context->m_appOptions->general()->rmvpePath = previousPitch;
        context->m_appOptions->general()->gameDir = previousMidi;
        if (workers)
            workers->resume();
        for (const auto &id : attempts)
            runtime().tasks().cancelTask(commandContext(), id);
        workers.reset();
        QThreadPool::globalInstance()->waitForDone(10000);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(runtime().documents().commitNewDocument(
            commandContext(), DocumentAutomationFacade::newDocumentDraft(false)));
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
        if (QTest::currentTestFailed())
            files.setAutoRemove(false);
    });
    auto &modelPath = pitch ? context->m_appOptions->general()->rmvpePath
                            : context->m_appOptions->general()->gameDir;
    modelPath.clear();
    ClipDraftDto audio;
    audio.type = ClipDraftDto::Type::Audio;
    audio.properties.length = 480;
    audio.properties.clipLen = 480;
    audio.audioPath = sourcePath;
    const auto inserted = runtime().project().insertClips(commandContext(), {
                                                                                {trackId, audio}
    });
    QVERIFY(inserted && inserted.get().affectedObjects.size() == 1);
    const ClipId sourceId(inserted.get().affectedObjects.first().value);
    auto *sourceClip =
        qobject_cast<AudioClip *>(context->m_appModel->findClipById(sourceId.value()));
    QVERIFY(sourceClip);
    QTRY_COMPARE(sourceClip->audioInfo().frames, qint64{4800});
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    QVERIFY(runtime().project().setAudioClipHash(commandContext(), sourceId,
                                                 audioAssetSnapshotDto(*sourceClip), digest));
    const auto before = runtime().documentVersion();
    const auto beforeModel = TestSupport::projectSnapshot(*context->m_appModel);
    const auto *beforeUndo = HistoryManager::instance()->nextUndoEntry();
    const auto tasksBefore = runtime().automationTasks().size();
    AutomationAccessPolicy access(AutomationWire::ControlLevel::L3);
    AutomationFileGuard fileGuard;
    AdmissionController admission;
    QVERIFY(fileGuard.setConfiguredRoots({files.path()}));
    PublicAutomationRegistry registry(
        runtime(), access, fileGuard, admission,
        createPublicAutomationHostServices(runtime(), context->m_appModel,
                                           &SynthrtEngine::instance()));
    QJsonObject arguments{
        {QStringLiteral("document_id"),          before.documentId.toString()        },
        {QStringLiteral("expected_revision"),    static_cast<qint64>(before.revision)},
        {QStringLiteral("source_audio_clip_id"), sourceId.value()                    },
        {QStringLiteral("options"),              QJsonObject{}                       }
    };
    if (pitch)
        arguments.insert(QStringLiteral("target_singing_clip_id"), clip->id());
    else
        arguments.insert(QStringLiteral("destination"),
                         QJsonObject{
                             {QStringLiteral("mode"),            QStringLiteral("create_clip")},
                             {QStringLiteral("target_track_id"), trackId.value()              },
                             {QStringLiteral("start"),           480                          }
        });
    const auto tool =
        pitch ? QStringLiteral("extract.pitch.start") : QStringLiteral("extract.midi.start");
    const auto start = [&] { return registry.invoke(tool, arguments); };
    const auto unconfigured = start();
    QVERIFY(!unconfigured);
    QVERIFY2(unconfigured.getError().code == AutomationErrorCode::ModuleNotReady,
             qPrintable(unconfigured.getError().fieldPath + QStringLiteral(": ") +
                        unconfigured.getError().message));
    QCOMPARE(unconfigured.getError().fieldPath,
             pitch ? QStringLiteral("rmvpe_model_path") : QStringLiteral("game_model_path"));
    modelPath = files.filePath(QStringLiteral("unavailable-model"));
    const auto missingModel = start();
    QVERIFY(!missingModel);
    QCOMPARE(missingModel.getError().code, AutomationErrorCode::FileNotFound);
    arguments.insert(QStringLiteral("options"),
                     QJsonObject{
                         {QStringLiteral("model_id"), QStringLiteral("unavailable-model")}
    });
    const auto unsupportedModel = start();
    QVERIFY(!unsupportedModel);
    QCOMPARE(unsupportedModel.getError().code, AutomationErrorCode::InvalidArgument);
    QCOMPARE(unsupportedModel.getError().fieldPath, QStringLiteral("options.model_id"));
    arguments.insert(QStringLiteral("options"), QJsonObject{});
    QCOMPARE(runtime().automationTasks().size(), tasksBefore);
    QCOMPARE(runtime().documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);

    // Preparation only requires a resource path. Every accepted attempt stops before inference.
    modelPath = files.filePath(pitch ? QStringLiteral("placeholder.onnx")
                                     : QStringLiteral("placeholder-model"));
    if (pitch) {
        QFile placeholder(modelPath);
        QVERIFY(placeholder.open(QIODevice::WriteOnly));
    } else {
        QVERIFY(QDir().mkpath(modelPath));
    }
    int modelTasks = 0;
    QObject observations;
    connect(taskManager, &TaskManager::taskChanged, &observations,
            [&](TaskManager::TaskChangeType change, Task *task, qsizetype) {
                if (change == TaskManager::Added && (qobject_cast<ExtractPitchTask *>(task) ||
                                                     qobject_cast<ExtractMidiTask *>(task)))
                    ++modelTasks;
            });
    const auto detachedSource = files.filePath(QStringLiteral("prepared-source.wav"));
    QVERIFY(QFile::copy(sourcePath, detachedSource));
    const auto services = createExtractionAutomationServices(context->m_appOptions, taskManager);
    const auto verifySourceLoss = [&](auto prepare, auto input) {
        input.audioPath = detachedSource;
        input.sourceAsset.pathInfo.sha512 = digest;
        const auto prepared = prepare(input);
        QVERIFY(prepared);
        QVERIFY(QFile::remove(detachedSource));
        struct Outcome {
            std::optional<ExtractionBackendState> state;
            AutomationErrorCode errorCode{};
            QString message;
        };
        const auto outcome = std::make_shared<Outcome>();
        const auto collect = [outcome](auto result) {
            outcome->state = result.state;
            outcome->errorCode = result.errorCode;
            outcome->message = std::move(result.errorMessage);
        };
        const auto stop = qScopeGuard([&] { prepared.get().job->cancel(); });
        prepared.get().job->start({}, collect);
        QTRY_VERIFY_WITH_TIMEOUT(outcome->state.has_value(), 10000);
        QCOMPARE(*outcome->state, ExtractionBackendState::Failed);
        QCOMPARE(outcome->errorCode, AutomationErrorCode::IoError);
        QVERIFY(outcome->message.contains(QStringLiteral("snapshot")));
        QCOMPARE(modelTasks, 0);
        QCOMPARE(runtime().documentVersion(), before);
        QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
        QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);
        QVERIFY(QFile::copy(sourcePath, detachedSource));
        const auto recovered = prepare(input);
        QVERIFY(recovered);
        recovered.get().job->cancel();
        outcome->state.reset();
        recovered.get().job->start({}, collect);
        QVERIFY(outcome->state.has_value());
        QCOMPARE(*outcome->state, ExtractionBackendState::Canceled);
        QCOMPARE(modelTasks, 0);
    };
    if (pitch) {
        verifySourceLoss(services.preparePitch,
                         PitchExtractionInput{.audioClipId = sourceId,
                                              .singingClipId = ClipId(clip->id()),
                                              .timeline = context->m_appModel->timeline()});
    } else {
        verifySourceLoss(services.prepareMidi,
                         MidiExtractionInput{.audioClipId = sourceId,
                                             .timeline = context->m_appModel->timeline(),
                                             .audioClipLengthTick = sourceClip->length()});
    }
    if (QTest::currentTestFailed())
        return;
    QTRY_COMPARE(QThreadPool::globalInstance()->activeThreadCount(), 0);
    workers = std::make_unique<TestSupport::ThreadPoolBarrier>();
    QTRY_VERIFY_WITH_TIMEOUT(workers->ready(), 5000);
    const auto accepted = start();
    QVERIFY2(accepted, qPrintable(accepted ? QString{} : accepted.getError().message));
    const auto canceledId =
        TaskId::fromString(accepted.get().value(QStringLiteral("task_id")).toString());
    QVERIFY(!canceledId.isNull());
    attempts.append(canceledId);
    const auto canceledTask = [&] {
        return runtime().tasks().getTask(before.documentId, canceledId);
    };
    QTRY_VERIFY(canceledTask() && canceledTask().get().state == AutomationTaskState::Running);
    QCOMPARE(modelTasks, 0);
    QVERIFY(runtime().tasks().cancelTask(commandContext(), canceledId));
    workers->resume();
    QTRY_VERIFY_WITH_TIMEOUT(
        canceledTask() && canceledTask().get().state == AutomationTaskState::Canceled, 10000);
    workers.reset();
    QCOMPARE(modelTasks, 0);
    QCOMPARE(runtime().documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);

    QVERIFY(source.open(QIODevice::WriteOnly | QIODevice::Append));
    QCOMPARE(source.write("changed"), qint64{7});
    source.close();
    const auto changed = start();
    QVERIFY2(changed, qPrintable(changed ? QString{} : changed.getError().message));
    const auto changedId =
        TaskId::fromString(changed.get().value(QStringLiteral("task_id")).toString());
    QVERIFY(!changedId.isNull() && changedId != canceledId);
    attempts.append(changedId);
    const auto changedTask = [&] {
        return runtime().tasks().getTask(before.documentId, changedId);
    };
    QTRY_VERIFY_WITH_TIMEOUT(
        changedTask() && changedTask().get().state == AutomationTaskState::Failed, 10000);
    const auto failed = changedTask().get();
    QVERIFY(failed.error && !failed.mutation);
    QCOMPARE(failed.error->code, AutomationErrorCode::InvalidArgument);
    QVERIFY(failed.error->message.contains(QStringLiteral("does not match")));
    QCOMPARE(modelTasks, 0);
    QCOMPARE(runtime().documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);
    QVERIFY(restoreSource());
    const auto retry = start();
    QVERIFY2(retry, qPrintable(retry ? QString{} : retry.getError().message));
    const auto retryId =
        TaskId::fromString(retry.get().value(QStringLiteral("task_id")).toString());
    QVERIFY(!retryId.isNull() && retryId != changedId);
    attempts.append(retryId);
    QVERIFY(runtime().tasks().cancelTask(commandContext(), retryId));
    QTRY_VERIFY_WITH_TIMEOUT(runtime().tasks().getTask(before.documentId, retryId).get().state ==
                                 AutomationTaskState::Canceled,
                             10000);
    QCOMPARE(modelTasks, 0);
    QCOMPARE(runtime().documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);
}

void ApplicationWorkflowTests::audioBatchFailurePolicy_data() {
    QTest::addColumn<bool>("bestEffort");
    QTest::addColumn<bool>("includeValid");
    QTest::addColumn<bool>("removeSourceAfterAdmission");
    QTest::addColumn<bool>("decodedRangeOverflow");
    QTest::newRow("atomic-preserves-project") << false << true << false << false;
    QTest::newRow("best-effort-imports-decoded-audio") << true << true << false << false;
    QTest::newRow("best-effort-no-decodable-audio") << true << false << false << false;
    QTest::newRow("atomic-source-removed-after-admission") << false << true << true << false;
    QTest::newRow("best-effort-source-removed-after-admission") << true << true << true << false;
    QTest::newRow("atomic-decoded-range-out-of-bounds") << false << true << false << true;
    QTest::newRow("best-effort-decoded-range-out-of-bounds") << true << true << false << true;
}

void ApplicationWorkflowTests::audioBatchFailurePolicy() {
    QFETCH(bool, bestEffort);
    QFETCH(bool, includeValid);
    QFETCH(bool, removeSourceAfterAdmission);
    QFETCH(bool, decodedRangeOverflow);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto validPath = files.filePath(QStringLiteral("phrase.wav"));
    const auto invalidPath = files.filePath(QStringLiteral("damaged.wav"));
    QVERIFY(writeAudio(validPath));
    if (removeSourceAfterAdmission || decodedRangeOverflow) {
        QVERIFY(writeAudio(invalidPath));
    } else {
        QFile invalid(invalidPath);
        QVERIFY(invalid.open(QIODevice::WriteOnly));
        QCOMPARE(invalid.write("not an audio file"), qint64{17});
    }
    const auto before = runtime().documentVersion();
    const auto beforeModel = TestSupport::projectSnapshot(*context->m_appModel);
    const auto *beforeUndo = HistoryManager::instance()->nextUndoEntry();
    const auto beforeClips = context->m_appModel->tracks().first()->clips().count();
    AutomationAccessPolicy access(AutomationWire::ControlLevel::L3);
    AutomationFileGuard fileGuard;
    AdmissionController admission;
    QVERIFY(fileGuard.setConfiguredRoots({files.path()}));
    PublicAutomationRegistry registry(
        runtime(), access, fileGuard, admission,
        createPublicAutomationHostServices(runtime(), context->m_appModel,
                                           &SynthrtEngine::instance()));
    QJsonArray items;
    if (includeValid) {
        items.append(QJsonObject{
            {"track_id", trackId.value()},
            {"path",     validPath      },
            {"name",     "valid-audio"  },
            {"start",    480            },
            {"gain",     0.5            },
            {"mute",     true           }
        });
    }
    items.append(QJsonObject{
        {"track_id", trackId.value()                                             },
        {"path",     invalidPath                                                 },
        {"name",     "invalid-audio"                                             },
        {"start",    decodedRangeOverflow ? std::numeric_limits<int>::max() : 960}
    });
    std::unique_ptr<TestSupport::ThreadPoolBarrier> workers;
    if (removeSourceAfterAdmission) {
        QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));
        workers = std::make_unique<TestSupport::ThreadPoolBarrier>();
        QTRY_VERIFY_WITH_TIMEOUT(workers->ready(), 5000);
    }
    QJsonObject arguments{
        {"document_id",       before.documentId.toString()                      },
        {"expected_revision", static_cast<qint64>(before.revision)              },
        {"items",             items                                             },
        {"failure_policy",    bestEffort ? "best_effort" : "atomic"             },
        {"idempotency_key",   QUuid::createUuid().toString(QUuid::WithoutBraces)},
    };
    const auto invoke = [&] {
        return registry.invoke(QStringLiteral("audio_clips.import_batch"), arguments,
                               {.clientId = QStringLiteral("audio-import-client"),
                                .source = InvocationSource::PublicJsonRpc});
    };
    const auto accepted = invoke();
    QVERIFY2(accepted, qPrintable(accepted ? QString{} : accepted.getError().message));
    const auto taskId =
        TaskId::fromString(accepted.get().value(QStringLiteral("task_id")).toString());
    QVERIFY(!taskId.isNull());
    if (removeSourceAfterAdmission) {
        QVERIFY(QFile::remove(invalidPath));
        workers->resume();
    }
    QTRY_VERIFY_WITH_TIMEOUT(terminal(runtime(), {taskId, before}), 10000);
    auto task = runtime().tasks().getTask(before.documentId, taskId);
    QVERIFY(task);
    auto completedTaskId = taskId;
    bool repaired = false;
    if (!bestEffort || !includeValid) {
        QCOMPARE(task.get().state, AutomationTaskState::Failed);
        QVERIFY(task.get().error);
        QCOMPARE(task.get().error->code, decodedRangeOverflow ? AutomationErrorCode::InvalidArgument
                                                              : AutomationErrorCode::IoError);
        if (includeValid)
            QCOMPARE(task.get().error->fieldPath, decodedRangeOverflow
                                                      ? QStringLiteral("items[1].start")
                                                      : QStringLiteral("items[1].path"));
        if (decodedRangeOverflow)
            QVERIFY(task.get().error->taskId == taskId);
        QVERIFY(!task.get().mutation);
        QCOMPARE(runtime().documentVersion(), before);
        QCOMPARE(context->m_appModel->tracks().first()->clips().count(), beforeClips);
        QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
        QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);
        if (decodedRangeOverflow) {
            auto corrected = items.last().toObject();
            corrected.insert(QStringLiteral("start"), 960);
            items.replace(items.size() - 1, corrected);
            arguments.insert(QStringLiteral("items"), items);
            arguments.insert(QStringLiteral("idempotency_key"),
                             QUuid::createUuid().toString(QUuid::WithoutBraces));
        } else {
            QVERIFY(writeAudio(invalidPath));
        }
        const auto retried = invoke();
        QVERIFY2(retried, qPrintable(retried ? QString{} : retried.getError().message));
        completedTaskId =
            TaskId::fromString(retried.get().value(QStringLiteral("task_id")).toString());
        QVERIFY(!completedTaskId.isNull() && completedTaskId != taskId);
        QTRY_VERIFY_WITH_TIMEOUT(terminal(runtime(), {completedTaskId, before}), 10000);
        task = runtime().tasks().getTask(before.documentId, completedTaskId);
        QVERIFY(task);
        const auto failedAttempt = runtime().tasks().getTask(before.documentId, taskId);
        QVERIFY(failedAttempt);
        QCOMPARE(failedAttempt.get().state, AutomationTaskState::Failed);
        repaired = true;
    }

    QVERIFY2(task.get().state == AutomationTaskState::Succeeded,
             qPrintable(task.get().error ? task.get().error->message : QString{}));
    QVERIFY(task.get().mutation);
    if (repaired) {
        QVERIFY(task.get().mutation->warnings.isEmpty());
    } else {
        QCOMPARE(task.get().mutation->warnings.size(), 1);
        QVERIFY(task.get().mutation->warnings.first().contains(QStringLiteral("damaged.wav")));
    }
    QCOMPARE(runtime().documentVersion().revision, before.revision + 1);
    QCOMPARE(context->m_appModel->tracks().first()->clips().count(),
             beforeClips + (repaired ? items.size() : 1));
    const auto snapshot = runtime().project().getProject(before.documentId);
    QVERIFY(snapshot);
    const auto &clips = snapshot.get().tracks.first().clips;
    for (const auto &value : items) {
        const auto item = value.toObject();
        if (!repaired && item.value(QStringLiteral("path")).toString() == invalidPath)
            continue;
        const auto found = std::find_if(clips.cbegin(), clips.cend(), [&](const auto &clip) {
            return clip.data.properties.name == item.value(QStringLiteral("name")).toString();
        });
        QVERIFY(found != clips.cend());
        QCOMPARE(QFileInfo(found->data.audioPath).canonicalFilePath(),
                 QFileInfo(item.value(QStringLiteral("path")).toString()).canonicalFilePath());
        QCOMPARE(found->data.audioInfo.frames, qint64{4800});
        QCOMPARE(found->data.audioInfo.sampleRate, 48000);
        QCOMPARE(found->data.properties.materialLengthMs, 100.0);
        QCOMPARE(found->data.properties.start, item.value(QStringLiteral("start")).toInt());
        QCOMPARE(found->data.properties.gain, item.value(QStringLiteral("gain")).toDouble());
        QCOMPARE(found->data.properties.mute, item.value(QStringLiteral("mute")).toBool());
        QVERIFY(!found->data.audioPathInfo.sha512.isEmpty());
    }
    if (removeSourceAfterAdmission && !repaired)
        QVERIFY(writeAudio(invalidPath));
    const auto committedModel = TestSupport::projectSnapshot(*context->m_appModel);
    const auto committedVersion = runtime().documentVersion();
    const auto *committedUndo = HistoryManager::instance()->nextUndoEntry();
    const auto replay = invoke();
    QVERIFY2(replay, qPrintable(replay ? QString{} : replay.getError().message));
    QCOMPARE(TaskId::fromString(replay.get().value(QStringLiteral("task_id")).toString()),
             completedTaskId);
    QCOMPARE(runtime().documentVersion(), committedVersion);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), committedModel);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), committedUndo);
    QVERIFY(runtime().history().undo(commandContext()));
    QCOMPARE(context->m_appModel->tracks().first()->clips().count(), beforeClips);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);
}

void ApplicationWorkflowTests::audioBatchCancellationReleasesRetry_data() {
    QTest::addColumn<bool>("prepareDecoders");
    QTest::newRow("cancel-on-admission") << false;
    QTest::newRow("cancel-pending-decode") << true;
}

void ApplicationWorkflowTests::audioBatchCancellationReleasesRetry() {
    QFETCH(bool, prepareDecoders);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto path = files.filePath(QStringLiteral("phrase.wav"));
    QVERIFY(writeAudio(path));
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    const auto before = runtime().documentVersion();
    const auto beforeModel = TestSupport::projectSnapshot(*context->m_appModel);
    const auto *beforeUndo = HistoryManager::instance()->nextUndoEntry();
    const auto beforeClips = context->m_appModel->tracks().first()->clips().count();
    PublicAudioClipBatchImportRequest request{
        .command = commandContext(),
        .items = {audioItem(trackId, path, QStringLiteral("first-copy"), 480),
                  audioItem(trackId, path, QStringLiteral("second-copy"), 960)}
    };
    request.command.idempotencyKey = QStringLiteral("cancel-and-retry-audio-batch");
    const auto services = createPublicAutomationHostServices(runtime(), context->m_appModel,
                                                             &SynthrtEngine::instance());
    const auto accepted = services.importAudioClips(request);
    QVERIFY2(accepted, qPrintable(accepted ? QString{} : accepted.getError().message));
    std::unique_ptr<TestSupport::ThreadPoolBarrier> decoders;
    if (prepareDecoders) {
        QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));
        decoders = std::make_unique<TestSupport::ThreadPoolBarrier>();
        QTRY_VERIFY_WITH_TIMEOUT(decoders->ready(), 5000);
        // Publishing hash results queues decode work behind the occupied pool thread.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    }
    const auto cancel = runtime().tasks().cancelTask(commandContext(), accepted.get().taskId);
    QVERIFY2(cancel, qPrintable(cancel ? QString{} : cancel.getError().message));
    if (decoders)
        decoders->resume();
    QTRY_VERIFY_WITH_TIMEOUT(terminal(runtime(), accepted.get()), 10000);
    const auto canceled = runtime().tasks().getTask(before.documentId, accepted.get().taskId);
    QVERIFY(canceled);
    QCOMPARE(canceled.get().state, AutomationTaskState::Canceled);
    QVERIFY(!canceled.get().mutation);
    QCOMPARE(runtime().documentVersion(), before);
    QCOMPARE(context->m_appModel->tracks().first()->clips().count(), beforeClips);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);

    const auto retried = services.importAudioClips(request);
    QVERIFY2(retried, qPrintable(retried ? QString{} : retried.getError().message));
    QVERIFY(retried.get().taskId != accepted.get().taskId);
    QTRY_VERIFY_WITH_TIMEOUT(terminal(runtime(), retried.get()), 10000);
    const auto completed = runtime().tasks().getTask(before.documentId, retried.get().taskId);
    QVERIFY(completed);
    QVERIFY2(completed.get().state == AutomationTaskState::Succeeded,
             qPrintable(completed.get().error ? completed.get().error->message : QString{}));
    QCOMPARE(runtime().documentVersion().revision, before.revision + 1);
    QCOMPARE(context->m_appModel->tracks().first()->clips().count(), beforeClips + 2);
    QVERIFY(runtime().history().undo(commandContext()));
    QCOMPARE(context->m_appModel->tracks().first()->clips().count(), beforeClips);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);
}

void ApplicationWorkflowTests::audioBatchValidationDoesNotStartTasks_data() {
    QTest::addColumn<bool>("bestEffort");
    QTest::addColumn<bool>("includeValid");
    QTest::addColumn<bool>("includeMissing");
    QTest::newRow("valid-batch") << false << true << false;
    QTest::newRow("atomic-missing-file") << false << true << true;
    QTest::newRow("best-effort-keeps-valid-file") << true << true << true;
    QTest::newRow("best-effort-has-no-valid-file") << true << false << true;
}

void ApplicationWorkflowTests::audioBatchValidationDoesNotStartTasks() {
    QFETCH(bool, bestEffort);
    QFETCH(bool, includeValid);
    QFETCH(bool, includeMissing);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto path = files.filePath(QStringLiteral("phrase.wav"));
    QVERIFY(writeAudio(path));
    const auto before = runtime().documentVersion();
    const auto beforeModel = TestSupport::projectSnapshot(*context->m_appModel);
    const auto *beforeUndo = HistoryManager::instance()->nextUndoEntry();
    PublicAudioClipBatchImportRequest request{.command = commandContext(),
                                              .failurePolicy =
                                                  bestEffort ? PublicBatchFailurePolicy::BestEffort
                                                             : PublicBatchFailurePolicy::Atomic};
    request.command.validateOnly = true;
    if (includeValid)
        request.items.append(audioItem(trackId, path, QStringLiteral("valid"), 480));
    if (includeMissing)
        request.items.append(audioItem(trackId, files.filePath(QStringLiteral("missing.wav")),
                                       QStringLiteral("missing"), 960));
    QObject observations;
    int started = 0;
    connect(taskManager, &TaskManager::taskChanged, &observations,
            [&](TaskManager::TaskChangeType change, Task *, qsizetype) {
                if (change == TaskManager::Added)
                    ++started;
            });
    const auto services = createPublicAutomationHostServices(runtime(), context->m_appModel,
                                                             &SynthrtEngine::instance());
    const auto result = services.importAudioClips(request);
    if (includeValid && (!includeMissing || bestEffort)) {
        QVERIFY2(result, qPrintable(result ? QString{} : result.getError().message));
        QVERIFY(result.get().validatedOnly);
        QVERIFY(result.get().taskId.isNull());
        QCOMPARE(result.get().document, before);
    } else {
        QVERIFY(!result);
        QCOMPARE(result.getError().code,
                 bestEffort ? AutomationErrorCode::InvalidArgument : AutomationErrorCode::IoError);
        QCOMPARE(result.getError().fieldPath,
                 bestEffort ? QStringLiteral("path") : QStringLiteral("items[1].path"));
    }
    QCoreApplication::processEvents();
    QCOMPARE(started, 0);
    QCOMPARE(runtime().documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);
}

void ApplicationWorkflowTests::audioBatchRejectsChangesBeforeCommit_data() {
    QTest::addColumn<QString>("change");
    QTest::newRow("target-track-removed") << QStringLiteral("remove");
    QTest::newRow("document-replaced") << QStringLiteral("replace");
    QTest::newRow("document-edited-during-decoding") << QStringLiteral("rename");
}

void ApplicationWorkflowTests::audioBatchRejectsChangesBeforeCommit() {
    QFETCH(QString, change);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto path = files.filePath(QStringLiteral("phrase.wav"));
    QVERIFY(writeAudio(path));
    const auto before = runtime().documentVersion();
    PublicAudioClipBatchImportRequest request{
        .command = commandContext(),
        .items = {audioItem(trackId, path, QStringLiteral("pending"), 480)}};
    const auto services = createPublicAutomationHostServices(runtime(), context->m_appModel,
                                                             &SynthrtEngine::instance());
    const auto accepted = services.importAudioClips(request);
    QVERIFY2(accepted, qPrintable(accepted ? QString{} : accepted.getError().message));
    // Worker results are queued to this thread; change the target before they are delivered.
    if (change == QStringLiteral("replace")) {
        QVERIFY(runtime().documents().commitNewDocument(
            commandContext(), DocumentAutomationFacade::newDocumentDraft(false)));
    } else if (change == QStringLiteral("remove")) {
        QVERIFY(runtime().project().removeTracks(commandContext(), {trackId}));
    } else {
        QVERIFY(runtime().project().renameTrack(commandContext(), trackId,
                                                QStringLiteral("Edited while decoding")));
    }
    const auto afterChange = runtime().documentVersion();
    const auto afterModel = TestSupport::projectSnapshot(*context->m_appModel);
    const auto *afterUndo = HistoryManager::instance()->nextUndoEntry();
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    if (change != QStringLiteral("replace")) {
        QTRY_VERIFY_WITH_TIMEOUT(terminal(runtime(), accepted.get()), 10000);
        const auto completed = runtime().tasks().getTask(before.documentId, accepted.get().taskId);
        QVERIFY(completed && completed.get().error);
        QCOMPARE(completed.get().state, AutomationTaskState::Failed);
        QCOMPARE(completed.get().error->code, change == QStringLiteral("remove")
                                                  ? AutomationErrorCode::IoError
                                                  : AutomationErrorCode::RevisionConflict);
        if (change == QStringLiteral("remove"))
            QVERIFY(completed.get().error->message.contains(QStringLiteral("Target track")));
        QVERIFY(!completed.get().mutation);
    }
    QCOMPARE(runtime().documentVersion(), afterChange);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), afterModel);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), afterUndo);
}
