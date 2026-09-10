#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Controller/TrackController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Dialogs/Base/MessageDialog.h"
#include "UI/Dialogs/Options/AppOptionsDialog.h"
#include "UI/Views/BottomPanelView.h"
#include "UI/Views/ClipEditor/ClipEditorView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"
#include "UI/Views/Common/TabPanelTitleBar.h"
#include "UI/Views/MainTitleBar/MainMenuView.h"
#include "UI/Views/MixConsole/MixConsoleView.h"
#include "UI/Views/TrackEditor/GraphicsItem/AbstractClipView.h"
#include "UI/Views/TrackEditor/TrackEditorView.h"
#include "UI/Views/TrackEditor/TracksGraphicsView.h"
#include "UI/Window/EmbeddedModalHost.h"
#include "UI/Window/MainWindow.h"

#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/SwitchButton.h>
#include <lite/GUI/Controls/Toast.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <QApplication>
#include <QCloseEvent>
#include <QMenu>
#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSplitter>
#include <QTabBar>
#include <QtTest/QTest>

namespace {
    struct MainWindowFixture {
        MainWindowFixture() {
            appOptions->appearance()->useNativeFrame = true;
            appOptions->appearance()->enableDirectManipulation = false;
            appOptions->developer()->enablePanelDetach = true;
            appOptions->developer()->enableEmbeddedOptionsDialog = true;
            window = std::make_unique<MainWindow>();
            window->findChild<MainMenuView *>()->setNativeMenuBar(false);
        }

        ~MainWindowFixture() {
            window->closeAppOptions();
            appOptions->developer()->enablePanelDetach = false;
            window->updatePanelDetachEnabled();
            window->setEditorPanelVisibility(trackVisible, bottomVisible);
            documentWorkflowController->setUi(nullptr);
            trackController->setParentWidget(nullptr);
            Dialog::setGlobalContext(nullptr);
            Toast::setGlobalContext(nullptr);
            window.reset();
            appOptions->appearance()->useNativeFrame = nativeFrame;
            appOptions->appearance()->enableDirectManipulation = directManipulation;
            appOptions->developer()->enablePanelDetach = detachEnabled;
            appOptions->developer()->enableEmbeddedOptionsDialog = embeddedEnabled;
        }

        void show() const {
            window->resize(1200, 900);
            window->show();
            window->activateWindow();
            QTRY_VERIFY(window->isActiveWindow());
            QVERIFY(window->setEditorPanelVisibility(true, true));
            QCoreApplication::processEvents();
        }

        const bool nativeFrame = appOptions->appearance()->useNativeFrame;
        const bool directManipulation = appOptions->appearance()->enableDirectManipulation;
        const bool detachEnabled = appOptions->developer()->enablePanelDetach;
        const bool embeddedEnabled = appOptions->developer()->enableEmbeddedOptionsDialog;
        const bool trackVisible = !appStatus->trackPanelCollapsed;
        const bool bottomVisible = !appStatus->bottomPanelCollapsed;
        std::unique_ptr<MainWindow> window;
    };

    void clickPanelButton(BottomPanelView &panel, const char *name) {
        auto *button = panel.titleBar()->findChild<Button *>(QLatin1String(name));
        QVERIFY(button);
        QTRY_VERIFY(button->isVisible());
        QVERIFY(button->isEnabled());
        QTest::mouseClick(button, Qt::LeftButton);
    }

    void openAppearanceFromMenu(MainWindow &window) {
        auto *bar = window.findChild<MainMenuView *>();
        QVERIFY(bar);
        QMenu *options = nullptr;
        QAction *appearance = nullptr;
        for (auto *menuAction : bar->actions()) {
            if (auto *menu = menuAction->menu()) {
                for (auto *action : menu->actions()) {
                    if (action->text() ==
                        QCoreApplication::translate("MainMenuViewPrivate", "A&ppearance...")) {
                        options = menu;
                        appearance = action;
                    }
                }
            }
        }
        QVERIFY(options);
        QVERIFY(appearance);
        QVERIFY(appearance->isEnabled());
        QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier,
                          bar->actionGeometry(options->menuAction()).center());
        QTRY_VERIFY(options->isVisible());
        const auto closeMenu = qScopeGuard([&] { options->close(); });
        QTest::mouseClick(options, Qt::LeftButton, Qt::NoModifier,
                          options->actionGeometry(appearance).center());
        QTRY_VERIFY(!options->isVisible());
    }

    void clickPanelTab(BottomPanelView &panel, const QString &name) {
        auto *tabs = panel.titleBar()->tabBar();
        int index = -1;
        for (int i = 0; i < tabs->count(); ++i) {
            if (tabs->tabText(i) == name)
                index = i;
        }
        QVERIFY(index >= 0);
        QTest::mouseClick(tabs, Qt::LeftButton, Qt::NoModifier, tabs->tabRect(index).center());
    }
}

void ApplicationGuiTests::panelButtonsAndClipDoubleClickRestoreTheEditorView() {
    MainWindowFixture host;
    host.show();
    if (QTest::currentTestFailed())
        return;
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    view->hide();
    auto &window = *host.window;
    window.activateWindow();
    auto *bottom = window.findChild<BottomPanelView *>();
    auto *tracks = window.findChild<TracksGraphicsView *>();
    QVERIFY(bottom);
    QVERIFY(tracks);
    auto *splitter = qobject_cast<QSplitter *>(bottom->parentWidget());
    QVERIFY(splitter);
    bottom->clipEditorView()->onActiveClipChanged(singingClip->id());
    QTRY_VERIFY(bottom->clipEditorView()->hasActiveSingingClip());
    QVERIFY(window.setTrackPanelScale(1.5, 1.0));
    QVERIFY(window.centerTrackPanelAt(1920, 0));
    QVERIFY(window.setPianoRollScale(1.5, 1.25));
    QVERIFY(window.centerPianoRollAt(1920, 62));
    QVERIFY(window.setParameterForeground(ParamInfo::MouthOpening));
    QVERIFY(window.setParameterBackground(ParamInfo::Breathiness));
    QVERIFY(window.setParameterEditMode(EditorViewGlobal::ParameterEditMode::Shape));
    QVERIFY(window.setParameterValueViewport(0.4, 2));
    QVERIFY(window.focusEditorRegion(EditorViewGlobal::Region::PianoRoll));
    QCoreApplication::processEvents();
    const auto saved = window.captureEditorViewState();
    const auto sizes = splitter->sizes();
    QVERIFY(sizes.at(0) > 0 && sizes.at(1) > 0);
    auto &runtime = *context->m_coreRuntime;
    historyManager->reset();
    const auto before = runtime.documentVersion();
    clickPanelButton(*bottom, "btnPanelMaximize");
    if (QTest::currentTestFailed())
        return;
    QTRY_COMPARE(splitter->sizes().at(0), 0);
    QVERIFY(appStatus->trackPanelCollapsed);
    QVERIFY(!appStatus->bottomPanelCollapsed);
    clickPanelButton(*bottom, "btnPanelMaximize");
    if (QTest::currentTestFailed())
        return;
    QTRY_COMPARE(splitter->sizes(), sizes);
    clickPanelButton(*bottom, "btnPanelHide");
    if (QTest::currentTestFailed())
        return;
    QTRY_COMPARE(splitter->sizes().at(1), 0);
    QVERIFY(appStatus->bottomPanelCollapsed);
    AbstractClipView *clipItem = nullptr;
    for (auto *item : tracks->scene()->items()) {
        auto *candidate = dynamic_cast<AbstractClipView *>(item);
        if (candidate && candidate->id() == singingClip->id())
            clipItem = candidate;
    }
    QVERIFY(clipItem);
    const auto visible = tracks->mapFromScene(clipItem->sceneBoundingRect())
                             .boundingRect()
                             .intersected(tracks->viewport()->rect());
    QVERIFY(!visible.isEmpty());
    QTest::mouseDClick(tracks->viewport(), Qt::LeftButton, Qt::NoModifier, visible.center());
    QTest::mouseRelease(tracks->viewport(), Qt::LeftButton, Qt::NoModifier, visible.center());
    QTRY_VERIFY(!appStatus->bottomPanelCollapsed);
    QCOMPARE(bottom->currentPageId(), QStringLiteral("ClipEditor"));
    QVERIFY(bottom->clipEditorView()->hasActiveSingingClip());
    QVERIFY(window.setPianoRollScale(0.75, 1.0));
    QVERIFY(window.setParameterEditMode(EditorViewGlobal::ParameterEditMode::Draw));
    QVERIFY(window.restoreEditorViewState(saved));
    QCoreApplication::processEvents();
    const auto restored = window.captureEditorViewState();
    QCOMPARE(restored.layout, saved.layout);
    QCOMPARE(restored.parameters, saved.parameters);
    QCOMPARE(restored.pianoRoll.horizontalScale, saved.pianoRoll.horizontalScale);
    QCOMPARE(restored.pianoRoll.verticalScale, saved.pianoRoll.verticalScale);
    auto *piano = bottom->findChild<PianoRollGraphicsView *>();
    QVERIFY(piano);
    const auto tickPerPixel = qAbs(piano->sceneXToTick(1) - piano->sceneXToTick(0));
    QVERIFY(qAbs(restored.pianoRoll.centerTick - saved.pianoRoll.centerTick) <= tickPerPixel);
    auto invalid = saved;
    invalid.layout.bottomPanelPageId = QStringLiteral("missing-page");
    QVERIFY(!window.restoreEditorViewState(invalid));
    QCOMPARE(window.captureEditorViewState(), restored);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::detachedBottomPanelReattachesWithItsEditingContext() {
    MainWindowFixture host;
    host.show();
    if (QTest::currentTestFailed())
        return;
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    view->hide();
    auto &window = *host.window;
    window.activateWindow();
    auto *bottom = window.findChild<BottomPanelView *>();
    QVERIFY(bottom);
    auto *splitter = qobject_cast<QSplitter *>(bottom->parentWidget());
    QVERIFY(splitter);
    bottom->clipEditorView()->onActiveClipChanged(singingClip->id());
    QVERIFY(window.setPianoRollEditMode(EditorViewGlobal::DrawPitch));
    QTRY_VERIFY(window.isActiveWindow());
    QCoreApplication::processEvents();
    const auto before = context->m_coreRuntime->documentVersion();
    const auto sizes = splitter->sizes();
    clickPanelButton(*bottom, "btnPanelDetach");
    if (QTest::currentTestFailed())
        return;
    QTRY_VERIFY(bottom->isWindow() && bottom->isVisible());
    QVERIFY(!bottom->parentWidget());
    QCOMPARE(splitter->indexOf(bottom), -1);
    QVERIFY(bottom->clipEditorView()->hasActiveSingingClip());
    QCOMPARE(bottom->clipEditorView()->viewState().editMode, EditorViewGlobal::DrawPitch);
    auto *mix = bottom->findChild<MixConsoleView *>();
    QVERIFY(mix);
    clickPanelTab(*bottom, mix->tabName());
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(bottom->currentPageId(), QStringLiteral("MixConsole"));
    QCloseEvent close;
    QApplication::sendEvent(bottom, &close);
    QVERIFY(!close.isAccepted());
    QTRY_VERIFY(!bottom->isWindow() && bottom->isVisible());
    QCOMPARE(bottom->parentWidget(), splitter);
    QCOMPARE(splitter->indexOf(bottom), 1);
    QTRY_COMPARE(splitter->sizes(), sizes);
    QCOMPARE(bottom->currentPageId(), QStringLiteral("MixConsole"));
    clickPanelTab(*bottom, bottom->clipEditorView()->tabName());
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(bottom->currentPageId(), QStringLiteral("ClipEditor"));
    QVERIFY(bottom->clipEditorView()->hasActiveSingingClip());
    QCOMPARE(appStatus->activeClipId.get(), singingClip->id());
    QCOMPARE(bottom->clipEditorView()->viewState().editMode, EditorViewGlobal::DrawPitch);
    QCOMPARE(context->m_coreRuntime->documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::embeddedSettingsSuspendAndRestoreBackgroundInteraction() {
    auto &runtime = *context->m_coreRuntime;
    const auto options = runtime.settings().getSettings();
    QVERIFY(options);
    const auto restoreOptions =
        qScopeGuard([&] { runtime.settings().updateAppearance({}, options.get().appearance); });
    MainWindowFixture host;
    host.show();
    if (QTest::currentTestFailed())
        return;
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    const auto noteId = insertSelectedNote();
    QVERIFY(noteId >= 0);
    view->hide();
    auto &window = *host.window;
    window.activateWindow();
    auto *bottom = window.findChild<BottomPanelView *>();
    QVERIFY(bottom);
    bottom->clipEditorView()->onActiveClipChanged(singingClip->id());
    QVERIFY(window.focusEditorRegion(EditorViewGlobal::Region::PianoRoll));
    QCoreApplication::processEvents();
    QPointer<QWidget> previousFocus = QApplication::focusWidget();
    QVERIFY(previousFocus);
    auto *menu = window.findChild<MainMenuView *>();
    QVERIFY(menu);
    QAction *undo = nullptr;
    for (auto *entry : menu->actions()) {
        if (auto *submenu = entry->menu()) {
            for (auto *action : submenu->actions()) {
                if (action->shortcut() == QKeySequence("Ctrl+Z"))
                    undo = action;
            }
        }
    }
    QVERIFY(undo);
    QVERIFY(undo->isEnabled());
    QSignalSpy undoRequested(undo, &QAction::triggered);
    const auto before = runtime.documentVersion();
    const auto *historyEntry = historyManager->nextUndoEntry();
    QVERIFY(historyEntry);
    openAppearanceFromMenu(window);
    if (QTest::currentTestFailed())
        return;
    auto *modal = window.findChild<EmbeddedModalHost *>();
    auto *panel = window.findChild<AppOptionsDialog *>();
    QVERIFY(modal);
    QVERIFY(panel);
    QTRY_VERIFY(modal->isOpen() && panel->isVisible());
    QVERIFY(!window.focusEditorRegion(EditorViewGlobal::Region::TrackPanel));
    auto *animation = panel->findChild<SwitchButton *>("appearanceAnimationEnabled");
    QVERIFY(animation);
    QTRY_VERIFY(animation->isVisible());
    QTest::mouseClick(animation, Qt::LeftButton);
    QCOMPARE(animation->value(), !options.get().appearance.animationEnabled);
    QCOMPARE(appOptions->appearance()->animationEnabled, animation->value());
    QTest::keySequence(panel, undo->shortcut());
    QVERIFY(undoRequested.isEmpty());
    QVERIFY(singingClip->findNoteById(noteId));
    QCOMPARE(historyManager->nextUndoEntry(), historyEntry);
    QTest::keyClick(panel, Qt::Key_Escape);
    QTRY_VERIFY(!modal->isOpen());
    QTRY_COMPARE(QApplication::focusWidget(), previousFocus.data());
    openAppearanceFromMenu(window);
    if (QTest::currentTestFailed())
        return;
    QTRY_VERIFY(modal->isOpen() && panel->isVisible());
    QCOMPARE(animation->value(), !options.get().appearance.animationEnabled);
    auto *frame = modal->findChild<QWidget *>("EmbeddedModalPanel");
    QVERIFY(frame);
    QVERIFY(!frame->geometry().contains(QPoint(2, 2)));
    QTest::mouseClick(modal, Qt::LeftButton, Qt::NoModifier, QPoint(2, 2));
    QTRY_VERIFY(!modal->isOpen());
    QTRY_COMPARE(QApplication::focusWidget(), previousFocus.data());
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(historyManager->nextUndoEntry(), historyEntry);
    QTest::keySequence(previousFocus.data(), undo->shortcut());
    QCOMPARE(undoRequested.count(), 1);
    QVERIFY(!singingClip->findNoteById(noteId));
}
