#include "PianoRollEditorView.h"

#include "Controller/EditorViewController.h"
#include "Controller/PlaybackController.h"
#include "ParamEditor/ParamEditorGraphicsView.h"
#include "ParamEditor/ParamEditorView.h"
#include "PianoRoll/PianoRollView.h"

#include <QScrollBar>

PianoRollEditorView::PianoRollEditorView(QWidget *parent) : OverlaySplitter(Qt::Vertical, parent) {
    setContentsMargins(0, 0, 0, 0);

    m_pianoRollView = new PianoRollView;
    m_paramEditorView = new ParamEditorView;
    addWidget(m_pianoRollView);
    addWidget(m_paramEditorView);
    editorViewController->registerInteractionArea(m_paramEditorView, AppGlobal::ClipEditor,
                                                  EditorInteraction::Target::Parameters);

    setCollapsible(0, false);
    // 让参数面板在剪辑编辑器调整高度时尽量保持高度不变，优先调整钢琴卷帘区域的高度
    setStretchFactor(0, 100);
    setStretchFactor(1, 1);

    const auto paramGraphicsView = m_paramEditorView->graphicsView();
    // The piano roll owns page turning and forwards its horizontal position to this view.
    paramGraphicsView->setAutoTurnPage(false);
    connect(playbackController, &PlaybackController::visualPositionChanged, this,
            [=](const double tick) {
                m_pianoRollView->setPlaybackPosition(tick);
            });
    connect(playbackController, &PlaybackController::visualPositionChanged, this,
            [=](const double tick) {
                paramGraphicsView->setPlaybackPosition(tick);
            });
    connect(playbackController, &PlaybackController::lastPositionChanged, this,
            [=](const double tick) {
                m_pianoRollView->setLastPlaybackPosition(tick);
                paramGraphicsView->setLastPlaybackPosition(tick);
            });
    connect(m_pianoRollView, &PianoRollView::scaleChanged, [=](const double sx) {
        paramGraphicsView->setScaleX(sx);
        paramGraphicsView->setHorizontalBarValue(m_pianoRollView->horizontalBarValue());
    });
    connect(m_pianoRollView, &PianoRollView::horizontalBarValueChanged, this, [=] {
        paramGraphicsView->setHorizontalBarValue(m_pianoRollView->horizontalBarValue());
    });
    connect(paramGraphicsView, &ParamEditorGraphicsView::wheelHorScale, m_pianoRollView,
            &PianoRollView::onWheelHorScale);
    connect(paramGraphicsView, &ParamEditorGraphicsView::wheelHorScroll, m_pianoRollView,
            &PianoRollView::onWheelHorScroll);
}

PianoRollEditorView::~PianoRollEditorView() {
    editorViewController->unregisterInteractionArea(m_paramEditorView);
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

    const auto paramGraphicsView = m_paramEditorView->graphicsView();
    paramGraphicsView->setScaleX(m_pianoRollView->scaleX());
    paramGraphicsView->setHorizontalBarValue(m_pianoRollView->horizontalBarValue());
}
