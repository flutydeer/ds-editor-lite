//
// Created by fluty on 2024/1/27.
//

#include "Curve.h"

#include <QDebug>

int Curve::compareTo(const Curve *obj) const {
    int otherStart = obj->start;
    if (start < otherStart)
        return -1;
    if (start > otherStart)
        return 1;
    return 0;
}

bool Curve::isOverlappedWith(Curve *obj) const {
    int otherStart = obj->start;
    auto otherEnd = obj->endTick();
    if (otherEnd <= start || endTick() <= otherStart)
        return false;
    return true;
}

std::tuple<qsizetype, qsizetype> Curve::interval() const {
    return std::make_tuple(start, endTick());
}

ProbeLine::ProbeLine(int startTick, int endTick) : Curve(-1), m_endTick(endTick) {
    start = startTick;
}

int ProbeLine::endTick() const {
    return m_endTick;
}