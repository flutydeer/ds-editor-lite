#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Controller/DocumentWorkflow/IDocumentWorkflowUi.h"
#include "Controller/Tasks/OpenDspxProjectTask.h"
#include "UI/Dialogs/Base/ProgressDialog.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectConverters/DspxProjectConverter.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/Tasking/TaskManager.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QAbstractButton>
#include <QPointer>
#include <QSemaphore>
#include <QtTest>

#include <functional>
#include <atomic>
#include <algorithm>

namespace {
    class SavePrompt final : public IDocumentWorkflowUi {
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

void ApplicationGuiTests::newDocumentHonorsTheSaveDecision_data() {
    QTest::addColumn<QString>("choice");
    QTest::newRow("cancel-preserves-dirty-project") << QStringLiteral("cancel");
    QTest::newRow("discard-replaces-project") << QStringLiteral("discard");
    QTest::newRow("cancel-save-path-preserves-project") << QStringLiteral("cancel-path");
    QTest::newRow("save-before-new") << QStringLiteral("save");
    QTest::newRow("failed-save-can-be-canceled") << QStringLiteral("failed-save");
}

void ApplicationGuiTests::newDocumentHonorsTheSaveDecision() {
    QFETCH(QString, choice);
    auto &runtime = *context->m_coreRuntime;
    Automation::TrackDraftDto track;
    track.name = QStringLiteral("Unsaved source");
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, track));
    QVERIFY(!HistoryManager::instance()->isOnSavePoint());
    QVERIFY(!documentWorkflowController->busy());
    const auto before = runtime.documentVersion();
    const auto beforeModel = context->m_appModel->serialize();
    const auto *beforeUndo = HistoryManager::instance()->nextUndoEntry();
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SavePrompt prompt;
    if (choice == QStringLiteral("cancel"))
        prompt.decisions = {SaveDecision::Cancel};
    else if (choice == QStringLiteral("discard"))
        prompt.decisions = {SaveDecision::Discard};
    else
        prompt.decisions = {SaveDecision::Save, SaveDecision::Cancel};
    if (choice == QStringLiteral("save") || choice == QStringLiteral("failed-save"))
        prompt.savePath = directory.filePath(QStringLiteral("source.dspx"));
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
    const bool replaced = choice == QStringLiteral("discard") || choice == QStringLiteral("save");
    if (replaced) {
        QVERIFY(runtime.documentVersion().documentId != before.documentId);
        QVERIFY(HistoryManager::instance()->isOnSavePoint());
        QVERIFY(!HistoryManager::instance()->canUndo());
    } else {
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(context->m_appModel->serialize(), beforeModel);
        QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);
        QVERIFY(!HistoryManager::instance()->isOnSavePoint());
    }
    if (choice == QStringLiteral("save")) {
        QVERIFY(QFileInfo(prompt.savePath).isFile());
        AppModel saved;
        DspxProjectConverter converter;
        QString error;
        QVERIFY2(converter.load(prompt.savePath, &saved, error, ImportMode::NewProject),
                 qPrintable(error));
        QVERIFY(!saved.tracks().isEmpty());
        QCOMPARE(saved.tracks().first()->name(), track.name);
        QVERIFY(documentWorkflowController->recentProjectFiles().contains(prompt.savePath));
    }
    if (choice == QStringLiteral("cancel")) {
        prompt.duringPrompt = {};
        documentWorkflowController->requestSave();
        QTRY_VERIFY(!documentWorkflowController->busy());
        QCOMPARE(prompt.pathCalls, 1);
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(context->m_appModel->serialize(), beforeModel);
        QCOMPARE(historyManager->nextUndoEntry(), beforeUndo);
        QVERIFY(!historyManager->isOnSavePoint());
        prompt.savePath = directory.filePath(QStringLiteral("retained.dspx"));
        documentWorkflowController->requestSave();
        QTRY_VERIFY(!documentWorkflowController->busy());
        QCOMPARE(prompt.pathCalls, 2);
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(context->m_appModel->serialize(), beforeModel);
        QCOMPARE(historyManager->nextUndoEntry(), beforeUndo);
        QVERIFY(historyManager->isOnSavePoint());
        QVERIFY(QFileInfo(prompt.savePath).isFile());
        QVERIFY(prompt.errors.isEmpty());
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
    const auto model = context->m_appModel->serialize();
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
    SavePrompt prompt;
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
    QCOMPARE(context->m_appModel->serialize(), model);
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
    const auto beforeModel = context->m_appModel->serialize();
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

    SavePrompt prompt;
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

    if (action == QStringLiteral("cancel")) {
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
        QTest::mouseClick(cancel, Qt::LeftButton);
        QTRY_VERIFY(progress.isNull());
    } else if (action == QStringLiteral("edit")) {
        QVERIFY(runtime.project().renameTrack(
            commandContext(), Automation::TrackId(context->m_appModel->tracks().first()->id()),
            QStringLiteral("Edit made while loading")));
        expectedVersion = runtime.documentVersion();
        expectedModel = context->m_appModel->serialize();
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
    QCOMPARE(prompt.decisionCalls, action == QStringLiteral("cancel") ? 1 : 2);
    QVERIFY(prompt.errors.isEmpty());
    QCOMPARE(runtime.documentVersion(), expectedVersion);
    QCOMPARE(context->m_appModel->serialize(), expectedModel);
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
