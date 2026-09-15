#include "tst_application_workflows.h"

#include "Automation/Public/PublicAutomationHostAdapter.h"
#include "Automation/Public/PublicAutomationRegistry.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>
#include <lite/Tasking/TaskManager.h>
#include <TalcsFormat/AudioFormatIO.h>

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QtTest>

using namespace Automation;

namespace {
    bool writeWave(const QString &path, const int sampleRate, const float value) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return false;
        talcs::AudioFormatIO writer(&file);
        writer.setSampleRate(sampleRate);
        writer.setChannelCount(1);
        writer.setFormat(static_cast<int>(talcs::AudioFormatIO::WAV) |
                         static_cast<int>(talcs::AudioFormatIO::FLOAT));
        const QVector<float> samples(sampleRate / 10, value);
        return writer.open(talcs::AbstractAudioFormatIO::Write) &&
               writer.write(samples.constData(), samples.size()) == samples.size();
    }

    QString fileHash(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return QString::fromLatin1(
            QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha512).toHex());
    }

    bool isTerminal(CoreRuntime &runtime, const DocumentId &documentId, const TaskId &taskId) {
        const auto task = runtime.tasks().getTask(documentId, taskId);
        return task && (task.get().state == AutomationTaskState::Succeeded ||
                        task.get().state == AutomationTaskState::Failed ||
                        task.get().state == AutomationTaskState::Canceled);
    }
}

void ApplicationWorkflowTests::publicAudioPathUpdatesPrepareCommitAndUndo() {
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto candidateFile = files.filePath(QStringLiteral("candidate.wav"));
    const auto replacementFile = files.filePath(QStringLiteral("replacement.wav"));
    QVERIFY(writeWave(candidateFile, 48000, 0.125f));
    QVERIFY(writeWave(replacementFile, 44100, 0.25f));
    const auto candidatePath = QFileInfo(candidateFile).canonicalFilePath();
    const auto replacementPath = QFileInfo(replacementFile).canonicalFilePath();
    const auto candidateHash = fileHash(candidatePath);
    const auto replacementHash = fileHash(replacementPath);
    QVERIFY(!candidateHash.isEmpty());
    QVERIFY(!replacementHash.isEmpty());
    QVERIFY(candidateHash != replacementHash);

    ClipDraftDto draft;
    draft.type = ClipDraftDto::Type::Audio;
    draft.properties.name = QStringLiteral("Candidate audio");
    draft.properties.length = 96;
    draft.properties.clipLen = 96;
    draft.audioPath = candidatePath;
    TrackDraftDto track;
    track.clips = {draft};
    auto document = DocumentAutomationFacade::newDocumentDraft(false);
    document.tracks = {track};
    QVERIFY(runtime().documents().commitNewDocument(commandContext(), document));
    QVERIFY(runtime().timeline().setTempo(commandContext(), 0, 120));
    auto *audio =
        dynamic_cast<AudioClip *>(*context->m_appModel->tracks().first()->clips().begin());
    QVERIFY(audio);
    QTRY_VERIFY_WITH_TIMEOUT(
        audio->audioInfo().frames == 4800 && !audio->audioInfo().peakCache.isEmpty(), 10000);
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    QVERIFY(runtime().project().setAudioClipPathStatus(commandContext(), ClipId(audio->id()),
                                                       audioAssetSnapshotDto(*audio),
                                                       AudioClip::PathStatus::Unconfirmed));
    HistoryManager::instance()->reset();

    AutomationAccessPolicy access(AutomationWire::ControlLevel::L3);
    AutomationFileGuard fileGuard;
    AdmissionController admission;
    QVERIFY(fileGuard.setConfiguredRoots({files.path()}));
    PublicAutomationRegistry registry(
        runtime(), access, fileGuard, admission,
        createPublicAutomationHostServices(runtime(), context->m_appModel,
                                           &SynthrtEngine::instance()));
    const auto invoke = [&](const QString &operation, const QString &path = QString{}) {
        const auto version = runtime().documentVersion();
        QJsonObject arguments{
            {QStringLiteral("document_id"),       version.documentId.toString()        },
            {QStringLiteral("expected_revision"), static_cast<qint64>(version.revision)},
            {QStringLiteral("clip_id"),           audio->id()                          },
        };
        if (!path.isEmpty())
            arguments.insert(QStringLiteral("path"), path);
        return registry.invoke(operation, arguments,
                               {.clientId = QStringLiteral("audio-path-client"),
                                .source = InvocationSource::PublicJsonRpc});
    };

    const auto verifyUpdate = [&](const QString &operation, const QString &requestedPath,
                                  const QString &expectedPath, const QString &expectedHash,
                                  const int expectedRate) {
        const auto before = runtime().documentVersion();
        const auto previousPath = audio->path();
        const auto previousInfo = audio->pathInfo();
        const auto previousFormat = audio->workspace().value("diffscope.audio.formatData");
        const auto previousStatus = audio->pathStatus();
        const auto *beforeUndo = HistoryManager::instance()->nextUndoEntry();
        const auto accepted = invoke(operation, requestedPath);
        QVERIFY2(accepted, qPrintable(accepted ? QString{} : accepted.getError().message));
        const auto taskId = TaskId::fromString(accepted.get().value("task_id").toString());
        QVERIFY(!taskId.isNull());
        const auto cancelOnFailure = qScopeGuard([&] {
            if (!isTerminal(runtime(), before.documentId, taskId))
                runtime().tasks().cancelTask(commandContext(), taskId);
        });
        QCOMPARE(runtime().documentVersion(), before);
        QCOMPARE(audio->path(), previousPath);
        QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);
        QTRY_VERIFY_WITH_TIMEOUT(isTerminal(runtime(), before.documentId, taskId), 10000);
        const auto task = runtime().tasks().getTask(before.documentId, taskId);
        QVERIFY(task);
        QVERIFY2(task.get().state == AutomationTaskState::Succeeded,
                 qPrintable(task.get().error ? task.get().error->message : QString{}));
        QVERIFY(task.get().mutation);
        QVERIFY(task.get().mutation->changed);
        QCOMPARE(task.get().mutation->previous, before);
        QCOMPARE(task.get().mutation->current.revision, before.revision + 1);
        QCOMPARE(audio->path(), expectedPath);
        QCOMPARE(audio->pathInfo().sha512, expectedHash);
        QCOMPARE(audio->pathStatus(), AudioClip::PathStatus::Normal);
        const auto format = audio->workspace().value("diffscope.audio.formatData");
        QVERIFY(!format.value("entryClassName").toString().isEmpty());
        QVERIFY(!format.value("userData").toString().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(audio->audioInfo().sampleRate == expectedRate &&
                                     audio->audioInfo().frames == expectedRate / 10 &&
                                     !audio->audioInfo().peakCache.isEmpty(),
                                 10000);
        QCOMPARE(audio->audioInfo().channels, 1);
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
        QVERIFY(HistoryManager::instance()->nextUndoEntry() != beforeUndo);
        QVERIFY(runtime().history().undo(commandContext()));
        QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);
        QCOMPARE(audio->path(), previousPath);
        QCOMPARE(audio->pathInfo().relativeDir, previousInfo.relativeDir);
        QCOMPARE(audio->pathInfo().sha512, previousInfo.sha512);
        QCOMPARE(audio->workspace().value("diffscope.audio.formatData"), previousFormat);
        QCOMPARE(audio->pathStatus(), previousStatus);
        QTRY_VERIFY_WITH_TIMEOUT(audio->audioInfo().sampleRate == 48000 &&
                                     audio->audioInfo().frames == 4800 &&
                                     !audio->audioInfo().peakCache.isEmpty(),
                                 10000);
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    };

    verifyUpdate(QStringLiteral("audio_clips.confirm_path"), {}, candidatePath, candidateHash,
                 48000);
    if (QTest::currentTestFailed())
        return;
    verifyUpdate(QStringLiteral("audio_clips.relocate"), replacementPath, replacementPath,
                 replacementHash, 44100);
    if (QTest::currentTestFailed())
        return;

    const auto damagedPath = files.filePath(QStringLiteral("damaged.wav"));
    {
        QFile damaged(damagedPath);
        QVERIFY(damaged.open(QIODevice::WriteOnly));
        QCOMPARE(damaged.write("not an audio file"), qint64{17});
    }
    const auto beforeFailure = runtime().documentVersion();
    const auto beforeModel = context->m_appModel->serialize();
    const auto *beforeUndo = HistoryManager::instance()->nextUndoEntry();
    const auto damaged = invoke(QStringLiteral("audio_clips.relocate"), damagedPath);
    QVERIFY2(damaged, qPrintable(damaged ? QString{} : damaged.getError().message));
    const auto failedTaskId = TaskId::fromString(damaged.get().value("task_id").toString());
    QVERIFY(!failedTaskId.isNull());
    QTRY_VERIFY_WITH_TIMEOUT(isTerminal(runtime(), beforeFailure.documentId, failedTaskId), 10000);
    const auto failed = runtime().tasks().getTask(beforeFailure.documentId, failedTaskId);
    QVERIFY(failed);
    QCOMPARE(failed.get().state, AutomationTaskState::Failed);
    QVERIFY(failed.get().error);
    QCOMPARE(failed.get().error->code, AutomationErrorCode::IoError);
    QCOMPARE(failed.get().error->fieldPath, QStringLiteral("path"));
    QVERIFY(!failed.get().mutation);
    QCOMPARE(runtime().documentVersion(), beforeFailure);
    QCOMPARE(context->m_appModel->serialize(), beforeModel);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);

    const auto accepted = invoke(QStringLiteral("audio_clips.relocate"), replacementPath);
    QVERIFY2(accepted, qPrintable(accepted ? QString{} : accepted.getError().message));
    const auto canceledTaskId = TaskId::fromString(accepted.get().value("task_id").toString());
    QVERIFY(!canceledTaskId.isNull());
    // Path preparation commits through a queued callback; cancel before dispatching it.
    QVERIFY(runtime().tasks().cancelTask(commandContext(), canceledTaskId));
    QTRY_VERIFY_WITH_TIMEOUT(isTerminal(runtime(), beforeFailure.documentId, canceledTaskId),
                             10000);
    const auto canceled = runtime().tasks().getTask(beforeFailure.documentId, canceledTaskId);
    QVERIFY(canceled);
    QCOMPARE(canceled.get().state, AutomationTaskState::Canceled);
    QVERIFY(!canceled.get().mutation);
    QCOMPARE(runtime().documentVersion(), beforeFailure);
    QCOMPARE(context->m_appModel->serialize(), beforeModel);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}
