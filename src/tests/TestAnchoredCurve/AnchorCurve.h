//
// Created by fluty on 24-8-17.
//

#ifndef ANCHORCURVE_H
#define ANCHORCURVE_H

#include "anchoredcurve.h"
#include "../../app/Utils/Overlappable.h"
#include "../../app/Utils/OverlappableSerialList.h"
#include "../../app/Utils/UniqueObject.h"

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
    InterpMode m_interpMode = Hermite;
};

class AnchorCurve final {
public:
    [[nodiscard]] const OverlappableSerialList<AnchorNode> &nodes() const;
    void insertNode(AnchorNode *node);
    void removeNode(AnchorNode *node);
    double valueAt(int pos) const;

private:
    OverlappableSerialList<AnchorNode> m_nodes;
    AnchoredCurve<int, double> curve; // 仅用于获取平滑曲线上的值，不用于存储实际锚点
};



#endif // ANCHORCURVE_H
