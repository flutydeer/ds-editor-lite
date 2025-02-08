//
// Created by FlutyDeer on 25-1-22.
//

#include "TestAnchorEditor.h"

#include "../../app/Utils/Linq.h"

#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include <QMouseEvent>

TestAnchorEditor::TestAnchorEditor(QWidget *parent) {
    resize(1280, 400);
    setAttribute(Qt::WA_Hover);

    // curve.insert({100, 100});
    // curve.insert({200, 50});
}

void TestAnchorEditor::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);


    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen;
    auto primaryColor = QColor(112, 156, 255);
    pen.setColor(primaryColor);
    pen.setWidthF(1.5);

    if (curve.nodes().count() >= 2) {
        // Draw curve
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        auto knots = curve.nodes();
        auto startTick = knots.toList().first()->pos();
        auto endTick = knots.toList().last()->pos();

        QPainterPath curvePath;
        bool firstPoint = true;
        for (int i = startTick; i <= endTick; i++) {
            auto value = curve.valueAt(i);
            // qDebug() << value;
            if (firstPoint) {
                curvePath.moveTo(i, value);
                firstPoint = false;
            } else
                curvePath.lineTo(i, value);
        }
        painter.drawPath(curvePath);
    }

    // Draw knots
    pen.setColor({255, 255, 255});
    pen.setWidthF(2);
    painter.setPen(pen);
    painter.setBrush(primaryColor);
    for (const auto &knot : curve.nodes()) {
        auto pos = QPoint(knot->pos(), knot->value());
        painter.drawEllipse(pos, 6, 6);
    }
}

void TestAnchorEditor::mousePressEvent(QMouseEvent *event) {
    auto pos = event->position().toPoint();
    if (event->button() == Qt::LeftButton) {
        if (const auto node = findNode(pos)) {
            currentEditNode = node;
        } else {
            auto newNode = new AnchorNode{pos.x(), pos.y()};
            curve.insertNode(newNode);
            currentEditNode = newNode;
        }
    } else if (event->button() == Qt::RightButton) {
        if (auto node = findNode(pos)) {
            curve.removeNode(node);
            if (currentEditNode == node)
                currentEditNode = nullptr;
            delete node;
        }
    }
    update();

    // QWidget::mousePressEvent(event);
}

void TestAnchorEditor::mouseMoveEvent(QMouseEvent *event) {
    auto pos = event->position().toPoint();
    if (currentEditNode) {
        curve.removeNode(currentEditNode);
        currentEditNode->setPos(pos.x());
        currentEditNode->setValue(pos.y());
        curve.insertNode(currentEditNode);
        update();
    }
    // QWidget::mouseMoveEvent(event);
}

void TestAnchorEditor::mouseReleaseEvent(QMouseEvent *event) {
    currentEditNode = nullptr;

    update();
    // QWidget::mouseReleaseEvent(event);
}

bool TestAnchorEditor::event(QEvent *event) {
    switch (event->type()) {
        case QEvent::HoverEnter:
        case QEvent::HoverMove:
        case QEvent::HoverLeave:
            handleHoverEvent(static_cast<QHoverEvent *>(event));
            break;
        default:
            break;
    }
    return QWidget::event(event);
}

void TestAnchorEditor::handleHoverEvent(QHoverEvent *event) {
    if (event->type() == QEvent::HoverMove) {
        auto node = findNode(event->position().toPoint());
        hoveredNode = node;
        // if (node)
        //     qDebug() << "Hovered node at " << node->pos();
    } else if (event->type() == QEvent::HoverLeave) {
        hoveredNode = nullptr;
    }
}

AnchorNode *TestAnchorEditor::findNode(QPointF position) const {
    auto nodes = curve.nodes().toList();
    auto results = Linq::where(nodes, [&](AnchorNode *node) {
        auto distance =
            sqrt(pow(node->pos() - position.x(), 2) + pow(node->value() - position.y(), 2));
        return distance < 8;
    });
    if (results.isEmpty())
        return nullptr;
    return results.first();
}