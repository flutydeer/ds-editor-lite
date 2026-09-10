#include "tst_application_workflows.h"

#include "Automation/Public/PublicAutomationHostAdapter.h"
#include "Automation/Public/PublicAutomationRegistry.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/InferController.h"
#include "Modules/Inference/Tasks/InferPitchTask.h"
#include "../TestSupport/VoicebankFixture.h"

#include <lite/PackageManager/PackageManager.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/InferenceData/InferPiece.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>
#include <lite/Tasking/TaskManager.h>

#include <QJsonArray>
#include <QEvent>
#include <QPointer>
#include <QScopeGuard>
#include <QSemaphore>
#include <QThreadPool>
#include <QtTest>

#include <algorithm>
#include <atomic>

using namespace Automation;

namespace {
    QJsonObject inferenceStage(const QJsonObject &response, const QString &name) {
        const auto stages = response.value(QStringLiteral("status"))
                                .toObject()
                                .value(QStringLiteral("stages"))
                                .toArray();
        for (const auto &value : stages) {
            const auto stage = value.toObject();
            if (stage.value(QStringLiteral("stage")) == name)
                return stage;
        }
        return {};
    }
}

void ApplicationWorkflowTests::publicInferenceStatusAssociatesTasksWithTheirScope() {
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->languageModuleStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->inferEngineEnvStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    QTRY_COMPARE_WITH_TIMEOUT(appStatus->packageModuleStatus.get(), AppStatus::ModuleStatus::Ready,
                              10000);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);

    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const auto previousCache = appOptions->inference()->cacheDirectory;
    appOptions->inference()->cacheDirectory = cache.path();
    const auto restoreCache =
        qScopeGuard([&] { appOptions->inference()->cacheDirectory = previousCache; });
    SingerInfo singer;
    for (const auto &package : packageManager->installedPackages().successfulPackages) {
        for (const auto &candidate : package.singers()) {
            if (candidate.singerId() == TestSupport::fixtureSingerId())
                singer = candidate;
        }
    }
    QVERIFY2(!singer.isEmpty(), "The configured fixture singer must be installed");
    QVERIFY(!singer.speakers().isEmpty());
    const QList<QPointer<SingingClip>> targets{clip, otherClip};
    for (const auto &target : targets) {
        QVERIFY(target);
        NoteWordPatchDto word;
        word.noteId = NoteId((*target->notes().begin())->id());
        word.lyric = TestSupport::fixtureLyric();
        word.language = TestSupport::fixtureLanguage();
        word.pronunciation = Pronunciation{};
        word.pronunciationCandidates = QStringList{};
        word.phonemes = Phonemes{};
        QVERIFY(
            runtime().notes().patchWordProperties(commandContext(), ClipId(target->id()), {word}));
        QVERIFY(runtime().parameters().selectClipSingleSpeaker(
            commandContext(), ClipId(target->id()), singer, singer.speakers().first()));
    }
    // A fresh cache keeps both background pipelines waiting for an explicit acoustic request.
    QTRY_VERIFY_WITH_TIMEOUT(
        std::all_of(targets.cbegin(), targets.cend(),
                    [](const auto &target) {
                        return target && !target->pieces().isEmpty() &&
                               std::all_of(target->pieces().cbegin(), target->pieces().cend(),
                                           [](const InferPiece *part) {
                                               return part->state ==
                                                      QStringLiteral("Acoustic.Awaiting");
                                           });
                    }) &&
            taskManager->tasks().isEmpty(),
        15000);
    const QPointer<InferPiece> targetPiece(clip->pieces().first());
    const auto targetPieceId = targetPiece->id();
    const auto documentId = runtime().documentVersion().documentId;
    const QJsonObject scope{
        {QStringLiteral("kind"),     QStringLiteral("clip")},
        {QStringLiteral("clip_ids"), QJsonArray{clip->id()}},
    };
    const QJsonObject unrelatedScope{
        {QStringLiteral("kind"),     QStringLiteral("clip")     },
        {QStringLiteral("clip_ids"), QJsonArray{otherClip->id()}},
    };

    AutomationAccessPolicy access(AutomationWire::ControlLevel::L3);
    AutomationFileGuard fileGuard;
    AdmissionController admission;
    PublicAutomationRegistry registry(
        runtime(), access, fileGuard, admission,
        createPublicAutomationHostServices(runtime(), context->m_appModel,
                                           &SynthrtEngine::instance()));
    const PublicInvocationContext invocation{
        .clientId = QStringLiteral("inference-status-client"),
        .source = InvocationSource::PublicJsonRpc,
    };
    const auto status = [&](const QJsonObject &requestedScope) {
        return registry.invoke(QStringLiteral("inference.get_status"),
                               {
                                   {QStringLiteral("document_id"), documentId.toString()},
                                   {QStringLiteral("scope"),       requestedScope       }
        },
                               invocation);
    };

    QSemaphore workerEntered;
    QSemaphore releaseWorker;
    std::atomic_bool paused = false;
    QObject observations;
    TaskId taskId;
    connect(taskManager, &TaskManager::taskChanged, &observations,
            [&](TaskManager::TaskChangeType change, Task *task, qsizetype) {
                auto *pitch = qobject_cast<InferPitchTask *>(task);
                if (change != TaskManager::Added || !pitch || pitch->pieceId() != targetPieceId)
                    return;
                connect(
                    pitch, &Task::statusUpdated, &observations,
                    [&](const TaskStatus &) {
                        if (!paused.exchange(true)) {
                            workerEntered.release();
                            releaseWorker.acquire();
                        }
                    },
                    Qt::DirectConnection);
            });
    const auto drainTasks = qScopeGuard([&] {
        releaseWorker.release();
        if (!taskId.isNull())
            runtime().automationTasks().requestCancel(documentId, taskId);
        inferController->cancelPieceInference(targetPieceId);
        QThreadPool::globalInstance()->waitForDone();
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    });
    const auto accepted =
        registry.invoke(QStringLiteral("inference.start"),
                        {
                            {QStringLiteral("document_id"),       documentId.toString()},
                            {QStringLiteral("expected_revision"),
                             static_cast<qint64>(runtime().documentVersion().revision) },
                            {QStringLiteral("scope"),             scope                },
                            {QStringLiteral("stages"),
                             QJsonArray{QStringLiteral("pitch"), QStringLiteral("variance"),
                                        QStringLiteral("acoustic")}                    },
                            {QStringLiteral("options"),           QJsonObject{}        }
    },
                        invocation);
    QVERIFY2(accepted, qPrintable(accepted ? QString{} : accepted.getError().message));
    taskId = TaskId::fromString(accepted.get().value(QStringLiteral("task_id")).toString());
    QVERIFY(!taskId.isNull());
    QTRY_COMPARE_WITH_TIMEOUT(workerEntered.available(), 1, 5000);

    const auto running = status(scope);
    QVERIFY2(running, qPrintable(running ? QString{} : running.getError().message));
    const auto duration = inferenceStage(running.get(), QStringLiteral("duration"));
    QCOMPARE(duration.value(QStringLiteral("state")).toString(), QStringLiteral("ready"));
    QVERIFY(duration.value(QStringLiteral("task_id")).isNull());
    const auto pitch = inferenceStage(running.get(), QStringLiteral("pitch"));
    QCOMPARE(pitch.value(QStringLiteral("state")).toString(), QStringLiteral("running"));
    QCOMPARE(pitch.value(QStringLiteral("task_id")).toString(), taskId.toString());
    const auto acoustic = inferenceStage(running.get(), QStringLiteral("acoustic"));
    QCOMPARE(acoustic.value(QStringLiteral("state")).toString(), QStringLiteral("queued"));
    QCOMPARE(acoustic.value(QStringLiteral("task_id")).toString(), taskId.toString());

    const auto unrelated = status(unrelatedScope);
    QVERIFY2(unrelated, qPrintable(unrelated ? QString{} : unrelated.getError().message));
    const auto background = inferenceStage(unrelated.get(), QStringLiteral("acoustic"));
    QVERIFY(background.value(QStringLiteral("task_id")).isNull());

    releaseWorker.release();
    const auto terminal = [&] {
        const auto task = runtime().tasks().getTask(documentId, taskId);
        return task && (task.get().state == AutomationTaskState::Succeeded ||
                        task.get().state == AutomationTaskState::Failed ||
                        task.get().state == AutomationTaskState::Canceled);
    };
    QTRY_VERIFY_WITH_TIMEOUT(terminal(), 15000);
    const auto completed = runtime().tasks().getTask(documentId, taskId);
    QVERIFY(completed);
    QVERIFY2(completed.get().state == AutomationTaskState::Succeeded,
             qPrintable(completed.get().error ? completed.get().error->message : QString{}));
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    const auto finished = status(scope);
    QVERIFY2(finished, qPrintable(finished ? QString{} : finished.getError().message));
    for (const auto &name : {QStringLiteral("pitch"), QStringLiteral("acoustic")}) {
        const auto stage = inferenceStage(finished.get(), name);
        QCOMPARE(stage.value(QStringLiteral("state")).toString(), QStringLiteral("ready"));
        QVERIFY(stage.value(QStringLiteral("task_id")).isNull());
    }
}
