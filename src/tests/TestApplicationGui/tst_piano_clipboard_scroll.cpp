#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Global/ControllerGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Views/ClipEditor/PianoRoll/NoteView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollContextMenuController.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollCoord.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsScene.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollView.h"
#include "UI/Views/ClipEditor/PianoRoll/PronunciationView.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QCursor>
#include <QMenu>
#include <QMimeData>
#include <QScopeGuard>
#include <QTimer>
#include <QWindow>
#include <QtTest/QTest>

#include <algorithm>
#include <functional>

namespace {
    void withPianoMenu(PianoRollView &owner, PianoRollGraphicsView &canvas, const QPoint &position,
                       const std::function<void(QMenu &)> &interact) {
        bool entered = false;
        QTimer action;
        action.setSingleShot(true);
        QObject::connect(&action, &QTimer::timeout, &owner, [&] {
            const auto closeMenu = qScopeGuard([&] {
                for (auto *owned :
                     owner.findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly)) {
                    if (owned->isVisible())
                        owned->close();
                }
            });
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            QVERIFY(menu);
            QCOMPARE(menu->parentWidget(), &owner);
            entered = true;
            interact(*menu);
        });
        const auto global = canvas.viewport()->mapToGlobal(position);
        QCursor::setPos(global);
        QTest::mouseMove(owner.windowHandle(), owner.mapFromGlobal(global));
        QContextMenuEvent event(QContextMenuEvent::Mouse, position, global);
        action.start(0);
        QApplication::sendEvent(canvas.viewport(), &event);
        action.stop();
        QVERIFY(entered);
    }

    QAction *actionNamed(QMenu &menu, const QString &text) {
        for (auto *action : menu.actions()) {
            if (action->text() == text)
                return action;
        }
        return nullptr;
    }
}

void ApplicationGuiTests::pianoContextMenuPastePreservesRelativeNotesAndManualWords() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.project().patchClipProperties(
        commandContext(), {.id = Automation::ClipId(singingClip->id()), .start = 4800}));
    Automation::NoteDraftDto first;
    first.localStart = 480;
    first.length = 240;
    first.keyIndex = 60;
    first.lyric = QStringLiteral("kept");
    first.language = QStringLiteral("eng");
    first.pronunciation.edited = QStringLiteral("k eh p t");
    auto second = first;
    second.localStart = 960;
    second.keyIndex = 64;
    second.lyric = QStringLiteral("word");
    second.pronunciation.edited = QStringLiteral("w er d");
    const QList<Automation::NoteDraftDto> drafts{first, second};
    QVERIFY(runtime.notes().insertNotes(commandContext(), Automation::ClipId(singingClip->id()),
                                        drafts));
    const auto originalNotes = singingClip->notes().toList();
    QCOMPARE(originalNotes.size(), drafts.size());
    const QList<int> originalIds{originalNotes.at(0)->id(), originalNotes.at(1)->id()};

    auto previousClipboard = std::make_unique<QMimeData>();
    if (const auto *mime = QApplication::clipboard()->mimeData()) {
        for (const auto &format : mime->formats())
            previousClipboard->setData(format, mime->data(format));
    }
    const auto restoreClipboard =
        qScopeGuard([&] { QApplication::clipboard()->setMimeData(previousClipboard.release()); });
    const auto previousCursor = QCursor::pos();
    const auto restoreCursor = qScopeGuard([&] { QCursor::setPos(previousCursor); });
    view->hide();
    PianoRollView piano;
    piano.setDataContext(singingClip);
    const auto detach = qScopeGuard([&] { piano.setDataContext(nullptr); });
    piano.resize(1000, 600);
    piano.show();
    piano.activateWindow();
    QTRY_VERIFY(piano.isActiveWindow());
    QVERIFY(piano.setViewScale(1.0, 1.0));
    QVERIFY(piano.centerAt(singingClip->start() + 1920, 62));
    auto *canvas = piano.findChild<PianoRollGraphicsView *>();
    QVERIFY(canvas);
    canvas->setAnimationEnabled(false);
    canvas->setViewportStartTick(singingClip->start());
    piano.onEditModeChanged(ClipEditorGlobal::Select);
    QTRY_VERIFY(canvas->isVisible() && canvas->viewport()->width() > 800);
    const auto pointAt = [&](int tick, int key) {
        return canvas->mapFromScene(QPointF(
            canvas->tickToSceneX(tick), PianoRollCoord::keyIndexToCenterY(
                                            key, ClipEditorGlobal::noteHeight * canvas->scaleY())));
    };
    const auto sourcePosition = pointAt(600, 60);
    const auto secondPosition = pointAt(1080, 64);
    const auto destinationPosition = pointAt(2180, 58);
    for (const auto &point : {sourcePosition, secondPosition, destinationPosition})
        QVERIFY(canvas->viewport()->rect().contains(point));
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, sourcePosition);
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::ControlModifier, secondPosition);
    QCOMPARE(canvas->selectedNotesId(), originalIds);
    historyManager->reset();
    const auto before = runtime.documentVersion();
    withPianoMenu(piano, *canvas, sourcePosition, [&](QMenu &menu) {
        auto *copy = actionNamed(menu, PianoRollContextMenuController::tr("&Copy"));
        QVERIFY(copy);
        QVERIFY(copy->isEnabled());
        QTest::mouseClick(&menu, Qt::LeftButton, Qt::NoModifier,
                          menu.actionGeometry(copy).center());
    });
    if (QTest::currentTestFailed())
        return;
    QVERIFY(QApplication::clipboard()->mimeData()->hasFormat(
        ControllerGlobal::ElemMimeType.at(ControllerGlobal::NoteWithParams)));
    const auto previews = [&] {
        QList<NoteView *> notes;
        for (auto *item : canvas->scene()->items()) {
            if (auto *note = dynamic_cast<NoteView *>(item); note && note->id() < 0)
                notes.append(note);
        }
        std::sort(notes.begin(), notes.end(), [](const auto *left, const auto *right) {
            return left->rStart() < right->rStart();
        });
        return notes;
    };
    const auto noPreviewItems = [&] {
        for (auto *item : canvas->scene()->items()) {
            if (const auto *note = dynamic_cast<NoteView *>(item); note && note->id() < 0)
                return false;
            if (const auto *pronunciation = dynamic_cast<PronunciationView *>(item);
                pronunciation && pronunciation->id() < 0)
                return false;
        }
        return true;
    };
    QList<int> previewStarts;
    QList<QRectF> previewRects;
    const auto previewAndFinish = [&](bool commit) {
        withPianoMenu(piano, *canvas, destinationPosition, [&](QMenu &menu) {
            auto *paste = actionNamed(menu, PianoRollContextMenuController::tr("&Paste"));
            QVERIFY(paste);
            QVERIFY(paste->isEnabled());
            const auto position = menu.actionGeometry(paste).center();
            QVERIFY(menu.windowHandle());
            QTest::mouseMove(menu.windowHandle(), position);
            QTRY_COMPARE(menu.activeAction(), paste);
            QTRY_COMPARE(previews().size(), drafts.size());
            previewStarts.clear();
            previewRects.clear();
            const auto ghosts = previews();
            for (qsizetype index = 0; index < ghosts.size(); ++index) {
                const auto *ghost = ghosts.at(index);
                const auto &source = drafts.at(index);
                QCOMPARE(ghost->length(), source.length);
                QCOMPARE(ghost->keyIndex(), source.keyIndex);
                QCOMPARE(ghost->lyric(), source.lyric);
                QVERIFY(ghost->pronunciationView());
                QCOMPARE(ghost->pronunciationView()->scene(), canvas->scene());
                previewStarts.append(ghost->rStart());
                previewRects.append(ghost->sceneBoundingRect());
            }
            QCOMPARE(previewStarts, QList<int>({2160, 2640}));
            QCOMPARE(singingClip->notes().count(), originalNotes.size());
            QCOMPARE(runtime.documentVersion(), before);
            QVERIFY(!historyManager->canUndo());
            if (commit)
                QTest::mouseClick(&menu, Qt::LeftButton, Qt::NoModifier, position);
            else
                QTest::keyClick(&menu, Qt::Key_Escape);
        });
    };
    previewAndFinish(false);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(noPreviewItems());
    QCOMPARE(singingClip->notes().count(), originalNotes.size());
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
    previewAndFinish(true);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(noPreviewItems());
    QList<Note *> pasted;
    for (auto *note : singingClip->notes()) {
        if (!originalIds.contains(note->id()))
            pasted.append(note);
    }
    QCOMPARE(pasted.size(), drafts.size());
    for (qsizetype index = 0; index < pasted.size(); ++index) {
        const auto *note = pasted.at(index);
        const auto &source = drafts.at(index);
        QCOMPARE(note->localStart(), previewStarts.at(index));
        QCOMPARE(note->length(), source.length);
        QCOMPARE(note->keyIndex(), source.keyIndex);
        QCOMPARE(note->lyric(), source.lyric);
        QCOMPARE(note->pronunciation().edited, source.pronunciation.edited);
        const NoteView *item = nullptr;
        for (auto *candidate : canvas->scene()->items()) {
            if (const auto *noteView = dynamic_cast<NoteView *>(candidate);
                noteView && noteView->id() == note->id())
                item = noteView;
        }
        QVERIFY(item);
        QCOMPARE(item->sceneBoundingRect(), previewRects.at(index));
    }
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(singingClip->notes().toList(), originalNotes);
    QVERIFY(noPreviewItems());
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::pianoNoteDragContinuesDuringEdgeScrollingAndStopsOnFinish() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &runtime = *context->m_coreRuntime;
    Automation::NoteDraftDto draft;
    draft.localStart = 480;
    draft.length = 240;
    draft.keyIndex = 60;
    draft.lyric = QStringLiteral("la");
    draft.language = QStringLiteral("eng");
    QVERIFY(runtime.notes().insertNotes(commandContext(), Automation::ClipId(singingClip->id()),
                                        {draft}));
    const auto *note = *singingClip->notes().begin();
    const auto noteId = note->id();
    view->setEditMode(ClipEditorGlobal::Select);
    QVERIFY(view->setViewportScale(3.0, 1.0));
    view->activateWindow();
    QTRY_VERIFY(view->isActiveWindow());
    historyManager->reset();
    const auto before = runtime.documentVersion();
    const auto previousCursor = QCursor::pos();
    QPoint releasePosition;
    bool pressed = false;
    const auto releaseInput = qScopeGuard([&] {
        if (pressed) {
            QTest::keyClick(view.get(), Qt::Key_Escape);
            QTest::mouseRelease(view->windowHandle(), Qt::LeftButton, Qt::NoModifier,
                                view->viewport()->mapTo(view.get(), releasePosition));
        }
        QCursor::setPos(previousCursor);
    });
    const auto dragToEdge = [&](bool commit) {
        view->setViewportStartTick(0);
        QCoreApplication::processEvents();
        const auto press = pointFor(draft.localStart + draft.length / 2, draft.keyIndex);
        releasePosition = QPoint(view->viewport()->rect().right() - 1, press.y());
        QVERIFY(view->viewport()->rect().contains(press));
        QVERIFY(view->viewport()->rect().contains(releasePosition));
        const auto windowPoint = [&](const QPoint &point) {
            return view->viewport()->mapTo(view.get(), point);
        };
        QCursor::setPos(view->viewport()->mapToGlobal(press));
        QTest::mouseMove(view->windowHandle(), windowPoint(press));
        QTest::mousePress(view->windowHandle(), Qt::LeftButton, Qt::NoModifier, windowPoint(press));
        pressed = true;
        QVERIFY(QGuiApplication::mouseButtons().testFlag(Qt::LeftButton));
        QCursor::setPos(view->viewport()->mapToGlobal(releasePosition));
        QTest::mouseMove(view->windowHandle(), windowPoint(releasePosition));
        QCOMPARE(appStatus->pianoRollNoteEditPreview.get().size(), 1);
        const auto firstPreview = appStatus->pianoRollNoteEditPreview.get().first();
        const auto firstVisibleTick = view->startTick();
        // No further move event is sent: the real scroll timer must continue the same drag.
        QTRY_VERIFY(view->startTick() > firstVisibleTick &&
                    appStatus->pianoRollNoteEditPreview.get().size() == 1 &&
                    appStatus->pianoRollNoteEditPreview.get().first().rStart > firstPreview.rStart);
        const auto lastPreview = appStatus->pianoRollNoteEditPreview.get().first();
        QCOMPARE(lastPreview.id, noteId);
        QCOMPARE(lastPreview.length, draft.length);
        QCOMPARE(lastPreview.keyIndex, draft.keyIndex);
        QCOMPARE(note->localStart(), draft.localStart);
        QCOMPARE(runtime.documentVersion(), before);
        QVERIFY(!historyManager->canUndo());
        QVERIFY(editSessionManager->hasActiveTransaction());
        if (!commit)
            QTest::keyClick(view.get(), Qt::Key_Escape);
        QTest::mouseRelease(view->windowHandle(), Qt::LeftButton, Qt::NoModifier,
                            windowPoint(releasePosition));
        pressed = false;
        QVERIFY(appStatus->pianoRollNoteEditPreview.get().isEmpty());
        QVERIFY(!editSessionManager->hasActiveTransaction());
        if (commit)
            QVERIFY(note->localStart() >= lastPreview.rStart);
        else
            QCOMPARE(note->localStart(), draft.localStart);
        const auto stoppedAt = view->startTick();
        QTest::qWait(100);
        QCOMPARE(view->startTick(), stoppedAt);
        QCOMPARE(runtime.documentVersion().revision, before.revision + (commit ? 1 : 0));
    };
    dragToEdge(false);
    if (QTest::currentTestFailed())
        return;
    dragToEdge(true);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(historyManager->canUndo());
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(note->localStart(), draft.localStart);
    QVERIFY(sceneNote(noteId));
    QCOMPARE(sceneNote(noteId)->rStart(), draft.localStart);
    QVERIFY(!historyManager->canUndo());
}
