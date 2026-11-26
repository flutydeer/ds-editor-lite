#include "tst_native_desktop.h"
#include <lite/ProjectModel/AppModel/Note.h>
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Audio/utils/MidiInputNoteMonitor.h"
#include "Modules/Audio/subsystem/MidiSystem.h"
#include "Controller/DocumentMidiInput.h"
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <QJsonDocument>
#include <QMimeData>
#include <QClipboard>
#include "Global/ControllerGlobal.h"
#include "Model/ClipboardDataModel/DecodedClipboardPayload.h"
#include "Controller/PlaybackController.h"
#include "Controller/TrackController.h"
#include "Bootstrap/WindowPlacement.h"
#include "../TestSupport/GuiAppFixture.h"

#include "Automation/CoreRuntime.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Model/AppOptions/AppOptions.h"
#include "UI/Dialogs/Base/RestartDialog.h"
#include "UI/Dialogs/Base/TaskDialog.h"
#include "UI/Views/BottomPanelView.h"
#include "UI/Views/Common/TabPanelTitleBar.h"
#include "UI/Window/DetachedPanelWindow.h"
#include "UI/Window/MainWindow.h"
#include "UI/Window/DocumentWindowManager.h"
#include "Modules/ProjectConverters/DspxProjectConverterUi.h"
#include <lite/GUI/Utils/WindowDialogs.h>

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
    bool answerSavePrompt(const QString &text) {
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
    window->activateWindow();
    QTRY_VERIFY(window->isActiveWindow());
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
        return target.project().renameTrack({.expected = target.documentVersion()},
            Automation::TrackId(entry.context->m_appModel->tracks().first()->id()), name);
    };
    QVERIFY(rename(a, "Edited A"));
    QVERIFY(rename(b, "Edited B"));
    QVERIFY(QMetaObject::invokeMethod(a.window.get(), "detachBottomPanel"));
    QTRY_VERIFY(a.window->detachedPanelWindow()->isVisible());
    bool projectScope = false;
    QTimer::singleShot(0, this, [&] {
        projectScope = WindowDialogs::blocks(a.window.get()) &&
                       WindowDialogs::blocks(a.window->detachedPanelWindow()) &&
                       !WindowDialogs::blocks(b.window.get());
        answerSavePrompt(MainWindow::tr("Cancel"));
    });
    QVERIFY(!windows.close(a, TerminationSavePolicy::Prompt));
    QVERIFY(projectScope);
    QVERIFY(a.window->detachedPanelWindow()->isVisible());
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
        answerSavePrompt(decisions == 1 ? MainWindow::tr(save ? "Save" : "Don't save")
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

void NativeDesktopTests::exitRemembersTheLastEditorGeometry_data() {
    QTest::addColumn<bool>("detach");
    QTest::newRow("main-window") << false;
    QTest::newRow("detached-window") << true;
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
    TestSupport::placeWindowOnScreen(*second.window, {1250, 730});
    const auto oldDetached = appOptions->window()->detachedPanelGeometry();
    QWidget *lastEditor = first.window.get();
    if (detach) {
        QVERIFY(QMetaObject::invokeMethod(first.window.get(), "detachBottomPanel"));
        lastEditor = first.window->detachedPanelWindow();
    }
    lastEditor->raise();
    lastEditor->activateWindow();
    QTRY_VERIFY(lastEditor->isActiveWindow());
    QCOMPARE(windows.active(), &first);
    const auto expected = first.placement->saveGeometry();
    QVERIFY(expected != second.placement->saveGeometry());
    const auto detached = detach ? first.window->detachedGeometry() : oldDetached;
    QCOMPARE(windows.terminate(TerminationMode::Exit, TerminationSavePolicy::Discard),
             TerminationRequestResult::Accepted);
    QCOMPARE(appOptions->window()->mainWindowGeometry(), expected);
    QCOMPARE(appOptions->window()->detachedPanelGeometry(), detached);
}

void NativeDesktopTests::detachedPlaybackAndProjectClipboard() {
    GuiDocumentFixture fixture;
    QVERIFY(fixture.initialize(false));
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

void NativeDesktopTests::midiRecordingFollowsItsProjectAcrossWindowFocus() {
    GuiDocumentFixture fixture;
    QVERIFY(fixture.initialize(false));
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
