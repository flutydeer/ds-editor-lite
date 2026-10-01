#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Views/ClipEditor/PianoRoll/NoteView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsScene.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"
#include "UI/Views/Common/EditorPointerUtils.h"
#include "UI/Views/Common/EditorTouchGesture.h"

#include <lite/History/HistoryManager.h>
#include <lite/GUI/Controls/InlineTextEditOverlay.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <QApplication>
#include <QContextMenuEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QPointer>
#include <QScopeGuard>
#include <QTabletEvent>
#include <QTouchEvent>
#include <QtTest/QTest>

namespace {
    void deactivate(QWidget &widget) {
        QEvent event(QEvent::WindowDeactivate);
        QApplication::sendEvent(&widget, &event);
    }

    bool sendTablet(QWidget &viewport, const QPointingDevice &device, QEvent::Type type,
                    const QPoint &position, qreal pressure, Qt::MouseButton button,
                    Qt::MouseButtons buttons) {
        QTabletEvent event(type, &device, position, viewport.mapToGlobal(position), pressure, 0, 0,
                           0, 0, 0, Qt::NoModifier, button, buttons);
        event.setAccepted(false);
        QApplication::sendEvent(&viewport, &event);
        return event.isAccepted();
    }
}

void ApplicationGuiTests::pianoTouchInlineLyricsKeepsEditingAndRecovers_data() {
    QTest::addColumn<QString>("finish");
    QTest::newRow("text-drag-release") << QStringLiteral("release");
    QTest::newRow("text-drag-platform-cancel") << QStringLiteral("cancel");
    QTest::newRow("submit-before-lift") << QStringLiteral("submit");
    QTest::newRow("text-menu-on-lift") << QStringLiteral("menu");
}

void ApplicationGuiTests::pianoTouchInlineLyricsKeepsEditingAndRecovers() {
    QFETCH(QString, finish);
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    const auto id = insertSelectedNote();
    QVERIFY(id >= 0);
    view->setEditMode(ClipEditorGlobal::Select);
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto original = TestSupport::projectSnapshot(*context->m_appModel);
    auto *overlay = view->findChild<InlineTextEditOverlay *>();
    QVERIFY(overlay);
    auto *device = QTest::createTouchDevice();
    auto sequence = QTest::touchEvent(view->viewport(), device, false);
    const auto cleanup = qScopeGuard([&] {
        if (auto *popup = QApplication::activePopupWidget())
            popup->close();
        if (overlay->isEditing())
            QTest::keyClick(overlay->findChild<QLineEdit *>(), Qt::Key_Escape);
        deactivate(*view);
    });
    const auto notePoint = pointFor(600, 62);
    for (int tap = 0; tap < 2; ++tap) {
        sequence.press(0, notePoint).commit();
        sequence.release(0, notePoint).commit();
    }
    QTRY_VERIFY(overlay->isEditing());
    auto *input = overlay->findChild<QLineEdit *>();
    QVERIFY(input);
    QTRY_VERIFY(input->hasFocus());
    QCOMPARE(runtime.documentVersion(), before);
    QTest::keySequence(input, QKeySequence::SelectAll);
    QTest::keyClicks(input, QStringLiteral("a draft line"));
    const auto left = input->mapTo(view->viewport(), QPoint(3, input->height() / 2));
    const auto right =
        input->mapTo(view->viewport(), QPoint(input->width() - 4, input->height() / 2));
    const auto viewport = view->visibleRect();
    sequence.press(0, right).commit();
    if (finish == QStringLiteral("menu")) {
        QTest::qWait(EditorTouchGesture::Config{}.longPressMs + 50);
        QVERIFY(QApplication::activePopupWidget() == nullptr);
        QCOMPARE(runtime.documentVersion(), before);
        sequence.release(0, right).commit();
        QTRY_VERIFY(qobject_cast<QMenu *>(QApplication::activePopupWidget()));
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        QVERIFY(menu->isVisible());
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(input->text(), QStringLiteral("a draft line"));
        QTest::keyClick(menu, Qt::Key_Escape);
        QTRY_VERIFY(QApplication::activePopupWidget() == nullptr);
        QTRY_VERIFY(input->hasFocus());
    } else if (finish != QStringLiteral("submit")) {
        sequence.move(0, left).commit();
        QVERIFY(input->hasSelectedText());
        QVERIFY(overlay->isEditing());
        QCOMPARE(view->visibleRect(), viewport);
        QVERIFY(appStatus->pianoRollNoteEditPreview.get().isEmpty());
        if (finish == QStringLiteral("cancel")) {
            QTouchEvent cancel(QEvent::TouchCancel, device);
            cancel.setAccepted(false);
            QApplication::sendEvent(view->viewport(), &cancel);
            QVERIFY(cancel.isAccepted());
        }
        sequence.release(0, left).commit();
    }
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), original);
    QVERIFY(!historyManager->canUndo());
    QTest::keySequence(input, QKeySequence::SelectAll);
    QTest::keyClicks(input, QStringLiteral("finger lyric"));
    QTest::keyClick(input, Qt::Key_Return);
    QVERIFY(!overlay->isEditing());
    QCOMPARE(singingClip->findNoteById(id)->lyric(), QStringLiteral("finger lyric"));
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    if (finish == QStringLiteral("submit"))
        sequence.release(0, right).commit();
    QVERIFY(!EditorPointer::isTouchStreamActive());
    const auto committed = TestSupport::projectSnapshot(*context->m_appModel);
    view->setEditMode(ClipEditorGlobal::DrawNote);
    const auto nextPress = pointFor(1530, 66);
    const auto nextEnd = pointFor(1890, 66);
    QVERIFY(view->viewport()->rect().contains(nextPress));
    QVERIFY(view->viewport()->rect().contains(nextEnd));
    sequence.press(0, nextPress).commit();
    sequence.move(0, nextEnd).commit();
    QTRY_COMPARE(appStatus->pianoRollNoteEditPreview.get().size(), 1);
    sequence.release(0, nextEnd).commit();
    QCOMPARE(singingClip->notes().count(), 2);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 2);
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), committed);
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), original);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::pianoPenClickOwnsOnlyItsContextMenu() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    const auto id = insertSelectedNote();
    QVERIFY(id >= 0);
    view->setEditMode(ClipEditorGlobal::Select);
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto original = TestSupport::projectSnapshot(*context->m_appModel);
    const auto position = pointFor(600, 62);
    const QPointingDevice pen(
        QStringLiteral("Fixture barrel pen"), 1002, QInputDevice::DeviceType::Stylus,
        QPointingDevice::PointerType::Pen,
        QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 2);
    QList<PianoRollMenuContext> menus;
    const auto connection =
        connect(view.get(), &PianoRollGraphicsView::contextMenuRequested, view.get(),
                [&](const PianoRollMenuContext &menu) { menus.append(menu); });
    const auto cleanup = qScopeGuard([&] {
        deactivate(*view);
        disconnect(connection);
    });
    QVERIFY(sendTablet(*view->viewport(), pen, QEvent::TabletPress, position, 0.7, Qt::RightButton,
                       Qt::RightButton));
    QVERIFY(menus.isEmpty());
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QVERIFY(sendTablet(*view->viewport(), pen, QEvent::TabletRelease, position, 0, Qt::RightButton,
                       Qt::NoButton));
    QTRY_COMPARE(menus.size(), 1);
    QCOMPARE(menus.first().target, PianoRollMenuContext::Target::Note);
    QCOMPARE(menus.first().noteId, id);
    QCOMPARE(menus.first().globalPos, view->viewport()->mapToGlobal(position));
    QContextMenuEvent platformCopy(QContextMenuEvent::Mouse, position,
                                   view->viewport()->mapToGlobal(position));
    platformCopy.setAccepted(false);
    QApplication::sendEvent(view->viewport(), &platformCopy);
    QVERIFY(platformCopy.isAccepted());
    QCOMPARE(menus.size(), 1);
    QContextMenuEvent keyboardMenu(QContextMenuEvent::Keyboard, position,
                                   view->viewport()->mapToGlobal(position));
    QApplication::sendEvent(view->viewport(), &keyboardMenu);
    QCOMPARE(menus.size(), 2);
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), original);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(!EditorPointer::isPenStreamActive());
    QVERIFY(!EditorPointer::isPenEraseIntentActive());
}

void ApplicationGuiTests::pianoTouchDrawingCommitsOrCancels_data() {
    QTest::addColumn<QString>("finish");
    QTest::newRow("release-commits") << QStringLiteral("release");
    QTest::newRow("platform-cancel") << QStringLiteral("cancel");
    QTest::newRow("second-finger-navigates") << QStringLiteral("navigation");
}

void ApplicationGuiTests::pianoTouchDrawingCommitsOrCancels() {
    QFETCH(QString, finish);
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    auto *device = QTest::createTouchDevice();
    auto sequence = QTest::touchEvent(view->viewport(), device, false);
    const auto cleanup = qScopeGuard([&] { deactivate(*view); });
    const auto before = runtime.documentVersion();
    const auto original = TestSupport::projectSnapshot(*context->m_appModel);
    const auto press = pointFor(510, 60);
    const auto last = pointFor(990, 60);
    QVERIFY(view->viewport()->rect().contains(press));
    QVERIFY(view->viewport()->rect().contains(last));
    sequence.press(0, press).commit();
    QVERIFY(!editSessionManager->hasActiveTransaction());
    sequence.move(0, last).commit();
    QTRY_COMPARE(appStatus->pianoRollNoteEditPreview.get().size(), 1);
    QCOMPARE(appStatus->pianoRollNoteEditPreview.get().first().rStart, 480);
    QCOMPARE(appStatus->pianoRollNoteEditPreview.get().first().length, 480);
    QCOMPARE(sceneNoteCount(-1), 1);
    QVERIFY(editSessionManager->hasActiveTransaction());
    QVERIFY(EditorPointer::isTouchStreamActive());
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), original);

    if (finish == QStringLiteral("cancel")) {
        QTouchEvent cancel(QEvent::TouchCancel, device);
        cancel.setAccepted(false);
        QApplication::sendEvent(view->viewport(), &cancel);
        QVERIFY(cancel.isAccepted());
        sequence.release(0, last).commit();
    } else if (finish == QStringLiteral("navigation")) {
        const auto other = last + QPoint(90, -90);
        QVERIFY(view->viewport()->rect().contains(other));
        sequence.stationary(0).press(1, other).commit();
        QVERIFY(appStatus->pianoRollNoteEditPreview.get().isEmpty());
        QVERIFY(!editSessionManager->hasActiveTransaction());
        QVERIFY(!EditorPointer::isTouchStreamActive());
        const auto scaleX = view->scaleX();
        const auto scaleY = view->scaleY();
        sequence.move(0, last + QPoint(-20, 20)).move(1, other + QPoint(20, -20)).commit();
        QVERIFY(view->scaleX() > scaleX);
        QVERIFY(view->scaleY() > scaleY);
        sequence.stationary(0).release(1, other + QPoint(20, -20)).commit();
        sequence.move(0, last + QPoint(-40, 20)).commit();
        QVERIFY(appStatus->pianoRollNoteEditPreview.get().isEmpty());
        sequence.release(0, last + QPoint(-40, 20)).commit();
        deactivate(*view);
    } else {
        sequence.release(0, last).commit();
    }

    QVERIFY(!EditorPointer::isTouchStreamActive());
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QVERIFY(appStatus->pianoRollNoteEditPreview.get().isEmpty());
    if (finish != QStringLiteral("release")) {
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), original);
        QVERIFY(!historyManager->canUndo());
        QCOMPARE(sceneNoteCount(-1), 0);
        QVERIFY(view->setViewportScale(1.0, 1.0));
        view->setViewportCenterAt(1920, 60, false);
        const auto retryPress = pointFor(510, 60);
        const auto retryEnd = pointFor(990, 60);
        sequence.press(0, retryPress).commit();
        sequence.move(0, retryEnd).commit();
        QTRY_COMPARE(appStatus->pianoRollNoteEditPreview.get().size(), 1);
        sequence.release(0, retryEnd).commit();
    }

    QTRY_COMPARE(singingClip->notes().count(), 1);
    const auto *note = *singingClip->notes().begin();
    const auto id = note->id();
    QCOMPARE(note->localStart(), 480);
    QCOMPARE(note->length(), 480);
    QCOMPARE(note->keyIndex(), 60);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QCOMPARE(sceneNoteCount(id), 1);
    QVERIFY(!EditorPointer::isTouchStreamActive());
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), original);
    QCOMPARE(sceneNoteCount(id), 0);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(runtime.history().redo(commandContext()));
    QCOMPARE(singingClip->findNoteById(id)->length(), 480);
    QCOMPARE(sceneNoteCount(id), 1);
}

void ApplicationGuiTests::pianoTouchSelectionAndNavigationStayIndependent() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    const auto id = insertSelectedNote();
    QVERIFY(id >= 0);
    view->setEditMode(ClipEditorGlobal::Select);
    QVERIFY(view->setViewportScale(2.0, 1.0));
    view->setViewportStartTick(0);
    view->clearNoteSelections();
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto original = TestSupport::projectSnapshot(*context->m_appModel);
    auto *device = QTest::createTouchDevice();
    auto sequence = QTest::touchEvent(view->viewport(), device, false);
    const auto cleanup = qScopeGuard([&] { deactivate(*view); });

    const auto unselected = pointFor(600, 62);
    const auto panTo = unselected + QPoint(-50, -40);
    const auto left = view->horizontalBarValue();
    const auto top = view->verticalBarValue();
    sequence.press(0, unselected).commit();
    sequence.move(0, panTo).commit();
    QVERIFY(view->horizontalBarValue() > left);
    QVERIFY(view->verticalBarValue() > top);
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), original);
    QVERIFY(view->selectedNotesId().isEmpty());
    QVERIFY(!editSessionManager->hasActiveTransaction());
    sequence.release(0, panTo).commit();
    deactivate(*view);

    const auto selected = pointFor(600, 62);
    QVERIFY(view->viewport()->rect().contains(selected));
    sequence.press(0, selected).commit();
    sequence.release(0, selected).commit();
    QCOMPARE(view->selectedNotesId(), QList<int>{id});
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    const auto destination = pointFor(1080, 64);
    sequence.press(0, selected).commit();
    sequence.move(0, destination).commit();
    QTRY_COMPARE(appStatus->pianoRollNoteEditPreview.get().size(), 1);
    QCOMPARE(appStatus->pianoRollNoteEditPreview.get().first().rStart, 960);

    for (const auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
        QMouseEvent promoted(type, destination, destination,
                             view->viewport()->mapToGlobal(destination), Qt::LeftButton,
                             type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton,
                             Qt::NoModifier, device);
        promoted.setAccepted(false);
        QApplication::sendEvent(view->viewport(), &promoted);
        QVERIFY(promoted.isAccepted());
    }
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(editSessionManager->hasActiveTransaction());
    sequence.release(0, destination).commit();
    QCOMPARE(singingClip->findNoteById(id)->localStart(), 960);
    QCOMPARE(singingClip->findNoteById(id)->keyIndex(), 64);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QVERIFY(!EditorPointer::isTouchStreamActive());
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), original);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::pianoTouchLongPressDefersMenus_data() {
    QTest::addColumn<bool>("content");
    QTest::addColumn<bool>("drag");
    QTest::newRow("note-menu-on-release") << true << false;
    QTest::newRow("blank-menu-on-release") << false << false;
    QTest::newRow("blank-held-selection") << false << true;
}

void ApplicationGuiTests::pianoTouchLongPressDefersMenus() {
    QFETCH(bool, content);
    QFETCH(bool, drag);
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    const auto id = insertSelectedNote();
    QVERIFY(id >= 0);
    view->setEditMode(ClipEditorGlobal::Select);
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto original = TestSupport::projectSnapshot(*context->m_appModel);
    QList<PianoRollMenuContext> menus;
    const auto connection =
        connect(view.get(), &PianoRollGraphicsView::contextMenuRequested, view.get(),
                [&](const PianoRollMenuContext &menu) { menus.append(menu); });
    auto *device = QTest::createTouchDevice();
    auto sequence = QTest::touchEvent(view->viewport(), device, false);
    const auto cleanup = qScopeGuard([&] {
        deactivate(*view);
        disconnect(connection);
    });
    const auto start = content ? pointFor(600, 62) : pointFor(360, 64);
    sequence.press(0, start).commit();
    QTest::qWait(EditorTouchGesture::Config{}.longPressMs + 50);
    QVERIFY(menus.isEmpty());
    QCOMPARE(view->selectedNotesId(), QList<int>{id});
    const auto end = drag ? pointFor(840, 60) : start;
    if (drag)
        sequence.move(0, end).commit();
    sequence.release(0, end).commit();
    if (drag) {
        QCoreApplication::sendPostedEvents();
        QVERIFY(menus.isEmpty());
        QCOMPARE(view->selectedNotesId(), QList<int>{id});
    } else {
        QTRY_COMPARE(menus.size(), 1);
        QCOMPARE(menus.first().target, content ? PianoRollMenuContext::Target::Note
                                               : PianoRollMenuContext::Target::Background);
        QCOMPARE(menus.first().globalPos, view->viewport()->mapToGlobal(start));
        if (content)
            QCOMPARE(menus.first().noteId, id);
    }
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), original);
    QVERIFY(!historyManager->canUndo());
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QVERIFY(!EditorPointer::isTouchStreamActive());
}

void ApplicationGuiTests::pianoPenErasingCommitsOrInterrupts_data() {
    QTest::addColumn<bool>("barrel");
    QTest::addColumn<bool>("interrupt");
    QTest::addColumn<bool>("unsupported");
    QTest::newRow("eraser-release") << false << false << false;
    QTest::newRow("barrel-drag-release") << true << false << false;
    QTest::newRow("eraser-interruption") << false << true << false;
    QTest::newRow("barrel-drag-interruption") << true << true << false;
    QTest::newRow("eraser-does-not-split") << false << false << true;
}

void ApplicationGuiTests::pianoPenErasingCommitsOrInterrupts() {
    QFETCH(bool, barrel);
    QFETCH(bool, interrupt);
    QFETCH(bool, unsupported);
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    const auto firstId = insertSelectedNote();
    QVERIFY(firstId >= 0);
    Automation::NoteDraftDto second;
    second.localStart = 960;
    second.length = 240;
    second.keyIndex = 64;
    second.lyric = QStringLiteral("world");
    second.language = QStringLiteral("eng");
    const auto inserted = runtime.notes().insertNotes(
        commandContext(), Automation::ClipId(singingClip->id()), {second});
    QVERIFY(inserted);
    const auto secondId = inserted.get().affectedObjects.first().value;
    view->setEditMode(unsupported ? ClipEditorGlobal::SplitNote : ClipEditorGlobal::Select);
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto original = TestSupport::projectSnapshot(*context->m_appModel);
    const auto first = pointFor(600, 62);
    const auto last = pointFor(1080, 64);
    const QPointingDevice device(
        QStringLiteral("Fixture pen"), 1001, QInputDevice::DeviceType::Stylus,
        barrel ? QPointingDevice::PointerType::Pen : QPointingDevice::PointerType::Eraser,
        QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 2);
    const auto button = barrel ? Qt::RightButton : Qt::LeftButton;
    const auto cleanup = qScopeGuard([&] { deactivate(*view); });
    QVERIFY(sendTablet(*view->viewport(), device, QEvent::TabletPress, first, 0.7, button, button));
    QVERIFY(sendTablet(*view->viewport(), device, QEvent::TabletMove, first + QPoint(20, 0), 0.7,
                       Qt::NoButton, button));
    QVERIFY(
        sendTablet(*view->viewport(), device, QEvent::TabletMove, last, 0.7, Qt::NoButton, button));
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), original);
    if (unsupported) {
        QCOMPARE(sceneNoteCount(firstId), 1);
        QCOMPARE(sceneNoteCount(secondId), 1);
        QVERIFY(!editSessionManager->hasActiveTransaction());
        QVERIFY(sendTablet(*view->viewport(), device, QEvent::TabletRelease, last, 0, button,
                           Qt::NoButton));
        QVERIFY(!historyManager->canUndo());
        view->setEditMode(ClipEditorGlobal::Select);
    } else {
        QCOMPARE(sceneNoteCount(firstId), 0);
        QCOMPARE(sceneNoteCount(secondId), 0);
        QVERIFY(editSessionManager->hasActiveTransaction());
        QVERIFY(EditorPointer::isPenStreamActive());
        QVERIFY(EditorPointer::isPenEraseIntentActive());
        if (interrupt) {
            deactivate(*view);
            QCOMPARE(runtime.documentVersion(), before);
            QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), original);
            QCOMPARE(sceneNoteCount(firstId), 1);
            QCOMPARE(sceneNoteCount(secondId), 1);
            QVERIFY(!historyManager->canUndo());
        } else {
            QVERIFY(sendTablet(*view->viewport(), device, QEvent::TabletRelease, last, 0, button,
                               Qt::NoButton));
            QCOMPARE(singingClip->notes().count(), 0);
            QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
            QVERIFY(runtime.history().undo(commandContext()));
            QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), original);
            QVERIFY(!historyManager->canUndo());
        }
    }
    QVERIFY(!EditorPointer::isPenStreamActive());
    QVERIFY(!EditorPointer::isPenEraseIntentActive());
    QVERIFY(!editSessionManager->hasActiveTransaction());
    const auto retryBefore = runtime.documentVersion();
    QVERIFY(sendTablet(*view->viewport(), device, QEvent::TabletPress, first, 0.7, button, button));
    QVERIFY(sendTablet(*view->viewport(), device, QEvent::TabletMove, first + QPoint(20, 0), 0.7,
                       Qt::NoButton, button));
    QVERIFY(
        sendTablet(*view->viewport(), device, QEvent::TabletMove, last, 0.7, Qt::NoButton, button));
    QVERIFY(sendTablet(*view->viewport(), device, QEvent::TabletRelease, last, 0, button,
                       Qt::NoButton));
    QCOMPARE(singingClip->notes().count(), 0);
    QCOMPARE(runtime.documentVersion().revision, retryBefore.revision + 1);
    QVERIFY(!EditorPointer::isPenStreamActive());
    QVERIFY(!EditorPointer::isPenEraseIntentActive());
    QVERIFY(!editSessionManager->hasActiveTransaction());
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), original);
    QCOMPARE(sceneNoteCount(firstId), 1);
    QCOMPARE(sceneNoteCount(secondId), 1);
    QVERIFY(!historyManager->canUndo());
}
