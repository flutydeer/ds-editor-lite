//
// Created by fluty on 2024/1/27.
//

#ifndef DSCURVE_H
#define DSCURVE_H

#include <QVector>

class DsCurve {
public:
    enum DsCurveType { Generic, Draw, Anchor };

    virtual ~DsCurve();

    virtual DsCurveType type() {
        return Generic;
    }
    int start();
    void setStart(int offset);

private:
    int m_start = 0;
};

class DsDrawCurve final : public DsCurve {
public:
    DsCurveType type() override {
        return Draw;
    }
    int step = 5;
    QVector<int> values;
    // QVector<int> values() const;
    // void setValues(const QVector<int> &values);

    // private:
    //     QVector<int> m_values;
};

class DsAnchorNode {
public:
    enum interpMode { Linear, Hermite, Cubic, None };

    DsAnchorNode(const int pos, const int value) : pos(pos), value(value) {
    }

    int pos;
    int value;
    interpMode interpMode = Cubic;
};

class DsAnchorCurve final : public DsCurve {
public:
    DsCurveType type() override {
        return Anchor;
    }
    QVector<DsAnchorNode> nodes;
};

#endif //DSCURVE_H
