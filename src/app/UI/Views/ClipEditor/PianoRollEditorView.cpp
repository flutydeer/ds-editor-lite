#include "PianoRollEditorView.h"

#include "Controller/PlaybackController.h"
#include "ParamEditor/ParamEditorGraphicsView.h"
#include "ParamEditor/ParamEditorView.h"
#include "PianoRoll/PianoRollView.h"

#include <QScrollBar>

#include <algorithm>

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
    // The piano roll owns page turning and forwards its horizontal position to this view.
    paramGraphicsView->setAutoTurnPage(false);
    connect(playbackController, &PlaybackController::visualPositionChanged, this,
            [=](const double tick) { m_pianoRollView->setPlaybackPosition(tick); });
    connect(playbackController, &PlaybackController::visualPositionChanged, this,
            [=](const double tick) { paramGraphicsView->setPlaybackPosition(tick); });
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

PianoRollView *PianoRollEditorView::pianoRollView() const {
    return m_pianoRollView;
}

ParamEditorView *PianoRollEditorView::paramEditorView() const {
    return m_paramEditorView;
}

bool PianoRollEditorView::regionVisible(const EditorViewGlobal::Region region) const {
    const auto currentSizes = sizes();
    if (currentSizes.size() < 2)
        return false;
    if (region == EditorViewGlobal::Region::PianoRoll)
        return currentSizes.at(0) > 0;
    if (region == EditorViewGlobal::Region::Parameters)
        return currentSizes.at(1) > 0;
    return false;
}

void PianoRollEditorView::setDataContext(SingingClip *clip) const {
    m_pianoRollView->setDataContext(clip);
    m_paramEditorView->setDataContext(clip);

    const auto paramGraphicsView = m_paramEditorView->graphicsView();
    paramGraphicsView->setScaleX(m_pianoRollView->scaleX());
    paramGraphicsView->setHorizontalBarValue(m_pianoRollView->horizontalBarValue());
}

bool PianoRollEditorView::setRegionVisibility(const bool pianoRollVisible,
                                              const bool parametersVisible) {
    if (!pianoRollVisible && !parametersVisible)
        return false;
    const auto currentSizes = sizes();
    if (currentSizes.size() < 2)
        return false;
    const auto currentPianoRollVisible = currentSizes.at(0) > 0;
    const auto currentParametersVisible = currentSizes.at(1) > 0;
    if (currentPianoRollVisible == pianoRollVisible &&
        currentParametersVisible == parametersVisible) {
        return true;
    }
    const auto total = std::max(3, currentSizes.at(0) + currentSizes.at(1));
    if (pianoRollVisible && parametersVisible)
        setSizes({total * 2 / 3, total / 3});
    else if (pianoRollVisible)
        setSizes({total, 0});
    else
        setSizes({0, total});
    return regionVisible(EditorViewGlobal::Region::PianoRoll) == pianoRollVisible &&
           regionVisible(EditorViewGlobal::Region::Parameters) == parametersVisible;
}

bool PianoRollEditorView::showRegion(const EditorViewGlobal::Region region) {
    if (region != EditorViewGlobal::Region::PianoRoll &&
        region != EditorViewGlobal::Region::Parameters) {
        return false;
    }
    const auto currentSizes = sizes();
    if (currentSizes.size() < 2)
        return false;
    if ((region == EditorViewGlobal::Region::Parameters && currentSizes.at(1) == 0) ||
        (region == EditorViewGlobal::Region::PianoRoll && currentSizes.at(0) == 0)) {
        const auto total = std::max(3, currentSizes.at(0) + currentSizes.at(1));
        setSizes({total * 2 / 3, total / 3});
    }
    return true;
}

bool PianoRollEditorView::focusRegion(const EditorViewGlobal::Region region) {
    if (!showRegion(region))
        return false;
    return region == EditorViewGlobal::Region::PianoRoll ? m_pianoRollView->focusEditor()
                                                         : m_paramEditorView->focusEditor();
}
