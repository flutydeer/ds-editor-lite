#include "tst_application_workflows.h"

#include "Automation/Public/PublicAutomationHostAdapter.h"
#include "Automation/Public/PublicAutomationRegistry.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>
#include <lite/Tasking/TaskManager.h>

#include <TalcsFormat/AudioFormatIO.h>

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

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

void ApplicationWorkflowTests::audioBatchFailurePolicy_data() {
    QTest::addColumn<bool>("bestEffort");
    QTest::newRow("atomic-preserves-project") << false;
    QTest::newRow("best-effort-imports-decoded-audio") << true;
}

void ApplicationWorkflowTests::audioBatchFailurePolicy() {
    QFETCH(bool, bestEffort);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto validPath = files.filePath(QStringLiteral("phrase.wav"));
    const auto invalidPath = files.filePath(QStringLiteral("damaged.wav"));
    QVERIFY(writeAudio(validPath));
    QFile invalid(invalidPath);
    QVERIFY(invalid.open(QIODevice::WriteOnly));
    QCOMPARE(invalid.write("not an audio file"), qint64{17});
    invalid.close();
    const auto before = runtime().documentVersion();
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
    const QJsonArray items{
        QJsonObject{{"track_id", trackId.value()},
                    {"path", validPath},
                    {"name", "valid-audio"},
                    {"start", 480},
                    {"gain", 0.5},
                    {"mute", true}},
        QJsonObject{{"track_id", trackId.value()},
                    {"path", invalidPath},
                    {"name", "invalid-audio"},
                    {"start", 960}}
    };
    const auto accepted =
        registry.invoke(QStringLiteral("audio_clips.import_batch"),
                        {
                            {"document_id",       before.documentId.toString()         },
                            {"expected_revision", static_cast<qint64>(before.revision) },
                            {"items",             items                                },
                            {"failure_policy",    bestEffort ? "best_effort" : "atomic"}
    },
                        {.clientId = QStringLiteral("audio-import-client"),
                         .source = InvocationSource::PublicJsonRpc});
    QVERIFY2(accepted, qPrintable(accepted ? QString{} : accepted.getError().message));
    const auto taskId =
        TaskId::fromString(accepted.get().value(QStringLiteral("task_id")).toString());
    QVERIFY(!taskId.isNull());
    QTRY_VERIFY_WITH_TIMEOUT(terminal(runtime(), {taskId, before}), 10000);
    const auto task = runtime().tasks().getTask(before.documentId, taskId);
    QVERIFY(task);
    if (!bestEffort) {
        QCOMPARE(task.get().state, AutomationTaskState::Failed);
        QVERIFY(task.get().error);
        QCOMPARE(task.get().error->code, AutomationErrorCode::IoError);
        QCOMPARE(task.get().error->fieldPath, QStringLiteral("items[1].path"));
        QVERIFY(!task.get().mutation);
        QCOMPARE(runtime().documentVersion(), before);
        QCOMPARE(context->m_appModel->tracks().first()->clips().count(), beforeClips);
        QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);
        return;
    }

    QVERIFY2(task.get().state == AutomationTaskState::Succeeded,
             qPrintable(task.get().error ? task.get().error->message : QString{}));
    QVERIFY(task.get().mutation);
    QCOMPARE(task.get().mutation->warnings.size(), 1);
    QVERIFY(task.get().mutation->warnings.first().contains(QStringLiteral("damaged.wav")));
    QCOMPARE(runtime().documentVersion().revision, before.revision + 1);
    QCOMPARE(context->m_appModel->tracks().first()->clips().count(), beforeClips + 1);
    const auto snapshot = runtime().project().getProject(before.documentId);
    QVERIFY(snapshot);
    const auto &clips = snapshot.get().tracks.first().clips;
    const auto found = std::find_if(clips.cbegin(), clips.cend(), [&](const auto &clip) {
        return clip.data.properties.name == QStringLiteral("valid-audio");
    });
    QVERIFY(found != clips.cend());
    QCOMPARE(QFileInfo(found->data.audioPath).canonicalFilePath(),
             QFileInfo(validPath).canonicalFilePath());
    QCOMPARE(found->data.audioInfo.frames, qint64{4800});
    QCOMPARE(found->data.audioInfo.sampleRate, 48000);
    QCOMPARE(found->data.properties.materialLengthMs, 100.0);
    QCOMPARE(found->data.properties.start, 480);
    QCOMPARE(found->data.properties.gain, 0.5);
    QVERIFY(found->data.properties.mute);
    QVERIFY(!found->data.audioPathInfo.sha512.isEmpty());
    QVERIFY(runtime().history().undo(commandContext()));
    QCOMPARE(context->m_appModel->tracks().first()->clips().count(), beforeClips);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);
}

void ApplicationWorkflowTests::audioBatchCancellationReleasesRetry() {
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto path = files.filePath(QStringLiteral("phrase.wav"));
    QVERIFY(writeAudio(path));
    const auto before = runtime().documentVersion();
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
    // Completion reaches the document through queued connections; cancel before dispatching them.
    const auto cancel = runtime().tasks().cancelTask(commandContext(), accepted.get().taskId);
    QVERIFY2(cancel, qPrintable(cancel ? QString{} : cancel.getError().message));
    QTRY_VERIFY_WITH_TIMEOUT(terminal(runtime(), accepted.get()), 10000);
    const auto canceled = runtime().tasks().getTask(before.documentId, accepted.get().taskId);
    QVERIFY(canceled);
    QCOMPARE(canceled.get().state, AutomationTaskState::Canceled);
    QVERIFY(!canceled.get().mutation);
    QCOMPARE(runtime().documentVersion(), before);
    QCOMPARE(context->m_appModel->tracks().first()->clips().count(), beforeClips);
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
    const auto beforeModel = context->m_appModel->serialize();
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
    QCOMPARE(context->m_appModel->serialize(), beforeModel);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);
}

void ApplicationWorkflowTests::audioBatchRejectsChangesBeforeCommit_data() {
    QTest::addColumn<bool>("replaceDocument");
    QTest::newRow("target-track-removed") << false;
    QTest::newRow("document-replaced") << true;
}

void ApplicationWorkflowTests::audioBatchRejectsChangesBeforeCommit() {
    QFETCH(bool, replaceDocument);
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
    if (replaceDocument) {
        QVERIFY(runtime().documents().commitNewDocument(
            commandContext(), DocumentAutomationFacade::newDocumentDraft(false)));
    } else {
        QVERIFY(runtime().project().removeTracks(commandContext(), {trackId}));
    }
    const auto afterChange = runtime().documentVersion();
    const auto afterModel = context->m_appModel->serialize();
    const auto *afterUndo = HistoryManager::instance()->nextUndoEntry();
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    if (!replaceDocument) {
        QTRY_VERIFY_WITH_TIMEOUT(terminal(runtime(), accepted.get()), 10000);
        const auto completed = runtime().tasks().getTask(before.documentId, accepted.get().taskId);
        QVERIFY(completed && completed.get().error);
        QCOMPARE(completed.get().state, AutomationTaskState::Failed);
        QCOMPARE(completed.get().error->code, AutomationErrorCode::IoError);
        QVERIFY(completed.get().error->message.contains(QStringLiteral("Target track")));
        QVERIFY(!completed.get().mutation);
    }
    QCOMPARE(runtime().documentVersion(), afterChange);
    QCOMPARE(context->m_appModel->serialize(), afterModel);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), afterUndo);
}
