//
// Created by fluty on 2024/1/25.
//

#define CLASS_NAME "CommonParamEditorView"

#include "CommonParamEditorView.h"

#include "ClipEditorGlobal.h"
#include "Model/AppModel/Clip.h"
#include "UI/Views/Common/CommonGraphicsScene.h"
#include "Utils/Log.h"
#include "Utils/MathUtils.h"

#include <QElapsedTimer>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>

using namespace ClipEditorGlobal;

CommonParamEditorView::CommonParamEditorView() {
    setBackgroundColor(Qt::transparent);
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
    auto y = (1 - value / 1000) * scene()->height();
    auto clippedY = MathUtils::clip(y, 0, scene()->height());
    // Logger::d(CLASS_NAME, QString("valueToSceneY value:%1 y:%2").arg(value).arg(clippedY));
    return clippedY;
}

double CommonParamEditorView::sceneYToValue(double y) const {
    auto value = (1 - y / scene()->height()) * 1000;
    auto clippedValue = MathUtils::clip(value, 0, 1000);
    // Logger::d(CLASS_NAME, QString("sceneYToValue y:%1 value:%2").arg(y).arg(clippedValue));
    return clippedValue;
}

void CommonParamEditorView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                  QWidget *widget) {
    QElapsedTimer mstimer;
    mstimer.start();
    OverlayGraphicsItem::paint(painter, option, widget);

    auto hideThreshold = 0.4;
    auto fadeLength = 0.1;
    if (scaleX() < hideThreshold)
        return;

    auto dpr = painter->device()->devicePixelRatio();
    if ((endTick() - startTick()) / 5 / (visibleRect().width() * dpr) < 1)
        painter->setRenderHint(QPainter::Antialiasing, true);
    else
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

    const auto time = static_cast<double>(mstimer.nsecsElapsed()) / 1000000.0;
    // Logger::d(className, "Render time: " + QString::number(time));
}

void CommonParamEditorView::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (m_transparentForMouseEvents) {
        OverlayGraphicsItem::mousePressEvent(event);
        return;
    }

    auto scenePos = event->scenePos().toPoint();
    auto tick = MathUtils::round(static_cast<int>(sceneXToTick(scenePos.x())), 5);
    auto value = static_cast<int>(sceneYToValue(scenePos.y()));

    if (event->button() == Qt::LeftButton) {
        if (auto curve = curveAt(tick)) {
            m_editingCurve = curve;
            m_editType = DrawOnCurve;
            Log::d(className, QString("Edit exist curve: #") + QString::number(curve->id()));
        } else {
            m_editingCurve = nullptr;
            m_editType = DrawOnInterval;
        }
    } else if (event->button() == Qt::RightButton) {
        m_editType = Erase;
    } else {
        m_editType = None;
    }

    m_mouseDownPos = QPoint(tick, value);
    m_prevPos = m_mouseDownPos;
}

void CommonParamEditorView::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (m_editType == None)
        return;

    m_mouseMoved = true;
    auto scenePos = event->scenePos();
    auto tick = MathUtils::round(static_cast<int>(sceneXToTick(scenePos.x())), 5);
    auto value = static_cast<int>(sceneYToValue(scenePos.y()));
    auto curPos = QPoint(tick, value);

    int startTick;
    int endTick;
    if (m_prevPos.x() < curPos.x()) {
        startTick = m_prevPos.x();
        endTick = curPos.x();
    } else {
        endTick = m_prevPos.x();
        startTick = curPos.x();
    }

    auto overlappedCurves = curvesIn(startTick, endTick);
    if (m_editType == Erase) {
        if (!overlappedCurves.isEmpty()) {
            for (auto curve : overlappedCurves) {
                if (curve->start >= startTick &&
                    curve->endTick() <= endTick) { // 区间覆盖整条曲线，直接移除该曲线
                    m_drawCurvesEdited.removeOne(curve);
                    Log::d(className,
                              QString("Erase: Remove curve #") + QString::number(curve->id()));
                } else if (curve->start < startTick &&
                           curve->endTick() > endTick) { // 区间在曲线内，将曲线切成两段
                    auto newCurve = new DrawCurve;
                    newCurve->start = endTick;
                    auto rightPoints = curve->mid(endTick);
                    newCurve->setValues(rightPoints); // 将区间右端点之后的点移动到新曲线上
                    curve->eraseTailFrom(startTick);
                    MathUtils::binaryInsert(m_drawCurvesEdited, newCurve);
                } else {
                    curve->erase(startTick, endTick);
                    // Logger::d(className,
                    //           QString("Erase curve: #%1 start: %2 end: %3")
                    //               .arg(QString::number(curve->id()), QString::number(startTick),
                    //                    QString::number(endTick)));
                }
            }
        }
    } else { // Draw
        if (!m_newCurveCreated && m_editType == DrawOnInterval) {
            m_editingCurve = new DrawCurve;
            m_editingCurve->start = m_mouseDownPos.x();
            m_editingCurve->appendValue(m_mouseDownPos.y());
            MathUtils::binaryInsert(m_drawCurvesEdited, m_editingCurve);
            Log::d(className,
                      QString("Create new curve: #") + QString::number(m_editingCurve->id()));
            m_newCurveCreated = true;
        }

        drawLine(m_prevPos, curPos, *m_editingCurve);
        if (!overlappedCurves.isEmpty()) {
            for (auto curve : overlappedCurves) {
                if (curve == m_editingCurve)
                    continue;

                m_editingCurve->mergeWith(*curve);
                m_drawCurvesEdited.removeOne(curve);
                // delete curve;
            }
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
        Log::d(className, "Edit completed");
        emit editCompleted(editedCurves());
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
    gradient.setColorAt(1, QColor(155, 186, 255, 10));

    QPainterPath fillPath;
    int start = curve.start;
    auto firstValue = curve.values().first();
    auto firstPos = QPointF(tickToItemX(start), valueToItemY(firstValue));
    int startIndex =
        start >= startTick()
            ? 0
            : (MathUtils::roundDown(static_cast<int>(startTick()), curve.step) - start) /
                  curve.step;

    // 绘制多边形填充
    if (m_fillCurve) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(gradient);
        fillPath.moveTo(firstPos.x(), sceneHeight);
        fillPath.lineTo(firstPos);
        double lastX = 0;
        for (int i = startIndex; i < curve.values().count(); i++) {
            const auto pos = start + curve.step * i;
            const auto value = curve.values().at(i);
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

    if (m_showDebugInfo)
        painter->drawText(firstPos, QString("#%1").arg(curve.id()));

    int pointCount = 0;
    QPainterPath curvePath;
    curvePath.moveTo(firstPos);
    for (int i = startIndex; i < curve.values().count(); i++) {
        const auto pos = start + curve.step * i;
        const auto value = curve.values().at(i);
        if (pos > endTick())
            break;
        const auto x = tickToItemX(pos);
        curvePath.lineTo(x, valueToItemY(value));
        pointCount++;
    }
    painter->drawPath(curvePath);

    // Logger::d(className, "points:" + QString::number(pointCount));
}