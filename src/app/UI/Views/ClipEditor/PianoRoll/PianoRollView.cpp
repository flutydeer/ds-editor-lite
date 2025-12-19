//
// Created by fluty on 24-8-21.
//

#include "PianoRollView.h"

#include "PianoKeyboardView.h"
#include "PianoRollGraphicsScene.h"
#include "PianoRollGraphicsView.h"
#include "PhonemeView.h"
#include "UI/Views/Common/TimeGraphicsView.h"
#include "UI/Views/Common/TimelineView.h"

#include <QLabel>
#include <QVBoxLayout>

PianoRollView::PianoRollView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setMinimumHeight(128);

    m_scene = new PianoRollGraphicsScene;
    m_graphicsView = new PianoRollGraphicsView(m_scene);

    m_timelineView = new TimelineView;
    m_timelineView->setObjectName("pianoRollTimelineView");
    m_timelineView->setTimeRange(m_graphicsView->startTick(), m_graphicsView->endTick());
    m_timelineView->setPixelsPerQuarterNote(pixelsPerQuarterNote);
    m_timelineView->setFixedHeight(timelineViewHeight);

    m_keyboardView = new PianoKeyboardView;
    m_keyboardView->setKeyRange(m_graphicsView->topKeyIndex(), m_graphicsView->bottomKeyIndex());

    m_phonemeView = new PhonemeView;
    m_phonemeView->setTimeRange(m_graphicsView->startTick(), m_graphicsView->endTick());
    m_phonemeView->setFixedHeight(40);
    m_phonemeView->setVisible(false);
    connect(m_graphicsView, &TimeGraphicsView::timeRangeChanged, m_phonemeView,
            &PhonemeView::setTimeRange);
    connect(m_phonemeView, &PhonemeView::wheelHorScale, m_graphicsView,
            &TimeGraphicsView::onWheelHorScale);
    connect(m_phonemeView, &PhonemeView::wheelHorScroll, m_graphicsView,
            &TimeGraphicsView::onWheelHorScroll);

    m_lbTip = new QLabel(tr("Select a singing clip to edit"));
    m_lbTip->setObjectName("lbNullClipTip");
    m_lbTip->setAlignment(Qt::AlignCenter);

    const auto topLeftSpacing = new QWidget();
    topLeftSpacing->setObjectName("pianoRollTopLeftSpacing");
    topLeftSpacing->setMinimumWidth(0);
    topLeftSpacing->setFixedHeight(timelineViewHeight);

    const auto bottomLeftSpacing = new QWidget();
    bottomLeftSpacing->setObjectName("pianoRollBottomLeftSpacing");
    bottomLeftSpacing->setMinimumWidth(0);
    bottomLeftSpacing->setFixedHeight(m_phonemeView->height());

    const auto pianoKeyboardLayout = new QVBoxLayout;
    pianoKeyboardLayout->setContentsMargins(0, 0, 0, 0);
    pianoKeyboardLayout->setSpacing(0);
    // pianoKeyboardLayout->addSpacing(timelineViewHeight);
    pianoKeyboardLayout->addWidget(topLeftSpacing);
    pianoKeyboardLayout->addWidget(m_keyboardView);
    // pianoKeyboardLayout->addSpacing(m_phonemeView->height());
    pianoKeyboardLayout->addWidget(bottomLeftSpacing);

    const auto rightLayout = new QVBoxLayout;
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    rightLayout->addWidget(m_timelineView);
    rightLayout->addWidget(m_graphicsView);
    rightLayout->addWidget(m_phonemeView);

    const auto layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(pianoKeyboardLayout);
    layout->addLayout(rightLayout);
    layout->addWidget(m_lbTip);
    setLayout(layout);

    connect(m_timelineView, &TimelineView::wheelHorScale, m_graphicsView,
            &TimeGraphicsView::onWheelHorScale);
    connect(m_keyboardView, &PianoKeyboardView::wheelScroll, m_graphicsView,
            &TimeGraphicsView::onWheelVerScale);
    connect(m_graphicsView, &TimeGraphicsView::timeRangeChanged, m_timelineView,
            &TimelineView::setTimeRange);
    connect(m_graphicsView, &PianoRollGraphicsView::keyRangeChanged, m_keyboardView,
            &PianoKeyboardView::setKeyRange);
    connect(m_graphicsView, &PianoRollGraphicsView::keyHovered, m_keyboardView,
            &PianoKeyboardView::setHoveredKeyIndex);
    connect(m_graphicsView, &PianoRollGraphicsView::keyHoverCleared, m_keyboardView,
            [this]() { m_keyboardView->setHoveredKeyIndex(-1); });
}

PianoRollGraphicsView *PianoRollView::graphicsView() const {
    return m_graphicsView;
}

void PianoRollView::setDataContext(SingingClip *clip) const {
    m_graphicsView->setDataContext(clip);
    m_phonemeView->setDataContext(clip);
    m_timelineView->setDataContext(clip);

    const bool notNull = clip != nullptr;
    m_timelineView->setVisible(notNull);
    m_graphicsView->setVisible(notNull);
    m_phonemeView->setVisible(notNull);
    m_keyboardView->setVisible(notNull);
    m_lbTip->setVisible(!notNull);
}

void PianoRollView::onEditModeChanged(const PianoRollEditMode mode) const {
    m_graphicsView->setEditMode(mode);
}