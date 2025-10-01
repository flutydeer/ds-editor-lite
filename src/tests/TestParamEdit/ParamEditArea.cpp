//
// Created by fluty on 2023/9/5.
//

#include "ParamEditArea.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

void ParamEditArea::paintEvent(QPaintEvent *event) {
    auto rectHeight = rect().height();
    auto colorPrimary = QColor(155, 186, 255);
    auto colorAccent = QColor(255, 175, 95);

    QLinearGradient gradient(0, 0, 0, rectHeight);
    gradient.setColorAt(0, QColor(155, 186, 255, 180));
    gradient.setColorAt(1, QColor(155, 186, 255, 0));

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(32, 32, 32));

    QPen pen;
    pen.setColor(colorPrimary);
    pen.setWidthF(1.5);
    painter.setPen(pen);
    QPainterPath path;

    //    for (const auto curve : m_curves){
    //
    //
    //    }
    if (m_editingCurve->values().count() > 0) {
        auto firstValue = m_editingCurve->values().first();
        path.moveTo(m_editingCurve->start(), rectHeight);
        int i = 0;
        painter.setBrush(gradient);
        for (const auto value : m_editingCurve->values()) {
            auto x = m_editingCurve->start() + i * m_editingCurve->step();
            auto y = value;
            path.lineTo(x, y);
            i++;
        }
        int lastX = m_editingCurve->start() + (i - 1) * m_editingCurve->step();
        path.lineTo(lastX, rectHeight);
        painter.drawPath(path);

        pen.setColor(colorAccent);
        painter.setPen(pen);
        painter.drawLine(QPoint(m_editingCurve->start(), rectHeight),
                         QPoint(m_editingCurve->start(), firstValue));

        // i = 0;
        // pen.setColor(colorPrimary);
        // painter.setPen(pen);
        // painter.setBrush(colorPrimary);
        // for (const auto value : curve.values()) {
        //     auto x = curve.pos() + i * curve.step();
        //     auto y = value;
        //     painter.drawEllipse(x - 2, y - 2, 4, 4);
        //     i++;
        // }
    }

    //    if (m_points.count() > 0) {
    //        auto firstValue = m_points.first();
    //        path.moveTo(firstValue);
    //        for (int i = 0; i < m_points.size() - 1; ++i) {
    //            QPointF sp = m_points[i];
    //            QPointF ep = m_points[i + 1];
    //            QPointF c1 = QPointF((sp.x() + ep.x()) / 2, sp.y());
    //            QPointF c2 = QPointF((sp.x() + ep.x()) / 2, ep.y());
    //            path.cubicTo(c1, c2, ep);
    //        }
    //        painter.drawPath(path);
    //
    //        painter.setBrush(Qt::white);
    //        for (const auto point : qAsConst(m_points)) {
    //            //            path.lineTo(point.x(), point.y());
    //            painter.drawEllipse(point.x() - 2, point.y() - 2, 4, 4);
    //        }
    //        painter.end();
    //    }

    QFrame::paintEvent(event);
}

void ParamEditArea::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
}

bool cmpy(QPoint const &a, QPoint const &b) {
    return a.x() < b.x();
}

void ParamEditArea::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        auto tick = event->x();
        auto value = event->y();
        if (firstDraw) {
            m_editingCurve->setPos(tick);
            firstDraw = false;
        }
        m_editingCurve->drawPoint(tick, value);
        update();
    }
    QWidget::mousePressEvent(event);
}

void ParamEditArea::mouseMoveEvent(QMouseEvent *event) {
    auto tick = event->pos().x();
    auto value = event->pos().y();
    m_editingCurve->drawPoint(tick, value);
    update();
    QWidget::mouseMoveEvent(event);
}

void ParamEditArea::mouseReleaseEvent(QMouseEvent *event) {
    m_editingCurve->drawEnd();
    QWidget::mouseReleaseEvent(event);
}

HandDrawCurve *ParamEditArea::findCurveByPos(double tick) {
    for (auto curve : m_curves)
        if (curve->start() <= tick && curve->end() >= tick)
            return curve;

    return nullptr;
}

int HandDrawCurve::start() const {
    return m_start;
}

QList<int> HandDrawCurve::values() const {
    return m_values;
}

int HandDrawCurve::end() const {
    return m_start + m_step * m_values.count();
}

void HandDrawCurve::setPos(int tick) {
    qDebug() << "set pos" << tick;
    m_start = tick;
}

void HandDrawCurve::drawPoint(int tick, int value) {
    qDebug() << "draw point" << tick << value;
    // snap tick to grid
    auto roundPos = [](int i, int step) {
        int times = i / step;
        int mod = i % step;
        if (mod > step / 2)
            return step * (times + 1);
        else
            return step * times;
    };
    tick = roundPos(tick, m_step);

    if (m_values.empty()) {
        setPos(tick);
        m_values.append(value);
    }

    if (!m_drawing) {
        m_mouseDownPos.setX(tick);
        m_mouseDownPos.setY(value);
        m_drawing = true;
    }

    auto valueAt = [](int x1, int y1, int x2, int y2, int x) {
        double dx = x2 - x1;
        double dy = y2 - y1;
        double ratio = dy / dx;
        qDebug() << "ratio" << ratio;
        return int(y1 + (x - x1) * ratio);
    };

    if (tick > m_start) {
        int prevPos = m_mouseDownPos.x();
        int prevValue = m_mouseDownPos.y();
        int curPos = qMin(prevPos, tick);
        int right = qMax(prevPos, tick);
        int curveEnd = m_start + m_values.count() * m_step;

        while (curPos < right) {
            auto curValue = valueAt(prevPos, prevValue, tick, value, curPos);
            if (curPos < curveEnd) { // Draw in curve
                int index = (curPos - m_start) / m_step;
                m_values.replace(index, curValue);
            } else {
                m_values.append(curValue);
            }
            curPos += m_step;
            qDebug() << "paint" << curPos << curValue;
        }
        //        m_values.replace((tick - m_pos) / m_step, value);
    } else if (tick < m_start) {
        qDebug() << "<pos";
        int curPos = tick;
        int i = 0;
        while (curPos < m_start) {
            auto curValue = valueAt(tick, value, m_start, m_values.first(), curPos);
            m_values.insert(i, curValue);
            curPos += m_step;
            i++;
        }
        m_start = tick;
    } else {
        qDebug() << "=pos";
        m_values.replace(0, value);
    }

    m_mouseDownPos.setX(tick);
    m_mouseDownPos.setY(value);
}

int HandDrawCurve::valueAt(int tick) {
    int curveEnd = m_start + m_values.count() * m_step;
    if (tick < m_start || tick > curveEnd)
        return 0;

    int index = (tick - m_start) / m_step;
    int value = m_values.at(index);
    return value;
}

std::tuple<qsizetype, qsizetype> HandDrawCurve::interval() const {
    return std::make_tuple(0, 0);
}

int HandDrawCurve::step() const {
    return m_step;
}

void HandDrawCurve::drawEnd() {
    m_drawing = false;
}
