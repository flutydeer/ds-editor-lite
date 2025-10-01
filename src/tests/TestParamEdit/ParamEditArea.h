//
// Created by fluty on 2023/9/5.
//

#ifndef DATASET_TOOLS_PARAMEDITAREA_H
#define DATASET_TOOLS_PARAMEDITAREA_H

#include "../../app/Utils/Overlappable.h"
#include "../../app/Utils/UniqueObject.h"
#include "../../app/Utils/OverlappableSerialList.h"

#include <QFrame>

class HandDrawCurve : public UniqueObject, public Overlappable {
public:
    int start() const;
    int step() const;
    QList<int> values() const;
    int end() const;

    void setPos(int tick);
    void drawPoint(int tick, int value);
    void drawEnd();
    int valueAt(int tick);
    void insert(int tick, int value);
    bool remove(int tick);
    bool merge(const HandDrawCurve &curve);
    void clear();
    std::tuple<qsizetype, qsizetype> interval() const override;

protected:
    int m_start = 0;
    int m_step = 1;
    QList<int> m_values;

    QPoint m_mouseDownPos;
    bool m_drawing = false;
};

class ParamEditArea : public QFrame {
    Q_OBJECT

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    OverlappableSerialList<HandDrawCurve> m_curves;
    HandDrawCurve *m_editingCurve = new HandDrawCurve;
    bool firstDraw = true;
    QPoint m_prevPos;

    HandDrawCurve *findCurveByPos(double tick);
};



#endif // DATASET_TOOLS_PARAMEDITAREA_H
