#include "CommonParamEditorView.h"

#include "ClipEditorGlobal.h"
#include "CurveRenderUtils.h"
#include <lite/ProjectModel/AppModel/AnchorCurve.h>
#include <lite/ProjectModel/AppModel/ParamProperties.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include "UI/Views/Common/TimeGraphicsScene.h"
#include "UI/Views/ClipEditor/PianoRoll/NoteView.h"
#include "UI/Utils/AppColorPalette.h"
#include <lite/ProjectModel/Utils/AppModelUtils.h>
#include <lite/Support/MathUtils.h>

#include <QElapsedTimer>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

CommonParamEditorView::CommonParamEditorView(const ParamProperties &properties)
    : m_properties(&properties) {
    // setBackgroundColor(Qt::transparent);
    setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
}

CommonParamEditorView::~CommonParamEditorView() {
    qDeleteAll(m_drawCurvesEditedBak);
    qDeleteAll(m_anchorCurvesEdited);
    qDeleteAll(m_drawCurvesEdited);
    qDeleteAll(m_drawCurvesOriginal);
}

void CommonParamEditorView::setParamProperties(const ParamProperties &properties) {
    clearParams();
    m_properties = &properties;
}

void CommonParamEditorView::loadOriginal(const QList<DrawCurve *> &curves) {
    AppModelUtils::copyCurves(curves, m_drawCurvesOriginal);
    update();
}

void CommonParamEditorView::loadEdited(const QList<DrawCurve *> &curves) {
    AppModelUtils::copyCurves(curves, m_drawCurvesEdited);
    update();
}

void CommonParamEditorView::loadAnchorEdited(const QList<AnchorCurve *> &curves) {
    qDeleteAll(m_anchorCurvesEdited);
    m_anchorCurvesEdited.clear();
    for (const auto *curve : curves) {
        if (curve) {
            if (auto *sampled = curve->toDrawCurve())
                m_anchorCurvesEdited.append(sampled);
        }
    }
    update();
}

void CommonParamEditorView::clearParams() {
    for (const auto curve : m_drawCurvesOriginal)
        delete curve;
    for (const auto curve : m_drawCurvesEdited)
        delete curve;
    for (const auto curve : m_anchorCurvesEdited)
        delete curve;
    m_drawCurvesOriginal.clear();
    m_drawCurvesEdited.clear();
    m_anchorCurvesEdited.clear();
    update();
}

void CommonParamEditorView::cancelEdit() {
    if (cancelEditState())
        update();
}

void CommonParamEditorView::setEraseMode(const bool on) {
    m_eraseMode = on;
    if (on)
        m_bakeMode = false;
    update();
}

void CommonParamEditorView::setBakeMode(const bool on) {
    m_bakeMode = on;
    if (on)
        m_eraseMode = false;
    update();
}

void CommonParamEditorView::setBaseCurveVisible(const bool visible) {
    if (m_baseCurveVisible == visible)
        return;
    m_baseCurveVisible = visible;
    update();
}

const QList<DrawCurve *> &CommonParamEditorView::editedCurves() const {
    return m_drawCurvesEdited;
}

double CommonParamEditorView::sceneYForValue(const double value) const {
    return valueToSceneY(value);
}

double CommonParamEditorView::valueAtSceneY(const double y) const {
    return sceneYToValue(y);
}

const QList<DrawCurve *> &CommonParamEditorView::originalCurves() const {
    return m_drawCurvesOriginal;
}

QColor CommonParamEditorView::graduateColor() const {
    return m_graduateColor;
}

void CommonParamEditorView::setGraduateColor(const QColor &color) {
    if (m_graduateColor == color)
        return;
    m_graduateColor = color;
    update();
}

QColor CommonParamEditorView::originalCurveColor() const {
    return m_originalCurveColor;
}

void CommonParamEditorView::setOriginalCurveColor(const QColor &color) {
    if (m_originalCurveColor == color)
        return;
    m_originalCurveColor = color;
    update();
}

QColor CommonParamEditorView::editedCurveColor() const {
    return m_editedCurveColor;
}

void CommonParamEditorView::setEditedCurveColor(const QColor &color) {
    if (m_editedCurveColor == color)
        return;
    m_editedCurveColor = color;
    update();
}

QColor CommonParamEditorView::backgroundLayerColor() const {
    return m_backgroundLayerColor;
}

void CommonParamEditorView::setBackgroundLayerColor(const QColor &color) {
    if (m_backgroundLayerColor == color)
        return;
    m_backgroundLayerColor = color;
    update();
}

bool CommonParamEditorView::useTrackColorForEditedCurve() const {
    return m_useTrackColorForEditedCurve;
}

void CommonParamEditorView::setUseTrackColorForEditedCurve(const bool on) {
    if (m_useTrackColorForEditedCurve == on)
        return;
    m_useTrackColorForEditedCurve = on;
    update();
}

QColor CommonParamEditorView::resolvedEditedCurveColor() const {
    if (!m_useTrackColorForEditedCurve)
        return m_editedCurveColor;
    return AppColorPalette::instance()->paramLine(NoteView::trackColorIndex());
}

void CommonParamEditorView::discardAction() {
    if (!cancelEditState()) {
        return;
    }
    emit editDiscarded();
    update();
}

void CommonParamEditorView::commitAction() {
    if (m_mouseMoved) {
        qDebug() << "Edit completed";
        emit editCompleted(editedCurves());
    }

    m_drawStroke = {};
    m_bakeSource.clear();
    m_editType = None;
    m_mouseMoved = false;
    cancelRequested = false;
    for (const auto curve : m_drawCurvesEditedBak)
        delete curve;
    m_drawCurvesEditedBak.clear();
    m_mouseDown = false;
    m_mouseDownButton = Qt::NoButton;
    update();
    emit editCommitted();
}

double CommonParamEditorView::valueToSceneY(const double value) const {
    const auto yMin = paddingTopBottom;
    const auto yMax = scene()->height() - paddingTopBottom;
    const auto availableHeight = yMax - yMin;
    const auto finiteValue = std::isfinite(value) ? value : m_properties->defaultValue;
    const auto clippedValue =
        MathUtils::clip(finiteValue, m_properties->minimum, m_properties->maximum);
    const auto normalizedValue = m_properties->valueToNormalized(clippedValue);
    const auto y = (1 - normalizedValue) * availableHeight + yMin;
    const auto clippedY = MathUtils::clip(y, yMin, yMax);
    return clippedY;
}

double CommonParamEditorView::sceneYToValue(const double y) const {
    const auto yMin = paddingTopBottom;
    const auto yMax = scene()->height() - paddingTopBottom;
    const auto availableHeight = yMax - yMin;
    const auto value = 1 - (y - yMin) / availableHeight;
    const auto clippedValue = MathUtils::clip(value, 0, 1);
    const auto scaledValue = m_properties->valueFromNormalized(clippedValue);
    return scaledValue;
}

void CommonParamEditorView::drawGraduates(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                          QWidget *widget) {
    const bool isBackground = transparentMouseEvents();
    if (isBackground || !m_properties->showDivision)
        return;
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(Qt::NoBrush);
    QPen pen;
    pen.setWidthF(1);
    pen.setColor(m_graduateColor);
    painter->setPen(pen);
    const int step = m_properties->divisionValue;
    const auto min = m_properties->minimum;
    const auto max = m_properties->maximum;
    for (int i = min; i <= max; i += step) {
        constexpr int lineLength = 4;
        const auto y = valueToItemY(i);
        painter->drawLine(0, y, lineLength, y);
    }
}

void CommonParamEditorView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                  QWidget *widget) {
    QElapsedTimer mstimer;
    mstimer.start();
    drawGraduates(painter, option, widget);
    painter->setBrush(Qt::NoBrush);

    const bool foreground = !transparentMouseEvents();
    constexpr auto penWidth = 1.5;
    QPen pen;
    pen.setWidthF(penWidth);
    auto editedLayers = m_drawCurvesEdited;
    editedLayers.append(m_anchorCurvesEdited);
    const auto effectiveEdited = AppModelUtils::mergeCurves({}, editedLayers);
    if (m_properties->displayMode == ParamProperties::DisplayMode::CurveOnly) {
        painter->setBrush(Qt::NoBrush);
        if (m_baseCurveVisible && !m_drawCurvesOriginal.isEmpty()) {
            pen.setColor(m_originalCurveColor);
            painter->setPen(pen);
            drawCurveBorder(painter, m_drawCurvesOriginal);
        }
        if (!effectiveEdited.isEmpty()) {
            auto editedColor = resolvedEditedCurveColor();
            editedColor.setAlpha(foreground ? 230 : 60);
            pen.setColor(editedColor);
            drawEditedCurveBorders(painter, pen);
        }
    } else {
        // 绘制填充图形
        painter->setPen(Qt::NoPen);
        if (foreground) {
            if (m_properties->displayMode == ParamProperties::DisplayMode::FillFromBottom) {
                const auto ci = NoteView::trackColorIndex();
                QLinearGradient gradient(0, 0, 0, visibleRect().height());
                gradient.setColorAt(0, AppColorPalette::instance()->paramFillTop(ci));
                gradient.setColorAt(1, AppColorPalette::instance()->paramFillBottom(ci));
                painter->setBrush(gradient);
            } else if (m_properties->displayMode == ParamProperties::DisplayMode::FillFromDefault) {
                painter->setBrush(
                    AppColorPalette::instance()->paramFillFlat(NoteView::trackColorIndex()));
            }
        } else {
            painter->setBrush(m_backgroundLayerColor);
        }

        DrawCurveList base;
        DrawCurve *baseCurve = nullptr;
        if (m_baseCurveVisible && m_properties->valueType == ParamProperties::ValueType::Relative) {
            const int start = MathUtils::roundDown(qRound(startTick()), 5);
            const int end = MathUtils::round(qRound(endTick()), 5) + 5;
            baseCurve = new DrawCurve(-1);
            baseCurve->setLocalStart(start);
            for (int i = start; i <= end; i += 5)
                baseCurve->appendValue(m_properties->defaultValue);
            base.append(baseCurve);
        } else if (m_baseCurveVisible) {
            base = m_drawCurvesOriginal;
        }
        auto curves = AppModelUtils::mergeCurves(base, effectiveEdited);
        if (!curves.isEmpty()) {
            drawCurvePolygon(painter, curves);
            for (const auto curve : curves)
                delete curve;
        }

        if (baseCurve && m_properties->showDefaultValue) {
            painter->setBrush(Qt::NoBrush);
            pen.setColor(foreground
                             ? AppColorPalette::instance()->paramLine(NoteView::trackColorIndex())
                             : m_backgroundLayerColor);
            painter->setPen(pen);
            drawCurveBorder(painter, base);
        }

        // 绘制已编辑描边
        if (foreground && !effectiveEdited.isEmpty()) {
            painter->setBrush(Qt::NoBrush);
            pen.setColor(resolvedEditedCurveColor());
            drawEditedCurveBorders(painter, pen);
        }
        delete baseCurve;
    }
    qDeleteAll(effectiveEdited);
}

QPainterPath CommonParamEditorView::anchorCoveragePath() const {
    QPainterPath path;
    const auto top = rect().top() - 2.0;
    const auto height = rect().height() + 4.0;
    for (const auto *curve : m_anchorCurvesEdited) {
        if (!curve || curve->values().size() < 2)
            continue;
        const auto left = tickToItemX(curve->localStart());
        const auto right = tickToItemX(curve->localEndTick());
        if (right <= rect().left() || left >= rect().right())
            continue;
        const auto clippedLeft = std::max(left, rect().left());
        const auto clippedRight = std::min(right, rect().right());
        path.addRect(QRectF(clippedLeft, top, clippedRight - clippedLeft, height));
    }
    return path;
}

void CommonParamEditorView::drawEditedCurveBorders(QPainter *painter, const QPen &pen) const {
    const auto anchorCoverage = anchorCoveragePath();
    if (!m_drawCurvesEdited.isEmpty()) {
        QPainterPath fullPath;
        fullPath.addRect(rect().adjusted(-2, -2, 2, 2));
        const auto solidPath = fullPath.subtracted(anchorCoverage);
        if (!solidPath.isEmpty()) {
            painter->save();
            painter->setClipPath(solidPath, Qt::IntersectClip);
            painter->setPen(pen);
            drawCurveBorder(painter, m_drawCurvesEdited);
            painter->restore();
        }
        if (!anchorCoverage.isEmpty()) {
            auto dashedPen = pen;
            dashedPen.setStyle(Qt::CustomDashLine);
            dashedPen.setDashPattern({4.0, 3.0});
            painter->save();
            painter->setClipPath(anchorCoverage, Qt::IntersectClip);
            painter->setPen(dashedPen);
            drawCurveBorder(painter, m_drawCurvesEdited);
            painter->restore();
        }
    }

    if (!m_anchorCurvesEdited.isEmpty()) {
        painter->setPen(pen);
        drawCurveBorder(painter, m_anchorCurvesEdited);
    }
}

void CommonParamEditorView::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (transparentMouseEvents()) {
        TimeOverlayView::mousePressEvent(event);
        return;
    }

    if (m_mouseDown) {
        qWarning() << "Ignored mousePressEvent" << event
                   << "because there is already one mouse button pressed";
        return;
    }
    m_mouseDown = true;
    m_mouseDownButton = event->button();
    cancelRequested = false;
    emit editStarted();
    AppModelUtils::copyCurves(m_drawCurvesEdited, m_drawCurvesEditedBak);
    const auto scenePos = event->scenePos().toPoint();
    auto tick = MathUtils::round(static_cast<int>(sceneXToTick(scenePos.x())), 5);
    if (tick < 0) {
        tick = 0;
        qDebug() << "mousePressEvent: Negative tick, clipped to 0";
    }
    const auto value = static_cast<int>(sceneYToValue(scenePos.y()));

    m_mouseDownPos = QPoint(tick, value);
    m_prevPos = m_mouseDownPos;

    if (event->button() == Qt::LeftButton) {
        if (m_eraseMode) {
            m_editType = Erase;
        } else {
            m_drawStroke = DrawCurveEditUtils::beginStroke(m_drawCurvesEdited, m_mouseDownPos);
            m_editType = m_bakeMode ? Bake : Draw;
            if (m_editType == Bake)
                m_bakeSource.capture(m_drawCurvesOriginal);
        }
    } else if (event->button() == Qt::RightButton) {
        m_editType = m_eraseMode || m_bakeMode ? None : Erase;
    } else {
        m_editType = None;
    }
}

void CommonParamEditorView::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (cancelRequested || m_editType == None || transparentMouseEvents() || m_mouseDown == false)
        return;

    const auto scenePos = event->scenePos();
    auto tick = MathUtils::round(static_cast<int>(sceneXToTick(scenePos.x())), 5);
    if (tick < 0)
        tick = 0;
    const auto value = static_cast<int>(sceneYToValue(scenePos.y()));
    const auto curPos = QPoint(tick, value);

    const auto [startTick, endTick] =
        DrawCurveEditUtils::strokeTickRange(m_prevPos.x(), curPos.x());

    bool changed = false;
    if (m_editType == Erase) {
        changed = AppModelUtils::eraseDrawCurveRange(m_drawCurvesEdited, startTick, endTick);
    } else {
        const DrawCurveEditUtils::ValueProvider valueAtTick =
            m_editType == Bake
                ? DrawCurveEditUtils::ValueProvider([this](const int sampleTick) {
                      return m_bakeSource.valueAt(sampleTick);
                  })
                : DrawCurveEditUtils::ValueProvider(
                      [previous = m_prevPos, current = curPos](const int sampleTick) {
                          return std::optional<int>(
                              qRound(MathUtils::linearValueAt(previous, current, sampleTick)));
                      });
        changed = DrawCurveEditUtils::updateStroke(m_drawCurvesEdited, m_drawStroke, m_prevPos,
                                                   curPos, valueAtTick);
    }

    m_mouseMoved = m_mouseMoved || changed;
    m_prevPos = curPos;
    update();
}

void CommonParamEditorView::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() != m_mouseDownButton) {
        qWarning() << "Ignored mouseReleaseEvent" << event;
        return;
    }
    m_mouseDown = false;
    m_mouseDownButton = Qt::NoButton;
    if (!cancelRequested)
        commitAction();
}

bool CommonParamEditorView::cancelEditState() {
    const bool hadEdit = m_mouseDown || m_editType != None || m_mouseMoved ||
                         m_drawStroke.newCurveCreated || !m_drawCurvesEditedBak.isEmpty();
    if (!hadEdit)
        return false;

    const bool shouldRestoreBackup = m_editType != None || m_mouseMoved ||
                                     m_drawStroke.newCurveCreated ||
                                     !m_drawCurvesEditedBak.isEmpty();
    if (shouldRestoreBackup) {
        for (const auto curve : m_drawCurvesEdited)
            delete curve;
        m_drawCurvesEdited = m_drawCurvesEditedBak;
        m_drawCurvesEditedBak.clear();
    }

    m_drawStroke = {};
    m_bakeSource.clear();
    m_editType = None;
    m_mouseMoved = false;
    cancelRequested = true;
    m_mouseDown = false;
    m_mouseDownButton = Qt::NoButton;
    return true;
}

void CommonParamEditorView::updateRectAndPos() {
    const auto pos = visibleRect().topLeft();
    setPos(pos);
    setRect(QRectF(0, 0, visibleRect().width(), visibleRect().height()));
    update();
}

double CommonParamEditorView::valueToItemY(const double value) const {
    return sceneYToItemY(valueToSceneY(value));
}

void CommonParamEditorView::drawCurveBorder(QPainter *painter,
                                            const QList<DrawCurve *> &curves) const {
    auto drawCurve = [painter, this](const DrawCurve &curve) {
        const auto dpr = painter->device()->devicePixelRatio();
        const auto pixelsPerTick = std::abs(tickToItemX(1.0) - tickToItemX(0.0));
        const auto indices =
            CurveRenderUtils::sampleCurve(curve, startTick(), endTick(), pixelsPerTick, dpr)
                .pointIndices;
        if (indices.isEmpty())
            return;

        const auto start = curve.localStart();
        const auto firstIndex = indices.first();
        const auto x = tickToItemX(start + firstIndex * curve.step);
        const auto y = valueToItemY(curve.values().at(firstIndex));
        const QPointF visibleFirstPoint(x, y);

        if (m_showDebugInfo) {
            const auto firstValue = curve.values().first();
            const auto firstPos = QPointF(tickToItemX(start), valueToItemY(firstValue));
            painter->drawText(firstPos, QString("#%1").arg(curve.id()));
        }

        QPainterPath curvePath;
        curvePath.moveTo(visibleFirstPoint);
        for (auto index = 1; index < indices.size(); ++index) {
            const auto valueIndex = indices.at(index);
            curvePath.lineTo(tickToItemX(start + valueIndex * curve.step),
                             valueToItemY(curve.values().at(valueIndex)));
        }
        painter->drawPath(curvePath);
    };
    for (const auto curve : curves) {
        if (curve->localEndTick() < startTick())
            continue;
        if (curve->localStart() > endTick())
            break;
        drawCurve(*curve);
    }
}

void CommonParamEditorView::drawCurvePolygon(QPainter *painter,
                                             const QList<DrawCurve *> &curves) const {
    auto drawCurve = [painter, this](const DrawCurve &curve) {
        const auto dpr = painter->device()->devicePixelRatio();
        const auto pixelsPerTick = std::abs(tickToItemX(1.0) - tickToItemX(0.0));
        const auto samples =
            CurveRenderUtils::sampleCurve(curve, startTick(), endTick(), pixelsPerTick, dpr);
        if (samples.pointIndices.isEmpty())
            return;

        const auto start = curve.localStart();
        const auto firstIndex = samples.pointIndices.first();
        const auto visibleFirstPoint = QPointF(tickToItemX(start + firstIndex * curve.step),
                                               valueToItemY(curve.values().at(firstIndex)));

        const auto fillFromBottom =
            m_properties->displayMode == ParamProperties::DisplayMode::FillFromBottom;
        const auto defaultValue = m_properties->defaultValue;
        const auto baseValue = fillFromBottom ? scene()->height() : valueToItemY(defaultValue);

        QPainterPath fillPath;
        fillPath.moveTo(visibleFirstPoint.x(), baseValue);
        fillPath.lineTo(visibleFirstPoint);

        for (auto index = 1; index < samples.pointIndices.size(); ++index) {
            const auto valueIndex = samples.pointIndices.at(index);
            fillPath.lineTo(tickToItemX(start + valueIndex * curve.step),
                            valueToItemY(curve.values().at(valueIndex)));
        }
        const auto lastX = tickToItemX(start + samples.lastVisitedIndex * curve.step);
        fillPath.lineTo(lastX, baseValue);
        painter->drawPath(fillPath);
    };

    for (const auto curve : curves) {
        if (curve->localEndTick() < startTick())
            continue;
        if (curve->localStart() > endTick())
            break;
        drawCurve(*curve);
    }
}
