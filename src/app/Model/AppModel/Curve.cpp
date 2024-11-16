//
// Created by fluty on 2024/1/27.
//

#include "Curve.h"

#include <QDebug>

int Curve::compareTo(const Curve *obj) const {
    int otherStart = obj->localStart();
    if (localStart() < otherStart)
        return -1;
    if (localStart() > otherStart)
        return 1;
    return 0;
}

int Curve::localStart() const {
    return m_startTick;
}

void Curve::setLocalStart(int start) {
    m_startTick = start;
}

int Curve::localEndTick() const {
    return m_startTick;
}

bool Curve::isOverlappedWith(Curve *obj) const {
    int otherStart = obj->localStart();
    auto otherEnd = obj->localEndTick();
    if (otherEnd <= localStart() || localEndTick() <= otherStart)
        return false;
    return true;
}

std::tuple<qsizetype, qsizetype> Curve::interval() const {
    return std::make_tuple(localStart(), localEndTick());
}

ProbeLine::ProbeLine(int startTick, int endTick) : Curve(-1), m_endTick(endTick) {
    Curve::setLocalStart(startTick);
}

int ProbeLine::localEndTick() const {
    return m_endTick;
}