#include "tst_application_gui.h"
#include "../TestSupport/WaveFixture.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Controller/TrackController.h"
#include "Controller/ClipController.h"
#include "Controller/UndoRedoController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Import/DocumentImportController.h"
#include "UI/Dialogs/Base/MessageDialog.h"
#include "UI/Dialogs/Options/AppOptionsDialog.h"
#include "UI/Dialogs/Note/QuantizeDialog.h"
#include "UI/Views/BottomPanelView.h"
#include "UI/Views/ClipEditor/ClipEditorView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollCoord.h"
#include "UI/Views/Common/TabPanelTitleBar.h"
#include "UI/Views/MainTitleBar/MainMenuView.h"
#include "UI/Views/MixConsole/MixConsoleView.h"
#include "UI/Views/TrackEditor/GraphicsItem/AbstractClipView.h"
#include "UI/Views/TrackEditor/TrackEditorView.h"
#include "UI/Views/TrackEditor/TrackListView.h"
#include "UI/Views/TrackEditor/TrackControlView.h"
#include "UI/Views/TrackEditor/TracksGraphicsView.h"
#include "UI/Window/EmbeddedModalHost.h"
#include "UI/Window/MainWindow.h"

#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/AccentButton.h>
#include <lite/GUI/Controls/SwitchButton.h>
#include <lite/GUI/Controls/Toast.h>
#include <lite/History/HistoryManager.h>
#include <lite/History/ActionSequence.h>
#include <lite/ProjectConverters/DspxProjectConverter.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/Tasking/TaskManager.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QClipboard>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSplitter>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest/QTest>

namespace {
    void createDroppedProject(const QString &path) {
        AppModel source;
        auto *track = new Track;
        track->setName(QStringLiteral("Dropped track"));
        auto *clip = new SingingClip;
        clip->setLength(1920);
        clip->setClipLen(1920);
        clip->setDefaultLanguage(QStringLiteral("eng"));
        auto *note = new Note(clip);
        note->setLocalStart(0);
        note->setLength(480);
        note->setKeyIndex(64);
        note->setLyric(QStringLiteral("la"));
        clip->insertNote(note);
        track->insertClip(clip);
        QVERIFY(source.appendTrack(track));
        DspxProjectConverter converter;
        QString error;
        QVERIFY2(converter.save(path, &source, error), qPrintable(error));
    }

    void dropFiles(MainWindow &window, const QList<QUrl> &urls) {
        QMimeData mime;
        mime.setUrls(urls);
        const QPoint position(20, 20);
        QDragEnterEvent enter(position, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&window, &enter);
        QVERIFY(enter.isAccepted());
        QDropEvent drop(position, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&window, &drop);
    }

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

    void clickMainMenuAction(MainWindow &window, const char *text) {
        auto *bar = window.findChild<MainMenuView *>();
        QVERIFY(bar);
        QMenu *owner = nullptr;
        QAction *choice = nullptr;
        for (auto *menuAction : bar->actions()) {
            if (auto *menu = menuAction->menu()) {
                for (auto *action : menu->actions()) {
                    if (action->text() ==
                        QCoreApplication::translate("MainMenuViewPrivate", text)) {
                        owner = menu;
                        choice = action;
                    }
                }
            }
        }
        QVERIFY(owner);
        QVERIFY(choice);
        QVERIFY(choice->isEnabled());
        QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier,
                          bar->actionGeometry(owner->menuAction()).center());
        QTRY_VERIFY(owner->isVisible());
        const auto closeMenu = qScopeGuard([&] { owner->close(); });
        QTest::mouseClick(owner, Qt::LeftButton, Qt::NoModifier,
                          owner->actionGeometry(choice).center());
        QTRY_VERIFY(!owner->isVisible());
    }

    void openAppearanceFromMenu(MainWindow &window) {
        clickMainMenuAction(window, "A&ppearance...");
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

void ApplicationGuiTests::mainMenuQuantizationUsesTheChosenScopeAndOptions_data() {
    QTest::addColumn<bool>("selectFirst");
    QTest::addColumn<bool>("quantizeStart");
    QTest::addColumn<bool>("accept");
    QTest::newRow("selected-starts") << true << true << true;
    QTest::newRow("all-lengths") << false << false << true;
    QTest::newRow("cancel") << true << true << false;
}

void ApplicationGuiTests::mainMenuQuantizationUsesTheChosenScopeAndOptions() {
    QFETCH(bool, selectFirst);
    QFETCH(bool, quantizeStart);
    QFETCH(bool, accept);
    MainWindowFixture host;
    host.show();
    if (QTest::currentTestFailed())
        return;
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    view->hide();
    Automation::NoteDraftDto first;
    first.localStart = 73;
    first.length = 170;
    first.keyIndex = 60;
    first.lyric = QStringLiteral("first");
    first.language = QStringLiteral("eng");
    auto second = first;
    second.localStart = 650;
    second.length = 190;
    second.keyIndex = 64;
    second.lyric = QStringLiteral("second");
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.notes().insertNotes(commandContext(), Automation::ClipId(singingClip->id()),
                                        {first, second}));
    const auto notes = singingClip->notes().toList();
    QCOMPARE(notes.size(), 2);
    auto &window = *host.window;
    window.activateWindow();
    QTRY_VERIFY(window.isActiveWindow());
    QVERIFY(window.showBottomPanelPage(QStringLiteral("ClipEditor")));
    auto *editor = window.findChild<ClipEditorView *>();
    QVERIFY(editor);
    editor->onActiveClipChanged(singingClip->id());
    QTRY_VERIFY(editor->hasActiveSingingClip() && editor->isVisible());
    QVERIFY(window.setPianoRollScale(1.0, 1.0));
    QVERIFY(window.centerPianoRollAt(1920, 62));
    QVERIFY(window.focusEditorRegion(EditorViewGlobal::Region::PianoRoll));
    auto *canvas = editor->findChild<PianoRollGraphicsView *>();
    QVERIFY(canvas);
    QTRY_VERIFY(canvas->isVisible());
    canvas->setEditMode(ClipEditorGlobal::Select);
    if (selectFirst) {
        const auto position = canvas->mapFromScene(QPointF(
            canvas->tickToSceneX(150), PianoRollCoord::keyIndexToCenterY(
                                           60, ClipEditorGlobal::noteHeight * canvas->scaleY())));
        QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, position);
    }
    QCOMPARE(appStatus->selectedNotes.get(),
             selectFirst ? QList<int>{notes.first()->id()} : QList<int>{});
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    historyManager->reset();
    const auto before = runtime.documentVersion();
    bool answered = false;
    QTimer answer;
    answer.setInterval(10);
    connect(&answer, &QTimer::timeout, &window, [&] {
        auto *dialog = qobject_cast<QuantizeDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        answer.stop();
        const auto close = qScopeGuard([&] {
            if (dialog->isVisible())
                dialog->reject();
        });
        auto *grid = dialog->findChild<QComboBox *>();
        QVERIFY(grid);
        QCOMPARE(grid->currentText(), QStringLiteral("1/16"));
        const auto eighth = grid->findText(QStringLiteral("1/8"));
        QVERIFY(eighth >= 0);
        grid->setFocus();
        QTest::keyClick(grid, Qt::Key_Home);
        for (int index = 0; index < eighth; ++index)
            QTest::keyClick(grid, Qt::Key_Down);
        QCOMPARE(grid->currentText(), QStringLiteral("1/8"));
        QCheckBox *start = nullptr;
        QCheckBox *length = nullptr;
        for (auto *box : dialog->findChildren<QCheckBox *>()) {
            if (box->text() == QuantizeDialog::tr("Quantize start position"))
                start = box;
            if (box->text() == QuantizeDialog::tr("Quantize length"))
                length = box;
        }
        QVERIFY(start && length);
        QVERIFY(start->isChecked() && length->isChecked());
        auto *unchecked = quantizeStart ? length : start;
        unchecked->setFocus();
        QTest::keyClick(unchecked, Qt::Key_Space);
        QCOMPARE(start->isChecked(), quantizeStart);
        QCOMPARE(length->isChecked(), !quantizeStart);
        QCOMPARE(runtime.documentVersion(), before);
        answered = true;
        QTest::mouseClick(accept ? static_cast<QWidget *>(dialog->okButton())
                                 : static_cast<QWidget *>(dialog->cancelButton()),
                          Qt::LeftButton);
    });
    answer.start();
    clickMainMenuAction(window, "Quantize...");
    answer.stop();
    if (QTest::currentTestFailed())
        return;
    QVERIFY(answered);
    if (accept) {
        QCOMPARE(notes.first()->localStart(), quantizeStart ? 0 : first.localStart);
        QCOMPARE(notes.first()->length(), quantizeStart ? first.length : 240);
        QCOMPARE(notes.last()->localStart(), second.localStart);
        QCOMPARE(notes.last()->length(), selectFirst ? second.length : 240);
        QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
        QVERIFY(runtime.history().undo(commandContext()));
    } else {
        QCOMPARE(runtime.documentVersion(), before);
    }
    QCOMPARE(notes.first()->localStart(), first.localStart);
    QCOMPARE(notes.first()->length(), first.length);
    QCOMPARE(notes.last()->localStart(), second.localStart);
    QCOMPARE(notes.last()->length(), second.length);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::mainMenuOctaveEditsFollowThePianoSelection() {
    MainWindowFixture host;
    host.show();
    if (QTest::currentTestFailed())
        return;
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    view->hide();
    Automation::NoteDraftDto first;
    first.localStart = 480;
    first.length = 480;
    first.keyIndex = 60;
    first.lyric = QStringLiteral("first");
    auto second = first;
    second.localStart = 1200;
    second.keyIndex = 64;
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.notes().insertNotes(commandContext(), Automation::ClipId(singingClip->id()),
                                        {first, second}));
    const auto notes = singingClip->notes().toList();
    QCOMPARE(notes.size(), 2);
    auto &window = *host.window;
    window.activateWindow();
    QTRY_VERIFY(window.isActiveWindow());
    QVERIFY(window.showBottomPanelPage(QStringLiteral("ClipEditor")));
    auto *editor = window.findChild<ClipEditorView *>();
    QVERIFY(editor);
    editor->onActiveClipChanged(singingClip->id());
    QTRY_VERIFY(editor->hasActiveSingingClip() && editor->isVisible());
    QVERIFY(window.focusEditorRegion(EditorViewGlobal::Region::PianoRoll));
    QCoreApplication::processEvents();
    historyManager->reset();
    const auto before = runtime.documentVersion();
    clickMainMenuAction(window, "Select &all");
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(appStatus->selectedNotes.get().size(), 2);
    QVERIFY(appStatus->selectedNotes.get().contains(notes.first()->id()));
    QVERIFY(appStatus->selectedNotes.get().contains(notes.last()->id()));
    QCOMPARE(runtime.documentVersion(), before);
    clickMainMenuAction(window, "Move an octave up");
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(notes.first()->keyIndex(), 72);
    QCOMPARE(notes.last()->keyIndex(), 76);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    clickMainMenuAction(window, "Move an octave down");
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(notes.first()->keyIndex(), first.keyIndex);
    QCOMPARE(notes.last()->keyIndex(), second.keyIndex);
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(notes.first()->keyIndex(), 72);
    QCOMPARE(notes.last()->keyIndex(), 76);
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(notes.first()->keyIndex(), first.keyIndex);
    QCOMPARE(notes.last()->keyIndex(), second.keyIndex);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::undoShortcutRevealsTheTrackEditBeforeChangingIt_data() {
    QTest::addColumn<bool>("hiddenPanel");
    QTest::newRow("offscreen-clip") << false;
    QTest::newRow("hidden-track-panel") << true;
}

void ApplicationGuiTests::undoShortcutRevealsTheTrackEditBeforeChangingIt() {
    QFETCH(bool, hiddenPanel);
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
    QTRY_VERIFY(window.isActiveWindow());
    auto *tracks = window.findChild<TrackEditorView *>();
    auto *canvas = window.findChild<TracksGraphicsView *>();
    auto *editor = window.findChild<ClipEditorView *>();
    QVERIFY(tracks && canvas && editor);
    canvas->setAnimationEnabled(false);
    editor->onActiveClipChanged(singingClip->id());
    QVERIFY(window.showBottomPanelPage(QStringLiteral("ClipEditor")));
    QTRY_VERIFY(editor->hasActiveSingingClip() && editor->isVisible());
    QVERIFY(window.setTrackPanelScale(1.0, 1.0));
    QVERIFY(window.centerTrackPanelAt(1920, 0));
    QVERIFY(window.focusEditorRegion(EditorViewGlobal::Region::PianoRoll));
    historyManager->reset();
    auto &runtime = *context->m_coreRuntime;
    const auto clipId = Automation::ClipId(singingClip->id());
    QVERIFY(runtime.project().moveClips(commandContext(), {
                                                              {clipId, trackId, 24000}
    }));
    const auto *entry = historyManager->nextUndoEntry();
    QVERIFY(entry && entry->focusTransition());
    const auto focus = *entry->focusTransition();
    QVERIFY(window.centerTrackPanelAt(1920, 0));
    if (hiddenPanel)
        QVERIFY(window.setEditorPanelVisibility(false, true));
    QTRY_COMPARE(window.focusVisibility(focus.after),
                 hiddenPanel ? HistoryFocusVisibility::ContextSwitchRequired
                             : HistoryFocusVisibility::ScrollRequired);
    const auto beforeUndo = runtime.documentVersion();
    QVERIFY(runtime.windowId());
    auto *menuBar = window.findChild<MainMenuView *>();
    QVERIFY(menuBar);
    QAction *undoAction = nullptr;
    for (auto *menuAction : menuBar->actions()) {
        if (auto *menu = menuAction->menu()) {
            for (auto *action : menu->actions()) {
                if (action->shortcut() == QKeySequence(QStringLiteral("Ctrl+Z")))
                    undoAction = action;
            }
        }
    }
    QVERIFY(undoAction && undoAction->isEnabled());
    QSignalSpy undoTriggered(undoAction, &QAction::triggered);
    QSignalSpy navigation(undoRedoController, &UndoRedoController::focusNavigationRequested);
    auto *input = QApplication::focusWidget();
    QVERIFY(input);
    QTest::keySequence(input, QKeySequence(QStringLiteral("Ctrl+Z")));
    QCOMPARE(undoTriggered.size(), 1);
    QCOMPARE(singingClip->start(), 24000);
    QTRY_COMPARE(navigation.size(), 1);
    QVERIFY(!appStatus->trackPanelCollapsed);
    QCOMPARE(singingClip->start(), 24000);
    QCOMPARE(runtime.documentVersion(), beforeUndo);
    QCOMPARE(historyManager->nextUndoEntry(), entry);
    QTRY_COMPARE(window.focusVisibility(focus.after), HistoryFocusVisibility::Visible);
    auto *item = tracks->findClipItemById(singingClip->id());
    QVERIFY(item);
    QVERIFY(canvas->logicalVisibleRect().contains(item->mapRectToScene(item->rect())));
    input = QApplication::focusWidget();
    QVERIFY(input && (input == tracks || tracks->isAncestorOf(input)));
    QTest::keySequence(input, QKeySequence(QStringLiteral("Ctrl+Z")));
    QTRY_COMPARE(singingClip->start(), 0);
    QCOMPARE(navigation.size(), 1);
    QCOMPARE(runtime.documentVersion().revision, beforeUndo.revision + 1);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(historyManager->canRedo());
    QTRY_COMPARE(window.focusVisibility(focus.before), HistoryFocusVisibility::Visible);
    input = QApplication::focusWidget();
    QVERIFY(input);
    QTest::keySequence(input, QKeySequence(QStringLiteral("Ctrl+Y")));
    QTRY_COMPARE(singingClip->start(), 24000);
    QCOMPARE(navigation.size(), 1);
    QTRY_COMPARE(window.focusVisibility(focus.after), HistoryFocusVisibility::Visible);
    QVERIFY(historyManager->canUndo());
    QVERIFY(!historyManager->canRedo());
}

void ApplicationGuiTests::undoShortcutRevealsThePianoEditBeforeChangingIt() {
    MainWindowFixture host;
    host.show();
    if (QTest::currentTestFailed())
        return;
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    view->hide();
    auto &runtime = *context->m_coreRuntime;
    Automation::NoteDraftDto draft;
    draft.localStart = 480;
    draft.length = 240;
    draft.keyIndex = 60;
    draft.lyric = QStringLiteral("la");
    const auto clipId = Automation::ClipId(singingClip->id());
    QVERIFY(runtime.notes().insertNotes(commandContext(), clipId, {draft}));
    QCOMPARE(singingClip->notes().count(), 1);
    auto *note = *singingClip->notes().begin();
    auto &window = *host.window;
    window.activateWindow();
    QTRY_VERIFY(window.isActiveWindow());
    auto *editor = window.findChild<ClipEditorView *>();
    QVERIFY(editor);
    editor->onActiveClipChanged(singingClip->id());
    QVERIFY(window.showBottomPanelPage(QStringLiteral("ClipEditor")));
    QTRY_VERIFY(editor->hasActiveSingingClip() && editor->isVisible());
    QVERIFY(window.setPianoRollScale(1.0, 1.0));
    QVERIFY(window.centerPianoRollAt(1920, 60));
    QVERIFY(window.focusEditorRegion(EditorViewGlobal::Region::PianoRoll));
    QTRY_VERIFY_WITH_TIMEOUT(taskManager->tasks().isEmpty(), 10000);
    historyManager->reset();
    QVERIFY(runtime.notes().moveNotes(commandContext(), clipId, {Automation::NoteId(note->id())}, 0,
                                      67));
    const auto *entry = historyManager->nextUndoEntry();
    QVERIFY(entry && entry->focusTransition());
    const auto focus = *entry->focusTransition();
    QVERIFY(window.centerPianoRollAt(1920, 60));
    QTRY_COMPARE(window.focusVisibility(focus.after), HistoryFocusVisibility::ScrollRequired);
    const auto beforeUndo = runtime.documentVersion();
    QSignalSpy navigation(undoRedoController, &UndoRedoController::focusNavigationRequested);
    auto *input = QApplication::focusWidget();
    QVERIFY(input);
    QTest::keySequence(input, QKeySequence(QStringLiteral("Ctrl+Z")));
    QCOMPARE(note->keyIndex(), 127);
    QTRY_COMPARE(navigation.size(), 1);
    QCOMPARE(runtime.documentVersion(), beforeUndo);
    QCOMPARE(historyManager->nextUndoEntry(), entry);
    QTRY_COMPARE(window.focusVisibility(focus.after), HistoryFocusVisibility::Visible);
    input = QApplication::focusWidget();
    QVERIFY(input && editor->isAncestorOf(input));
    QTest::keySequence(input, QKeySequence(QStringLiteral("Ctrl+Z")));
    QTRY_COMPARE(note->keyIndex(), 60);
    QCOMPARE(navigation.size(), 1);
    QVERIFY(!historyManager->canUndo());
    QTRY_COMPARE(window.focusVisibility(focus.before), HistoryFocusVisibility::Visible);
    input = QApplication::focusWidget();
    QVERIFY(input);
    QTest::keySequence(input, QKeySequence(QStringLiteral("Ctrl+Y")));
    QTRY_COMPARE(note->keyIndex(), 127);
    QCOMPARE(navigation.size(), 1);
    QTRY_COMPARE(window.focusVisibility(focus.after), HistoryFocusVisibility::Visible);
    QVERIFY(!historyManager->canRedo());
}

void ApplicationGuiTests::fileMenuOpensAndSavesThroughTheActualPicker() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto sourcePath = directory.filePath(QStringLiteral("打开工程.dspx"));
    const auto savedPath = directory.filePath(QStringLiteral("保存工程.dspx"));
    createDroppedProject(sourcePath);
    if (QTest::currentTestFailed())
        return;
    MainWindowFixture host;
    host.show();
    if (QTest::currentTestFailed())
        return;
    auto &window = *host.window;
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.documents().commitNewDocument(
        commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false)));
    QTRY_VERIFY(!documentWorkflowController->busy());
    const auto nativeDialogsDisabled = QApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    const auto restoreDialogs = qScopeGuard(
        [&] { QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, nativeDialogsDisabled); });
    const auto chooseFile = [&](const char *action, const QString &path, bool save, bool accept) {
        const auto before = runtime.documentVersion();
        const auto beforeModel = context->m_appModel->serialize();
        bool chosen = false;
        QTimer answer;
        answer.setInterval(10);
        connect(&answer, &QTimer::timeout, &window, [&] {
            QPointer<QFileDialog> picker =
                qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
            if (!picker)
                return;
            answer.stop();
            const auto close = qScopeGuard([&] {
                if (picker && picker->isVisible())
                    picker->reject();
            });
            QCOMPARE(picker->acceptMode(),
                     save ? QFileDialog::AcceptSave : QFileDialog::AcceptOpen);
            auto *name = picker->findChild<QLineEdit *>(QStringLiteral("fileNameEdit"));
            QVERIFY(name);
            QTest::mouseClick(name, Qt::LeftButton);
            QTRY_VERIFY(name->hasFocus());
            QTest::keySequence(name, QKeySequence::SelectAll);
            QApplication::clipboard()->setText(QDir::toNativeSeparators(path));
            QTest::keySequence(name, QKeySequence::Paste);
            QCOMPARE(runtime.documentVersion(), before);
            QCOMPARE(context->m_appModel->serialize(), beforeModel);
            if (accept) {
                auto *buttons = picker->findChild<QDialogButtonBox *>();
                QVERIFY(buttons);
                auto *button =
                    buttons->button(save ? QDialogButtonBox::Save : QDialogButtonBox::Open);
                QVERIFY(button && button->isEnabled());
                QTest::mouseClick(button, Qt::LeftButton);
            } else {
                QTest::keyClick(name, Qt::Key_Escape);
            }
            chosen = true;
        });
        answer.start();
        clickMainMenuAction(window, action);
        QTRY_VERIFY_WITH_TIMEOUT(chosen, 10000);
        answer.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!documentWorkflowController->busy(), 10000);
    };
    const auto initial = runtime.documentVersion();
    chooseFile("&Open...", sourcePath, false, false);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(runtime.documentVersion(), initial);
    QVERIFY(documentWorkflowController->projectPath().isEmpty());
    chooseFile("&Open...", sourcePath, false, true);
    if (QTest::currentTestFailed())
        return;
    QTRY_COMPARE(QFileInfo(documentWorkflowController->projectPath()).canonicalFilePath(),
                 QFileInfo(sourcePath).canonicalFilePath());
    QVERIFY(runtime.documentVersion().documentId != initial.documentId);
    QCOMPARE(context->m_appModel->tracks().size(), 1);
    auto *track = context->m_appModel->tracks().first();
    QCOMPARE(track->name(), QStringLiteral("Dropped track"));
    QVERIFY(historyManager->isOnSavePoint());
    QVERIFY(documentWorkflowController->recentProjectFiles().contains(
        documentWorkflowController->projectPath()));
    const auto document = runtime.documentVersion().documentId;
    QVERIFY(runtime.project().renameTrack(commandContext(), Automation::TrackId(track->id()),
                                          QStringLiteral("Saved from the menu")));
    const auto *edit = historyManager->nextUndoEntry();
    QVERIFY(edit);
    chooseFile("Save &as...", savedPath, true, false);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(!QFileInfo::exists(savedPath));
    QCOMPARE(QFileInfo(documentWorkflowController->projectPath()).canonicalFilePath(),
             QFileInfo(sourcePath).canonicalFilePath());
    QCOMPARE(historyManager->nextUndoEntry(), edit);
    QVERIFY(!historyManager->isOnSavePoint());
    chooseFile("Save &as...", savedPath, true, true);
    if (QTest::currentTestFailed())
        return;
    QTRY_VERIFY(QFileInfo(savedPath).isFile());
    QTRY_COMPARE(QFileInfo(documentWorkflowController->projectPath()).canonicalFilePath(),
                 QFileInfo(savedPath).canonicalFilePath());
    QCOMPARE(runtime.documentVersion().documentId, document);
    QVERIFY(historyManager->isOnSavePoint());
    QCOMPARE(historyManager->nextUndoEntry(), edit);
    QVERIFY(documentWorkflowController->recentProjectFiles().contains(
        documentWorkflowController->projectPath()));
    QVERIFY(runtime.project().renameTrack(commandContext(), Automation::TrackId(track->id()),
                                          QStringLiteral("Saved again")));
    clickMainMenuAction(window, "&Save");
    if (QTest::currentTestFailed())
        return;
    QTRY_VERIFY(historyManager->isOnSavePoint());
    QVERIFY(!QApplication::activeModalWidget());
    DspxProjectConverter converter;
    AppModel loaded;
    QString error;
    QVERIFY2(converter.load(savedPath, &loaded, error, ImportMode::NewProject), qPrintable(error));
    QCOMPARE(loaded.tracks().size(), 1);
    QCOMPARE(loaded.tracks().first()->name(), QStringLiteral("Saved again"));
    AppModel original;
    QVERIFY2(converter.load(sourcePath, &original, error, ImportMode::NewProject),
             qPrintable(error));
    QCOMPARE(original.tracks().first()->name(), QStringLiteral("Dropped track"));
    QCOMPARE(runtime.documentVersion().documentId, document);
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
    QTRY_VERIFY(window.isActiveWindow());
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

void ApplicationGuiTests::projectDropCanCancelThenOpenTheDocument() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("拖入工程.dspx"));
    createDroppedProject(path);
    if (QTest::currentTestFailed())
        return;
    MainWindowFixture host;
    host.show();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    Automation::TrackDraftDto unsaved;
    unsaved.name = QStringLiteral("Unsaved track");
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, unsaved));
    QVERIFY(!historyManager->isOnSavePoint());
    const auto before = runtime.documentVersion();
    const auto *undo = historyManager->nextUndoEntry();
    for (bool discard : {false, true}) {
        bool answered = false;
        QTimer answer;
        answer.setInterval(10);
        connect(&answer, &QTimer::timeout, host.window.get(), [&] {
            QPointer<QDialog> dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            answer.stop();
            const auto closeOnFailure = qScopeGuard([&] {
                if (dialog && !answered)
                    dialog->reject();
            });
            const auto text = discard ? MainWindow::tr("Don't save") : MainWindow::tr("Cancel");
            Button *choice = nullptr;
            for (auto *button : dialog->findChildren<Button *>()) {
                if (button->text() == text)
                    choice = button;
            }
            QVERIFY(choice);
            QTest::mouseClick(choice, Qt::LeftButton);
            answered = true;
        });
        answer.start();
        dropFiles(*host.window, {QUrl::fromLocalFile(path)});
        if (QTest::currentTestFailed())
            return;
        QTRY_VERIFY(answered && !documentWorkflowController->busy());
        answer.stop();
        if (!discard) {
            QCOMPARE(runtime.documentVersion(), before);
            QCOMPARE(historyManager->nextUndoEntry(), undo);
            QCOMPARE(context->m_appModel->tracks().first()->name(), unsaved.name);
            continue;
        }
        QVERIFY(runtime.documentVersion().documentId != before.documentId);
        QCOMPARE(QFileInfo(documentWorkflowController->projectPath()).canonicalFilePath(),
                 QFileInfo(path).canonicalFilePath());
        QCOMPARE(context->m_appModel->tracks().size(), 1);
        QCOMPARE(context->m_appModel->tracks().first()->name(), QStringLiteral("Dropped track"));
        auto *list = host.window->findChild<TrackListView *>();
        QVERIFY(list);
        QTRY_COMPARE(list->trackCount(), 1);
        auto *control = qobject_cast<TrackControlView *>(list->itemWidget(list->item(0)));
        QVERIFY(control);
        QCOMPARE(control->name(), QStringLiteral("Dropped track"));
        QTRY_VERIFY(host.window->windowTitle().contains(QFileInfo(path).completeBaseName()));
        QVERIFY(historyManager->isOnSavePoint());
        QVERIFY(!historyManager->canUndo());
    }
}

void ApplicationGuiTests::mixedFileDropRejectsAtomicallyAndAllowsTheNextImport() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto projectPath = directory.filePath(QStringLiteral("project.dspx"));
    const auto audioPath = directory.filePath(QStringLiteral("导入.wav"));
    createDroppedProject(projectPath);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(TestSupport::writeWave(audioPath, QVector<float>(4800, 0.2f)));
    MainWindowFixture host;
    host.show();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.playback().setPosition(commandContext(), 7200));
    const auto releaseAudio = qScopeGuard([&] {
        const auto reset = runtime.documents().commitNewDocument(
            commandContext(), Automation::DocumentAutomationFacade::newDocumentDraft(false));
        QVERIFY(reset);
        QTRY_VERIFY(taskManager->tasks().isEmpty());
    });
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto tracksBefore = context->m_appModel->tracks();
    QMimeData unsupported;
    unsupported.setUrls({QUrl(QStringLiteral("https://example.invalid/project.dspx")),
                         QUrl::fromLocalFile(directory.filePath(QStringLiteral("notes.txt")))});
    QDragEnterEvent rejected(QPoint(20, 20), Qt::CopyAction, &unsupported, Qt::LeftButton,
                             Qt::NoModifier);
    QApplication::sendEvent(host.window.get(), &rejected);
    QVERIFY(!rejected.isAccepted());

    bool errorShown = false;
    QTimer acknowledge;
    acknowledge.setInterval(10);
    connect(&acknowledge, &QTimer::timeout, host.window.get(), [&] {
        QPointer<QDialog> dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        acknowledge.stop();
        const auto closeOnFailure = qScopeGuard([&] {
            if (dialog && !errorShown)
                dialog->reject();
        });
        QCOMPARE(dialog->windowTitle(), DocumentImportController::tr("Import"));
        Button *close = nullptr;
        for (auto *button : dialog->findChildren<Button *>()) {
            if (button->text() == DocumentImportController::tr("Close"))
                close = button;
        }
        QVERIFY(close);
        QTest::mouseClick(close, Qt::LeftButton);
        errorShown = true;
    });
    acknowledge.start();
    dropFiles(*host.window, {QUrl::fromLocalFile(projectPath), QUrl::fromLocalFile(audioPath)});
    if (QTest::currentTestFailed())
        return;
    QTRY_VERIFY(errorShown);
    acknowledge.stop();
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(context->m_appModel->tracks(), tracksBefore);
    QVERIFY(!historyManager->canUndo());

    dropFiles(*host.window, {QUrl::fromLocalFile(audioPath)});
    if (QTest::currentTestFailed())
        return;
    QTRY_COMPARE(context->m_appModel->tracks().size(), tracksBefore.size() + 1);
    QTRY_VERIFY(taskManager->tasks().isEmpty());
    const auto *importedTrack = context->m_appModel->tracks().last();
    QCOMPARE(importedTrack->clips().count(), 1);
    const auto *audio = dynamic_cast<const AudioClip *>(*importedTrack->clips().begin());
    QVERIFY(audio);
    QCOMPARE(QFileInfo(audio->path()).canonicalFilePath(),
             QFileInfo(audioPath).canonicalFilePath());
    QCOMPARE(audio->start() + audio->clipStart(), 7200);
    QCOMPARE(audio->audioInfo().frames, 4800);
    QCOMPARE(audio->playLengthMs(), 100.0);
    auto *list = host.window->findChild<TrackListView *>();
    QVERIFY(list);
    QTRY_COMPARE(list->trackCount(), tracksBefore.size() + 1);
    auto *control =
        qobject_cast<TrackControlView *>(list->itemWidget(list->item(list->trackCount() - 1)));
    QVERIFY(control);
    QCOMPARE(control->name(), QFileInfo(audioPath).baseName());
    QVERIFY(runtime.documentVersion().revision > before.revision);
    historyManager->undo();
    QCOMPARE(context->m_appModel->tracks(), tracksBefore);
    QTRY_COMPARE(list->trackCount(), tracksBefore.size());
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
    QTRY_VERIFY(window.isActiveWindow());
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
