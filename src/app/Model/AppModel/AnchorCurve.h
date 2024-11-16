//
// Created by fluty on 24-8-17.
//

#ifndef ANCHORCURVE_H
#define ANCHORCURVE_H

#include "Curve.h"
#include "Utils/OverlappableSerialList.h"

class AnchorNode : public Overlappable, public UniqueObject {
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
    [[nodiscard]] CurveType type() const override {
        return Anchor;
    }

    [[nodiscard]] const OverlappableSerialList<AnchorNode> &nodes() const;
    void insertNode(AnchorNode *node);
    void removeNode(AnchorNode *node);
    [[nodiscard]] int localEndTick() const override;

private:
    OverlappableSerialList<AnchorNode> m_nodes;
};



#endif // ANCHORCURVE_H
