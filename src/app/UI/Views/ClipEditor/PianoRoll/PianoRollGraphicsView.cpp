//
// Created by fluty on 2024/1/23.
//

#define CLASS_NAME "PianoRollGraphicsView"

#include "PianoRollGraphicsView.h"

#include "ClipRangeOverlay.h"
#include "NoteView.h"
#include "PianoRollBackground.h"
#include "PianoRollGraphicsScene.h"
#include "PianoRollGraphicsViewHelper.h"
#include "PianoRollGraphicsView_p.h"
#include "PitchEditorView.h"
#include "PronunciationView.h"
#include "Controller/ClipController.h"
#include "Controller/PlaybackController.h"
#include "Global/AppGlobal.h"
#include "Model/AppModel/DrawCurve.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Dialogs/Note/NotePropertyDialog.h"
#include "UI/Views/Common/ScrollBarView.h"
#include "Utils/Linq.h"
#include "Utils/Log.h"
#include "Utils/MathUtils.h"

#include <QApplication>
#include <QMouseEvent>
#include <QScrollBar>
#include <QMWidgets/cmenu.h>

namespace Helper = PianoRollGraphicsViewHelper;

PianoRollGraphicsView::PianoRollGraphicsView(PianoRollGraphicsScene *scene, const QWidget *parent)
    : TimeGraphicsView(scene, parent), d_ptr(new PianoRollGraphicsViewPrivate(this)) {
    Q_D(PianoRollGraphicsView);
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("PianoRollGraphicsView");
    setScaleXMax(5);
    setPixelsPerQuarterNote(pixelsPerQuarterNote);
    setSceneVisibility(false);
    setDragBehavior(DragBehavior::RectSelect);
    setMinimumHeight(0);
    // QScroller::grabGesture(this, QScroller::TouchGesture);

    d->m_currentDrawingNote = new NoteView(-1);
    // d->m_currentDrawingNote->setPronunciation("", false);
    d->m_currentDrawingNote->setSelected(true);

    d->m_gridItem = new PianoRollBackground;
    d->m_gridItem->setPixelsPerQuarterNote(pixelsPerQuarterNote);
    setGridItem(d->m_gridItem);

    d->m_pitchEditor = new PitchEditorView;
    d->m_pitchEditor->setZValue(2);
    connect(d->m_pitchEditor, &CommonParamEditorView::editCompleted, this,
            [](const QList<DrawCurve *> &curves) { Helper::editPitch(curves); });
    scene->addCommonItem(d->m_pitchEditor);
    d->m_pitchEditor->setTransparentMouseEvents(true);

    d->m_clipRangeOverlay = new ClipRangeOverlay;
    d->m_clipRangeOverlay->setZValue(3);
    scene->addCommonItem(d->m_clipRangeOverlay);

    connect(scene, &QGraphicsScene::selectionChanged, this,
            &PianoRollGraphicsView::onSceneSelectionChanged);

    connect(this, &TimeGraphicsView::scaleChanged, this,
            &PianoRollGraphicsView::notifyKeyRangeChanged);
    connect(this, &TimeGraphicsView::visibleRectChanged, this,
            &PianoRollGraphicsView::notifyKeyRangeChanged);

    connect(appStatus, &AppStatus::noteSelectionChanged, d,
            &PianoRollGraphicsViewPrivate::onNoteSelectionChanged);
}

PianoRollGraphicsView::~PianoRollGraphicsView() {
    Q_D(PianoRollGraphicsView);
    delete d->m_pitchEditor;
    delete d_ptr;
}

void PianoRollGraphicsView::setDataContext(SingingClip *clip) {
    Q_D(PianoRollGraphicsView);
    if (clip == nullptr)
        d->moveToNullClipState();
    else
        d->moveToSingingClipState(clip);
}

void PianoRollGraphicsView::onSceneSelectionChanged() const {
    Q_D(const PianoRollGraphicsView);
    if (!d->m_selectionChangeBarrier) {
        const auto notes = selectedNotesId();
        clipController->selectNotes(notes, true);
    }
}

void PianoRollGraphicsView::notifyKeyRangeChanged() {
    emit keyRangeChanged(topKeyIndex(), bottomKeyIndex());
}

bool PianoRollGraphicsView::event(QEvent *event) {
    Q_D(PianoRollGraphicsView);
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) {
        const auto key = dynamic_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Escape) {
            discardAction();
        }
    } else if (event->type() == QEvent::WindowDeactivate) {
        discardAction();
    } else if (event->type() == QEvent::HoverEnter)
        d->onHoverEnter(dynamic_cast<QHoverEvent *>(event));
    else if (event->type() == QEvent::HoverLeave)
        d->onHoverLeave(dynamic_cast<QHoverEvent *>(event));
    else if (event->type() == QEvent::HoverMove)
        d->onHoverMove(dynamic_cast<QHoverEvent *>(event));
    return TimeGraphicsView::event(event);
}

void PianoRollGraphicsView::contextMenuEvent(QContextMenuEvent *event) {
    Q_D(PianoRollGraphicsView);
    if (d->m_editMode == Select || d->m_editMode == IntervalSelect || d->m_editMode == DrawNote) {
        if (const auto noteView = d->noteViewAt(event->pos())) {
            const auto menu = d->buildNoteContextMenu(noteView);
            menu->exec(event->globalPos());
        }
    }

    TimeGraphicsView::contextMenuEvent(event);
}

void PianoRollGraphicsView::mousePressEvent(QMouseEvent *event) {
    Q_D(PianoRollGraphicsView);
    if (d->m_mouseDown) {
        qWarning() << "Ignored mousePressEvent" << event
                   << "because there is already one mouse button pressed";
        return;
    }
    d->m_mouseDown = true;
    d->m_mouseDownButton = event->button();

    // 在滚动条上按下时，交还给基类处理
    if (dynamic_cast<ScrollBarView *>(itemAt(event->pos()))) {
        TimeGraphicsView::mousePressEvent(event);
        event->ignore();
        return;
    }

    cancelRequested = false;
    d->m_selecting = true;
    d->m_selectionChangeBarrier = true;
    if (event->button() != Qt::LeftButton &&
        (d->m_editMode == Select || d->m_editMode == DrawNote || d->m_editMode == EraseNote)) {
        d->m_mouseMoveBehavior = PianoRollGraphicsViewPrivate::None;
        if (const auto noteView = d->noteViewAt(event->pos())) {
            if (d->selectedNoteItems().count() <= 1 || !d->selectedNoteItems().contains(noteView))
                clearNoteSelections();
            noteView->setSelected(true);
        } else {
            clearNoteSelections();
            TimeGraphicsView::mousePressEvent(event);
        }
        // TimeGraphicsView::mousePressEvent(event);
        event->ignore();
        return;
    }

    // event->accept();
    const auto scenePos = mapToScene(event->position().toPoint());
    const auto tick = static_cast<int>(sceneXToTick(scenePos.x()) + d->m_offset);
    const auto keyIndex = d->sceneYToKeyIndexInt(scenePos.y());
    const auto noteView = d->noteViewAt(event->pos());
    const auto pronView = d->pronViewAt(event->pos());

    if (d->m_editMode == Select) {
        if (noteView) {
            d->prepareForEditingNotes(event, scenePos, keyIndex, noteView);
        } else
            TimeGraphicsView::mousePressEvent(event);
    } else if (d->m_editMode == DrawNote) {
        clearNoteSelections();
        if (noteView) {
            d->prepareForEditingNotes(event, scenePos, keyIndex, noteView);
        } else if (pronView) {
            const auto currentNoteView = d->findNoteViewById(pronView->id());
            currentNoteView->setSelected(true);
        } else
            d->PrepareForDrawingNote(tick, keyIndex);
    } else if (d->m_editMode == EraseNote) {
        d->m_mouseMoveBehavior = PianoRollGraphicsViewPrivate::EraseNotes;
        if (noteView) {
            d->eraseNoteFromView(noteView);
        } else
            TimeGraphicsView::mousePressEvent(event);
    } else
        TimeGraphicsView::mousePressEvent(event);
    event->ignore();
}

void PianoRollGraphicsView::mouseMoveEvent(QMouseEvent *event) {
    Q_D(PianoRollGraphicsView);
    if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::None) {
        TimeGraphicsView::mouseMoveEvent(event);
        return;
    }
    if (cancelRequested || d->m_mouseDown == false)
        return;

    if (event->modifiers() == Qt::AltModifier)
        d->m_tempQuantizeOff = true;
    else
        d->m_tempQuantizeOff = false;

    const auto scenePos = mapToScene(event->position().toPoint());
    const auto tick = static_cast<int>(sceneXToTick(scenePos.x()) + d->m_offset);
    const auto quantizedTickLength = d->m_tempQuantizeOff ? 1 : 1920 / appStatus->quantize;
    const auto snappedTick = MathUtils::roundDown(tick, quantizedTickLength);
    const auto keyIndex = d->sceneYToKeyIndexInt(scenePos.y());
    const auto deltaX = static_cast<int>(sceneXToTick(scenePos.x() - d->m_mouseDownPos.x()));

    const auto noteView = d->noteViewAt(event->pos());

    // TODO: 优化移动和调整音符
    if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::Move) {
        const auto startOffset = MathUtils::round(deltaX, quantizedTickLength);
        auto keyOffset = keyIndex - d->m_mouseDownKeyIndex;
        if (keyOffset > d->m_moveMaxDeltaKey)
            keyOffset = d->m_moveMaxDeltaKey;
        if (keyOffset < d->m_moveMinDeltaKey)
            keyOffset = d->m_moveMinDeltaKey;
        d->m_deltaTick = startOffset;
        d->m_deltaKey = keyOffset;
        d->moveSelectedNotes(d->m_deltaTick, d->m_deltaKey);
        d->m_movedBeforeMouseUp = true;
    } else if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::ResizeLeft) {
        const auto rStart = snappedTick - d->m_offset;
        auto deltaStart = rStart - d->m_mouseDownRStart;
        const auto length = d->m_mouseDownLength - deltaStart;
        if (length < quantizedTickLength)
            deltaStart = d->m_mouseDownLength - quantizedTickLength;
        d->m_deltaTick = deltaStart;
        d->resizeLeftSelectedNote(d->m_deltaTick);
        d->m_movedBeforeMouseUp = true;
    } else if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::ResizeRight) {
        const auto lengthOffset = MathUtils::round(deltaX, quantizedTickLength);
        const auto right = d->m_mouseDownRStart + d->m_mouseDownLength + lengthOffset;
        const auto length = right - d->m_mouseDownRStart;
        auto deltaLength = length - d->m_mouseDownLength;
        if (length < quantizedTickLength)
            deltaLength = -(d->m_mouseDownLength - quantizedTickLength);
        d->m_deltaTick = deltaLength;
        d->resizeRightSelectedNote(d->m_deltaTick);
        d->m_movedBeforeMouseUp = true;
    } else if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::UpdateDrawingNote) {
        const auto targetLength = snappedTick - d->m_offset - d->m_currentDrawingNote->rStart();
        if (targetLength >= quantizedTickLength)
            d->m_currentDrawingNote->setLength(targetLength);
    } else if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::EraseNotes) {
        if (noteView)
            d->eraseNoteFromView(noteView);
        else
            TimeGraphicsView::mouseMoveEvent(event);
    } else
        TimeGraphicsView::mouseMoveEvent(event);
}

void PianoRollGraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    Q_D(PianoRollGraphicsView);
    if (event->button() != d->m_mouseDownButton) {
        qWarning() << "Ignored mouseReleaseEvent" << event;
        return;
    }
    d->m_mouseDown = false;
    d->m_mouseDownButton = Qt::NoButton;
    if (!cancelRequested)
        commitAction();
    const bool ctrlDown = event->modifiers() == Qt::ControlModifier;
    if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::Move) {
        if (!d->m_movedBeforeMouseUp && !ctrlDown) {
            clearNoteSelections(d->m_currentEditingNote);
        }
    }
    cancelRequested = false;
    TimeGraphicsView::mouseReleaseEvent(event);
}

void PianoRollGraphicsView::mouseDoubleClickEvent(QMouseEvent *event) {
    // 禁用双击事件，防止在滚动条上双击时触发取消选中音符
    // TimeGraphicsView::mouseDoubleClickEvent(event);
    Q_D(PianoRollGraphicsView);
    if (!(d->m_editMode == Select || d->m_editMode == IntervalSelect || d->m_editMode == DrawNote))
        return;
    if (event->button() != Qt::LeftButton)
        return;

    for (const auto item : items(event->pos())) {
        if (const auto noteView = dynamic_cast<NoteView *>(item)) {
            d->onOpenNotePropertyDialog(noteView->id(), AppGlobal::Lyric);
            break;
        }
        if (const auto pronView = dynamic_cast<PronunciationView *>(item)) {
            d->onOpenNotePropertyDialog(pronView->id(), AppGlobal::Pronunciation);
        }
    }
}

int PianoRollGraphicsView::noteFontPixelSize() const {
    return m_noteFontPixelSize;
}

void PianoRollGraphicsView::setNoteFontPixelSize(const int size) {
    Q_D(PianoRollGraphicsView);
    m_noteFontPixelSize = size;
    for (const auto noteView : d->noteViews)
        noteView->fontPixelSize = size;
}

QColor PianoRollGraphicsView::whiteKeyColor() const {
    Q_D(const PianoRollGraphicsView);
    return d->m_gridItem->whiteKeyColor();
}

void PianoRollGraphicsView::setWhiteKeyColor(const QColor &color) {
    Q_D(PianoRollGraphicsView);
    d->m_gridItem->setWhiteKeyColor(color);
}

QColor PianoRollGraphicsView::blackKeyColor() const {
    Q_D(const PianoRollGraphicsView);
    return d->m_gridItem->blackKeyColor();
}

void PianoRollGraphicsView::setBlackKeyColor(const QColor &color) {
    Q_D(PianoRollGraphicsView);
    d->m_gridItem->setBlackKeyColor(color);
}

QColor PianoRollGraphicsView::octaveDividerColor() const {
    Q_D(const PianoRollGraphicsView);
    return d->m_gridItem->octaveDividerColor();
}

void PianoRollGraphicsView::setOctaveDividerColor(const QColor &color) {
    Q_D(PianoRollGraphicsView);
    d->m_gridItem->setOctaveDividerColor(color);
}

void PianoRollGraphicsView::reset() {
    Q_D(PianoRollGraphicsView);
    for (const auto &noteView : d->noteViews) {
        d->removeNoteViewFromScene(noteView);
        delete noteView;
    }
}

QList<int> PianoRollGraphicsView::selectedNotesId() const {
    Q_D(const PianoRollGraphicsView);
    QList<int> list;
    for (const auto noteView : d->noteViews) {
        if (noteView->isSelected())
            list.append(noteView->id());
    }
    return list;
}

void PianoRollGraphicsView::clearNoteSelections(const NoteView *except) {
    Q_D(PianoRollGraphicsView);
    for (const auto noteView : d->noteViews) {
        if (noteView != except && noteView->isSelected())
            noteView->setSelected(false);
    }
}

void PianoRollGraphicsView::discardAction() {
    Q_D(PianoRollGraphicsView);
    d->m_pitchEditor->discardAction();
    cancelRequested = true;
    if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::Move) {
        if (d->m_movedBeforeMouseUp) {
            d->resetSelectedNotesOffset();
        }
    } else if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::ResizeLeft) {
        d->resetSelectedNotesOffset();
    } else if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::ResizeRight) {
        d->resetSelectedNotesOffset();
    } else if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::UpdateDrawingNote) {
        scene()->removeCommonItem(d->m_currentDrawingNote);
    } else if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::EraseNotes) {
        d->cancelEraseNote();
    }
    d->m_mouseMoveBehavior = PianoRollGraphicsViewPrivate::None;
    d->m_deltaTick = 0;
    d->m_deltaKey = 0;
    d->m_movedBeforeMouseUp = false;
    d->m_currentEditingNote = nullptr;

    d->m_selecting = false;
    const auto notes = selectedNotesId();
    clipController->selectNotes(notes, true);

    appStatus->currentEditObject = AppStatus::EditObjectType::None;
}

void PianoRollGraphicsView::commitAction() {
    Q_D(PianoRollGraphicsView);
    d->m_pitchEditor->commitAction();
    if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::Move) {
        if (d->m_movedBeforeMouseUp) {
            d->resetSelectedNotesOffset();
            d->handleNotesMoved(d->m_deltaTick, d->m_deltaKey);
        }
    } else if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::ResizeLeft) {
        if (d->m_movedBeforeMouseUp) {
            d->resetSelectedNotesOffset();
            PianoRollGraphicsViewPrivate::handleNoteLeftResized(d->m_currentEditingNote->id(),
                                                                d->m_deltaTick);
        }
    } else if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::ResizeRight) {
        if (d->m_movedBeforeMouseUp) {
            d->resetSelectedNotesOffset();
            PianoRollGraphicsViewPrivate::handleNoteRightResized(d->m_currentEditingNote->id(),
                                                                 d->m_deltaTick);
        }
    } else if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::UpdateDrawingNote) {
        d->removeNoteViewFromScene(d->m_currentDrawingNote);
        Helper::drawNote(d->m_currentDrawingNote->rStart(), d->m_currentDrawingNote->length(),
                         d->m_currentDrawingNote->keyIndex());
    } else if (d->m_mouseMoveBehavior == PianoRollGraphicsViewPrivate::EraseNotes) {
        d->handleNotesErased();
    }
    d->m_mouseMoveBehavior = PianoRollGraphicsViewPrivate::None;
    d->m_deltaTick = 0;
    d->m_deltaKey = 0;
    d->m_movedBeforeMouseUp = false;
    d->m_currentEditingNote = nullptr;

    d->m_selecting = false;
    const auto notes = selectedNotesId();
    clipController->selectNotes(notes, true);

    appStatus->currentEditObject = AppStatus::EditObjectType::None;
}

double PianoRollGraphicsView::topKeyIndex() const {
    Q_D(const PianoRollGraphicsView);
    return d->sceneYToKeyIndexDouble(visibleRect().top());
}

double PianoRollGraphicsView::bottomKeyIndex() const {
    Q_D(const PianoRollGraphicsView);
    return d->sceneYToKeyIndexDouble(visibleRect().bottom());
}

void PianoRollGraphicsView::setViewportCenterAt(const double tick, const double keyIndex) {
    setViewportCenterAtTick(tick);
    setViewportCenterAtKeyIndex(keyIndex);
}

void PianoRollGraphicsView::setViewportCenterAtKeyIndex(const double keyIndex) {
    Q_D(PianoRollGraphicsView);
    const auto keyIndexRange = topKeyIndex() - bottomKeyIndex();
    const auto keyIndexStart = keyIndex + keyIndexRange / 2 + 0.5;
    const auto vBarValue = qRound(d->keyIndexToSceneY(keyIndexStart));
    // verticalScrollBar()->setValue(vBarValue);
    verticalBarAnimateTo(vBarValue);
}

void PianoRollGraphicsView::setEditMode(const PianoRollEditMode mode) {
    Q_D(PianoRollGraphicsView);
    if (d->m_editMode != mode)
        commitAction();
    d->m_editMode = mode;
    if (mode == Select) {
        setDragBehavior(DragBehavior::RectSelect);
        d->setPitchEditMode(false, false);
    } else if (mode == IntervalSelect) {
        setDragBehavior(DragBehavior::IntervalSelect);
        d->setPitchEditMode(false, false);
    } else if (mode == DrawNote || mode == EraseNote) {
        setDragBehavior(DragBehavior::None);
        d->setPitchEditMode(false, false);
    } else if (mode == DrawPitch || mode == EditPitchAnchor) {
        setDragBehavior(DragBehavior::None);
        d->setPitchEditMode(true, false);
    } else if (mode == ErasePitch || mode == FreezePitch) { // TODO: 实现冻结自动音高
        setDragBehavior(DragBehavior::None);
        d->setPitchEditMode(true, true);
    }
}

void PianoRollGraphicsViewPrivate::onNoteChanged(const SingingClip::NoteChangeType type,
                                                 const QList<Note *> &notes) {
    if (type == SingingClip::Insert)
        for (const auto &note : notes)
            handleNoteInserted(note);
    else if (type == SingingClip::Remove) {
        for (const auto &note : notes)
            handleNoteRemoved(note);
        QList<int> notesId;
        for (const auto &note : notes)
            notesId.append(note->id());
        clipController->unselectNotes(notesId);
    } else if (type == SingingClip::TimeKeyPropertyChange) {
        for (const auto &note : notes)
            updateNoteTimeAndKey(note);
    } else if (type == SingingClip::OriginalWordPropertyChange ||
               type == SingingClip::EditedWordPropertyChange) {
        for (const auto &note : notes)
            updateNoteWord(note);
    }

    updateOverlappedState();
}

void PianoRollGraphicsViewPrivate::onNoteSelectionChanged() {
    // qDebug() << "on note selection changed" << appStatus->selectedNotes.get();
    Q_Q(PianoRollGraphicsView);
    // if (!m_selecting)
    if (m_clip)
        updateSceneSelectionState();
}

void PianoRollGraphicsViewPrivate::onParamChanged(const ParamInfo::Name name,
                                                  const Param::Type type) const {
    if (name == ParamInfo::Pitch) {
        const auto pitchParam = m_clip->params.getParamByName(name);
        updatePitch(type, *pitchParam);
    }
}

void PianoRollGraphicsViewPrivate::onDeleteSelectedNotes() const {
    Q_Q(const PianoRollGraphicsView);
    const auto notes = q->selectedNotesId();
    clipController->onRemoveNotes(notes);
}

void PianoRollGraphicsViewPrivate::onOpenNotePropertyDialog(
    const int noteId, const AppGlobal::NotePropertyType propertyType) {
    Q_Q(PianoRollGraphicsView);
    const auto note = m_clip->findNoteById(noteId);
    Q_ASSERT(note);
    const auto dlg = new NotePropertyDialog(note, propertyType, q);
    connect(dlg, &NotePropertyDialog::accepted, this,
            [=] { clipController->onNotePropertiesEdited(noteId, dlg->result()); });
    dlg->show();
}

CMenu *PianoRollGraphicsViewPrivate::buildNoteContextMenu(NoteView *noteView) {
    Q_Q(PianoRollGraphicsView);
    const auto menu = new CMenu(q);

    const auto actionEditLyric = menu->addAction(tr("Fill lyrics..."));
    connect(actionEditLyric, &QAction::triggered, clipController,
            [=] { clipController->onFillLyric(q); });

    const auto actionSearchLyric = menu->addAction(tr("Search lyrics..."));
    connect(actionSearchLyric, &QAction::triggered, clipController,
            [=] { clipController->onSearchLyric(q); });

    menu->addSeparator();

    const auto actionRemove = menu->addAction(tr("Delete"));
    connect(actionRemove, &QAction::triggered, this,
            &PianoRollGraphicsViewPrivate::onDeleteSelectedNotes);

    menu->addSeparator();

    const auto actionProperties = menu->addAction(tr("Properties..."));
    connect(actionProperties, &QAction::triggered, this,
            [noteView, this] { onOpenNotePropertyDialog(noteView->id(), AppGlobal::Lyric); });
    return menu;
}

void PianoRollGraphicsViewPrivate::moveToNullClipState() {
    Q_Q(PianoRollGraphicsView);
    q->setSceneVisibility(false);
    q->setEnabled(false);
    m_pitchEditor->clearParams();
    while (m_notes.count() > 0)
        handleNoteRemoved(m_notes.first());
    if (m_clip) {
        disconnect(m_clip, nullptr, this, nullptr);
    }
    m_clip = nullptr;
}

void PianoRollGraphicsViewPrivate::moveToSingingClipState(SingingClip *clip) {
    Q_Q(PianoRollGraphicsView);
    m_selectionChangeBarrier = true;
    while (m_notes.count() > 0)
        handleNoteRemoved(m_notes.first());
    if (m_clip) {
        disconnect(m_clip, nullptr, this, nullptr);
    }

    m_clip = clip;
    m_offset = clip->start();
    q->setOffset(m_offset);
    q->setSceneVisibility(true);
    q->setEnabled(true);
    q->setSceneLength(m_clip->length());
    m_clipRangeOverlay->setClipRange(clip->clipStart(), clip->clipLen());

    if (clip->notes().count() > 0) {
        for (const auto note : clip->notes())
            handleNoteInserted(note);
        const auto firstNote = *clip->notes().begin();
        q->setViewportCenterAt(firstNote->globalStart(), firstNote->keyIndex());
    } else
        q->setViewportCenterAt(clip->start(), 60);

    updatePitch(Param::Original, *m_clip->params.getParamByName(ParamInfo::Pitch));
    updatePitch(Param::Edited, *m_clip->params.getParamByName(ParamInfo::Pitch));

    connect(clip, &SingingClip::propertyChanged, this,
            &PianoRollGraphicsViewPrivate::onClipPropertyChanged);
    connect(clip, &SingingClip::noteChanged, this, &PianoRollGraphicsViewPrivate::onNoteChanged);
    connect(clip, &SingingClip::paramChanged, this, &PianoRollGraphicsViewPrivate::onParamChanged);
    m_selectionChangeBarrier = false;
}

void PianoRollGraphicsViewPrivate::prepareForEditingNotes(const QMouseEvent *event,
                                                          const QPointF scenePos,
                                                          const int keyIndex, NoteView *noteItem) {
    Q_Q(PianoRollGraphicsView);
    appStatus->currentEditObject = AppStatus::EditObjectType::Note;
    const bool ctrlDown = event->modifiers() == Qt::ControlModifier;
    if (!ctrlDown) {
        if (selectedNoteItems().count() <= 1 || !selectedNoteItems().contains(noteItem))
            q->clearNoteSelections();
        noteItem->setSelected(true);
    } else {
        noteItem->setSelected(!noteItem->isSelected());
    }
    const auto rPos = noteItem->mapFromScene(scenePos);
    const auto rx = rPos.x();
    if (rx >= 0 && rx <= AppGlobal::resizeTolerance) {
        // setCursor(Qt::SizeHorCursor);
        m_mouseMoveBehavior = ResizeLeft;
        q->clearNoteSelections();
        noteItem->setSelected(true);
    } else if (rx >= noteItem->rect().width() - AppGlobal::resizeTolerance &&
               rx <= noteItem->rect().width()) {
        // setCursor(Qt::SizeHorCursor);
        m_mouseMoveBehavior = ResizeRight;
        q->clearNoteSelections();
        noteItem->setSelected(true);
    } else {
        // setCursor(Qt::ArrowCursor);
        m_mouseMoveBehavior = Move;
    }

    m_currentEditingNote = noteItem;
    m_mouseDownPos = scenePos;
    m_mouseDownRStart = m_currentEditingNote->rStart();
    m_mouseDownLength = m_currentEditingNote->length();
    m_mouseDownKeyIndex = keyIndex;
    updateMoveDeltaKeyRange();
}

void PianoRollGraphicsViewPrivate::PrepareForDrawingNote(const int tick, const int keyIndex) {
    Q_Q(PianoRollGraphicsView);
    appStatus->currentEditObject = AppStatus::EditObjectType::Note;
    const auto snappedTick = MathUtils::roundDown(tick, 1920 / appStatus->quantize);
    Log::d(CLASS_NAME, "Draw note at: " + qStrNum(snappedTick));
    m_currentDrawingNote->fontPixelSize = q->m_noteFontPixelSize;
    m_currentDrawingNote->setLyric(appOptions->general()->defaultLyric);
    m_currentDrawingNote->setRStart(snappedTick - m_offset);
    m_currentDrawingNote->setLength(1920 / appStatus->quantize);
    m_currentDrawingNote->setKeyIndex(keyIndex);
    q->scene()->addCommonItem(m_currentDrawingNote);
    m_mouseMoveBehavior = UpdateDrawingNote;
}

void PianoRollGraphicsViewPrivate::handleNotesMoved(const int deltaTick, const int deltaKey) const {
    Q_Q(const PianoRollGraphicsView);
    Log::d(CLASS_NAME, QString("Notes moved dt:%1 dk:%2 ").arg(deltaTick).arg(deltaKey));
    clipController->onMoveNotes(q->selectedNotesId(), deltaTick, deltaKey);
}

void PianoRollGraphicsViewPrivate::handleNoteLeftResized(const int noteId, const int deltaTick) {
    Log::d(CLASS_NAME,
           QString("Note left resized id:%1 dt:%2").arg(qStrNum(noteId), qStrNum(deltaTick)));
    QList<int> notes;
    notes.append(noteId);
    clipController->onResizeNotesLeft(notes, deltaTick);
}

void PianoRollGraphicsViewPrivate::handleNoteRightResized(const int noteId, const int deltaTick) {
    Log::d(CLASS_NAME,
           QString("Note right resized id:%1 dt:%2").arg(qStrNum(noteId), qStrNum(deltaTick)));
    QList<int> notes;
    notes.append(noteId);
    clipController->onResizeNotesRight(notes, deltaTick);
}

void PianoRollGraphicsViewPrivate::handleNotesErased() {
    qDebug() << "Note erased count:" << m_notesToErase.count();
    clipController->onRemoveNotes(m_notesToErase);
    m_notesToErase.clear();
    noteViewsToErase.clear();
}

void PianoRollGraphicsViewPrivate::eraseNoteFromView(NoteView *noteView) {
    Q_Q(PianoRollGraphicsView);
    appStatus->currentEditObject = AppStatus::EditObjectType::Note;
    m_notesToErase.append(noteView->id());
    noteViewsToErase.append(noteView);
    removeNoteViewFromScene(noteView);
}

void PianoRollGraphicsViewPrivate::cancelEraseNote() {
    Q_Q(PianoRollGraphicsView);
    m_notesToErase.clear();
    for (const auto noteView : noteViewsToErase)
        addNoteViewToScene(noteView);
    noteViewsToErase.clear();
}

void PianoRollGraphicsViewPrivate::updateSceneSelectionState() {
    Q_Q(PianoRollGraphicsView);
    m_selectionChangeBarrier = true;
    q->clearNoteSelections();

    for (const auto id : appStatus->selectedNotes.get()) {
        const auto noteItem = findNoteViewById(id);
        Q_ASSERT(noteItem);
        noteItem->setSelected(true);
    }
    m_selectionChangeBarrier = false;
}

void PianoRollGraphicsViewPrivate::updateOverlappedState() {
    Q_Q(PianoRollGraphicsView);
    for (const auto note : m_notes) {
        const auto noteView = findNoteViewById(note->id());
        Q_ASSERT(noteView);
        noteView->setOverlapped(note->overlapped());
    }
    q->update();
}

void PianoRollGraphicsViewPrivate::updateNoteTimeAndKey(const Note *note) const {
    const auto noteView = findNoteViewById(note->id());
    Q_ASSERT(noteView);
    Helper::updateNoteTimeAndKey(*noteView, *note);
}

void PianoRollGraphicsViewPrivate::updateNoteWord(const Note *note) const {
    const auto noteView = findNoteViewById(note->id());
    Q_ASSERT(noteView);
    Helper::updateNoteWord(*noteView, *note);
}

double PianoRollGraphicsViewPrivate::keyIndexToSceneY(const double index) const {
    Q_Q(const PianoRollGraphicsView);
    return (127 - index) * q->scaleY() * noteHeight;
}

double PianoRollGraphicsViewPrivate::sceneYToKeyIndexDouble(const double y) const {
    Q_Q(const PianoRollGraphicsView);
    return 127 - y / q->scaleY() / noteHeight;
}

int PianoRollGraphicsViewPrivate::sceneYToKeyIndexInt(const double y) const {
    const auto keyIndexD = sceneYToKeyIndexDouble(y);
    auto keyIndex = static_cast<int>(keyIndexD) + 1;
    if (keyIndex > 127)
        keyIndex = 127;
    return keyIndex;
}

void PianoRollGraphicsViewPrivate::moveSelectedNotes(const int startOffset,
                                                     const int keyOffset) const {
    for (const auto note : selectedNoteItems()) {
        note->setStartOffset(startOffset);
        note->setKeyOffset(keyOffset);
    }
}

void PianoRollGraphicsViewPrivate::resetSelectedNotesOffset() const {
    for (const auto note : selectedNoteItems())
        note->resetOffset();
}

void PianoRollGraphicsViewPrivate::updateMoveDeltaKeyRange() {
    auto selectedNotes = selectedNoteItems();
    int highestKey = 0;
    int lowestKey = 127;
    for (const auto note : selectedNotes) {
        const auto key = note->keyIndex();
        if (key > highestKey)
            highestKey = key;
        if (key < lowestKey)
            lowestKey = key;
    }
    m_moveMaxDeltaKey = 127 - highestKey;
    m_moveMinDeltaKey = -lowestKey;
}

void PianoRollGraphicsViewPrivate::resetMoveDeltaKeyRange() {
    m_moveMaxDeltaKey = 127;
    m_moveMinDeltaKey = 0;
}

void PianoRollGraphicsViewPrivate::resizeLeftSelectedNote(const int offset) const {
    // TODO: resize all selected notes
    m_currentEditingNote->setStartOffset(offset);
    m_currentEditingNote->setLengthOffset(-offset);
}

void PianoRollGraphicsViewPrivate::resizeRightSelectedNote(const int offset) const {
    m_currentEditingNote->setLengthOffset(offset);
}

QList<NoteView *> PianoRollGraphicsViewPrivate::selectedNoteItems() const {
    return Linq::where(noteViews, L_PRED(n, n->isSelected()));
}

void PianoRollGraphicsViewPrivate::setPitchEditMode(const bool on, const bool isErase) {
    Q_Q(PianoRollGraphicsView);
    if (on)
        q->setCursor(Qt::ArrowCursor);

    m_isEditPitchMode = on;
    for (const auto note : noteViews)
        note->setEditingPitch(on);
    if (on)
        q->clearNoteSelections();
    m_pitchEditor->setTransparentMouseEvents(!on);
    m_pitchEditor->setEraseMode(isErase);
}

NoteView *PianoRollGraphicsViewPrivate::noteViewAt(const QPoint &pos) {
    Q_Q(PianoRollGraphicsView);
    for (const auto item : q->items(pos))
        if (const auto noteItem = dynamic_cast<NoteView *>(item))
            return noteItem;
    return nullptr;
}

PronunciationView *PianoRollGraphicsViewPrivate::pronViewAt(const QPoint &pos) {
    Q_Q(PianoRollGraphicsView);
    for (const auto item : q->items(pos))
        if (const auto pronView = dynamic_cast<PronunciationView *>(item))
            return pronView;
    return nullptr;
}

// 正在擦除音符时，有可能会取消操作（如按下ESC），
// 某些情况下（如发音更新）仍需要找到并修改它们的属性
NoteView *PianoRollGraphicsViewPrivate::findNoteViewById(const int id) const {
    return MathUtils::findItemById<NoteView *>(noteViews + noteViewsToErase, id);
}

void PianoRollGraphicsViewPrivate::handleNoteInserted(Note *note) {
    Q_Q(PianoRollGraphicsView);
    m_selectionChangeBarrier = true;
    const auto noteView = Helper::buildNoteView(*note);
    noteView->fontPixelSize = q->m_noteFontPixelSize;
    noteView->setEditingPitch(m_isEditPitchMode);
    addNoteViewToScene(noteView);
    m_notes.append(note);
    m_selectionChangeBarrier = false;
}

void PianoRollGraphicsViewPrivate::handleNoteRemoved(Note *note) {
    m_selectionChangeBarrier = true;
    // qDebug() << "PianoRollGraphicsView::removeNote" << note->id() << note->lyric();
    const auto noteView = findNoteViewById(note->id());
    Q_ASSERT(noteView);
    removeNoteViewFromScene(noteView);
    delete noteView;
    m_notes.removeOne(note);
    disconnect(note, nullptr, this, nullptr);
    m_selectionChangeBarrier = false;
}

void PianoRollGraphicsViewPrivate::addNoteViewToScene(NoteView *view) {
    Q_Q(PianoRollGraphicsView);
    q->scene()->addCommonItem(view);
    q->scene()->addCommonItem(view->pronunciationView());
    noteViews.append(view);
}

void PianoRollGraphicsViewPrivate::removeNoteViewFromScene(NoteView *view) {
    Q_Q(PianoRollGraphicsView);
    if (view->scene() == q->scene()) {
        q->scene()->removeCommonItem(view);
        q->scene()->removeCommonItem(view->pronunciationView());
    }
    noteViews.removeOne(view);
}

void PianoRollGraphicsViewPrivate::onHoverEnter(QHoverEvent *event) {
}

void PianoRollGraphicsViewPrivate::onHoverLeave(QHoverEvent *event) {
}

void PianoRollGraphicsViewPrivate::onHoverMove(const QHoverEvent *event) {
    Q_Q(PianoRollGraphicsView);
    if (m_isEditPitchMode || m_mouseDown)
        return;
    const auto noteView = noteViewAt(event->position().toPoint());
    if (!noteView) {
        q->setCursor(Qt::ArrowCursor);
        return;
    }

    const auto scenePos = q->mapToScene(event->position().toPoint());
    const auto rPos = noteView->mapFromScene(scenePos);
    const auto rx = rPos.x();
    if (rx >= 0 && rx <= AppGlobal::resizeTolerance) {
        q->setCursor(Qt::SizeHorCursor);
    } else if (rx >= noteView->rect().width() - AppGlobal::resizeTolerance &&
               rx <= noteView->rect().width()) {
        q->setCursor(Qt::SizeHorCursor);
    } else {
        q->setCursor(Qt::ArrowCursor);
    }
}

void PianoRollGraphicsViewPrivate::onClipPropertyChanged() {
    Q_Q(PianoRollGraphicsView);
    m_offset = m_clip->start();
    q->setOffset(m_offset);
    q->setSceneLength(m_clip->length());
    m_clipRangeOverlay->setClipRange(m_clip->clipStart(), m_clip->clipLen());

    for (const auto note : m_notes) {
        updateNoteTimeAndKey(note);
    }
}

void PianoRollGraphicsViewPrivate::updatePitch(const Param::Type paramType,
                                               const Param &param) const {
    Helper::updatePitch(paramType, param, *m_pitchEditor);
}