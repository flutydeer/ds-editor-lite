//
// Created by fluty on 24-8-14.
//

#include "DrawCurve.h"

#include <QDebug>

// DrawCurve::DrawCurve(const DrawCurve &other)
//     : Curve(other), step(other.step), m_values(other.m_values) {
//     // qDebug() << "DrawCurve() copy from: #id" << other.id() << "start:" << other.start;
// }

void DrawCurve::setStart(int start) {
    if (start % 5 == 0)
        Curve::setStart(start);
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
    auto startIndex = (tick - start()) / step;
    QList<int> result;
    for (int i = startIndex; i < m_values.count(); i++)
        result.append(m_values.at(i));
    return result;
}

void DrawCurve::clip(int clipStart, int clipEnd) {
    if (clipStart < start())
        eraseTailFrom(clipEnd);
    else if (endTick() < clipEnd) {
        auto removeCount = (clipStart - start()) / step;
        removeValueRange(0, removeCount);
        setStart(clipStart);
    } else if (clipStart > start() && clipEnd < endTick()) {
        eraseTailFrom(clipEnd);
        auto removeCount = (clipStart - start()) / step;
        removeValueRange(0, removeCount);
        setStart(clipStart);
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

    int curStart = start();
    int otherStart = other.start();
    auto curEnd = endTick();
    auto otherEnd = other.endTick();

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
        setStart(otherStart);
        if (curEnd < otherEnd) {
            auto tailCount = (otherEnd - curEnd) / step;
            auto startIndex = other.values().count() - tailCount;
            for (auto i = startIndex; i < other.values().count(); i++)
                appendValue(other.values().at(i));
        }
    }
}

void DrawCurve::mergeWithOtherPriority(const DrawCurve &other) {
    int curStart = start();
    int otherStart = other.start();
    auto curEnd = endTick();
    auto otherEnd = other.endTick();
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
            setStart(otherStart);
            for (int i = 0; i < other.values().count(); i++)
                appendValue(other.values().at(i));
        } else { // otherEnd<curEnd
            auto removeEndIndex = (otherEnd - curStart) / step;
            removeValueRange(0, removeEndIndex);
            setStart(otherStart);
            insertValues(0, other.values());
        }
    }
}

void DrawCurve::erase(int otherStart, int otherEnd) {
    int curStart = start();
    auto curEnd = endTick();

    if (otherStart <= curStart && otherEnd >= curEnd)
        qFatal("DrawCurve::eraseWith: other curve fully covered current curve");

    if (otherStart > curStart) {
        auto eraseLength = curEnd - otherStart;
        eraseTail(eraseLength);
    } else { // otherStart <= curStart
        auto removeEndIndex = (otherEnd - curStart) / step;
        removeValueRange(0, removeEndIndex);
        setStart(otherEnd);
    }
}

void DrawCurve::eraseTail(int length) {
    auto count = length / step;
    removeValueRange(values().count() - count, count);
}

void DrawCurve::eraseTailFrom(int tick) {
    auto length = endTick() - tick;
    eraseTail(length);
}

int DrawCurve::endTick() const {
    return start() + step * m_values.count();
}

bool operator==(const DrawCurve &lhs, const DrawCurve &rhs) {
    return lhs.start() == rhs.start() && lhs.step == rhs.step && lhs.m_values == rhs.m_values;
}

bool operator!=(const DrawCurve &lhs, const DrawCurve &rhs) {
    return !(lhs == rhs);
}