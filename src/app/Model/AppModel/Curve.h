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

    [[nodiscard]] virtual CurveType type() const {
        return Generic;
    }

    int compareTo(const Curve *obj) const;
    [[nodiscard]] int localStart() const;
    virtual void setLocalStart(int start);
    [[nodiscard]] virtual int localEndTick() const;

    virtual bool isOverlappedWith(Curve *obj) const;
    [[nodiscard]] std::tuple<qsizetype, qsizetype> interval() const override;


private:
    int m_startTick = 0;
};

class ProbeLine final : public Curve {
public:
    ProbeLine() : Curve(-1){};
    ProbeLine(int startTick, int endTick);
    [[nodiscard]] int localEndTick() const override;

private:
    int m_endTick = 0;
};

#endif // DSCURVE_H
