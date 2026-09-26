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

void ApplicationWorkflowTests::publicAudioPathUpdatesPrepareCommitAndUndo_data() {
    QTest::addColumn<QString>("operation");
    QTest::addColumn<QString>("changeAfterAdmission");
    const auto confirm = QStringLiteral("audio_clips.confirm_path");
    const auto relocate = QStringLiteral("audio_clips.relocate");
    QTest::newRow("confirm") << confirm << QString{};
    QTest::newRow("relocate") << relocate << QString{};
    QTest::newRow("damaged-source") << relocate << QStringLiteral("damaged");
    QTest::newRow("cancel-before-commit") << relocate << QStringLiteral("cancel");
    QTest::newRow("edited-before-commit") << relocate << QStringLiteral("edit");
    QTest::newRow("relocation-access-revoked") << relocate << QStringLiteral("revoke");
    QTest::newRow("confirmation-access-revoked") << confirm << QStringLiteral("revoke");
}

void ApplicationWorkflowTests::publicAudioPathUpdatesPrepareCommitAndUndo() {
    QFETCH(QString, operation);
    QFETCH(QString, changeAfterAdmission);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto cleanup = qScopeGuard([&] {
        runtime().documents().commitNewDocument(commandContext(),
                                                DocumentAutomationFacade::newDocumentDraft(false));
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
        if (QTest::currentTestFailed())
            files.setAutoRemove(false);
        else
            QTRY_VERIFY(files.remove());
    });
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

    const bool confirms = operation == QStringLiteral("audio_clips.confirm_path");
    const auto expectedPath = confirms ? candidatePath : replacementPath;
    const auto expectedHash = confirms ? candidateHash : replacementHash;
    const auto expectedRate = confirms ? 48000 : 44100;
    const auto requestedPath = confirms ? QString{} : replacementPath;
    QByteArray replacementBytes;
    if (changeAfterAdmission == QStringLiteral("damaged")) {
        QFile replacement(replacementPath);
        QVERIFY(replacement.open(QIODevice::ReadOnly));
        replacementBytes = replacement.readAll();
        replacement.close();
        QVERIFY(replacement.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(replacement.write("not an audio file"), qint64{17});
    }

    const auto attempts = changeAfterAdmission.isEmpty() ? 1 : 2;
    for (int attempt = 0; attempt < attempts; ++attempt) {
        const auto before = runtime().documentVersion();
        const auto previousPath = audio->path();
        const auto previousInfo = audio->pathInfo();
        const auto previousFormat = audio->workspace().value("diffscope.audio.formatData");
        const auto previousStatus = audio->pathStatus();
        const auto previousAsset = audioAssetSnapshotDto(*audio);
        const auto *beforeUndo = HistoryManager::instance()->nextUndoEntry();
        const auto accepted = invoke(operation, requestedPath);
        QVERIFY2(accepted, qPrintable(accepted ? QString{} : accepted.getError().message));
        const auto taskId = TaskId::fromString(accepted.get().value("task_id").toString());
        QVERIFY(!taskId.isNull());
        const auto cancelOnFailure = qScopeGuard([&] {
            if (!isTerminal(runtime(), before.documentId, taskId))
                runtime().tasks().cancelTask(commandContext(), taskId);
        });
        const auto queried =
            registry.invoke(QStringLiteral("tasks.get"),
                            {
                                {QStringLiteral("scope"),       QStringLiteral("document")  },
                                {QStringLiteral("document_id"), before.documentId.toString()},
                                {QStringLiteral("task_id"),     taskId.toString()           }
        });
        QVERIFY2(queried, qPrintable(queried ? QString{} : queried.getError().message));
        QCOMPARE(queried.get().value(QStringLiteral("operation_id")).toString(), operation);
        QCOMPARE(runtime().documentVersion(), before);
        QCOMPARE(audio->path(), previousPath);
        QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);

        const bool shouldFail = attempt == 0 && !changeAfterAdmission.isEmpty();
        if (shouldFail) {
            // Preparation posts its commit back to the application thread.
            if (changeAfterAdmission == QStringLiteral("cancel")) {
                const auto cancellation = registry.invoke(
                    QStringLiteral("tasks.cancel"),
                    {
                        {QStringLiteral("scope"),       QStringLiteral("document")  },
                        {QStringLiteral("document_id"), before.documentId.toString()},
                        {QStringLiteral("task_id"),     taskId.toString()           }
                });
                QVERIFY2(cancellation,
                         qPrintable(cancellation ? QString{} : cancellation.getError().message));
            } else if (changeAfterAdmission == QStringLiteral("edit")) {
                QVERIFY(runtime().project().renameTrack(
                    commandContext(), TrackId(context->m_appModel->tracks().first()->id()),
                    QStringLiteral("Edited while preparing audio")));
            } else if (changeAfterAdmission == QStringLiteral("revoke")) {
                QVERIFY(fileGuard.setConfiguredRoots({}));
            }
        }
        const auto expectedVersion = runtime().documentVersion();
        const auto expectedModel = TestSupport::projectSnapshot(*context->m_appModel);
        const auto *expectedUndo = HistoryManager::instance()->nextUndoEntry();
        QTRY_VERIFY_WITH_TIMEOUT(isTerminal(runtime(), before.documentId, taskId), 10000);
        const auto task = runtime().tasks().getTask(before.documentId, taskId);
        QVERIFY(task);
        if (shouldFail) {
            if (changeAfterAdmission == QStringLiteral("cancel")) {
                QCOMPARE(task.get().state, AutomationTaskState::Canceled);
            } else {
                QCOMPARE(task.get().state, AutomationTaskState::Failed);
                QVERIFY(task.get().error);
                const auto expectedError = changeAfterAdmission == QStringLiteral("damaged")
                                               ? AutomationErrorCode::IoError
                                           : changeAfterAdmission == QStringLiteral("edit")
                                               ? AutomationErrorCode::RevisionConflict
                                               : AutomationErrorCode::PermissionDenied;
                QCOMPARE(task.get().error->code, expectedError);
                if (changeAfterAdmission == QStringLiteral("damaged"))
                    QCOMPARE(task.get().error->fieldPath, QStringLiteral("path"));
            }
            QVERIFY(!task.get().mutation);
            QCOMPARE(audioAssetSnapshotDto(*audio), previousAsset);
            QCOMPARE(audio->pathStatus(), previousStatus);
            QCOMPARE(runtime().documentVersion(), expectedVersion);
            QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), expectedModel);
            QCOMPARE(HistoryManager::instance()->nextUndoEntry(), expectedUndo);
            if (changeAfterAdmission == QStringLiteral("revoke")) {
                QVERIFY(fileGuard.setConfiguredRoots({files.path()}));
            } else if (changeAfterAdmission == QStringLiteral("damaged")) {
                QFile replacement(replacementPath);
                QVERIFY(replacement.open(QIODevice::WriteOnly | QIODevice::Truncate));
                QCOMPARE(replacement.write(replacementBytes), replacementBytes.size());
            }
            continue;
        }
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
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}
