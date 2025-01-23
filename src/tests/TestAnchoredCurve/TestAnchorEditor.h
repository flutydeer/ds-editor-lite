//
// Created by FlutyDeer on 25-1-22.
//

#ifndef TESTANCHOREDITOR_H
#define TESTANCHOREDITOR_H

#include "anchoredcurve.h"

#include <QWidget>

class TestAnchorEditor : public QWidget {
    Q_OBJECT

public:
    explicit TestAnchorEditor(QWidget *parent = nullptr);

private:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

    AnchoredCurve<int, double> curve;
};



#endif // TESTANCHOREDITOR_H
