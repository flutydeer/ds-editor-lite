//
// Created by FlutyDeer on 25-1-22.
//

#ifndef TESTANCHOREDITOR_H
#define TESTANCHOREDITOR_H

#include "AnchorCurve.h"

#include <QWidget>

class NodeView {
    public:

};

class TestAnchorEditor : public QWidget {
    Q_OBJECT

public:
    explicit TestAnchorEditor(QWidget *parent = nullptr);

private:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool event(QEvent *event) override;
    void handleHoverEvent(QHoverEvent *event);
    [[nodiscard]] AnchorNode *findNode(QPointF position) const;

    AnchorCurve curve;
    AnchorNode *hoveredNode = nullptr;
    AnchorNode *currentEditNode = nullptr;
};



#endif // TESTANCHOREDITOR_H
