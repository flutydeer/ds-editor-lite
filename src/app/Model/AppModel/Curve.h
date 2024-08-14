//
// Created by fluty on 2024/1/27.
//

#ifndef DSCURVE_H
#define DSCURVE_H

#include <QList>

#include "Utils/Overlappable.h"
#include "Utils/ISelectable.h"
#include "Utils/Property.h"
#include "Utils/UniqueObject.h"

class QPoint;

class Curve : public Overlappable, public UniqueObject {
public:
    enum CurveType { Generic, Draw, Anchor };

    Property<int> start = 0;

    Curve() = default;
    explicit Curve(int id) : UniqueObject(id) {
    }
    Curve(const Curve &other) : UniqueObject(other.id()), start(other.start) {
    }

    virtual CurveType type() const {
        return Generic;
    }

    int compareTo(const Curve *obj) const;
    [[nodiscard]] virtual int endTick() const {
        return start;
    }
    virtual bool isOverlappedWith(Curve *obj) const;
    [[nodiscard]] std::tuple<qsizetype, qsizetype> interval() const override;
};

class ProbeLine final : public Curve {
public:
    ProbeLine() : Curve(-1) {
    }
    void setEndTick(int tick);
    [[nodiscard]] int endTick() const override;

private:
    int m_endTick = 0;
};

class AnchorNode : public Overlappable, public UniqueObject, public ISelectable {
public:
    enum InterpMode { Linear, Hermite, Cubic, None };

    AnchorNode(const int pos, const int value) : m_pos(pos), m_value(value) {
    }

    [[nodiscard]] int pos() const;
    void setPos(int pos);
    [[nodiscard]] int value() const;
    void setValue(int value);
    [[nodiscard]] InterpMode interpMode() const;
    void setInterpMode(InterpMode mode);

    int compareTo(const AnchorNode *obj) const;
    bool isOverlappedWith(const AnchorNode *obj) const;
    [[nodiscard]] std::tuple<qsizetype, qsizetype> interval() const override;

private:
    int m_pos;
    int m_value;
    InterpMode m_interpMode = Cubic;
};

class AnchorCurve final : public Curve {
public:
    CurveType type() const override {
        return Anchor;
    }
    // TODO: use OverlapableSerialList
    [[nodiscard]] const QList<AnchorNode *> &nodes() const;
    void insertNode(AnchorNode *node);
    void removeNode(AnchorNode *node);
    [[nodiscard]] int endTick() const override;

private:
    QList<AnchorNode *> m_nodes;
};

#endif // DSCURVE_H
