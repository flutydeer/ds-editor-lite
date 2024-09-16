//
// Created by fluty on 24-9-16.
//

#include "PianoRollEditorView.h"

#include "Controller/PlaybackController.h"
#include "ParamEditor/ParamEditorGraphicsView.h"
#include "ParamEditor/ParamEditorView.h"
#include "PianoRoll/PianoRollGraphicsView.h"
#include "PianoRoll/PianoRollView.h"

#include <QScrollBar>

PianoRollEditorView::PianoRollEditorView(QWidget *parent) : QSplitter(Qt::Vertical, parent) {
    setObjectName("ClipEditorSplitter");
    setContentsMargins(0, 0, 0, 0);
    // m_splitter->setContentsMargins(6, 0, 6, 0);

    m_pianoRollView = new PianoRollView;
    m_paramEditorView = new ParamEditorView;
    addWidget(m_pianoRollView);
    addWidget(m_paramEditorView);

    setCollapsible(0, false);
    // 让参数面板在剪辑编辑器调整高度时尽量保持高度不变，优先调整钢琴卷帘区域的高度
    setStretchFactor(0, 100);
    setStretchFactor(1, 1);

    auto pianoGraphicsView = m_pianoRollView->graphicsView();
    auto paramGraphicsView = m_paramEditorView->graphicsView();
    connect(playbackController, &PlaybackController::positionChanged, this, [=](double tick) {
        pianoGraphicsView->setPlaybackPosition(tick);
        paramGraphicsView->setPlaybackPosition(tick);
    });
    connect(playbackController, &PlaybackController::lastPositionChanged, this, [=](double tick) {
        pianoGraphicsView->setLastPlaybackPosition(tick);
        paramGraphicsView->setLastPlaybackPosition(tick);
    });
    connect(pianoGraphicsView, &CommonGraphicsView::scaleChanged, [=](double sx, double sy) {
        paramGraphicsView->setScaleX(sx);
        paramGraphicsView->setHBarValue(pianoGraphicsView->hBarValue());
    });
    connect(pianoGraphicsView->horizontalScrollBar(), &QScrollBar::valueChanged, this,
            [=] { paramGraphicsView->setHBarValue(pianoGraphicsView->hBarValue()); });
    connect(paramGraphicsView, &ParamEditorGraphicsView::wheelHorScale, pianoGraphicsView,
            &CommonGraphicsView::onWheelHorScale);
    connect(paramGraphicsView, &ParamEditorGraphicsView::wheelHorScroll, pianoGraphicsView,
            &CommonGraphicsView::onWheelHorScroll);
}

PianoRollView *PianoRollEditorView::pianoRollView() const {
    return m_pianoRollView;
}

ParamEditorView *PianoRollEditorView::paramEditorView() const {
    return m_paramEditorView;
}

void PianoRollEditorView::setDataContext(SingingClip *clip) const {
    m_pianoRollView->setDataContext(clip);
    m_paramEditorView->setDataContext(clip);
}