//
// Created by fluty on 2023/11/14.
//

#include "TracksGraphicsView.h"

#include "TracksGraphicsScene.h"
#include "Controller/TracksViewController.h"
#include "Global/TracksEditorGlobal.h"
#include "GraphicsItem/AbstractClipGraphicsItem.h"
#include "GraphicsItem/TracksBackgroundGraphicsItem.h"
#include "Model/AppModel.h"
#include "UI/Controls/AccentButton.h"
#include "Utils/MathUtils.h"
#include "UI/Controls/Menu.h"
#include "UI/Dialogs/Base/Dialog.h"

#include <QMouseEvent>
#include <QFileDialog>

TracksGraphicsView::TracksGraphicsView(TracksGraphicsScene *scene, QWidget *parent)
    : TimeGraphicsView(scene, parent), m_scene(scene) {
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("TracksGraphicsView");
    setScaleYMin(0.575);
    // QScroller::grabGesture(m_graphicsView, QScroller::TouchGesture);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setEnsureSceneFillView(false);
    setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    setDragMode(RubberBandDrag);

    m_actionNewSingingClip = new QAction(tr("New singing clip"), this);
    connect(m_actionNewSingingClip, &QAction::triggered, this,
            &TracksGraphicsView::onNewSingingClip);

    m_actionAddAudioClip = new QAction(tr("Insert audio clip"), this);
    connect(m_actionAddAudioClip, &QAction::triggered, this, &TracksGraphicsView::onAddAudioClip);

    m_backgroundMenu = new Menu(this);
    m_backgroundMenu->addAction(m_actionNewSingingClip);
    m_backgroundMenu->addAction(m_actionAddAudioClip);

    // connect(appModel, &AppModel::activeClipChanged, this, [=](Clip *clip) {
    //     clearSelections();
    //     if (clip) {
    //         auto clipItem = findClipById(clip->id());
    //         clipItem->setSelected(true);
    //     }
    // });
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
    if (auto item = itemAt(event->pos())) {
        if (auto clipItem = dynamic_cast<AbstractClipGraphicsItem *>(item)) {
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
            trackController->setActiveClip(-1);
            CommonGraphicsView::mousePressEvent(event);
        }
    }
}
void TracksGraphicsView::mouseDoubleClickEvent(QMouseEvent *event) {
    auto scenePos = mapToScene(event->position().toPoint());
    m_trackIndex = m_scene->trackIndexAt(scenePos.y());
    if (m_trackIndex == -1)
        return;

    auto tick = m_scene->tickAt(scenePos.x());
    if (auto item = itemAt(event->pos())) {
        if (dynamic_cast<TracksBackgroundGraphicsItem *>(item)) {
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
        } else if (auto clip = dynamic_cast<AbstractClipGraphicsItem *>(item)) {
            qDebug() << "context menu on clip" << clip->id();
            auto actionDelete = new QAction(tr("&Delete"), this);
            connect(actionDelete, &QAction::triggered, this,
                    &TracksGraphicsView::onDeleteTriggered);
            Menu menu(this);
            menu.addAction(actionDelete);
            menu.exec(event->globalPos());
        } else {
            CommonGraphicsView::contextMenuEvent(event);
        }
    }
}
void TracksGraphicsView::prepareForMovingOrResizingClip(QMouseEvent *event,
                                                        AbstractClipGraphicsItem *clipItem) {
    auto scenePos = mapToScene(event->pos());
    qDebug() << "prepareForMovingOrResizingClip";
}
AbstractClipGraphicsItem *TracksGraphicsView::findClipById(int id) {
    for (auto item : m_scene->items())
        if (auto clip = dynamic_cast<AbstractClipGraphicsItem *>(item))
            if (clip->id() == id)
                return clip;
    return nullptr;
}
void TracksGraphicsView::clearSelections() {
    for (auto item : m_scene->items())
        if (item->isSelected())
            item->setSelected(false);
}
QList<AbstractClipGraphicsItem *> TracksGraphicsView::selectedClipItems() const {
    QList<AbstractClipGraphicsItem *> result;
    for (auto item : m_scene->items())
        if (auto clip = dynamic_cast<AbstractClipGraphicsItem *>(item))
            if (clip->isSelected())
                result.append(clip);
    return result;
}