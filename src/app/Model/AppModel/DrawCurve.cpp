//
// Created by fluty on 24-8-14.
//

#include "DrawCurve.h"

#include <QDebug>

// DrawCurve::DrawCurve(const DrawCurve &other)
//     : Curve(other), step(other.step), m_values(other.m_values) {
//     // qDebug() << "DrawCurve() copy from: #id" << other.id() << "start:" << other.start;
// }

void DrawCurve::setLocalStart(const int start) {
    if (start % 5 == 0)
        Curve::setLocalStart(start);
    else
        qCritical() << "setStart: start not divisible by 5 tick";
}

const QList<int> &DrawCurve::values() const {
    return m_values;
}

bool DrawCurve::isEmpty() const {
    return m_values.isEmpty();
}

QList<int> DrawCurve::mid(const int tick) const {
    const auto startIndex = (tick - localStart()) / step;
    QList<int> result;
    for (int i = startIndex; i < m_values.count(); i++)
        result.append(m_values.at(i));
    return result;
}

void DrawCurve::clip(const int clipStart, const int clipEnd) {
    if (clipStart < localStart())
        eraseTailFrom(clipEnd);
    else if (localEndTick() < clipEnd) {
        const auto removeCount = (clipStart - localStart()) / step;
        removeValueRange(0, removeCount);
        setLocalStart(clipStart);
    } else if (clipStart > localStart() && clipEnd < localEndTick()) {
        eraseTailFrom(clipEnd);
        const auto removeCount = (clipStart - localStart()) / step;
        removeValueRange(0, removeCount);
        setLocalStart(clipStart);
    }
}

void DrawCurve::setValues(const QList<int> &values) {
    m_values = values;
}

void DrawCurve::insertValue(const int index, const int value) {
    m_values.insert(index, value);
}

void DrawCurve::insertValues(const int index, const QList<int> &values) {
    for (int i = 0; i < values.count(); i++)
        m_values.insert(index + i, values.at(i));
}

void DrawCurve::removeValueRange(const qsizetype i, const qsizetype n) {
    m_values.remove(i, n);
}

void DrawCurve::clearValues() {
    m_values.clear();
}

void DrawCurve::appendValue(const int value) {
    m_values.append(value);
}

void DrawCurve::replaceValue(const int index, const int value) {
    m_values.replace(index, value);
}

void DrawCurve::mergeWithCurrentPriority(const DrawCurve &other) {
    if (!other.isOverlappedWith(this))
        qCritical() << "mergeWithCurrentPriority: other is not overlapped with this";

    const int curStart = localStart();
    const int otherStart = other.localStart();
    const auto curEnd = localEndTick();
    const auto otherEnd = other.localEndTick();

    if (otherStart >= curStart && otherEnd <= curEnd) {
        qWarning() << "mergeWithCurrentPriority:"
                   << "other is in current range";
        return;
    }

    if (otherStart > curStart) {
        const auto startIndex = (curEnd - otherStart) / step;
        for (int i = startIndex; i < other.values().count(); i++)
            appendValue(other.values().at(i));
    } else { // otherStart <= curStart
        QList<int> earlyPoints;
        const auto earlyCurvePointCount = (curStart - otherStart) / step;
        for (int i = 0; i < earlyCurvePointCount; i++)
            earlyPoints.append(other.values().at(i));
        insertValues(0, earlyPoints);
        setLocalStart(otherStart);
        if (curEnd < otherEnd) {
            const auto tailCount = (otherEnd - curEnd) / step;
            const auto startIndex = other.values().count() - tailCount;
            for (auto i = startIndex; i < other.values().count(); i++)
                appendValue(other.values().at(i));
        }
    }
}

void DrawCurve::mergeWithOtherPriority(const DrawCurve &other) {
    const int curStart = localStart();
    const int otherStart = other.localStart();
    const auto curEnd = localEndTick();
    const auto otherEnd = other.localEndTick();
    if (otherEnd < curStart || curEnd < otherStart) {
        qCritical() << "overlayMergeWith: other curve is not overlapped with current curve";
    }

    if (otherStart > curStart) {
        const auto editStartIndex = (otherStart - curStart) / step;
        if (curEnd >= otherEnd) {
            for (int i = 0; i < other.values().count(); i++)
                replaceValue(editStartIndex + i, other.values().at(i));
        } else {
            removeValueRange(editStartIndex, values().count() - editStartIndex);
            for (int i = 0; i < other.values().count(); i++)
                appendValue(other.values().at(i));
        }
    } else { // otherStart <= curStart
        if (otherEnd >= curEnd) {
            clearValues();
            setLocalStart(otherStart);
            for (int i = 0; i < other.values().count(); i++)
                appendValue(other.values().at(i));
        } else { // otherEnd<curEnd
            const auto removeEndIndex = (otherEnd - curStart) / step;
            removeValueRange(0, removeEndIndex);
            setLocalStart(otherStart);
            insertValues(0, other.values());
        }
    }
}

void DrawCurve::erase(const int otherStart, const int otherEnd) {
    const int curStart = localStart();
    const auto curEnd = localEndTick();

    if (otherStart <= curStart && otherEnd >= curEnd)
        qFatal("DrawCurve::eraseWith: other curve fully covered current curve");

    if (otherStart > curStart) {
        const auto eraseLength = curEnd - otherStart;
        eraseTail(eraseLength);
    } else { // otherStart <= curStart
        const auto removeEndIndex = (otherEnd - curStart) / step;
        removeValueRange(0, removeEndIndex);
        setLocalStart(otherEnd);
    }
}

void DrawCurve::eraseTail(const int length) {
    const auto count = length / step;
    removeValueRange(values().count() - count, count);
}

void DrawCurve::eraseTailFrom(const int tick) {
    const auto length = localEndTick() - tick;
    eraseTail(length);
}

int DrawCurve::localEndTick() const {
    return localStart() + step * m_values.count();
}

bool operator==(const DrawCurve &lhs, const DrawCurve &rhs) {
    return lhs.localStart() == rhs.localStart() && lhs.step == rhs.step &&
           lhs.m_values == rhs.m_values;
}

bool operator!=(const DrawCurve &lhs, const DrawCurve &rhs) {
    return !(lhs == rhs);
}