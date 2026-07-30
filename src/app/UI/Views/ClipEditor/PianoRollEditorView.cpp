//
// Created by fluty on 24-9-16.
//

#include "PianoRollEditorView.h"

#include "Controller/PlaybackController.h"
#include "ParamEditor/ParamEditorGraphicsView.h"
#include "ParamEditor/ParamEditorView.h"
#include "PianoRoll/IPianoRollCanvas.h"
#include "PianoRoll/PianoRollView.h"

#include <QScrollBar>

PianoRollEditorView::PianoRollEditorView(QWidget *parent) : OverlaySplitter(Qt::Vertical, parent) {
    setContentsMargins(0, 0, 0, 0);

    m_pianoRollView = new PianoRollView;
    m_paramEditorView = new ParamEditorView;
    addWidget(m_pianoRollView);
    addWidget(m_paramEditorView);

    setCollapsible(0, false);
    // 让参数面板在剪辑编辑器调整高度时尽量保持高度不变，优先调整钢琴卷帘区域的高度
    setStretchFactor(0, 100);
    setStretchFactor(1, 1);

    const auto paramGraphicsView = m_paramEditorView->graphicsView();
    connect(playbackController, &PlaybackController::positionChanged, this, [=](const double tick) {
        m_pianoRollView->setPlaybackPosition(tick);
        paramGraphicsView->setPlaybackPosition(tick);
    });
    connect(playbackController, &PlaybackController::lastPositionChanged, this,
            [=](const double tick) {
                m_pianoRollView->setLastPlaybackPosition(tick);
                paramGraphicsView->setLastPlaybackPosition(tick);
            });
    connect(m_pianoRollView, &PianoRollView::canvasScaleChanged, this, [=](const double sx) {
        paramGraphicsView->setScaleX(sx);
        paramGraphicsView->setHorizontalBarValue(m_pianoRollView->canvas()->horizontalBarValue());
    });
    connect(m_pianoRollView, &PianoRollView::horizontalScrollValueChanged, this, [=] {
        paramGraphicsView->setHorizontalBarValue(m_pianoRollView->canvas()->horizontalBarValue());
    });
    connect(paramGraphicsView, &ParamEditorGraphicsView::wheelHorScale, m_pianoRollView,
            &PianoRollView::onWheelHorScale);
    connect(paramGraphicsView, &ParamEditorGraphicsView::wheelHorScroll, m_pianoRollView,
            &PianoRollView::onWheelHorScroll);
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

    const auto pianoCanvas = m_pianoRollView->canvas();
    const auto paramGraphicsView = m_paramEditorView->graphicsView();
    paramGraphicsView->setScaleX(pianoCanvas->scaleX());
    paramGraphicsView->setHorizontalBarValue(pianoCanvas->horizontalBarValue());
}
