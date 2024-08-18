//
// Created by fluty on 2024/1/27.
//

#ifndef DSCURVE_H
#define DSCURVE_H

#include <QList>

#include "Utils/Overlappable.h"
#include "Utils/UniqueObject.h"

class QPoint;

class Curve : public Overlappable, public UniqueObject {
public:
    enum CurveType { Generic, Draw, Anchor };

    Curve() = default;
    explicit Curve(int id) : UniqueObject(id){};
    Curve(const Curve &other) : UniqueObject(other.id()), start(other.start){};

    [[nodiscard]] virtual CurveType type() const {
        return Generic;
    }

    int compareTo(const Curve *obj) const;

    [[nodiscard]] virtual int endTick() const {
        return start;
    }

    virtual bool isOverlappedWith(Curve *obj) const;
    [[nodiscard]] std::tuple<qsizetype, qsizetype> interval() const override;

    int start = 0;
};

class ProbeLine final : public Curve {
public:
    ProbeLine() : Curve(-1){};
    ProbeLine(int startTick, int endTick);
    int endTick() const override;

private:
    int m_endTick = 0;
};

#endif // DSCURVE_H
