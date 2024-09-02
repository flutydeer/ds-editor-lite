//
// Created by fluty on 2023/11/14.
//

#include "TracksGraphicsView.h"

#include "TracksGraphicsScene.h"
#include "Controller/AppController.h"
#include "Controller/TracksViewController.h"
#include "Global/TracksEditorGlobal.h"
#include "GraphicsItem/AbstractClipView.h"
#include "GraphicsItem/TracksBackgroundGraphicsItem.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Dialogs/Base/Dialog.h"
#include "UI/Views/Common/ScrollBarGraphicsItem.h"
#include "Utils/MathUtils.h"

#include <QFileDialog>
#include <QMouseEvent>
#include <QMWidgets/cmenu.h>

TracksGraphicsView::TracksGraphicsView(TracksGraphicsScene *scene, QWidget *parent)
    : TimeGraphicsView(scene, parent), m_scene(scene) {
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("TracksGraphicsView");
    setScaleYMin(0.575);
    // QScroller::grabGesture(m_graphicsView, QScroller::TouchGesture);
    setEnsureSceneFillViewY(false);
    setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    setDragBehavior(DragBehavior::RectSelect);
    setMinimumHeight(0);

    m_actionNewSingingClip = new QAction(tr("New singing clip"), this);
    connect(m_actionNewSingingClip, &QAction::triggered, this,
            &TracksGraphicsView::onNewSingingClip);

    m_actionAddAudioClip = new QAction(tr("Insert audio clip..."), this);
    connect(m_actionAddAudioClip, &QAction::triggered, this, &TracksGraphicsView::onAddAudioClip);

    m_backgroundMenu = new CMenu(this);
    m_backgroundMenu->addAction(m_actionNewSingingClip);
    m_backgroundMenu->addAction(m_actionAddAudioClip);

    connect(appStatus, &AppStatus::activeClipIdChanged, this, [=](int clipId) {
        if (clipId == -1) {
            resetActiveClips();
            return;
        }

        if (auto clipItem = findClipById(clipId)) {
            resetActiveClips();
            clipItem->setActiveClip(true);
        } else
            qFatal() << "Clip not found: " << clipId;
    });
}

void TracksGraphicsView::setQuantize(int quantize) {
    m_quantize = quantize;
}

QList<int> TracksGraphicsView::selectedClipsId() const {
    QList<int> result;
    for (const auto clipItem : selectedClipItems())
        result.append(clipItem->id());
    return result;
}

void TracksGraphicsView::onNewSingingClip() const {
    trackController->onNewSingingClip(m_trackIndex, m_tick);
}

void TracksGraphicsView::onAddAudioClip() {
    auto fileName =
        QFileDialog::getOpenFileName(this, tr("Select an Audio File"), ".",
                                     tr("All Audio File (*.wav *.flac *.mp3);;Wave File "
                                        "(*.wav);;Flac File (*.flac);;MP3 File (*.mp3)"));
    if (fileName.isNull())
        return;
    auto track = appModel->tracks().at(m_trackIndex);
    trackController->onAddAudioClip(fileName, track->id(), m_tick);
}

void TracksGraphicsView::onDeleteTriggered() {
    auto selectedClips = selectedClipItems();
    auto dlg = new Dialog(this);
    dlg->setWindowTitle(tr("Warning"));
    dlg->setTitle(tr("Do you want to delete these clips?"));
    QString msg;
    for (const auto clipItem : selectedClips)
        msg.append(clipItem->name() + "\n");
    dlg->setMessage(msg);
    dlg->setModal(true);

    auto btnDelete = new Button(tr("Delete"));
    connect(btnDelete, &Button::clicked, dlg, &Dialog::accept);
    dlg->setNegativeButton(btnDelete);

    auto btnCancel = new AccentButton(tr("Cancel"));
    connect(btnCancel, &Button::clicked, dlg, &Dialog::reject);
    dlg->setPositiveButton(btnCancel);

    connect(dlg, &Dialog::accepted, this,
            [=] { trackController->onRemoveClips(selectedClipsId()); });

    dlg->show();
}

void TracksGraphicsView::mousePressEvent(QMouseEvent *event) {
    // 在滚动条上按下时，交还给基类处理
    if (dynamic_cast<ScrollBarGraphicsItem *>(itemAt(event->pos()))) {
        CommonGraphicsView::mousePressEvent(event);
        event->ignore();
        return;
    }

    if (auto item = itemAt(event->pos())) {
        if (auto clipItem = dynamic_cast<AbstractClipView *>(item)) {
            qDebug() << "TracksGraphicsView::mousePressEvent mouse down on clip";
            if (selectedClipItems().count() <= 1 || !selectedClipItems().contains(clipItem))
                clearSelections();
            clipItem->setSelected(true);
            trackController->setActiveClip(clipItem->id());
            if (event->button() != Qt::LeftButton) {
                m_mouseMoveBehavior = None;
                setCursor(Qt::ArrowCursor);
            } else {
                prepareForMovingOrResizingClip(event, clipItem);
            }
        } else {
            clearSelections();
            // trackController->setActiveClip(-1);
            CommonGraphicsView::mousePressEvent(event);
        }
    }
    event->ignore();
}

void TracksGraphicsView::mouseMoveEvent(QMouseEvent *event) {
    if (event->modifiers() == Qt::AltModifier)
        m_tempQuantizeOff = true;
    else
        m_tempQuantizeOff = false;

    auto curPos = mapToScene(event->pos());
    if (event->modifiers() == Qt::AltModifier)
        m_tempQuantizeOff = true;
    else
        m_tempQuantizeOff = false;

    auto dx = (curPos.x() - m_mouseDownPos.x()) / scaleX() /
              TracksEditorGlobal::pixelsPerQuarterNote * 480;

    int start;
    int clipStart;
    int left;
    int clipLen;
    int right;
    int delta = qRound(dx);
    int quantize = m_tempQuantizeOff ? 1 : 1920 / m_quantize;
    if (m_mouseMoveBehavior == Move) {
        m_movedBeforeMouseUp = true;
        left = MathUtils::round(m_mouseDownStart + m_mouseDownClipStart + delta, quantize);
        start = left - m_mouseDownClipStart;
        m_currentEditingClip->setStart(start);
    } else if (m_mouseMoveBehavior == ResizeLeft) {
        m_movedBeforeMouseUp = true;
        left = MathUtils::round(m_mouseDownStart + m_mouseDownClipStart + delta, quantize);
        start = m_mouseDownStart;
        clipStart = left - start;
        clipLen = m_mouseDownStart + m_mouseDownClipStart + m_mouseDownClipLen - left;
        if (clipLen <= 0) {
            TimeGraphicsView::mouseMoveEvent(event);
            return;
        }

        if (clipStart < 0) {
            m_currentEditingClip->setClipStart(0);
            m_currentEditingClip->setClipLen(m_mouseDownClipStart + m_mouseDownClipLen);
        } else if (clipStart <= m_mouseDownClipStart + m_mouseDownClipLen) {
            m_currentEditingClip->setClipStart(clipStart);
            m_currentEditingClip->setClipLen(clipLen);
        } else {
            m_currentEditingClip->setClipStart(m_mouseDownClipStart + m_mouseDownClipLen);
            m_currentEditingClip->setClipLen(0);
        }
    } else if (m_mouseMoveBehavior == ResizeRight) {
        m_movedBeforeMouseUp = true;
        right = MathUtils::round(
            m_mouseDownStart + m_mouseDownClipStart + m_mouseDownClipLen + delta, quantize);
        clipLen = right - (m_mouseDownStart + m_mouseDownClipStart);
        if (clipLen <= 0) {
            TimeGraphicsView::mouseMoveEvent(event);
            return;
        }

        auto curClipStart = m_currentEditingClip->clipStart();
        auto curLength = m_currentEditingClip->length();
        if (!m_currentEditingClip->canResizeLength()) {
            if (curClipStart + clipLen >= curLength)
                m_currentEditingClip->setClipLen(curLength - curClipStart);
            else
                m_currentEditingClip->setClipLen(clipLen);
        } else {
            m_currentEditingClip->setLength(curClipStart + clipLen);
            m_currentEditingClip->setClipLen(clipLen);
        }
    }
    TimeGraphicsView::mouseMoveEvent(event);
}

void TracksGraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    // auto scenePos = mapToScene(event->pos());
    // if (m_mouseDownPos == scenePos) {
    //     m_propertyEdited = false;
    // }
    if (m_mouseMoveBehavior != None && m_movedBeforeMouseUp) {
        Clip::ClipCommonProperties args(*m_currentEditingClip);
        trackController->onClipPropertyChanged(args);
    }
    m_mouseMoveBehavior = None;
    m_movedBeforeMouseUp = false;
    m_currentEditingClip = nullptr;
    TimeGraphicsView::mouseReleaseEvent(event);
}

void TracksGraphicsView::mouseDoubleClickEvent(QMouseEvent *event) {
    auto scenePos = mapToScene(event->position().toPoint());
    m_trackIndex = m_scene->trackIndexAt(scenePos.y());
    if (m_trackIndex == -1)
        return;

    auto tick = m_scene->tickAt(scenePos.x());
    if (auto item = itemAt(event->pos())) {
        if (auto clipItem = dynamic_cast<AbstractClipView *>(item)) {
            appController->onPanelClicked(AppGlobal::ClipEditor);
        } else if (dynamic_cast<TracksBackgroundGraphicsItem *>(item)) {
            m_tick = MathUtils::roundDown(tick, 1920 / m_quantize);
            onNewSingingClip();
        }
    }

    CommonGraphicsView::mouseDoubleClickEvent(event);
}

void TracksGraphicsView::contextMenuEvent(QContextMenuEvent *event) {
    auto scenePos = mapToScene(event->pos());
    auto trackIndex = m_scene->trackIndexAt(scenePos.y());
    if (trackIndex == -1)
        return;

    auto tick = m_scene->tickAt(scenePos.x());
    if (auto item = itemAt(event->pos())) {
        if (dynamic_cast<TracksBackgroundGraphicsItem *>(item)) {
            m_trackIndex = trackIndex;
            m_tick = tick;
            m_snappedTick = MathUtils::roundDown(tick, 1920 / m_quantize);
            m_backgroundMenu->exec(event->globalPos());
        } else if (auto clip = dynamic_cast<AbstractClipView *>(item)) {
            qDebug() << "context menu on clip" << clip->id();
            auto actionDelete = new QAction(tr("&Delete"), this);
            connect(actionDelete, &QAction::triggered, this,
                    &TracksGraphicsView::onDeleteTriggered);
            CMenu menu(this);
            menu.addAction(actionDelete);
            menu.exec(event->globalPos());
        } else {
            CommonGraphicsView::contextMenuEvent(event);
        }
    }
}

void TracksGraphicsView::prepareForMovingOrResizingClip(QMouseEvent *event,
                                                        AbstractClipView *clipItem) {
    auto scenePos = mapToScene(event->pos());
    qDebug() << "prepareForMovingOrResizingClip";

    bool ctrlDown = event->modifiers() == Qt::ControlModifier;
    if (!ctrlDown) {
        if (selectedClipItems().count() <= 1 || !selectedClipItems().contains(clipItem))
            clearSelections();
        clipItem->setSelected(true);
    } else {
        clipItem->setSelected(!clipItem->isSelected());
    }
    auto rPos = clipItem->mapFromScene(scenePos);
    auto rx = rPos.x();
    if (rx >= 0 && rx <= AppGlobal::resizeTolarance) {
        // setCursor(Qt::SizeHorCursor);
        m_mouseMoveBehavior = ResizeLeft;
        // qDebug() << "ResizeLeft";
        clearSelections();
        clipItem->setSelected(true);
    } else if (rx >= clipItem->rect().width() - AppGlobal::resizeTolarance &&
               rx <= clipItem->rect().width()) {
        // setCursor(Qt::SizeHorCursor);
        m_mouseMoveBehavior = ResizeRight;
        // qDebug() << "ResizeRight";
        clearSelections();
        clipItem->setSelected(true);
    } else {
        // setCursor(Qt::ArrowCursor);
        m_mouseMoveBehavior = Move;
        // qDebug() << "Move";
    }

    m_currentEditingClip = clipItem;
    m_mouseDownPos = scenePos;
    m_mouseDownStart = m_currentEditingClip->start();
    m_mouseDownClipStart = m_currentEditingClip->clipStart();
    m_mouseDownLength = m_currentEditingClip->length();
    m_mouseDownClipLen = m_currentEditingClip->clipLen();
    m_movedBeforeMouseUp = false;
}

AbstractClipView *TracksGraphicsView::findClipById(int id) {
    for (auto item : m_scene->items())
        if (auto clip = dynamic_cast<AbstractClipView *>(item))
            if (clip->id() == id)
                return clip;
    return nullptr;
}

void TracksGraphicsView::clearSelections() const {
    for (auto item : m_scene->items())
        if (item->isSelected())
            item->setSelected(false);
}

void TracksGraphicsView::resetActiveClips() const {
    for (auto item : m_scene->items())
        if (auto clip = dynamic_cast<AbstractClipView *>(item))
            if (clip->activeClip())
                clip->setActiveClip(false);
}

QList<AbstractClipView *> TracksGraphicsView::selectedClipItems() const {
    QList<AbstractClipView *> result;
    for (auto item : m_scene->items())
        if (auto clip = dynamic_cast<AbstractClipView *>(item))
            if (clip->isSelected())
                result.append(clip);
    return result;
}