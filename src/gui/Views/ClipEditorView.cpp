//
// Created by fluty on 2024/2/10.
//

#include <QVBoxLayout>

#include "ClipEditorView.h"
#include "Controller/TracksViewController.h"
#include "Controls/PianoRoll/PianoRollBackgroundGraphicsItem.h"

ClipEditorView::ClipEditorView(QWidget *parent) : QWidget(parent) {
    m_toolbarView = new ClipEditorToolBarView;
    connect(m_toolbarView, &ClipEditorToolBarView::clipNameChanged, this,
            &ClipEditorView::onClipNameEdited);
    connect(m_toolbarView, &ClipEditorToolBarView::editModeChanged, this,
            &ClipEditorView::onEditModeChanged);

    // auto pitchItem = new PitchEditorGraphicsItem;
    // QObject::connect(pianoRollView, &TracksGraphicsView::visibleRectChanged, pitchItem,
    //                  &PitchEditorGraphicsItem::setVisibleRect);
    // QObject::connect(pianoRollView, &TracksGraphicsView::scaleChanged, pitchItem,
    // &PitchEditorGraphicsItem::setScale); pianoRollScene->addItem(pitchItem);

    m_pianoRollScene = new PianoRollGraphicsScene;
    m_graphicsView = new PianoRollGraphicsView(m_pianoRollScene);
    m_graphicsView->setSceneVisibility(false);
    m_graphicsView->setDragMode(QGraphicsView::RubberBandDrag);
    m_graphicsView->setPixelsPerQuarterNote(PianoRollGlobal::pixelsPerQuarterNote);
    connect(m_graphicsView, &PianoRollGraphicsView::scaleChanged, m_pianoRollScene,
            &PianoRollGraphicsScene::setScale);

    auto gridItem = new PianoRollBackgroundGraphicsItem;
    gridItem->setPixelsPerQuarterNote(PianoRollGlobal::pixelsPerQuarterNote);
    connect(m_graphicsView, &PianoRollGraphicsView::visibleRectChanged, gridItem,
            &TimeGridGraphicsItem::setVisibleRect);
    connect(m_graphicsView, &PianoRollGraphicsView::scaleChanged, gridItem,
            &PianoRollBackgroundGraphicsItem::setScale);
    auto appModel = AppModel::instance();
    connect(appModel, &AppModel::modelChanged, gridItem, [=] {
        gridItem->setTimeSignature(appModel->timeSignature().numerator,
                                   appModel->timeSignature().denominator);
    });
    connect(appModel, &AppModel::timeSignatureChanged, gridItem,
            &TimeGridGraphicsItem::setTimeSignature);
    m_pianoRollScene->addTimeGrid(gridItem);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_toolbarView);
    mainLayout->addWidget(m_graphicsView);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins({});
    setLayout(mainLayout);
}
void ClipEditorView::onModelChanged() {
    reset();
}
void ClipEditorView::onSelectedClipChanged(DsTrack *track, DsClip *clip) {
    reset();

    if (track == nullptr || clip == nullptr) {
        qDebug() << "ClipEditorView::setIsSingingClip null";
        m_toolbarView->setClipName("");
        m_toolbarView->setClipPropertyEditorEnabled(false);
        m_toolbarView->setPianoRollEditToolsEnabled(false);

        m_graphicsView->setIsSingingClip(false);
        m_graphicsView->setSceneVisibility(false);

        if (m_track != nullptr) {
            qDebug() << "disconnect track and ClipEditorView";
            disconnect(m_track, &DsTrack::clipChanged, this, &ClipEditorView::handleClipChanged);
        }
        m_track = nullptr;
        m_clip = nullptr;
        return;
    }

    m_track = track;
    m_clip = clip;
    qDebug() << "connect track and ClipEditorView";
    connect(m_track, &DsTrack::clipChanged, this, &ClipEditorView::handleClipChanged);
    m_toolbarView->setClipName(clip->name());
    m_toolbarView->setClipPropertyEditorEnabled(true);

    if (clip->type() != DsClip::Singing)
        return;
    m_toolbarView->setPianoRollEditToolsEnabled(true);
    m_graphicsView->setIsSingingClip(true);
    m_graphicsView->setSceneVisibility(true);
    auto singingClip = dynamic_cast<DsSingingClip *>(m_clip);
    if (singingClip->notes().count() <= 0)
        return;
    for (const auto note : singingClip->notes()) {
        m_graphicsView->insertNote(note);
    }
    auto firstNote = singingClip->notes().at(0);
    qDebug() << "first note start" << firstNote->start();
    m_graphicsView->setViewportCenterAt(firstNote->start(), firstNote->keyIndex());
}
void ClipEditorView::onClipNameEdited(const QString &name) {
    DsClip::ClipCommonProperties args;
    args.name = name;
    args.id = m_clip->id();
    args.start = m_clip->start();
    args.clipStart = m_clip->clipStart();
    args.length = m_clip->length();
    args.clipLen = m_clip->clipLen();
    args.gain = m_clip->gain();
    args.mute = m_clip->mute();
    args.trackIndex = AppModel::instance()->tracks().indexOf(m_track);

    TracksViewController::instance()->onClipPropertyChanged(args);
}
void ClipEditorView::handleClipChanged(DsTrack::ClipChangeType type, int id, DsClip *clip) {
    if (m_clip == nullptr)
        return;

    if (id == m_clip->id()) {
        if (type == DsTrack::PropertyChanged) {
            handleClipPropertyChange();
        }
    }
}
void ClipEditorView::onEditModeChanged(PianoRollEditMode mode) {
    m_mode = mode;
    m_graphicsView->setEditMode(m_mode);
}
void ClipEditorView::reset() {
    m_graphicsView->reset();
}
void ClipEditorView::handleClipPropertyChange() {
    // qDebug() << "handleClipPropertyChange" << m_clip->id() << m_clip->start();
}