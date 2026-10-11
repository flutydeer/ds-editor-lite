#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Controller/DocumentWorkflow/IDocumentWorkflowUi.h"
#include "Controller/Tasks/OpenDspxProjectTask.h"
#include "Controller/Tasks/ComputeAudioHashTask.h"
#include "Controller/TrackController.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Dialogs/Base/ProgressDialog.h"
#include "UI/Dialogs/Base/MessageDialog.h"
#include "../TestSupport/MainWindowFixture.h"
#include "../TestSupport/ThreadPoolBarrier.h"
#include "../TestSupport/WaveFixture.h"

#include <lite/GUI/Controls/Button.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectConverters/DspxProjectConverter.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/Tasking/TaskManager.h>

#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QAbstractButton>
#include <QPointer>
#include <QSemaphore>
#include <QTimer>
#include <QtTest>

#include <functional>
#include <atomic>
#include <algorithm>

namespace {
    class WorkflowPrompt final : public IDocumentWorkflowUi {
    public:
        QWidget *documentWorkflowParentWidget() override {
            return nullptr;
        }

        SaveDecision askDocumentSaveDecision() override {
            ++decisionCalls;
            promptsWereBusy &= documentWorkflowController->busy();
            if (duringPrompt)
                duringPrompt();
            return decisions.isEmpty() ? SaveDecision::Cancel : decisions.takeFirst();
        }

        QString chooseDocumentSavePath(const QString &suggestion) override {
            suggestedPath = suggestion;
            ++pathCalls;
            return savePath;
        }

        bool confirmOpenWithoutPackageMetadata() override {
            return false;
        }

        void showDocumentWorkflowError(const ProjectOperationError &error) override {
            errors.append(error);
        }

        void showDocumentWorkflowBusy() override {
            ++busyCalls;
        }

        QList<SaveDecision> decisions;
        QString savePath;
        QString suggestedPath;
        QList<ProjectOperationError> errors;
        int decisionCalls = 0;
        int pathCalls = 0;
        int busyCalls = 0;
        bool promptsWereBusy = true;
        std::function<void()> duringPrompt;
    };
}

void ApplicationGuiTests::projectOpenWaitsForPackageMetadata_data() {
    QTest::addColumn<QString>("completion");
    QTest::newRow("ready") << QStringLiteral("ready");
    QTest::newRow("scan-failed-open-anyway") << QStringLiteral("accept");
    QTest::newRow("scan-failed-cancel") << QStringLiteral("reject");
    QTest::newRow("cancel-before-ready") << QStringLiteral("cancel");
}

void ApplicationGuiTests::projectOpenWaitsForPackageMetadata() {
    QFETCH(QString, completion);
    QTRY_COMPARE(appStatus->packageModuleStatus.get(), AppStatus::ModuleStatus::Ready);
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    TestSupport::MainWindowFixture host;
    host.show();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
    const auto before = runtime.documentVersion();
    const auto beforeModel = TestSupport::projectSnapshot(*context->m_appModel);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("waiting-for-packages.dspx"));
    AppModel imported;
    auto *track = new Track;
    track->setName(QStringLiteral("Loaded after scanning"));
    QVERIFY(imported.appendTrack(track));
    DspxProjectConverter converter;
    QString error;
    QVERIFY2(converter.save(path, &imported, error), qPrintable(error));

    auto *workflow = documentWorkflowController;
    QSignalSpy failures(workflow, &DocumentWorkflowController::operationFailed);
    const auto previousStatus = appStatus->packageModuleStatus.get();
    const auto restore = qScopeGuard([&] {
        workflow->setUi(nullptr);
        appStatus->packageModuleStatus = previousStatus;
        if (QTest::currentTestFailed())
            directory.setAutoRemove(false);
    });
    const auto cancelPending = qScopeGuard([&] {
        workflow->cancelCurrentOperation();
        QTRY_VERIFY_WITH_TIMEOUT(!workflow->busy(), 10000);
    });
    int parseStarts = 0;
    QObject observations;
    connect(taskManager, &TaskManager::taskChanged, &observations,
            [&](TaskManager::TaskChangeType change, Task *task, qsizetype) {
                if (const auto *parse = dynamic_cast<OpenDspxProjectTask *>(task);
                    change == TaskManager::Added && parse && parse->filePath() == path)
                    ++parseStarts;
            });
    int metadataCalls = 0;
    QTimer answer;
    answer.setInterval(10);
    connect(&answer, &QTimer::timeout, host.window.get(), [&] {
        QPointer<QDialog> dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog || qobject_cast<ProgressDialog *>(dialog.data()))
            return;
        const auto dismissOnFailure = qScopeGuard([&] {
            if (QTest::currentTestFailed() && dialog) {
                answer.stop();
                dialog->reject();
            }
        });
        QVERIFY(qobject_cast<MessageDialog *>(dialog.data()));
        QVERIFY(completion == QStringLiteral("accept") || completion == QStringLiteral("reject"));
        QCOMPARE(metadataCalls, 0);
        QVERIFY(workflow->busy());
        QCOMPARE(parseStarts, 0);
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
        const auto label = completion == QStringLiteral("accept") ? MainWindow::tr("Open Anyway")
                                                                  : MainWindow::tr("Cancel");
        Button *choice = nullptr;
        for (auto *button : dialog->findChildren<Button *>()) {
            if (button->text() == label)
                choice = button;
        }
        QVERIFY(choice && choice->isVisible() && choice->isEnabled());
        ++metadataCalls;
        answer.stop();
        QTest::mouseClick(choice, Qt::LeftButton);
    });
    answer.start();
    appStatus->packageModuleStatus = AppStatus::ModuleStatus::Loading;
    workflow->requestOpen(path);
    QTRY_VERIFY(([&] {
        for (auto *window : QApplication::topLevelWidgets()) {
            if (auto *progress = qobject_cast<ProgressDialog *>(window);
                progress && progress->isVisible())
                return true;
        }
        return false;
    })());
    QVERIFY(workflow->busy());
    QCOMPARE(parseStarts, 0);
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
    if (completion == QStringLiteral("cancel")) {
        workflow->cancelCurrentOperation();
        QTRY_VERIFY(!workflow->busy());
        appStatus->packageModuleStatus = AppStatus::ModuleStatus::Ready;
    } else {
        appStatus->packageModuleStatus = completion == QStringLiteral("ready")
                                             ? AppStatus::ModuleStatus::Ready
                                             : AppStatus::ModuleStatus::Error;
    }
    QTRY_VERIFY_WITH_TIMEOUT(!workflow->busy(), 10000);
    QCOMPARE(metadataCalls,
             completion == QStringLiteral("accept") || completion == QStringLiteral("reject") ? 1
                                                                                              : 0);
    QVERIFY(failures.isEmpty());
    const bool opened =
        completion == QStringLiteral("ready") || completion == QStringLiteral("accept");
    QCOMPARE(parseStarts, opened ? 1 : 0);
    if (!opened) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        QCoreApplication::processEvents();
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
        QCOMPARE(parseStarts, 0);
        appStatus->packageModuleStatus = AppStatus::ModuleStatus::Ready;
        workflow->requestOpen(path);
        QTRY_VERIFY_WITH_TIMEOUT(!workflow->busy(), 10000);
        QCOMPARE(parseStarts, 1);
    }
    QVERIFY(runtime.documentVersion().documentId != before.documentId);
    QCOMPARE(context->m_appModel->tracks().size(), 1);
    QCOMPARE(context->m_appModel->tracks().first()->name(), track->name());
    QCOMPARE(QFileInfo(workflow->projectPath()).canonicalFilePath(),
             QFileInfo(path).canonicalFilePath());
    QVERIFY(historyManager->isOnSavePoint());
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::newDocumentHonorsTheSaveDecision_data() {
    QTest::addColumn<QString>("choice");
    QTest::newRow("cancel-preserves-dirty-project") << QStringLiteral("cancel");
    QTest::newRow("discard-replaces-project") << QStringLiteral("discard");
    QTest::newRow("cancel-save-path-preserves-project") << QStringLiteral("cancel-path");
    QTest::newRow("save-before-new") << QStringLiteral("save");
    QTest::newRow("save-existing-file-before-new") << QStringLiteral("save-existing");
    QTest::newRow("failed-save-can-be-canceled") << QStringLiteral("failed-save");
}

void ApplicationGuiTests::newDocumentHonorsTheSaveDecision() {
    QFETCH(QString, choice);
    auto &runtime = *context->m_coreRuntime;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const bool existingPath = choice == QStringLiteral("save-existing");
    const auto sourcePath = directory.filePath(QStringLiteral("source.dspx"));
    if (existingPath) {
        QVERIFY(runtime.documents().saveDocument(commandContext(), sourcePath));
        QCOMPARE(documentWorkflowController->projectPath(), sourcePath);
        QVERIFY(historyManager->isOnSavePoint());
    }
    Automation::TrackDraftDto track;
    track.name = QStringLiteral("Unsaved source");
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, track));
    QVERIFY(!HistoryManager::instance()->isOnSavePoint());
    QVERIFY(!documentWorkflowController->busy());
    const auto before = runtime.documentVersion();
    const auto beforeModel = TestSupport::projectSnapshot(*context->m_appModel);
    const auto *beforeUndo = HistoryManager::instance()->nextUndoEntry();
    WorkflowPrompt prompt;
    if (choice == QStringLiteral("cancel"))
        prompt.decisions = {SaveDecision::Cancel};
    else if (choice == QStringLiteral("discard"))
        prompt.decisions = {SaveDecision::Discard};
    else
        prompt.decisions = {SaveDecision::Save, SaveDecision::Cancel};
    if (choice == QStringLiteral("save") || choice == QStringLiteral("failed-save"))
        prompt.savePath = sourcePath;
    if (choice == QStringLiteral("failed-save"))
        QVERIFY(QDir().mkdir(prompt.savePath));
    if (choice == QStringLiteral("cancel"))
        prompt.duringPrompt = [] { documentWorkflowController->requestSaveAs(); };
    documentWorkflowController->setUi(&prompt);
    const auto clearUi = qScopeGuard([] { documentWorkflowController->setUi(nullptr); });
    QSignalSpy busy(documentWorkflowController, &DocumentWorkflowController::busyChanged);
    documentWorkflowController->requestNew();
    QTRY_VERIFY(!documentWorkflowController->busy());
    QVERIFY(prompt.promptsWereBusy);
    if (choice == QStringLiteral("cancel"))
        QCOMPARE(prompt.busyCalls, 1);
    QCOMPARE(busy.count(), 2);
    QCOMPARE(busy.first().first().toBool(), true);
    QCOMPARE(busy.last().first().toBool(), false);
    QCOMPARE(prompt.decisionCalls, choice == QStringLiteral("failed-save") ? 2 : 1);
    QCOMPARE(prompt.pathCalls, choice == QStringLiteral("save") ||
                                       choice == QStringLiteral("cancel-path") ||
                                       choice == QStringLiteral("failed-save")
                                   ? 1
                                   : 0);
    QCOMPARE(prompt.errors.size(), choice == QStringLiteral("failed-save") ? 1 : 0);
    const bool replaced =
        choice == QStringLiteral("discard") || choice == QStringLiteral("save") || existingPath;
    if (replaced) {
        QVERIFY(runtime.documentVersion().documentId != before.documentId);
        QVERIFY(HistoryManager::instance()->isOnSavePoint());
        QVERIFY(!HistoryManager::instance()->canUndo());
    } else {
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
        QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);
        QVERIFY(!HistoryManager::instance()->isOnSavePoint());
    }
    if (choice == QStringLiteral("save") || existingPath) {
        QVERIFY(QFileInfo(sourcePath).isFile());
        AppModel saved;
        DspxProjectConverter converter;
        QString error;
        QVERIFY2(converter.load(sourcePath, &saved, error, ImportMode::NewProject),
                 qPrintable(error));
        QVERIFY(!saved.tracks().isEmpty());
        QCOMPARE(saved.tracks().first()->name(), track.name);
        QVERIFY(documentWorkflowController->recentProjectFiles().contains(sourcePath));
    }
    if (choice == QStringLiteral("cancel")) {
        prompt.duringPrompt = {};
        documentWorkflowController->requestSave();
        QTRY_VERIFY(!documentWorkflowController->busy());
        QCOMPARE(prompt.pathCalls, 1);
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
        QCOMPARE(historyManager->nextUndoEntry(), beforeUndo);
        QVERIFY(!historyManager->isOnSavePoint());
        prompt.savePath = directory.filePath(QStringLiteral("retained.dspx"));
        QVERIFY(QDir().mkdir(prompt.savePath));
        documentWorkflowController->requestSave();
        QTRY_VERIFY(!documentWorkflowController->busy());
        QCOMPARE(prompt.pathCalls, 2);
        QCOMPARE(prompt.errors.size(), 1);
        QVERIFY(!prompt.errors.first().message.isEmpty());
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
        QCOMPARE(historyManager->nextUndoEntry(), beforeUndo);
        QVERIFY(!historyManager->isOnSavePoint());
        QVERIFY(documentWorkflowController->projectPath().isEmpty());
        QVERIFY(QDir().rmdir(prompt.savePath));
        documentWorkflowController->requestSave();
        QTRY_VERIFY(!documentWorkflowController->busy());
        QCOMPARE(prompt.pathCalls, 3);
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
        QCOMPARE(historyManager->nextUndoEntry(), beforeUndo);
        QVERIFY(historyManager->isOnSavePoint());
        QVERIFY(QFileInfo(prompt.savePath).isFile());
        QCOMPARE(prompt.errors.size(), 1);
    }
}

void ApplicationGuiTests::audioHashCompletionWaitsForTheSaveDecision_data() {
    QTest::addColumn<bool>("replaceDocument");
    QTest::newRow("cancel-new-applies-hash") << false;
    QTest::newRow("discard-old-document-rejects-hash") << true;
}

void ApplicationGuiTests::audioHashCompletionWaitsForTheSaveDecision() {
    QFETCH(bool, replaceDocument);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("pending-hash.wav"));
    QVERIFY(TestSupport::writeWave(path, QVector<float>(4800, 0.25f)));
    auto &runtime = *context->m_coreRuntime;
    const auto releaseDocument = qScopeGuard([&] {
        documentWorkflowController->setUi(nullptr);
        QVERIFY(runtime.documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
        QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
        if (QTest::currentTestFailed())
            directory.setAutoRemove(false);
    });
    Automation::ClipDraftDto draft;
    draft.type = Automation::ClipDraftDto::Type::Audio;
    draft.properties.length = 480;
    draft.properties.clipLen = 480;
    draft.audioPath = path;
    Automation::TrackDraftDto track;
    track.name = QStringLiteral("Audio hash");
    track.clips = {draft};
    auto document = Automation::DocumentAutomationFacade::newDocumentDraft(false);
    document.tracks = {track};
    QVERIFY(runtime.documents().commitNewDocument(commandContext(), document));
    QPointer<AudioClip> audio(
        qobject_cast<AudioClip *>(*context->m_appModel->tracks().first()->clips().begin()));
    QVERIFY(audio);
    QTRY_COMPARE(audio->audioInfo().frames, 4800);
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    QVERIFY(audio->pathInfo().sha512.isEmpty());
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const auto expectedHash = QString::fromLatin1(
        QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha512).toHex());
    file.close();
    historyManager->reset();
    QVERIFY(runtime.project().renameTrack(
        commandContext(), Automation::TrackId(context->m_appModel->tracks().first()->id()),
        QStringLiteral("Unsaved edit")));
    const auto before = runtime.documentVersion();
    const auto original = TestSupport::projectSnapshot(*context->m_appModel);
    const auto *beforeUndo = historyManager->nextUndoEntry();
    QTRY_COMPARE(QThreadPool::globalInstance()->activeThreadCount(), 0);
    TestSupport::ThreadPoolBarrier worker;
    QTRY_VERIFY_WITH_TIMEOUT(worker.ready(), 5000);
    QPointer<ComputeAudioHashTask> hashTask;
    Automation::TaskId taskId;
    bool completionDelivered = false;
    QObject observations;
    connect(taskManager, &TaskManager::taskChanged, &observations,
            [&](TaskManager::TaskChangeType type, Task *task, qsizetype) {
                auto *candidate = dynamic_cast<ComputeAudioHashTask *>(task);
                if (type != TaskManager::Added || !candidate || !taskId.isNull())
                    return;
                hashTask = candidate;
                taskId = candidate->automationTaskId;
                connect(
                    candidate, &Task::finished, &observations, [&] { completionDelivered = true; },
                    Qt::QueuedConnection);
            });
    TrackController::scheduleHashUpdate(audio);
    QVERIFY(hashTask && !taskId.isNull());
    WorkflowPrompt prompt;
    prompt.decisions = {replaceDocument ? SaveDecision::Discard : SaveDecision::Cancel};
    prompt.duringPrompt = [&] {
        QVERIFY(documentWorkflowController->busy());
        worker.resume();
        QTRY_VERIFY_WITH_TIMEOUT(completionDelivered, 5000);
        QVERIFY(hashTask && taskManager->tasks().contains(hashTask));
        QVERIFY(audio && audio->pathInfo().sha512.isEmpty());
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), original);
        QCOMPARE(historyManager->nextUndoEntry(), beforeUndo);
    };
    documentWorkflowController->setUi(&prompt);
    documentWorkflowController->requestNew();
    QTRY_VERIFY_WITH_TIMEOUT(prompt.decisionCalls == 1 && !documentWorkflowController->busy(),
                             10000);
    QVERIFY(prompt.errors.isEmpty() && prompt.promptsWereBusy);
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty() && !hashTask, 10000);
    if (replaceDocument) {
        QVERIFY(!audio);
        QVERIFY(runtime.documentVersion().documentId != before.documentId);
        for (const auto *newTrack : context->m_appModel->tracks()) {
            for (const auto *newClip : newTrack->clips())
                QVERIFY(newClip->clipType() != Clip::Audio);
        }
        QVERIFY(historyManager->isOnSavePoint() && !historyManager->canUndo());
    } else {
        QVERIFY(audio);
        QCOMPARE(audio->pathInfo().sha512, expectedHash);
        QCOMPARE(audio->audioInfo().frames, 4800);
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(historyManager->nextUndoEntry(), beforeUndo);
        const auto completed = runtime.tasks().getTask(before.documentId, taskId);
        QVERIFY(completed);
        QCOMPARE(completed.get().state, Automation::AutomationTaskState::Succeeded);
        QVERIFY(runtime.history().undo(commandContext()));
        QCOMPARE(context->m_appModel->tracks().first()->name(), QStringLiteral("Audio hash"));
        QCOMPARE(audio->pathInfo().sha512, expectedHash);
        QVERIFY(!historyManager->canUndo());
    }
}

void ApplicationGuiTests::rejectedProjectInputAllowsTheNextRequest_data() {
    QTest::addColumn<bool>("append");
    QTest::addColumn<bool>("missing");
    QTest::newRow("open-missing") << false << true;
    QTest::newRow("open-unsupported") << false << false;
    QTest::newRow("import-missing") << true << true;
    QTest::newRow("import-unsupported") << true << false;
}

void ApplicationGuiTests::rejectedProjectInputAllowsTheNextRequest() {
    QFETCH(bool, append);
    QFETCH(bool, missing);
    auto &runtime = *context->m_coreRuntime;
    Automation::TrackDraftDto original;
    original.name = QStringLiteral("Unsaved source");
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, original));
    const auto before = runtime.documentVersion();
    const auto model = TestSupport::projectSnapshot(*context->m_appModel);
    const auto *undo = historyManager->nextUndoEntry();
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto invalidPath = directory.filePath(missing ? QStringLiteral("incoming.dspx")
                                                        : QStringLiteral("incoming.unknown"));
    if (!missing) {
        QFile file(invalidPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("unsupported"), 11);
    }
    WorkflowPrompt prompt;
    prompt.decisions = {SaveDecision::Discard};
    auto *workflow = documentWorkflowController;
    workflow->setUi(&prompt);
    const auto clearUi = qScopeGuard([&] { workflow->setUi(nullptr); });
    const auto request = [&](const QString &path) {
        if (append)
            workflow->requestImport(path);
        else
            workflow->requestOpen(path);
    };
    request(invalidPath);
    QTRY_VERIFY(!workflow->busy());
    QCOMPARE(prompt.errors.size(), 1);
    QCOMPARE(prompt.errors.first().title, missing
                                              ? DocumentWorkflowController::tr("File not found")
                                              : DocumentWorkflowController::tr("Unsupported file"));
    QCOMPARE(prompt.decisionCalls, 0);
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), model);
    QCOMPARE(historyManager->nextUndoEntry(), undo);
    QVERIFY(!historyManager->isOnSavePoint());

    AppModel imported;
    auto *track = new Track;
    track->setName(QStringLiteral("Recovered input"));
    QVERIFY(imported.appendTrack(track));
    const auto validPath = directory.filePath(QStringLiteral("incoming.dspx"));
    DspxProjectConverter converter;
    QString error;
    QVERIFY2(converter.save(validPath, &imported, error), qPrintable(error));
    workflow->requestOpen(validPath);
    QTRY_VERIFY(!workflow->busy());
    QCOMPARE(prompt.errors.size(), 1);
    QCOMPARE(prompt.decisionCalls, 1);
    const auto &tracks = context->m_appModel->tracks();
    QVERIFY(std::any_of(tracks.cbegin(), tracks.cend(), [](const Track *candidate) {
        return candidate->name() == QStringLiteral("Recovered input");
    }));
    QVERIFY(runtime.documentVersion().documentId != before.documentId);
    QVERIFY(historyManager->isOnSavePoint());
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::pendingProjectLoadCanCancelOrRequestExit_data() {
    QTest::addColumn<QString>("action");
    QTest::newRow("cancel-progress-dialog") << QStringLiteral("cancel");
    QTest::newRow("close-progress-dialog") << QStringLiteral("close");
    QTest::newRow("cancel-exit-after-stopping-load") << QStringLiteral("exit-cancel");
    QTest::newRow("approve-exit-after-stopping-load") << QStringLiteral("exit-discard");
    QTest::newRow("revalidate-after-an-intervening-edit") << QStringLiteral("edit");
}

void ApplicationGuiTests::pendingProjectLoadCanCancelOrRequestExit() {
    QFETCH(QString, action);
    auto &runtime = *context->m_coreRuntime;
    Automation::TrackDraftDto draft;
    draft.name = QStringLiteral("Unsaved original");
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, draft));
    const auto before = runtime.documentVersion();
    const auto beforeModel = TestSupport::projectSnapshot(*context->m_appModel);
    const auto *beforeUndo = historyManager->nextUndoEntry();
    auto expectedVersion = before;
    auto expectedModel = beforeModel;
    const auto *expectedUndo = beforeUndo;

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("pending.dspx"));
    AppModel imported;
    auto *track = new Track;
    track->setName(QStringLiteral("Loaded document"));
    QVERIFY(imported.appendTrack(track));
    DspxProjectConverter converter;
    QString error;
    QVERIFY2(converter.save(path, &imported, error), qPrintable(error));

    WorkflowPrompt prompt;
    prompt.decisions = {SaveDecision::Discard, action == QStringLiteral("exit-discard")
                                                   ? SaveDecision::Discard
                                                   : SaveDecision::Cancel};
    auto *workflow = documentWorkflowController;
    workflow->setUi(&prompt);
    QSemaphore entered;
    QSemaphore release;
    std::atomic_bool paused = false;
    QPointer<OpenDspxProjectTask> loading;
    QObject observations;
    connect(taskManager, &TaskManager::taskChanged, &observations,
            [&](TaskManager::TaskChangeType change, Task *task, qsizetype) {
                auto *candidate = dynamic_cast<OpenDspxProjectTask *>(task);
                if (change != TaskManager::Added || !candidate || candidate->filePath() != path)
                    return;
                loading = candidate;
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
    const auto cleanup = qScopeGuard([&] {
        release.release();
        workflow->cancelCurrentOperation();
        const auto finished = QTest::qWaitFor([&] { return !loading && !workflow->busy(); }, 10000);
        QTest::qVerify(finished, "load released", "finish the pending project worker", __FILE__,
                       __LINE__);
        workflow->setUi(nullptr);
        if (QTest::currentTestFailed())
            directory.setAutoRemove(false);
    });
    QSignalSpy approved(workflow, &DocumentWorkflowController::terminationApproved);
    QSignalSpy revalidated(workflow, &DocumentWorkflowController::commitRevalidationRequired);
    workflow->requestOpen(path);
    QTRY_COMPARE(entered.available(), 1);
    QVERIFY(loading);
    QVERIFY(workflow->busy());
    QCOMPARE(prompt.decisionCalls, 1);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(approved.isEmpty());

    const bool cancelFromProgress =
        action == QStringLiteral("cancel") || action == QStringLiteral("close");
    if (cancelFromProgress) {
        QPointer<ProgressDialog> progress;
        QTRY_VERIFY(([&] {
            for (auto *window : QApplication::topLevelWidgets()) {
                if (auto *dialog = qobject_cast<ProgressDialog *>(window);
                    dialog && dialog->isVisible()) {
                    progress = dialog;
                    return true;
                }
            }
            return false;
        })());
        QAbstractButton *cancel = nullptr;
        for (auto *button : progress->findChildren<QAbstractButton *>()) {
            if (button->text() == ProgressDialog::tr("Cancel"))
                cancel = button;
        }
        QVERIFY(cancel && cancel->isEnabled());
        if (action == QStringLiteral("close"))
            progress->close();
        else
            QTest::mouseClick(cancel, Qt::LeftButton);
        QTRY_VERIFY(progress.isNull());
    } else if (action == QStringLiteral("edit")) {
        QVERIFY(runtime.project().renameTrack(
            commandContext(), Automation::TrackId(context->m_appModel->tracks().first()->id()),
            QStringLiteral("Edit made while loading")));
        expectedVersion = runtime.documentVersion();
        expectedModel = TestSupport::projectSnapshot(*context->m_appModel);
        expectedUndo = historyManager->nextUndoEntry();
        release.release();
    } else {
        QCOMPARE(
            workflow->requestTermination(TerminationMode::Exit, TerminationSavePolicy::Discard),
            TerminationRequestResult::Busy);
        QCOMPARE(workflow->requestTermination(TerminationMode::Exit),
                 TerminationRequestResult::Accepted);
    }
    QTRY_VERIFY(!workflow->busy());
    QCOMPARE(approved.count(), action == QStringLiteral("exit-discard") ? 1 : 0);
    QCOMPARE(revalidated.count(), action == QStringLiteral("edit") ? 1 : 0);
    if (!approved.isEmpty())
        QCOMPARE(approved.first().first().value<TerminationMode>(), TerminationMode::Exit);
    QCOMPARE(prompt.decisionCalls, cancelFromProgress ? 1 : 2);
    QVERIFY(prompt.errors.isEmpty());
    QCOMPARE(runtime.documentVersion(), expectedVersion);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), expectedModel);
    QCOMPARE(historyManager->nextUndoEntry(), expectedUndo);
    QVERIFY(!historyManager->isOnSavePoint());
    release.release();
    QTRY_VERIFY(!loading);

    prompt.decisions = {SaveDecision::Discard};
    workflow->requestOpen(path);
    QTRY_VERIFY(!workflow->busy());
    QVERIFY(prompt.errors.isEmpty());
    QVERIFY(runtime.documentVersion().documentId != before.documentId);
    QCOMPARE(workflow->projectPath(), path);
    QCOMPARE(context->m_appModel->tracks().first()->name(), QStringLiteral("Loaded document"));
    QVERIFY(historyManager->isOnSavePoint());
    QVERIFY(!historyManager->canUndo());
}
