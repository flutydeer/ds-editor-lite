//
// Created by fluty on 2024/1/25.
//

#include "CommonParamEditorView.h"

#include "Global/ClipEditorGlobal.h"
#include "Model/AppModel/Clip.h"
#include "UI/Views/Common/CommonGraphicsScene.h"
#include "Utils/MathUtils.h"

#include <QPainter>
#include <QGraphicsSceneMouseEvent>

using namespace ClipEditorGlobal;

CommonParamEditorView::CommonParamEditorView() {
    setBackgroundColor(Qt::transparent);
}
CommonParamEditorView::EditMode CommonParamEditorView::editMode() const {
    return m_editMode;
}
void CommonParamEditorView::loadOriginal(const QList<DrawCurve *> &curves) {
    SingingClip::copyCurves(curves, m_drawCurvesOriginal);
    update();
}
void CommonParamEditorView::loadEdited(const QList<DrawCurve *> &curves) {
    SingingClip::copyCurves(curves, m_drawCurvesEdited);
    update();
}
const QList<DrawCurve *> &CommonParamEditorView::editedCurves() const {
    return m_drawCurvesEdited;
}
bool CommonParamEditorView::fillCurve() const {
    return m_fillCurve;
}
void CommonParamEditorView::setFillCurve(bool on) {
    m_fillCurve = on;
    update();
}
double CommonParamEditorView::valueToSceneY(double value) const {
    auto y = value * scene()->height();
    return MathUtils::clip(y, 0, scene()->height());
}
double CommonParamEditorView::sceneYToValue(double y) const {
    auto value = y / scene()->height();
    return MathUtils::clip(value, 0, 1);
}
void CommonParamEditorView::setEditMode(const EditMode &mode) {
    setTransparentForMouseEvents(mode == Off);
    m_editMode = mode;
}
void CommonParamEditorView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                  QWidget *widget) {
    OverlayGraphicsItem::paint(painter, option, widget);

    // painter->setRenderHint(QPainter::Antialiasing, false);
    auto noAntialiasingThreshold = 0.8;
    auto hideThreshold = 0.4;
    auto fadeLength = 0.1;
    if (scaleX() < hideThreshold)
        return;
    if (scaleX() < noAntialiasingThreshold)
        painter->setRenderHint(QPainter::Antialiasing, false);

    QPen pen;
    pen.setWidthF(1.8);
    auto colorAlpha =
        scaleX() < hideThreshold + fadeLength ? 255 * (scaleX() - hideThreshold) / fadeLength : 255;

    if (m_drawCurvesOriginal.count() > 0) {
        // pen.setColor(QColor(127, 127, 127, static_cast<int>(colorAlpha)));
        // painter->setPen(pen);
        drawHandDrawCurves(painter, m_drawCurvesOriginal);
    }

    if (m_drawCurvesEdited.count() > 0) {
        // pen.setColor(QColor(255, 255, 255, static_cast<int>(colorAlpha)));
        // painter->setPen(pen);
        drawHandDrawCurves(painter, m_drawCurvesEdited);
    }
}
void CommonParamEditorView::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (m_transparentForMouseEvents) {
        OverlayGraphicsItem::mousePressEvent(event);
        return;
    }

    auto scenePos = event->scenePos().toPoint();
    auto tick = MathUtils::round(static_cast<int>(sceneXToTick(scenePos.x())), 5);
    auto value = static_cast<int>(sceneYToValue(scenePos.y()));

    if (auto curve = curveAt(tick)) {
        m_editingCurve = curve;
        m_editType = EditExistCurve;
        qDebug() << "Edit exist curve" << curve->id() << curve->start;
    } else {
        m_editingCurve = nullptr;
        m_editType = CreateCurve;
    }
    m_mouseDownPos = QPoint(tick, value);
    m_prevPos = m_mouseDownPos;
}
void CommonParamEditorView::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    m_mouseMoved = true;
    if (!m_newCurveCreated && m_editType == CreateCurve) {
        m_editingCurve = new DrawCurve;
        m_editingCurve->start = m_mouseDownPos.x();
        m_editingCurve->appendValue(m_mouseDownPos.y());
        MathUtils::binaryInsert(m_drawCurvesEdited, m_editingCurve);
        qDebug() << "New curve added" << m_editingCurve->id();
        m_newCurveCreated = true;
    }
    auto scenePos = event->scenePos();
    auto tick = MathUtils::round(static_cast<int>(sceneXToTick(scenePos.x())), 5);
    auto value = static_cast<int>(sceneYToValue(scenePos.y()));
    auto curPos = QPoint(tick, value);
    // qDebug() << "Draw curve at" << curPos;

    int startTick;
    int endTick;
    if (m_prevPos.x() < curPos.x()) {
        startTick = m_prevPos.x();
        endTick = curPos.x();
    } else {
        endTick = m_prevPos.x();
        startTick = curPos.x();
    }
    drawLine(m_prevPos, curPos, *m_editingCurve);
    auto overlappedCurves = curvesIn(startTick, endTick);
    if (!overlappedCurves.isEmpty()) {
        for (auto curve : overlappedCurves) {
            if (curve == m_editingCurve)
                continue;

            m_editingCurve->mergeWith(*curve);
            m_drawCurvesEdited.removeOne(curve);
            // delete curve;
        }
    }
    m_prevPos = curPos;
    update();
}
void CommonParamEditorView::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    if (!m_mouseMoved) {
        m_editingCurve = nullptr;
        m_editType = None;
    } else {
        emit editCompleted(editedCurves());
        qDebug() << "CommonParamEditorView editCompleted";
    }

    m_mouseMoved = false;
    m_newCurveCreated = false;
    update();
}
void CommonParamEditorView::updateRectAndPos() {
    auto pos = visibleRect().topLeft();
    setPos(pos);
    setRect(QRectF(0, 0, visibleRect().width(), visibleRect().height()));
    update();
}
double CommonParamEditorView::startTick() const {
    return sceneXToTick(visibleRect().left());
}
double CommonParamEditorView::endTick() const {
    return sceneXToTick(visibleRect().right());
}
double CommonParamEditorView::sceneXToTick(double x) const {
    return 480 * x / scaleX() / pixelsPerQuarterNote;
}
double CommonParamEditorView::tickToSceneX(double tick) const {
    return tick * scaleX() * pixelsPerQuarterNote / 480;
}
double CommonParamEditorView::sceneXToItemX(double x) const {
    return mapFromScene(QPointF(x, 0)).x();
}
double CommonParamEditorView::tickToItemX(double tick) const {
    return sceneXToItemX(tickToSceneX(tick));
}
double CommonParamEditorView::sceneYToItemY(double y) const {
    return mapFromScene(QPointF(0, y)).y();
}
double CommonParamEditorView::valueToItemY(double value) const {
    return sceneYToItemY(valueToSceneY(value));
}
DrawCurve *CommonParamEditorView::curveAt(double tick) {
    for (const auto curve : m_drawCurvesEdited)
        if (curve->start <= tick && curve->endTick() > tick)
            return curve;
    return nullptr;
}
QList<DrawCurve *> CommonParamEditorView::curvesIn(int startTick, int endTick) {
    QList<DrawCurve *> result;
    ProbeLine line(startTick, endTick);
    for (const auto &curve : m_drawCurvesEdited) {
        if (curve->isOverlappedWith(&line))
            result.append(curve);
    }
    return result;
}
void CommonParamEditorView::drawHandDrawCurves(QPainter *painter,
                                               const QList<DrawCurve *> &curves) const {
    for (const auto curve : curves) {
        if (curve->endTick() < startTick())
            continue;
        if (curve->start > endTick())
            break;

        drawCurve(painter, *curve);
    }
}
void CommonParamEditorView::drawLine(const QPoint &p1, const QPoint &p2, DrawCurve &curve) {
    if (p1.x() == p2.x())
        return;

    QPoint startPoint;
    QPoint endPoint;
    if (p1.x() < p2.x()) {
        startPoint = p1;
        endPoint = p2;
    } else {
        startPoint = p2;
        endPoint = p1;
    }
    auto line = DrawCurve(-1);
    auto start = startPoint.x();
    line.start = start;
    int linePointCount = (endPoint.x() - startPoint.x()) / curve.step;
    for (int i = 0; i < linePointCount; i++) {
        auto tick = start + i * curve.step;
        auto value = MathUtils::linearValueAt(startPoint, endPoint, tick);
        line.appendValue(qRound(value));
    }
    curve.overlayMergeWith(line);
}
void CommonParamEditorView::drawCurve(QPainter *painter, const DrawCurve &curve) const {
    auto sceneHeight = scene()->height();
    QLinearGradient gradient(0, 0, 0, visibleRect().height());
    gradient.setColorAt(0, QColor(155, 186, 255, 180));
    gradient.setColorAt(1, QColor(155, 186, 255, 0));

    QPainterPath fillPath;
    int start = curve.start;
    auto firstValue = curve.values().first();
    auto firstPos = QPointF(tickToItemX(start), valueToItemY(firstValue));

    // 绘制多边形填充
    if (m_fillCurve) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(gradient);
        fillPath.moveTo(firstPos.x(), sceneHeight);
        fillPath.lineTo(firstPos);
        double lastX = 0;
        for (int i = 0; i < curve.values().count(); i++) {
            const auto pos = start + curve.step * i;
            const auto value = curve.values().at(i);
            if (pos < startTick())
                continue;
            if (pos > endTick())
                break;
            const auto x = tickToItemX(pos);
            fillPath.lineTo(x, valueToItemY(value));
            lastX = x;
        }
        fillPath.lineTo(lastX, sceneHeight);
        painter->drawPath(fillPath);
    }

    // 绘制曲线
    QPen pen;
    pen.setWidthF(1.8);
    pen.setColor(QColor(240, 240, 240, 255));
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    QPainterPath curvePath;
    curvePath.moveTo(firstPos);

    if (m_showDebugInfo)
        painter->drawText(firstPos, QString("#%1").arg(curve.id()));

    for (int i = 0; i < curve.values().count(); i++) {
        const auto pos = start + curve.step * i;
        const auto value = curve.values().at(i);
        if (pos < startTick())
            continue;
        if (pos > endTick())
            break;
        const auto x = tickToItemX(pos);
        curvePath.lineTo(x, valueToItemY(value));
    }
    painter->drawPath(curvePath);
}