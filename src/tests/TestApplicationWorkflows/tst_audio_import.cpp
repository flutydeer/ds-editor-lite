#include "tst_application_workflows.h"

#include "Automation/Public/PublicAutomationHostAdapter.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>

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
        return {.trackId = trackId,
                .canonicalPath = path,
                .properties = PublicAudioClipProperties{
                    .name = clientRef, .start = start, .gain = 0.5, .mute = true},
                .clientRef = clientRef};
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
    PublicAudioClipBatchImportRequest request{
        .command = commandContext(),
        .items = {audioItem(trackId, validPath, QStringLiteral("valid-audio"), 480),
                  audioItem(trackId, invalidPath, QStringLiteral("invalid-audio"), 960)},
        .failurePolicy = bestEffort ? PublicBatchFailurePolicy::BestEffort
                                   : PublicBatchFailurePolicy::Atomic};
    const auto services = createPublicAutomationHostServices(
        runtime(), context->m_appModel, &SynthrtEngine::instance());
    const auto accepted = services.importAudioClips(request);
    QVERIFY2(accepted, qPrintable(accepted ? QString{} : accepted.getError().message));
    QTRY_VERIFY_WITH_TIMEOUT(terminal(runtime(), accepted.get()), 10000);
    const auto task = runtime().tasks().getTask(before.documentId, accepted.get().taskId);
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
    const auto &created = task.get().mutation->createdObjects;
    const auto imported = std::find_if(created.cbegin(), created.cend(), [](const auto &object) {
        return object.clientRef == QStringLiteral("valid-audio");
    });
    QVERIFY(imported != created.cend());
    const auto snapshot = runtime().project().getProject(before.documentId);
    QVERIFY(snapshot);
    const auto &clips = snapshot.get().tracks.first().clips;
    const auto found = std::find_if(clips.cbegin(), clips.cend(), [&](const auto &clip) {
        return clip.id.value() == imported->object.value;
    });
    QVERIFY(found != clips.cend());
    QCOMPARE(found->data.audioPath, validPath);
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
                  audioItem(trackId, path, QStringLiteral("second-copy"), 960)}};
    request.command.idempotencyKey = QStringLiteral("cancel-and-retry-audio-batch");
    const auto services = createPublicAutomationHostServices(
        runtime(), context->m_appModel, &SynthrtEngine::instance());
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
