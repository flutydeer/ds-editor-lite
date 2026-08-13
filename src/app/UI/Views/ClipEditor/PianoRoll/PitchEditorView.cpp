#include "PitchEditorView.h"

#include "UI/Views/ClipEditor/AnchorEditor/AnchorEditController.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include <lite/ProjectModel/Utils/AppModelUtils.h>
#include <lite/Support/MathUtils.h>

#include <QPainter>
#include <QPainterPath>

#include <algorithm>

PitchEditorView::PitchEditorView() : CommonParamEditorView(m_properties) {
    setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
}

PitchEditorView::~PitchEditorView() {
    qDeleteAll(m_mergedCurves);
}

void PitchEditorView::setDisplayMode(const PitchDisplayMode mode) {
    if (m_displayMode == mode)
        return;
    m_displayMode = mode;
    rebuildMergedCurves();
    update();
}

void PitchEditorView::setAnchorOverlayState(const AnchorEditor::AnchorOverlayState *state) {
    m_anchorState = state;
    update();
}

void PitchEditorView::loadOriginal(const QList<DrawCurve *> &curves) {
    CommonParamEditorView::loadOriginal(curves);
    rebuildMergedCurves();
}

void PitchEditorView::loadEdited(const QList<DrawCurve *> &curves) {
    CommonParamEditorView::loadEdited(curves);
    rebuildMergedCurves();
}

void PitchEditorView::clearParams() {
    CommonParamEditorView::clearParams();
    qDeleteAll(m_mergedCurves);
    m_mergedCurves.clear();
}

void PitchEditorView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                            QWidget *widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget)

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(Qt::NoBrush);

    const auto anchorCoverage = m_anchorState ? PitchDisplayStrategy::anchorCoverage(*m_anchorState)
                                              : QList<PitchDisplayInterval>();
    const auto layers =
        PitchDisplayStrategy::displayLayers(m_displayMode, editedCurves(), anchorCoverage);
    for (const auto &layer : layers) {
        const QList<DrawCurve *> *curves = nullptr;
        switch (layer.curveSource) {
            case PitchDisplayCurveSource::Original:
                curves = &originalCurves();
                break;
            case PitchDisplayCurveSource::Edited:
                curves = &editedCurves();
                break;
            case PitchDisplayCurveSource::Merged:
                curves = &m_mergedCurves;
                break;
        }
        auto color = layer.colorRole == PitchDisplayColorRole::Original ? originalCurveColor()
                                                                        : editedCurveColor();
        color.setAlpha(std::min(color.alpha(), layer.maximumAlpha));
        drawCurveLayer(painter, *curves, color, layer.hiddenCoverage, layer.dashedCoverage);
    }
}

void PitchEditorView::drawGraduates(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                    QWidget *widget) {
}

QPainterPath PitchEditorView::coveragePath(const QList<PitchDisplayInterval> &coverage) const {
    QPainterPath path;
    const auto verticalMargin = 4.0;
    const auto top = rect().top() - verticalMargin;
    const auto height = rect().height() + verticalMargin * 2.0;
    for (const auto &interval : coverage) {
        const auto left = tickToItemX(interval.startTick);
        const auto right = tickToItemX(interval.endTick);
        if (right <= rect().left() || left >= rect().right())
            continue;
        path.addRect(QRectF(std::max(left, rect().left()), top,
                            std::min(right, rect().right()) - std::max(left, rect().left()),
                            height));
    }
    return path;
}

void PitchEditorView::drawCurveLayer(QPainter *painter, const QList<DrawCurve *> &curves,
                                     const QColor &color,
                                     const QList<PitchDisplayInterval> &hiddenCoverage,
                                     const QList<PitchDisplayInterval> &dashedCoverage) const {
    if (curves.isEmpty())
        return;

    QPainterPath fullPath;
    fullPath.addRect(rect().adjusted(-2, -2, 2, 2));
    auto visiblePath = fullPath;
    if (!hiddenCoverage.isEmpty())
        visiblePath = visiblePath.subtracted(coveragePath(hiddenCoverage));
    if (visiblePath.isEmpty())
        return;

    const auto dashedPath = visiblePath.intersected(coveragePath(dashedCoverage));
    const auto solidPath = visiblePath.subtracted(dashedPath);

    QPen pen(color, 1.5);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::RoundJoin);
    if (!solidPath.isEmpty()) {
        painter->save();
        painter->setClipPath(solidPath, Qt::IntersectClip);
        painter->setPen(pen);
        drawCurveBorder(painter, curves);
        painter->restore();
    }
    if (!dashedPath.isEmpty()) {
        painter->save();
        painter->setClipPath(dashedPath, Qt::IntersectClip);
        pen.setStyle(Qt::CustomDashLine);
        pen.setDashPattern({4.0, 3.0});
        painter->setPen(pen);
        drawCurveBorder(painter, curves);
        painter->restore();
    }
}

void PitchEditorView::rebuildMergedCurves() {
    qDeleteAll(m_mergedCurves);
    m_mergedCurves = AppModelUtils::mergeCurves(originalCurves(), editedCurves());
}

double PitchEditorView::valueToSceneY(const double value) const {
    constexpr int min = 0;
    constexpr int max = 12700;
    return (12700 - MathUtils::clip(value, min, max) + 50) * scaleY() *
           ClipEditorGlobal::noteHeight / 100;
}

double PitchEditorView::sceneYToValue(const double y) const {
    constexpr int min = 0;
    constexpr int max = 12700;
    const auto value = -(y * 100 / ClipEditorGlobal::noteHeight / scaleY() - 12700 - 50);
    return MathUtils::clip(value, min, max);
}
