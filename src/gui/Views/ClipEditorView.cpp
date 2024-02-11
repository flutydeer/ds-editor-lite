//
// Created by fluty on 2024/2/10.
//

#include <QVBoxLayout>

#include "ClipEditorView.h"
#include "Controller/ClipEditorViewController.h"
#include "Controller/PlaybackController.h"
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
    m_pianoRollView = new PianoRollGraphicsView(m_pianoRollScene);
    m_pianoRollView->setSceneVisibility(false);
    m_pianoRollView->setDragMode(QGraphicsView::RubberBandDrag);
    m_pianoRollView->setPixelsPerQuarterNote(PianoRollGlobal::pixelsPerQuarterNote);
    connect(m_pianoRollView, &PianoRollGraphicsView::scaleChanged, m_pianoRollScene,
            &PianoRollGraphicsScene::setScale);

    auto gridItem = new PianoRollBackgroundGraphicsItem;
    gridItem->setPixelsPerQuarterNote(PianoRollGlobal::pixelsPerQuarterNote);
    connect(m_pianoRollView, &PianoRollGraphicsView::visibleRectChanged, gridItem,
            &TimeGridGraphicsItem::setVisibleRect);
    connect(m_pianoRollView, &PianoRollGraphicsView::scaleChanged, gridItem,
            &PianoRollBackgroundGraphicsItem::setScale);
    auto appModel = AppModel::instance();
    connect(appModel, &AppModel::modelChanged, gridItem, [=] {
        gridItem->setTimeSignature(appModel->timeSignature().numerator,
                                   appModel->timeSignature().denominator);
    });
    connect(appModel, &AppModel::timeSignatureChanged, gridItem,
            &TimeGridGraphicsItem::setTimeSignature);
    m_pianoRollScene->addTimeGrid(gridItem);

    m_timelineView = new TimelineView;
    m_timelineView->setTimeRange(m_pianoRollView->startTick(), m_pianoRollView->endTick());
    m_timelineView->setPixelsPerQuarterNote(PianoRollGlobal::pixelsPerQuarterNote);
    m_timelineView->setFixedHeight(timelineViewHeight);
    m_timelineView->setVisible(false);
    connect(m_timelineView, &TimelineView::wheelHorScale, m_pianoRollView,
            &CommonGraphicsView::onWheelHorScale);
    auto playbackController = PlaybackController::instance();
    connect(m_timelineView, &TimelineView::setLastPositionTriggered, playbackController,
            [=](double tick) {
                playbackController->setLastPosition(tick);
                playbackController->setPosition(tick);
            });
    connect(appModel, &AppModel::modelChanged, m_timelineView, [=] {
        m_timelineView->setTimeSignature(appModel->timeSignature().numerator,
                                         appModel->timeSignature().denominator);
    });
    connect(appModel, &AppModel::timeSignatureChanged, m_timelineView,
            &TimelineView::setTimeSignature);
    connect(m_pianoRollView, &TimeGraphicsView::timeRangeChanged, m_timelineView,
            &TimelineView::setTimeRange);
    connect(appModel, &AppModel::quantizeChanged, m_timelineView, &TimelineView::setQuantize);

    connect(playbackController, &PlaybackController::positionChanged, this,
            &ClipEditorView::onPositionChanged);
    connect(playbackController, &PlaybackController::lastPositionChanged, this,
            &ClipEditorView::onLastPositionChanged);

    connect(m_pianoRollView, &PianoRollGraphicsView::removeNoteTriggered, this,
            &ClipEditorView::onRemoveSelectedNotes);
    connect(m_pianoRollView, &PianoRollGraphicsView::editNoteLyricTriggered, this,
            &ClipEditorView::onEidtSelectedNotesLyrics);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_toolbarView);
    mainLayout->addWidget(m_timelineView);
    mainLayout->addWidget(m_pianoRollView);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins({});
    setLayout(mainLayout);
}
void ClipEditorView::onModelChanged() {
    reset();
}
void ClipEditorView::onSelectedClipChanged(Track *track, Clip *clip) {
    reset();

    if (track == nullptr || clip == nullptr) {
        qDebug() << "ClipEditorView::setIsSingingClip null";
        ClipEditorViewController::instance()->setCurrentSingingClip(nullptr);
        m_toolbarView->setClipName("");
        m_toolbarView->setClipPropertyEditorEnabled(false);
        m_toolbarView->setPianoRollEditToolsEnabled(false);

        m_pianoRollView->setIsSingingClip(false);
        m_pianoRollView->setSceneVisibility(false);

        m_timelineView->setVisible(false);
        if (m_track != nullptr) {
            qDebug() << "disconnect track and ClipEditorView";
            disconnect(m_track, &Track::clipChanged, this, &ClipEditorView::onClipChanged);
        }
        if (m_singingClip != nullptr)
            disconnect(m_singingClip, &DsSingingClip::noteChanged, this,
                       &ClipEditorView::onNoteChanged);
        m_track = nullptr;
        m_clip = nullptr;
        m_singingClip = nullptr;
        return;
    }

    m_track = track;
    m_clip = clip;
    qDebug() << "connect track and ClipEditorView";
    connect(m_track, &Track::clipChanged, this, &ClipEditorView::onClipChanged);
    connect(m_pianoRollView, &TimeGraphicsView::timeRangeChanged, m_timelineView,
            &TimelineView::setTimeRange);
    m_toolbarView->setClipName(clip->name());
    m_toolbarView->setClipPropertyEditorEnabled(true);

    if (clip->type() != Clip::Singing)
        return;
    m_toolbarView->setPianoRollEditToolsEnabled(true);
    m_pianoRollView->setIsSingingClip(true);
    m_pianoRollView->setSceneVisibility(true);
    m_timelineView->setVisible(true);
    m_singingClip = dynamic_cast<DsSingingClip *>(m_clip);
    if (m_singingClip->notes().count() <= 0)
        return;
    for (const auto note : m_singingClip->notes()) {
        m_pianoRollView->insertNote(note);
    }
    auto firstNote = m_singingClip->notes().at(0);
    qDebug() << "first note start" << firstNote->start();
    m_pianoRollView->setViewportCenterAt(firstNote->start(), firstNote->keyIndex());
    connect(m_singingClip, &DsSingingClip::noteChanged, this, &ClipEditorView::onNoteChanged);
    ClipEditorViewController::instance()->setCurrentSingingClip(m_singingClip);
}
void ClipEditorView::onClipNameEdited(const QString &name) {
    Clip::ClipCommonProperties args;
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
void ClipEditorView::onClipChanged(Track::ClipChangeType type, int id, Clip *clip) {
    if (m_clip == nullptr)
        return;

    if (id == m_clip->id()) {
        if (type == Track::PropertyChanged) {
            onClipPropertyChanged();
        }
    }
}
void ClipEditorView::onEditModeChanged(PianoRollEditMode mode) {
    m_mode = mode;
    m_pianoRollView->setEditMode(m_mode);
}
void ClipEditorView::onPositionChanged(double tick) {
    m_timelineView->setPosition(tick);
    m_pianoRollView->setPlaybackPosition(tick);
}
void ClipEditorView::onLastPositionChanged(double tick) {
    m_pianoRollView->setLastPlaybackPosition(tick);
}
void ClipEditorView::onRemoveSelectedNotes() {
    auto notes = m_pianoRollView->selectedNotesId();
    ClipEditorViewController::instance()->onRemoveNotes(notes);
}
void ClipEditorView::onEidtSelectedNotesLyrics() {
    qDebug() << "ClipEditorView::onEidtSelectedNotesLyrics";
    auto notes = m_pianoRollView->selectedNotesId();
    ClipEditorViewController::instance()->onEditNotesLyrics(notes);
}
void ClipEditorView::reset() {
    m_pianoRollView->reset();
}
void ClipEditorView::onClipPropertyChanged() {
    qDebug() << "ClipEditorView::handleClipPropertyChange" << m_clip->id() << m_clip->start();
    reset();
    auto singingClip = dynamic_cast<DsSingingClip *>(m_clip);
    if (singingClip->notes().count() <= 0)
        return;
    for (const auto note : singingClip->notes()) {
        m_pianoRollView->insertNote(note);
    }
}
void ClipEditorView::onNoteChanged(DsSingingClip::NoteChangeType type, int id, Note *note) {
    switch (type) {
        case DsSingingClip::Inserted:
            m_pianoRollView->insertNote(note);
            break;
        case DsSingingClip::PropertyChanged:
            m_pianoRollView->updateNote(note);
            break;
        case DsSingingClip::Removed:
            m_pianoRollView->removeNote(id);
            break;
    }
}