//
// Created by fluty on 2024/1/27.
//

#ifndef DSCURVE_H
#define DSCURVE_H

#include <QList>

#include "Utils/IOverlapable.h"
#include "Utils/ISelectable.h"
#include "Utils/UniqueObject.h"

class Curve : public IOverlapable, UniqueObject {
public:
    enum CurveType { Generic, Draw, Anchor };

    virtual ~Curve() = default;

    virtual CurveType type() {
        return Generic;
    }
    int start() const;
    void setStart(int offset);

    int compareTo(Curve *obj) const;
    virtual int endTick() const {
        return m_start;
    }
    bool isOverlappedWith(Curve *obj) const;

private:
    int m_start = 0;
};

class DrawCurve final : public Curve {
public:
    CurveType type() override {
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

class AnchorNode : public IOverlapable, public UniqueObject, public ISelectable {
public:
    enum InterpMode { Linear, Hermite, Cubic, None };

    AnchorNode(const int pos, const int value) : m_pos(pos), m_value(value) {
    }

    int pos() const;
    void setPos(int pos);
    int value() const;
    void setValue(int value);
    InterpMode interpMode() const;
    void setInterpMode(InterpMode mode);

    int compareTo(AnchorNode *obj) const;
    bool isOverlappedWith(AnchorNode *obj) const;

private:
    int m_pos;
    int m_value;
    InterpMode m_interpMode = Cubic;
};

class AnchorCurve final : public Curve {
public:
    CurveType type() override {
        return Anchor;
    }
    //TODO: use OverlapableSerialList
    const QList<AnchorNode *> &nodes() const;
    void insertNode(AnchorNode *node);
    void removeNode(AnchorNode *node);
    int endTick() const override;

private:
    QList<AnchorNode *> m_nodes;
};

#endif // DSCURVE_H
