#include "tst_native_desktop.h"
#include "../TestSupport/GuiAppFixture.h"
#include "../TestSupport/FixtureAudioDriver.h"
#include "../TestSupport/WaveFixture.h"

#include "Automation/CoreRuntime.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Controller/DocumentWorkflow/DocumentRecovery.h"
#include "Bootstrap/WindowPlacement.h"
#include "Model/AppOptions/AppOptions.h"
#include "UI/Dialogs/Base/RestartDialog.h"
#include "UI/Dialogs/Base/MessageDialog.h"
#include "UI/Dialogs/Options/Pages/GeneralPage.h"
#include "UI/Dialogs/Base/TaskDialog.h"
#include "UI/Views/BottomPanelView.h"
#include "UI/Views/MainTitleBar/MainMenuView.h"
#include "UI/Views/Common/TabPanelTitleBar.h"
#include "UI/Window/DetachedPanelWindow.h"
#include "UI/Window/MainWindow.h"
#include "UI/Window/DocumentWindowManager.h"
#include "Controller/TrackController.h"
#include "Controller/PlaybackController.h"
#include "Controller/DocumentMidiInput.h"
#include "Modules/Audio/subsystem/MidiSystem.h"
#include "Modules/Audio/utils/MidiInputNoteMonitor.h"
#include "Modules/Import/DocumentImportController.h"
#include "Model/AppStatus/AppStatus.h"
#include "Model/ClipboardDataModel/DecodedClipboardPayload.h"
#include "Global/ControllerGlobal.h"
#include "Modules/ProjectConverters/DspxProjectConverterUi.h"
#include <lite/GUI/Utils/WindowDialogs.h>
#include <QClipboard>
#include <QCheckBox>
#include <QMimeData>
#include <QJsonDocument>
#include <QMessageBox>
#include <QMenu>

#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/Tasking/Task.h>
#include <TalcsDevice/AudioDriverManager.h>

#include <QPointer>
#include <QScopeGuard>
#include <QSemaphore>
#include <QSignalSpy>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QtTest/QTest>

namespace {
    bool selectFixtureAudioOutput() {
        auto *output = AudioSystem::outputSystem()->outputContext();
        auto driver = std::make_unique<TestSupport::FixtureAudioDriver>(true);
        const auto name = driver->name();
        if (!output->driverManager()->addAudioDriver(driver.get()))
            return false;
        driver.release(); // The driver manager owns registered drivers.
        return output->setDriver(name);
    }

    bool clickDialogButton(const QString &text) {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return false;
        for (auto *button : dialog->findChildren<Button *>()) {
            if (button->text() == text) {
                button->click();
                return true;
            }
        }
        dialog->reject();
        return false;
    }

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
    auto *detached = window.detachedPanelWindow();
    QVERIFY(detached && detached->isVisible());

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
    QVERIFY(!detached->isVisible());
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

void NativeDesktopTests::closingLastProjectWindowExitsTheApplication_data() {
    QTest::addColumn<bool>("dirty");
    QTest::newRow("blank-project") << false;
    QTest::newRow("cancel-then-discard") << true;
}

void NativeDesktopTests::closingLastProjectWindowExitsTheApplication() {
    if (!qEnvironmentVariableIsSet("DSEL_TEST_GUI_LIFECYCLE")) {
        runIsolatedDesktopCase();
        return;
    }
    GuiAppFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.error));
    appOptions->appearance()->useNativeFrame = false;
    appOptions->developer()->enablePanelDetach = true;
    DocumentWindowManager windows(*fixture.context);
    auto &last = windows.createWindow();
    auto &first = windows.createWindow();
    const auto firstId = first.context->m_coreRuntime->documentVersion().documentId;
    const auto kept = last.context->m_coreRuntime->documentVersion();
    QPointer<MainWindow> importingWindow = first.window.get();
    {
        const SingletonRegistry::Scope scope(first.context->services());
        const auto audio = fixture.directory.filePath("import.wav");
        QVERIFY(TestSupport::writeWave(audio, QVector<float>(4800, 0.25f)));
        documentImportController->requestImport({audio});
        QVERIFY(first.context->hasPendingTasks());
    }
    first.window->close();
    QVERIFY(!windows.find(firstId));
    QTRY_VERIFY(importingWindow.isNull());
    QCOMPARE(last.context->m_coreRuntime->documentVersion(), kept);
    QVERIFY(last.window->isVisible());
    QVERIFY(!windows.terminating());

    auto *button = last.window->findChild<QAbstractButton *>("CloseButton");
#ifdef Q_OS_WIN
    QVERIFY(button);
#endif
    const auto closeLastWindow = [&] {
        if (button)
            button->click();
        else
            last.window->close();
    };
    QFETCH(bool, dirty);
    auto &runtime = *last.context->m_coreRuntime;
    if (!dirty) {
        const auto closeText = QCoreApplication::translate("MainMenuViewPrivate", "Close Project");
        QAction *closeProject = nullptr;
        for (auto *menu : last.window->findChildren<QMenu *>()) {
            for (auto *action : menu->actions()) {
                if (action->text() == closeText)
                    closeProject = action;
            }
        }
        QVERIFY(closeProject);
        const auto previousId = runtime.documentVersion().documentId;
        closeProject->trigger();
        QVERIFY(runtime.documentVersion().documentId != previousId);
        QVERIFY(last.window->isVisible());
        QVERIFY(!windows.terminating());
    } else {
        {
            const SingletonRegistry::Scope scope(last.context->services());
            QVERIFY(runtime.project().renameTrack(
                {.expected = runtime.documentVersion()},
                Automation::TrackId(last.context->m_appModel->tracks().first()->id()),
                "Unsaved track"));
            QVERIFY(QMetaObject::invokeMethod(last.window.get(), "detachBottomPanel"));
        }
        const auto version = runtime.documentVersion();
        auto *detached = last.window->detachedPanelWindow();
        QVERIFY(detached && detached->isVisible());
        bool canceled = false;
        QTimer::singleShot(0, last.window.get(), [&] {
            auto *prompt = QApplication::activeModalWidget();
            const bool applicationModal =
                prompt && prompt->windowModality() == Qt::ApplicationModal;
            canceled = clickDialogButton(MainWindow::tr("Cancel")) && applicationModal;
        });
        closeLastWindow();
        QVERIFY(canceled);
        QVERIFY(!windows.terminating());
        QCOMPARE(runtime.documentVersion(), version);
        QVERIFY(detached->isVisible());
        QVERIFY(!last.context->m_historyManager->isOnSavePoint());
    }

    bool discarded = false;
    QTimer::singleShot(0, last.window.get(), [&] {
        if (dirty)
            QTimer::singleShot(0, last.window.get(),
                               [&] { discarded = clickDialogButton(MainWindow::tr("Don't save")); });
        closeLastWindow();
    });
    QTimer deadline;
    deadline.setSingleShot(true);
    connect(&deadline, &QTimer::timeout, this, [] { QCoreApplication::exit(1); });
    deadline.start(8000);
    QCOMPARE(QCoreApplication::exec(), 0);
    QCOMPARE(discarded, dirty);
    QVERIFY(windows.terminating());
    QVERIFY(!last.window->isVisible());
}

void NativeDesktopTests::detachedPanelDiesWithItsMainWindow() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QSKIP("Detached windows require a native window backend");
    GuiDocumentFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.error));
    const auto detachEnabled = appOptions->developer()->enablePanelDetach;
    const auto restoreOptions =
        qScopeGuard([&] { appOptions->developer()->enablePanelDetach = detachEnabled; });
    appOptions->developer()->enablePanelDetach = true;

    auto window = std::make_unique<MainWindow>();
    TestSupport::placeWindowOnScreen(*window, {1100, 800});
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.get()));
    auto *bottom = window->findChild<BottomPanelView *>();
    QVERIFY(bottom);
    auto *detach = bottom->titleBar()->findChild<Button *>("btnPanelDetach");
    QVERIFY(detach && detach->isVisible());
    QTest::mouseClick(detach, Qt::LeftButton);
    QPointer<DetachedPanelWindow> detached = window->detachedPanelWindow();
    QVERIFY(detached && detached->isVisible());

    // The window has no QWidget parent, so the main window must destroy it
    // explicitly instead of relying on the QObject tree.
    window.reset();
    QVERIFY(detached.isNull());
}

void NativeDesktopTests::cancelingExitKeepsTheRemainingProject_data() {
    QTest::addColumn<bool>("save");
    QTest::newRow("save-then-cancel") << true;
    QTest::newRow("discard-then-cancel") << false;
}

void NativeDesktopTests::cancelingExitKeepsTheRemainingProject() {
    QFETCH(bool, save);
    GuiDocumentFixture fixture;
    QVERIFY(fixture.initialize());
    DocumentWindowManager windows(*fixture.context);
    auto &a = windows.createWindow();
    auto &b = windows.createWindow();
    const auto documentA = a.context->m_coreRuntime->documentVersion().documentId;
    const auto documentB = b.context->m_coreRuntime->documentVersion().documentId;
    const auto path = fixture.directory.filePath("first.dspx");
    auto &runtime = *a.context->m_coreRuntime;
    QVERIFY(runtime.documents().saveDocument({.expected = runtime.documentVersion()}, path));
    const auto rename = [](auto &entry, const QString &name) {
        const SingletonRegistry::Scope scope(entry.context->services());
        auto &target = *entry.context->m_coreRuntime;
        return target.project().renameTrack(
            {.expected = target.documentVersion()},
            Automation::TrackId(entry.context->m_appModel->tracks().first()->id()), name);
    };
    QVERIFY(rename(a, "Edited A"));
    QVERIFY(rename(b, "Edited B"));
    QVERIFY(QTest::qWaitForWindowActive(b.window.get()));
    windows.activate(a);
    QVERIFY(QTest::qWaitForWindowActive(a.window.get()));
    QVERIFY(QMetaObject::invokeMethod(a.window.get(), "detachBottomPanel"));
    auto *detached = a.window->detachedPanelWindow();
    QVERIFY(detached && QTest::qWaitForWindowExposed(detached));
    a.window->setWorkflowWindow(detached);
    QTimer::singleShot(0, this, [&] {
        const auto cancel = qScopeGuard([] { clickDialogButton(MainWindow::tr("Cancel")); });
        auto *prompt = QApplication::activeModalWidget();
        QVERIFY(prompt && QTest::qWaitForWindowFocused(prompt));
        QVERIFY(WindowDialogs::blocks(a.window.get()));
        QVERIFY(WindowDialogs::blocks(detached));
        QVERIFY(!WindowDialogs::blocks(b.window.get()));
        windows.activate(b);
        QVERIFY(QTest::qWaitForWindowActive(b.window.get()));
        windows.activate(a);
        QVERIFY(QTest::qWaitForWindowFocused(prompt));
    });
    QVERIFY(!windows.close(a, TerminationSavePolicy::Prompt));
    if (QTest::currentTestFailed())
        return;
    QVERIFY(detached->isVisible());
    const auto previousGeometry = appOptions->window()->mainWindowGeometry();
    int decisions = 0;
    bool applicationScope = true;
    QTimer answer;
    connect(&answer, &QTimer::timeout, this, [&] {
        auto *prompt = QApplication::activeModalWidget();
        if (!prompt)
            return;
        applicationScope &= prompt->windowModality() == Qt::ApplicationModal;
        ++decisions;
        clickDialogButton(decisions == 1 ? MainWindow::tr(save ? "Save" : "Don't save")
                                        : MainWindow::tr("Cancel"));
    });
    answer.start(10);
    QCOMPARE(windows.terminate(TerminationMode::Restart, TerminationSavePolicy::Prompt),
             TerminationRequestResult::Busy);
    answer.stop();
    QCOMPARE(decisions, 2);
    QVERIFY(applicationScope);
    QVERIFY(!windows.find(documentA));
    QCOMPARE(windows.find(documentB), &b);
    QCOMPARE(b.context->m_appModel->tracks().first()->name(), QStringLiteral("Edited B"));
    QVERIFY(!b.context->m_historyManager->isOnSavePoint());
    QVERIFY(!qApp->property("restart").toBool());
    QCOMPARE(appOptions->window()->mainWindowGeometry(), previousGeometry);
    AppModel disk;
    DspxProjectConverterUi converter;
    QString error;
    QVERIFY2(converter.load(path, &disk, error, IProjectConverter::NewProject), qPrintable(error));
    QCOMPARE(disk.tracks().first()->name() == QStringLiteral("Edited A"), save);
}

void NativeDesktopTests::newAndOpenFollowTheProjectWindowPreference() {
    using AppOptionsGlobal::ProjectWindowMode;
    GuiDocumentFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.error));
    auto &runtime = *fixture.context->m_coreRuntime;
    const auto initial = runtime.settings().getSettings();
    QVERIFY(initial);
    const auto restore =
        qScopeGuard([&] { QVERIFY(runtime.settings().updateGeneral({}, initial.get().general)); });
    auto general = initial.get().general;
    general.projectWindowMode = ProjectWindowMode::Ask;
    QVERIFY(runtime.settings().updateGeneral({}, general));

    DocumentWindowManager windows(*fixture.context);
    auto &source = windows.createWindow();
    const auto original = fixture.directory.filePath("original.dspx");
    QVERIFY(runtime.documents().saveDocument({.expected = runtime.documentVersion()}, original));
    const auto first = fixture.directory.filePath("first.dspx");
    const auto second = fixture.directory.filePath("second.dspx");
    const auto third = fixture.directory.filePath("third.dspx");
    for (const auto &path : {first, second, third})
        QVERIFY(QFile::copy(original, path));
    QVERIFY(QMetaObject::invokeMethod(source.window.get(), "detachBottomPanel"));
    auto *detached = source.window->detachedPanelWindow();
    QVERIFY(detached);
    auto *menu = detached->findChild<MainMenuView *>();
    QVERIFY(menu);

    const auto invoke = [&](const QString &button, bool remember, auto command) {
        bool asked = false;
        QTimer response;
        connect(&response, &QTimer::timeout, this, [&] {
            auto *dialog = qobject_cast<MessageDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            response.stop();
            asked = true;
            const auto dismiss = qScopeGuard([&] {
                if (dialog->isVisible())
                    dialog->reject();
            });
            QCOMPARE(dialog->parentWidget(), detached);
            auto *check = dialog->findChild<QCheckBox *>();
            QVERIFY(check && !check->isChecked());
            if (remember)
                check->click();
            if (!button.isEmpty())
                QVERIFY(clickDialogButton(button));
        });
        response.start(5);
        command();
        QCOMPARE(asked, !button.isEmpty());
    };
    const auto newProject = [&] { menu->actionNew()->trigger(); };
    const auto originalId = runtime.documentVersion().documentId;
    invoke(MainWindow::tr("Cancel"), true, newProject);
    QCOMPARE(windows.documents().size(), 1U);
    QCOMPARE(runtime.documentVersion().documentId, originalId);
    QCOMPARE(appOptions->general()->projectWindowMode, ProjectWindowMode::Ask);
    invoke(MainWindow::tr("New Window"), false, newProject);
    QCOMPARE(windows.documents().size(), 2U);
    QCOMPARE(runtime.documentVersion().documentId, originalId);
    QCOMPARE(appOptions->general()->projectWindowMode, ProjectWindowMode::Ask);
    auto *other = windows.documents().back().get();
    const auto otherId = other->context->m_coreRuntime->documentVersion().documentId;

    invoke(MainWindow::tr("Current Window"), true, [&] {
        source.window->newOrOpenProject(detached, {original, first, second});
    });
    QTRY_COMPARE(runtime.documentPath(), first);
    QTRY_VERIFY(windows.findPath(second) &&
                windows.findPath(second)->context->m_coreRuntime->documentPath() == second);
    QCOMPARE(windows.documents().size(), 3U);
    QCOMPARE(windows.find(otherId), other);
    QCOMPARE(AppOptions().general()->projectWindowMode, ProjectWindowMode::CurrentWindow);

    // A remembered current-window choice still needs the ordinary unsaved-changes decision.
    const auto track = Automation::TrackId(fixture.context->m_appModel->tracks().first()->id());
    QVERIFY(runtime.project().renameTrack({.expected = runtime.documentVersion()}, track,
                                          QStringLiteral("Unsaved")));
    const auto edited = runtime.documentVersion();
    bool savePrompt = false;
    QTimer cancelSave;
    connect(&cancelSave, &QTimer::timeout, this, [&] {
        if (auto *dialog = qobject_cast<Dialog *>(QApplication::activeModalWidget())) {
            cancelSave.stop();
            savePrompt =
                !qobject_cast<MessageDialog *>(dialog) && dialog->parentWidget() == detached;
            dialog->reject();
        }
    });
    cancelSave.start(5);
    newProject();
    QTRY_VERIFY(savePrompt);
    QTRY_VERIFY(!documentWorkflowController->busy());
    QCOMPARE(runtime.documentVersion(), edited);

    GeneralPage page;
    auto *setting = page.findChild<ComboBox *>("projectWindowMode");
    QVERIFY(setting);
    setting->setCurrentIndex(setting->findData(QVariant::fromValue(ProjectWindowMode::Ask)));
    QCOMPARE(AppOptions().general()->projectWindowMode, ProjectWindowMode::Ask);
    invoke({}, false, [&] { menu->openRecentProject(second); });
    QCOMPARE(windows.documents().size(), 3U);
    invoke(MainWindow::tr("New Window"), true, [&] { menu->openRecentProject(third); });
    QTRY_VERIFY(windows.findPath(third) &&
                windows.findPath(third)->context->m_coreRuntime->documentPath() == third);
    QCOMPARE(AppOptions().general()->projectWindowMode, ProjectWindowMode::NewWindow);
    invoke({}, false, newProject);
    QCOMPARE(windows.documents().size(), 5U);
    QCOMPARE(runtime.documentVersion(), edited);
}

void NativeDesktopTests::detachedPlaybackAndProjectClipboard() {
    if (!qEnvironmentVariableIsSet("DSEL_TEST_GUI_LIFECYCLE")) {
        runIsolatedDesktopCase();
        return;
    }
    GuiAppFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.error));
    QVERIFY(selectFixtureAudioOutput());
    DocumentWindowManager windows(*fixture.context);
    auto &a = windows.createWindow();
    auto &b = windows.createWindow();
    QTRY_VERIFY(b.window->isVisible());
    auto &first = *a.context->m_coreRuntime;
    auto &second = *b.context->m_coreRuntime;
    QVERIFY(QMetaObject::invokeMethod(a.window.get(), "detachBottomPanel"));
    auto *sourceTrack = a.context->m_appModel->tracks().first();
    auto *targetTrack = b.context->m_appModel->tracks().first();
    auto *clip = sourceTrack->clips().toList().first();
    const SingerIdentifier sourceVoice{"lead", "source", QVersionNumber(1, 0)};
    sourceTrack->setSingerAndSpeakerInfo(SingerInfo(sourceVoice), {});
    const auto sourceCount = sourceTrack->clips().count();
    const auto targetCount = targetTrack->clips().count();
    TrackController::setSelectedClips({clip->id()});
    TrackController::copySelectedClips();
    const auto bytes = QApplication::clipboard()->mimeData()->data(
        ControllerGlobal::ElemMimeType.at(ControllerGlobal::Clip));
    QVERIFY(!bytes.isEmpty());
    const auto versionA = first.documentVersion();
    {
        const SingletonRegistry::Scope scope(b.context->services());
        DecodedClipsPayload copy(QJsonDocument::fromJson(bytes).object());
        TrackController::pasteClips(copy.info(), 1920, 0);
        QCOMPARE(targetTrack->clips().count(), targetCount + 1);
        auto *pasted = qobject_cast<SingingClip *>(targetTrack->clips().toList().last());
        QVERIFY(pasted);
        QCOMPARE(pasted->start(), 1920);
        QCOMPARE(pasted->singerIdentifier(), sourceVoice);
        QVERIFY(!pasted->usesTrackVoiceContext());
        QVERIFY(second.history().undo({.expected = second.documentVersion()}));
        QCOMPARE(targetTrack->clips().count(), targetCount);
    }
    QCOMPARE(sourceTrack->clips().count(), sourceCount);
    QCOMPARE(first.documentVersion(), versionA);
    QVERIFY(first.playback().play({.expected = first.documentVersion()}));
    QCOMPARE(a.context->m_playbackController->playbackStatus(), PlaybackGlobal::Playing);
    {
        const SingletonRegistry::Scope scope(b.context->services());
        QVERIFY(second.playback().play({.expected = second.documentVersion()}));
        windows.activate(b);
        QTRY_COMPARE(QApplication::activeWindow(), b.window.get());
        QCOMPARE(b.context->m_playbackController->playbackStatus(), PlaybackGlobal::Playing);
        QTest::keyClick(b.window.get(), Qt::Key_Space);
        QTRY_COMPARE(b.context->m_playbackController->playbackStatus(), PlaybackGlobal::Paused);
        QVERIFY(second.playback().seek({.expected = second.documentVersion()}, 2400));
    }
    QCOMPARE(a.context->m_playbackController->playbackStatus(), PlaybackGlobal::Playing);
    QVERIFY(first.playback().stop({.expected = first.documentVersion()}));
}

void NativeDesktopTests::exitRemembersTheLastEditorGeometry_data() {
    QTest::addColumn<bool>("detach");
    QTest::newRow("main-window") << false;
    QTest::newRow("detached-window") << true;
}

void NativeDesktopTests::midiRecordingFollowsItsProjectAcrossWindowFocus() {
    if (!qEnvironmentVariableIsSet("DSEL_TEST_GUI_LIFECYCLE")) {
        runIsolatedDesktopCase();
        return;
    }
    GuiAppFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.error));
    QVERIFY(selectFixtureAudioOutput());
    DocumentWindowManager windows(*fixture.context);
    auto &a = windows.createWindow();
    QVERIFY(QTest::qWaitForWindowActive(a.window.get()));
    auto *clip = qobject_cast<SingingClip *>(
        a.context->m_appModel->tracks().first()->clips().toList().first());
    QVERIFY(clip);
    a.context->m_appStatus->activeClipId = clip->id();
    const auto previousNotes = clip->notes().count();
    windows.midiInput().setRecording(*a.context, true);
    QVERIFY(windows.midiInput().recording(*a.context));
    auto &b = windows.createWindow();
    QVERIFY(QTest::qWaitForWindowActive(b.window.get()));
    const auto otherVersion = b.context->m_coreRuntime->documentVersion();
    windows.midiInput().setRecording(*b.context, true);
    auto *input = AudioSystem::midiSystem()->hardwareNotes();
    QDialog prompt(b.window.get());
    prompt.setWindowModality(Qt::WindowModal);
    prompt.show();
    QTRY_VERIFY(prompt.isVisible());
    emit input->notePressed(64);
    emit input->noteReleased(64);
    QCOMPARE(clip->notes().count(), previousNotes + 1);
    QCOMPARE(clip->notes().toList().last()->keyIndex(), 64);
    QCOMPARE(b.context->m_coreRuntime->documentVersion(), otherVersion);
    prompt.hide();
    prompt.setWindowModality(Qt::ApplicationModal);
    prompt.show();
    emit input->notePressed(65);
    emit input->noteReleased(65);
    QCOMPARE(clip->notes().count(), previousNotes + 1);
    prompt.hide();
    windows.midiInput().setRecording(*a.context, false);
    emit input->notePressed(67);
    emit input->noteReleased(67);
    QCOMPARE(clip->notes().count(), previousNotes + 1);
}

void NativeDesktopTests::exitRemembersTheLastEditorGeometry() {
    QFETCH(bool, detach);
    if (!qEnvironmentVariableIsSet("DSEL_TEST_GUI_LIFECYCLE")) {
        runIsolatedDesktopCase();
        return;
    }
    GuiAppFixture fixture;
    QVERIFY(fixture.initialize());
    DocumentWindowManager windows(*fixture.context);
    auto &first = windows.createWindow();
    auto &second = windows.createWindow();
    TestSupport::placeWindowOnScreen(*first.window, {1090, 650});
    // A high-DPI desktop can be smaller than an editor's minimum size.
    second.window->setMinimumHeight(second.window->screen()->availableGeometry().height() + 1);
    second.window->showMaximized();
    QVERIFY(QTest::qWaitForWindowActive(second.window.get()));
    windows.activate(first);
    QVERIFY(QTest::qWaitForWindowActive(first.window.get()));
    const auto oldDetached = appOptions->window()->detachedPanelGeometry();
    if (detach) {
        QVERIFY(QMetaObject::invokeMethod(first.window.get(), "detachBottomPanel"));
        auto *editor = first.window->detachedPanelWindow();
        QVERIFY(editor && QTest::qWaitForWindowExposed(editor));
        editor->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(editor));
    }
    QCOMPARE(windows.active(), &first);
    const auto expected = first.placement->saveGeometry();
    QVERIFY(expected != second.placement->saveGeometry());
    const auto detached = detach ? first.window->detachedGeometry() : oldDetached;
    QCOMPARE(windows.terminate(TerminationMode::Exit, TerminationSavePolicy::Discard),
             TerminationRequestResult::Accepted);
    QCOMPARE(appOptions->window()->mainWindowGeometry(), expected);
    QCOMPARE(appOptions->window()->detachedPanelGeometry(), detached);
}

void NativeDesktopTests::recoveryRecordsKeepEachProjectsUnsavedChanges() {
    if (!qEnvironmentVariableIsSet("DSEL_TEST_GUI_LIFECYCLE")) {
        runIsolatedDesktopCase();
        return;
    }
    GuiAppFixture fixture;
    QVERIFY(fixture.initialize());
    const auto path = fixture.directory.filePath("original.dspx");
    QString error;
    {
        DocumentWindowManager windows(*fixture.context);
        auto &a = windows.createWindow();
        auto &b = windows.createWindow();
        QVERIFY(a.context->m_coreRuntime->documents().saveDocument(
            {.expected = a.context->m_coreRuntime->documentVersion()}, path));
        for (const auto &entry : windows.documents()) {
            const SingletonRegistry::Scope scope(entry->context->services());
            auto &runtime = *entry->context->m_coreRuntime;
            QVERIFY(runtime.project().renameTrack(
                {.expected = runtime.documentVersion()},
                Automation::TrackId(entry->context->m_appModel->tracks().first()->id()),
                entry.get() == &a ? "Recovered A" : "Recovered B"));
        }
        QTRY_COMPARE(DocumentRecovery::pending().size(), 2);
        for (const auto &record : DocumentRecovery::pending()) {
            Automation::DocumentDraftDto recovered;
            QVERIFY2(DocumentRecovery::read(record, recovered, error), qPrintable(error));
            QCOMPARE(recovered.tracks.first().name, record.path == path
                                                        ? QStringLiteral("Recovered A")
                                                        : QStringLiteral("Recovered B"));
        }
        AppModel saved;
        DspxProjectConverterUi converter;
        QVERIFY2(converter.load(path, &saved, error, IProjectConverter::NewProject),
                 qPrintable(error));
        QVERIFY(saved.tracks().first()->name() != QStringLiteral("Recovered A"));
        QVERIFY(windows.close(b, TerminationSavePolicy::Discard));
        QTRY_COMPARE(DocumentRecovery::pending().size(), 1);
        QCOMPARE(DocumentRecovery::pending().first().path, path);
    }

    DocumentWindowManager reopened(*fixture.context);
    reopened.createWindow();
    qApp->removeEventFilter(this);
    const auto reinstall = qScopeGuard([this] { qApp->installEventFilter(this); });
    QTimer::singleShot(0, this, [&] {
        auto *prompt = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        QVERIFY(prompt);
        auto *button = prompt->button(QMessageBox::Yes);
        QVERIFY(button);
        button->click();
    });
    reopened.offerRecovery();
    auto *restored = reopened.findPath(path);
    QVERIFY(restored);
    QCOMPARE(restored->context->m_appModel->tracks().first()->name(),
             QStringLiteral("Recovered A"));
    QVERIFY(!restored->context->m_historyManager->isOnSavePoint());
    AppModel onDisk;
    DspxProjectConverterUi converter;
    QVERIFY2(converter.load(path, &onDisk, error, IProjectConverter::NewProject),
             qPrintable(error));
    QVERIFY(onDisk.tracks().first()->name() != QStringLiteral("Recovered A"));
}
