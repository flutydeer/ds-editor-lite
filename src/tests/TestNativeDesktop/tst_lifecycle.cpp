#include "tst_native_desktop.h"
#include "../TestSupport/GuiAppFixture.h"

#include "Automation/CoreRuntime.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "UI/Dialogs/Base/RestartDialog.h"
#include "UI/Dialogs/Base/TaskDialog.h"
#include "UI/Views/BottomPanelView.h"
#include "UI/Views/Common/TabPanelTitleBar.h"
#include "UI/Window/MainWindow.h"

#include <lite/GUI/Controls/Button.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/Tasking/Task.h>

#include <QPointer>
#include <QScopeGuard>
#include <QSemaphore>
#include <QSignalSpy>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QtTest/QTest>

namespace {
    class PendingExitTask final : public Task {
    public:
        QSemaphore entered;
        QSemaphore release;

    protected:
        void runTask() override {
            entered.release();
            release.acquire();
        }
    };
}

void NativeDesktopTests::closingMainWindowWaitsForBackgroundTasks_data() {
    QTest::addColumn<bool>("restart");
    QTest::newRow("exit") << false;
    QTest::newRow("restart") << true;
}

void NativeDesktopTests::closingMainWindowWaitsForBackgroundTasks() {
    QFETCH(bool, restart);
    if (!qEnvironmentVariableIsSet("DSEL_TEST_GUI_LIFECYCLE")) {
        runIsolatedDesktopCase();
        return;
    }
    GuiAppFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.error));
    appOptions->appearance()->useNativeFrame = true;
    appOptions->developer()->enablePanelDetach = true;
    MainWindow window;
    TestSupport::placeWindowOnScreen(window, {1100, 800});
    window.show();
    window.activateWindow();
    QTRY_VERIFY(window.isActiveWindow());
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    auto &runtime = *fixture.context->m_coreRuntime;
    QVERIFY(runtime.project().renameTrack(
        {.expected = runtime.documentVersion(), .source = Automation::InvocationSource::Test},
        Automation::TrackId(fixture.context->m_appModel->tracks().first()->id()),
        QStringLiteral("Unsaved at exit")));
    const auto version = runtime.documentVersion();
    const auto model = TestSupport::projectSnapshot(*fixture.context->m_appModel);
    QVERIFY(!historyManager->isOnSavePoint());
    QVERIFY(window.showBottomPanelPage(QStringLiteral("MixConsole")));
    auto *bottom = window.findChild<BottomPanelView *>();
    QVERIFY(bottom);
    auto *detach = bottom->titleBar()->findChild<Button *>("btnPanelDetach");
    QVERIFY(detach && detach->isVisible());
    QTest::mouseClick(detach, Qt::LeftButton);
    QTRY_VERIFY(bottom->isWindow());

    PendingExitTask pending;
    connect(&pending, &Task::finished, &window, [&] { taskManager->removeTask(&pending); });
    const auto releaseOnExit = qScopeGuard([&] {
        pending.release.release();
        QThreadPool::globalInstance()->waitForDone();
        QCoreApplication::processEvents();
        auto *thread = taskManager->thread();
        if (thread != qApp->thread()) {
            QMetaObject::invokeMethod(
                taskManager, [] { taskManager->moveToThread(qApp->thread()); },
                Qt::BlockingQueuedConnection);
            thread->quit();
            QVERIFY(thread->wait(3000));
            delete thread;
        }
    });
    taskManager->addAndStartTask(&pending);
    QTRY_COMPARE(pending.entered.available(), 1);
    QSignalSpy approved(documentWorkflowController,
                        &DocumentWorkflowController::terminationApproved);
    QSignalSpy completed(taskManager, &TaskManager::allDone);
    bool discarded = false;
    QTimer answer;
    answer.setInterval(10);
    connect(&answer, &QTimer::timeout, &window, [&] {
        QPointer<QDialog> prompt = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!prompt || qobject_cast<RestartDialog *>(prompt.data()))
            return;
        answer.stop();
        const auto dismissOnFailure = qScopeGuard([&] {
            if (prompt && !discarded)
                prompt->reject();
        });
        Button *discard = nullptr;
        for (auto *button : prompt->findChildren<Button *>()) {
            if (button->text() == MainWindow::tr("Don't save"))
                discard = button;
        }
        QVERIFY(discard);
        discarded = true;
        QTest::mouseClick(discard, Qt::LeftButton);
    });
    answer.start();
    if (restart) {
        RestartDialog prompt(QStringLiteral("Apply changed settings"), true, &window);
        prompt.show();
        QTRY_VERIFY(prompt.isVisible());
        Button *now = nullptr;
        for (auto *button : prompt.findChildren<Button *>()) {
            if (button->text() == RestartDialog::tr("Restart Now"))
                now = button;
        }
        QVERIFY(now);
        QTest::mouseClick(now, Qt::LeftButton);
    } else {
        QVERIFY(!window.close());
    }
    QTRY_VERIFY(discarded && pending.terminated());
    QCOMPARE(approved.count(), 1);
    QCOMPARE(approved.first().first().value<TerminationMode>(),
             restart ? TerminationMode::Restart : TerminationMode::Exit);
    QVERIFY(window.isVisible());
    QVERIFY(!bottom->isWindow());
    QVERIFY(completed.isEmpty());
    QVERIFY(!pending.stopped());
    QVERIFY(!qApp->property("restart").toBool());
    auto *progress = window.findChild<TaskDialog *>();
    QVERIFY(progress);
    QTRY_VERIFY(progress->isVisible());
    QVERIFY(!window.close());
    QCOMPARE(approved.count(), 1);
    QCOMPARE(runtime.documentVersion(), version);
    QCOMPARE(TestSupport::projectSnapshot(*fixture.context->m_appModel), model);

    pending.release.release();
    QTRY_VERIFY(pending.stopped() && !completed.isEmpty());
    QTRY_VERIFY(!window.isVisible() && !progress->isVisible());
    QCOMPARE(qApp->property("restart").toBool(), restart);
    QVERIFY(taskManager->tasks().isEmpty());
}
