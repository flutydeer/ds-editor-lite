//
// Created by fluty on 24-8-14.
//

#include "DrawCurve.h"

#include <QDebug>

// DrawCurve::DrawCurve(const DrawCurve &other)
//     : Curve(other), step(other.step), m_values(other.m_values) {
//     // qDebug() << "DrawCurve() copy from: #id" << other.id() << "start:" << other.start;
// }

void DrawCurve::setLocalStart(int start) {
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

QList<int> DrawCurve::mid(int tick) const {
    auto startIndex = (tick - localStart()) / step;
    QList<int> result;
    for (int i = startIndex; i < m_values.count(); i++)
        result.append(m_values.at(i));
    return result;
}

void DrawCurve::clip(int clipStart, int clipEnd) {
    if (clipStart < localStart())
        eraseTailFrom(clipEnd);
    else if (localEndTick() < clipEnd) {
        auto removeCount = (clipStart - localStart()) / step;
        removeValueRange(0, removeCount);
        setLocalStart(clipStart);
    } else if (clipStart > localStart() && clipEnd < localEndTick()) {
        eraseTailFrom(clipEnd);
        auto removeCount = (clipStart - localStart()) / step;
        removeValueRange(0, removeCount);
        setLocalStart(clipStart);
    }
}

void DrawCurve::setValues(const QList<int> &values) {
    m_values = values;
}

void DrawCurve::insertValue(int index, int value) {
    m_values.insert(index, value);
}

void DrawCurve::insertValues(int index, const QList<int> &values) {
    for (int i = 0; i < values.count(); i++)
        m_values.insert(index + i, values.at(i));
}

void DrawCurve::removeValueRange(qsizetype i, qsizetype n) {
    m_values.remove(i, n);
}

void DrawCurve::clearValues() {
    m_values.clear();
}

void DrawCurve::appendValue(int value) {
    m_values.append(value);
}

void DrawCurve::replaceValue(int index, int value) {
    m_values.replace(index, value);
}

void DrawCurve::mergeWithCurrentPriority(const DrawCurve &other) {
    if (!other.isOverlappedWith(this))
        qCritical() << "mergeWithCurrentPriority: other is not overlapped with this";

    int curStart = localStart();
    int otherStart = other.localStart();
    auto curEnd = localEndTick();
    auto otherEnd = other.localEndTick();

    if (otherStart >= curStart && otherEnd <= curEnd) {
        qWarning() << "mergeWithCurrentPriority:"
                   << "other is in current range";
        return;
    }

    if (otherStart > curStart) {
        auto startIndex = (curEnd - otherStart) / step;
        for (int i = startIndex; i < other.values().count(); i++)
            appendValue(other.values().at(i));
    } else { // otherStart <= curStart
        QList<int> earlyPoints;
        auto earlyCurvePointCount = (curStart - otherStart) / step;
        for (int i = 0; i < earlyCurvePointCount; i++)
            earlyPoints.append(other.values().at(i));
        insertValues(0, earlyPoints);
        setLocalStart(otherStart);
        if (curEnd < otherEnd) {
            auto tailCount = (otherEnd - curEnd) / step;
            auto startIndex = other.values().count() - tailCount;
            for (auto i = startIndex; i < other.values().count(); i++)
                appendValue(other.values().at(i));
        }
    }
}

void DrawCurve::mergeWithOtherPriority(const DrawCurve &other) {
    int curStart = localStart();
    int otherStart = other.localStart();
    auto curEnd = localEndTick();
    auto otherEnd = other.localEndTick();
    if (otherEnd < curStart || curEnd < otherStart) {
        qCritical() << "overlayMergeWith: other curve is not overlapped with current curve";
    }

    if (otherStart > curStart) {
        auto editStartIndex = (otherStart - curStart) / step;
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
            auto removeEndIndex = (otherEnd - curStart) / step;
            removeValueRange(0, removeEndIndex);
            setLocalStart(otherStart);
            insertValues(0, other.values());
        }
    }
}

void DrawCurve::erase(int otherStart, int otherEnd) {
    int curStart = localStart();
    auto curEnd = localEndTick();

    if (otherStart <= curStart && otherEnd >= curEnd)
        qFatal("DrawCurve::eraseWith: other curve fully covered current curve");

    if (otherStart > curStart) {
        auto eraseLength = curEnd - otherStart;
        eraseTail(eraseLength);
    } else { // otherStart <= curStart
        auto removeEndIndex = (otherEnd - curStart) / step;
        removeValueRange(0, removeEndIndex);
        setLocalStart(otherEnd);
    }
}

void DrawCurve::eraseTail(int length) {
    auto count = length / step;
    removeValueRange(values().count() - count, count);
}

void DrawCurve::eraseTailFrom(int tick) {
    auto length = localEndTick() - tick;
    eraseTail(length);
}

int DrawCurve::localEndTick() const {
    return localStart() + step * m_values.count();
}

bool operator==(const DrawCurve &lhs, const DrawCurve &rhs) {
    return lhs.localStart() == rhs.localStart() && lhs.step == rhs.step && lhs.m_values == rhs.m_values;
}

bool operator!=(const DrawCurve &lhs, const DrawCurve &rhs) {
    return !(lhs == rhs);
}