//
// Created by fluty on 2024/1/27.
//

#include "Curve.h"

#include "SingingClip.h"

#include <QDebug>

SingingClip *Curve::clip() const {
    return m_clip;
}

void Curve::setClip(SingingClip *clip) {
    m_clip = clip;
}

int Curve::globalStart() const {
    if (!m_clip) {
        qFatal() << "SingingClip is null";
        return m_startTick;
    }
    auto offset = m_clip->start();
    return m_startTick + offset;
}

void Curve::setGlobalStart(int start) {
    if (!m_clip) {
        qFatal() << "SingingClip is null";
        setLocalStart(start);
        return;
    }
    auto offset = m_clip->start();
    auto rStart = start - offset;
    Q_ASSERT(rStart >= 0);
    setLocalStart(rStart);
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

int Curve::compareTo(const Curve *obj) const {
    if (!m_clip) {
        qWarning() << "SingingClip is null";
        return 0;
    }
    if (m_clip != obj->m_clip) {
        qWarning() << "SingingClip is not the same";
        return 0;
    }
    int otherStart = obj->localStart();
    if (localStart() < otherStart)
        return -1;
    if (localStart() > otherStart)
        return 1;
    return 0;
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