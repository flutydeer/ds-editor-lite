//
// Created by fluty on 2024/1/27.
//

#ifndef DSCURVE_H
#define DSCURVE_H

#include "../Utils/IOverlapable.h"
#include "../Utils/UniqueObject.h"

#include <QList>

class DsCurve : public IOverlapable, UniqueObject {
public:
    enum DsCurveType { Generic, Draw, Anchor };

    virtual ~DsCurve();

    virtual DsCurveType type() {
        return Generic;
    }
    int start() const;
    void setStart(int offset);

    int compareTo(DsCurve *obj) const;
    virtual int endTick() const {
        return m_start;
    }
    bool isOverlappedWith(DsCurve *obj) const;

private:
    int m_start = 0;
};

class DsDrawCurve final : public DsCurve {
public:
    DsCurveType type() override {
        return Draw;
    }
    int step; // TODO: remove
    const QList<int> &values() const;
    void setValues(const QList<int> &values);
    void insertValue(int value);

    int endTick() const override;

private:
    int m_step = 5;
    QList<int> m_values;
};

class DsAnchorNode : public IOverlapable, UniqueObject {
public:
    enum InterpMode { Linear, Hermite, Cubic, None };

    DsAnchorNode(const int pos, const int value) : m_pos(pos), m_value(value) {
    }

    int pos() const;
    void setPos(int pos);
    int value() const;
    void setValue(int value);
    InterpMode interpMode() const;
    void setInterpMode(InterpMode mode);

    int compareTo(DsAnchorNode *obj) const;
    bool isOverlappedWith(DsAnchorNode *obj) const;

private:
    int m_pos;
    int m_value;
    InterpMode m_interpMode = Cubic;
};

class DsAnchorCurve final : public DsCurve {
public:
    DsCurveType type() override {
        return Anchor;
    }
    //TODO: use OverlapableSerialList
    const QList<DsAnchorNode *> &nodes() const;
    void insertNode(DsAnchorNode *node);
    void removeNode(DsAnchorNode *node);
    int endTick() const override;

private:
    QList<DsAnchorNode *> m_nodes;
};

#endif // DSCURVE_H
