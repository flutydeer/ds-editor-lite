//
// Created by fluty on 24-8-17.
//

#ifndef ANCHORCURVE_H
#define ANCHORCURVE_H

#include "Curve.h"
#include <lite/ADT/OverlappableSerialList.h>

#include <opendspxinterpolator/interpolator.h>

class AnchorNode : public Overlappable, public UniqueObject {
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

    int compareTo(const AnchorNode *obj) const;
    bool isOverlappedWith(const AnchorNode *obj) const;
    std::tuple<qsizetype, qsizetype> interval() const override;

private:
    int m_pos;
    int m_value;
    InterpMode m_interpMode = Hermite;
};

class DrawCurve;

class AnchorCurve final : public Curve {
public:
    AnchorCurve() = default;
    AnchorCurve(const AnchorCurve &other);
    ~AnchorCurve();

    CurveType type() const override {
        return Anchor;
    }

    static opendspx::Interpolator<double> createInterpolator(
        const AnchorNode *n1, const AnchorNode *n2,
        const AnchorNode *ref1, const AnchorNode *ref2);

    DrawCurve *toDrawCurve() const;

    const OverlappableSerialList<AnchorNode> &nodes() const;
    void insertNode(AnchorNode *node);
    void removeNode(AnchorNode *node);
    int localEndTick() const override;

private:
    OverlappableSerialList<AnchorNode> m_nodes;
};


#endif // ANCHORCURVE_H