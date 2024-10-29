//
// Created by fluty on 2024/1/25.
//

#include "CommonParamEditorView.h"

#include "ClipEditorGlobal.h"
#include "Model/AppModel/ParamProperties.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Views/Common/TimeGraphicsScene.h"
#include "Utils/AppModelUtils.h"
#include "Utils/MathUtils.h"

#include <QElapsedTimer>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>

CommonParamEditorView::CommonParamEditorView(const ParamProperties &properties)
    : m_properties(&properties) {
    // setBackgroundColor(Qt::transparent);
    setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
}

void CommonParamEditorView::setParamProperties(const ParamProperties &properties) {
    clearParams();
    m_properties = &properties;
}

void CommonParamEditorView::loadOriginal(const QList<DrawCurve *> &curves) {
    for (const auto curve : m_drawCurvesOriginal)
        delete curve;
    AppModelUtils::copyCurves(curves, m_drawCurvesOriginal);
    update();
}

void CommonParamEditorView::loadEdited(const QList<DrawCurve *> &curves) {
    for (const auto curve : m_drawCurvesEdited)
        delete curve;
    AppModelUtils::copyCurves(curves, m_drawCurvesEdited);
    update();
}

void CommonParamEditorView::clearParams() {
    for (const auto curve : m_drawCurvesOriginal)
        delete curve;
    for (const auto curve : m_drawCurvesEdited)
        delete curve;
    m_drawCurvesOriginal.clear();
    m_drawCurvesEdited.clear();
    update();
}

void CommonParamEditorView::setEraseMode(bool on) {
    // qDebug() << "setEraseMode:" << on;
    m_eraseMode = on;
    update();
}

const QList<DrawCurve *> &CommonParamEditorView::editedCurves() const {
    return m_drawCurvesEdited;
}

void CommonParamEditorView::discardAction() {
    // qDebug() << "discardAction";
    if (m_editType == None) {
        // qWarning() << "Discard action called, but current edit type is None";
        return;
    }
    m_drawCurvesEdited = m_drawCurvesEditedBak;
    m_mouseMoved = false;
    m_newCurveCreated = false;
    cancelRequested = true;
    m_mouseDown = false;
    m_mouseDownButton = Qt::NoButton;
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
    update();
}

void CommonParamEditorView::commitAction() {
    // qDebug() << "commitAction";
    if (m_mouseMoved) {
        qDebug() << "Edit completed";
        emit editCompleted(editedCurves());
    }

    m_editingCurve = nullptr;
    m_editType = None;
    m_mouseMoved = false;
    m_newCurveCreated = false;
    cancelRequested = false;
    m_drawCurvesEditedBak.clear();
    m_mouseDown = false;
    m_mouseDownButton = Qt::NoButton;
    update();
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
}

double CommonParamEditorView::valueToSceneY(double value) const {
    auto yMin = paddingTopBottom;
    auto yMax = scene()->height() - paddingTopBottom;
    auto availableHeight = yMax - yMin;
    auto normalizedValue = m_properties->valueToNormalized(value);
    auto y = (1 - normalizedValue) * availableHeight + yMin;
    auto clippedY = MathUtils::clip(y, yMin, yMax);
    // Logger::d(CLASS_NAME, QString("valueToSceneY value:%1 y:%2").arg(value).arg(clippedY));
    return clippedY;
}

double CommonParamEditorView::sceneYToValue(double y) const {
    auto yMin = paddingTopBottom;
    auto yMax = scene()->height() - paddingTopBottom;
    auto availableHeight = yMax - yMin;
    auto value = 1 - (y - yMin) / availableHeight;
    auto clippedValue = MathUtils::clip(value, 0, 1);
    auto scaledValue = m_properties->valueFromNormalized(clippedValue);
    // qDebug() << "clipped" << clippedValue << "scaled" << scaledValue;
    // Logger::d(CLASS_NAME, QString("sceneYToValue y:%1 value:%2").arg(y).arg(clippedValue));
    return scaledValue;
}

void CommonParamEditorView::drawGraduates(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                          QWidget *widget) {
    bool isBackground = transparentMouseEvents();
    if (isBackground || !m_properties->showDivision)
        return;
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(Qt::NoBrush);
    QPen pen;
    pen.setWidthF(1);
    pen.setColor(QColor(72, 75, 78));
    // pen.setColor(QColor(57, 59, 61));
    painter->setPen(pen);
    int lineLength = 4;
    // int lineLength = visibleRect().width();
    int step = m_properties->divisionValue;
    auto min = m_properties->minimum;
    auto max = m_properties->maximum;
    for (int i = min; i <= max; i += step) {
        auto y = valueToItemY(i);
        painter->drawLine(0, y, lineLength, y);
    }
}

void CommonParamEditorView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                  QWidget *widget) {
    QElapsedTimer mstimer;
    mstimer.start();
    drawGraduates(painter, option, widget);
    painter->setBrush(Qt::NoBrush);

    // auto dpr = painter->device()->devicePixelRatio();
    // if ((endTick() - startTick()) / 5 / (visibleRect().width() * dpr) < 1)
    //     painter->setRenderHint(QPainter::Antialiasing, true);
    // else
    //     painter->setRenderHint(QPainter::Antialiasing, false);

    bool foreground = !transparentMouseEvents();
    auto penWidth = 1.5;
    QPen pen;
    pen.setWidthF(penWidth);
    if (m_properties->displayMode == ParamProperties::DisplayMode::CurveOnly) {
        painter->setBrush(Qt::NoBrush);
        if (!m_drawCurvesOriginal.isEmpty()) {
            pen.setColor(QColor(255, 255, 255, 96));
            painter->setPen(pen);
            drawCurveBorder(painter, m_drawCurvesOriginal);
        }
        if (!m_drawCurvesEdited.isEmpty()) {
            pen.setColor(QColor(255, 255, 255, 230));
            painter->setPen(pen);
            drawCurveBorder(painter, m_drawCurvesEdited);
        }
    } else {
        // 绘制填充图形
        painter->setPen(Qt::NoPen);
        if (foreground) {
            if (m_properties->displayMode == ParamProperties::DisplayMode::FillFromBottom) {
                QLinearGradient gradient(0, 0, 0, visibleRect().height());
                gradient.setColorAt(0, QColor(155, 186, 255, 200));
                gradient.setColorAt(1, QColor(155, 186, 255, 10));
                painter->setBrush(gradient);
            } else if (m_properties->displayMode == ParamProperties::DisplayMode::FillFromDefault) {
                // gradient.setColorAt(0, QColor(155, 186, 255, 200));
                // auto defaultValue =
                // m_properties->valueToNormalized(m_properties->defaultValue);
                // gradient.setColorAt(defaultValue, QColor(155, 186, 255, 80));
                // gradient.setColorAt(1, QColor(155, 186, 255, 200));
                painter->setBrush(QColor(155, 186, 255, 120));
            }
        } else {
            painter->setBrush(QColor(40, 40, 40));
        }

        DrawCurveList base;
        DrawCurve *baseCurve = nullptr;
        if (m_properties->valueType == ParamProperties::ValueType::Relative) {
            int start = MathUtils::roundDown(qRound(startTick()), 5);
            int end = MathUtils::round(qRound(endTick()), 5) + 5;
            baseCurve = new DrawCurve(-1);
            baseCurve->setStart(start);
            for (int i = start; i <= end; i += 5)
                baseCurve->appendValue(m_properties->defaultValue);
            base = {baseCurve};
        } else
            base = m_drawCurvesOriginal;
        auto overlay = m_drawCurvesEdited;
        auto curves = AppModelUtils::mergeCurves(base, overlay);
        if (!curves.isEmpty()) {
            drawCurvePolygon(painter, curves);
            for (auto curve : curves)
                delete curve;
        }

        if (baseCurve && m_properties->showDefaultValue) {
            painter->setBrush(Qt::NoBrush);
            pen.setColor(foreground ? QColor(155, 186, 255) : QColor(40, 40, 40));
            painter->setPen(pen);
            drawCurveBorder(painter, base);
        }

        // 绘制已编辑描边
        if (foreground && !m_drawCurvesEdited.isEmpty()) {
            painter->setBrush(Qt::NoBrush);
            pen.setColor(QColor(255, 255, 255));
            painter->setPen(pen);
            drawCurveBorder(painter, m_drawCurvesEdited);
        }
    }

    const auto time = static_cast<double>(mstimer.nsecsElapsed()) / 1000000.0;
    // Logger::d(className, "Render time: " + QString::number(time));
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
    appStatus->currentEditObject = AppStatus::EditObjectType::Param;
    AppModelUtils::copyCurves(m_drawCurvesEdited, m_drawCurvesEditedBak);
    auto scenePos = event->scenePos().toPoint();
    auto tick = MathUtils::round(static_cast<int>(sceneXToTick(scenePos.x())), 5);
    if (tick < 0) {
        tick = 0;
        qDebug() << "mousePressEvent: Negative tick, clipped to 0";
    }
    auto value = static_cast<int>(sceneYToValue(scenePos.y()));

    if (event->button() == Qt::LeftButton) {
        if (m_eraseMode) {
            m_editType = Erase;
        } else {
            if (auto curve = curveAt(tick)) {
                m_editingCurve = curve;
                m_editType = DrawOnCurve;
                qDebug() << "Edit exist curve: #" << curve->id();
            } else {
                m_editingCurve = nullptr;
                m_editType = DrawOnInterval;
            }
        }
    } else if (event->button() == Qt::RightButton) {
        m_editType = m_eraseMode ? None : Erase;
    } else {
        m_editType = None;
    }

    m_mouseDownPos = QPoint(tick, value);
    m_prevPos = m_mouseDownPos;
}

void CommonParamEditorView::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (cancelRequested || m_editType == None || transparentMouseEvents() || m_mouseDown == false)
        return;

    m_mouseMoved = true;
    auto scenePos = event->scenePos();
    auto tick = MathUtils::round(static_cast<int>(sceneXToTick(scenePos.x())), 5);
    if (tick < 0) {
        tick = 0;
        qDebug() << "mouseMoveEvent: Negative tick, clipped to 0";
    }
    auto value = static_cast<int>(sceneYToValue(scenePos.y()));
    auto curPos = QPoint(tick, value);
    qDebug() << "Draw at tick:" << tick << "value:"
             << m_properties->valueToString(value, m_properties->hasUnit(),
                                            m_properties->displayPrecision);

    int startTick;
    int endTick;
    if (m_prevPos.x() < curPos.x()) {
        startTick = m_prevPos.x();
        endTick = curPos.x();
    } else {
        endTick = m_prevPos.x();
        startTick = curPos.x();
    }

    auto overlappedCurves = AppModelUtils::curvesIn(m_drawCurvesEdited, startTick, endTick);
    if (m_editType == Erase) {
        if (!overlappedCurves.isEmpty()) {
            for (auto curve : overlappedCurves) {
                if (curve->start() >= startTick &&
                    curve->endTick() <= endTick) { // 区间覆盖整条曲线，直接移除该曲线
                    m_drawCurvesEdited.removeOne(curve);
                    qDebug() << "Erase: Remove curve #" << curve->id();
                } else if (curve->start() < startTick &&
                           curve->endTick() > endTick) { // 区间在曲线内，将曲线切成两段
                    auto newCurve = new DrawCurve;
                    newCurve->setStart(endTick);
                    auto rightPoints = curve->mid(endTick);
                    newCurve->setValues(rightPoints); // 将区间右端点之后的点移动到新曲线上
                    curve->eraseTailFrom(startTick);
                    MathUtils::binaryInsert(m_drawCurvesEdited, newCurve);
                } else {
                    curve->erase(startTick, endTick);
                }
            }
        }
    } else { // Draw
        // 在空白处绘制，如果未创建新曲线，则创建一条并将其设为正在编辑的曲线
        if (!m_newCurveCreated && m_editType == DrawOnInterval) {
            m_editingCurve = new DrawCurve;
            m_editingCurve->setStart(m_mouseDownPos.x());
            m_editingCurve->appendValue(m_mouseDownPos.y());
            MathUtils::binaryInsert(m_drawCurvesEdited, m_editingCurve);
            qDebug() << "Create new curve: #" << m_editingCurve->id();
            m_newCurveCreated = true;
        }

        drawLine(m_prevPos, curPos, *m_editingCurve);
        if (!overlappedCurves.isEmpty()) {
            for (auto curve : overlappedCurves) {
                if (curve == m_editingCurve)
                    continue;

                m_editingCurve->mergeWithCurrentPriority(*curve);
                m_drawCurvesEdited.removeOne(curve);
                // delete curve;
            }
        }
    }

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

void CommonParamEditorView::updateRectAndPos() {
    auto pos = visibleRect().topLeft();
    setPos(pos);
    setRect(QRectF(0, 0, visibleRect().width(), visibleRect().height()));
    update();
}

double CommonParamEditorView::valueToItemY(double value) const {
    return sceneYToItemY(valueToSceneY(value));
}

DrawCurve *CommonParamEditorView::curveAt(double tick) {
    for (const auto curve : m_drawCurvesEdited)
        if (curve->start() <= tick && curve->endTick() > tick)
            return curve;
    return nullptr;
}

void CommonParamEditorView::drawCurveBorder(QPainter *painter,
                                            const QList<DrawCurve *> &curves) const {
    auto drawCurve = [=](const DrawCurve &curve) {
        auto dpr = painter->device()->devicePixelRatio();
        bool peakMode = ((endTick() - startTick()) / 5 / (visibleRect().width() * dpr) > 1);

        int start = curve.start();
        int startIndex =
            start >= startTick()
                ? 0
                : (MathUtils::roundDown(static_cast<int>(startTick()), curve.step) - start) /
                      curve.step;
        auto x = tickToItemX(start + startIndex * curve.step);
        auto y = valueToItemY(curve.values().at(startIndex));
        auto visibleFirstPoint = QPointF(x, y);

        if (m_showDebugInfo) {
            auto firstValue = curve.values().first();
            auto firstPos = QPointF(tickToItemX(start), valueToItemY(firstValue));
            painter->drawText(firstPos, QString("#%1").arg(curve.id()));
        }

        int pointCount = 0;
        QPainterPath curvePath;
        curvePath.moveTo(visibleFirstPoint);
        double lastLineToX = visibleFirstPoint.x();
        bool breakFlag = false;
        for (int i = startIndex; i < curve.values().count(); i++) {
            const auto pos = start + curve.step * i;
            const auto value = curve.values().at(i);
            if (pos > endTick())
                breakFlag = true;
            const auto x = tickToItemX(pos);
            if (qAbs(lastLineToX - x) > dpr) {
                curvePath.lineTo(x, valueToItemY(value));
                lastLineToX = x;
            }
            pointCount++;

            if (breakFlag)
                break;
        }
        painter->drawPath(curvePath);
    };
    for (const auto curve : curves) {
        if (curve->endTick() < startTick())
            continue;
        if (curve->start() > endTick())
            break;
        drawCurve(*curve);
    }
}

void CommonParamEditorView::drawCurvePolygon(QPainter *painter,
                                             const QList<DrawCurve *> &curves) const {
    auto drawCurve = [=](const DrawCurve &curve) {
        auto dpr = painter->device()->devicePixelRatio();
        bool peakMode = ((endTick() - startTick()) / 5 / (visibleRect().width() * dpr) > 1);

        int start = curve.start();
        int startIndex =
            start >= startTick()
                ? 0
                : (MathUtils::roundDown(static_cast<int>(startTick()), curve.step) - start) /
                      curve.step;
        auto visibleFirstPoint = QPointF(tickToItemX(start + startIndex * curve.step),
                                         valueToItemY(curve.values().at(startIndex)));

        auto fillFromBottom =
            m_properties->displayMode == ParamProperties::DisplayMode::FillFromBottom;
        auto defaultValue = m_properties->defaultValue;
        auto baseValue = fillFromBottom ? scene()->height() : valueToItemY(defaultValue);

        QPainterPath fillPath;
        fillPath.moveTo(visibleFirstPoint.x(), baseValue);
        fillPath.lineTo(visibleFirstPoint);

        const double startTick = start;
        const double tempEndTick = endTick();
        const double step = curve.step;
        const double startX = tickToItemX(startTick);
        const double endX = tickToItemX(tempEndTick);
        const double interval = (endX - startX) / ((tempEndTick - startTick) / step);

        double lastX = 0;

        double lastLineToX = startX;
        bool breakFlag = false;
        for (int i = startIndex; i < curve.values().count(); i++) {
            const auto pos = start + curve.step * i;
            const auto value = curve.values().at(i);
            if (pos > tempEndTick)
                breakFlag = true;
            const double x = startX + (i * interval);
            // 只有在视图上两点距离达到一个像素以上时才绘制
            // TODO: 使用峰值模式来绘制
            if (qAbs(lastLineToX - x) > dpr) {
                fillPath.lineTo(x, valueToItemY(value));
                lastLineToX = x;
            }
            lastX = x;

            if (breakFlag)
                break;
        }
        fillPath.lineTo(lastX, baseValue);
        painter->drawPath(fillPath);
    };

    for (const auto curve : curves) {
        if (curve->endTick() < startTick())
            continue;
        if (curve->start() > endTick())
            break;
        drawCurve(*curve);
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
    line.setStart(start);
    int linePointCount = (endPoint.x() - startPoint.x()) / curve.step;
    for (int i = 0; i < linePointCount; i++) {
        auto tick = start + i * curve.step;
        auto value = MathUtils::linearValueAt(startPoint, endPoint, tick);
        line.appendValue(qRound(value));
    }
    curve.mergeWithOtherPriority(line);
}