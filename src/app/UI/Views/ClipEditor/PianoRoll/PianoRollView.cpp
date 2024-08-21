//
// Created by fluty on 24-8-21.
//

#include "PianoRollView.h"

#include "PianoKeyboardView.h"
#include "PianoRollGraphicsScene.h"
#include "PianoRollGraphicsView.h"
#include "UI/Views/Common/CommonGraphicsView.h"
#include "UI/Views/Common/TimelineView.h"

#include <QLabel>
#include <QVBoxLayout>

PianoRollView::PianoRollView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);

    m_scene = new PianoRollGraphicsScene;
    m_graphicsView = new PianoRollGraphicsView(m_scene);
    m_graphicsView->setSceneVisibility(false);
    m_graphicsView->setDragMode(QGraphicsView::RubberBandDrag);

    m_timelineView = new TimelineView;
    m_timelineView->setTimeRange(m_graphicsView->startTick(), m_graphicsView->endTick());
    m_timelineView->setPixelsPerQuarterNote(pixelsPerQuarterNote);
    m_timelineView->setFixedHeight(timelineViewHeight);

    m_keyboardView = new PianoKeyboardView;
    m_keyboardView->setKeyRange(m_graphicsView->topKeyIndex(), m_graphicsView->bottomKeyIndex());

    m_lbTip = new QLabel(tr("Select a singing clip to edit"));
    m_lbTip->setObjectName("lbNullClipTip");
    m_lbTip->setAlignment(Qt::AlignCenter);

    auto pianoKeyboardLayout = new QVBoxLayout;
    pianoKeyboardLayout->setContentsMargins(0, 0, 0, 0);
    pianoKeyboardLayout->setSpacing(0);
    pianoKeyboardLayout->addSpacing(timelineViewHeight);
    pianoKeyboardLayout->addWidget(m_keyboardView);
    pianoKeyboardLayout->addSpacing(AppGlobal::horizontalScrollBarHeight);

    auto timelineAndPianoRollLayout = new QVBoxLayout;
    timelineAndPianoRollLayout->setContentsMargins(0, 0, 0, 0);
    timelineAndPianoRollLayout->setSpacing(0);
    timelineAndPianoRollLayout->addWidget(m_timelineView);
    timelineAndPianoRollLayout->addWidget(m_graphicsView);

    auto layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(pianoKeyboardLayout);
    layout->addLayout(timelineAndPianoRollLayout);
    layout->addWidget(m_lbTip);
    setLayout(layout);

    connect(m_timelineView, &TimelineView::wheelHorScale, m_graphicsView,
            &CommonGraphicsView::onWheelHorScale);
    connect(m_graphicsView, &TimeGraphicsView::timeRangeChanged, m_timelineView,
            &TimelineView::setTimeRange);
    connect(m_graphicsView, &PianoRollGraphicsView::keyRangeChanged, m_keyboardView,
            &PianoKeyboardView::setKeyRange);
}

PianoRollGraphicsView *PianoRollView::graphicsView() const {
    return m_graphicsView;
}

void PianoRollView::setDataContext(SingingClip *clip) const {
    m_graphicsView->setDataContext(clip);

    const bool notNull = clip != nullptr;
    m_timelineView->setVisible(notNull);
    m_graphicsView->setVisible(notNull);
    m_keyboardView->setVisible(notNull);
    m_lbTip->setVisible(!notNull);
}

void PianoRollView::onEditModeChanged(PianoRollEditMode mode) const {
    m_graphicsView->setEditMode(mode);
}