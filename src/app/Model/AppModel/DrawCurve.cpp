//
// Created by fluty on 24-8-14.
//

#include "DrawCurve.h"

DrawCurve::DrawCurve(const DrawCurve &other)
    : Curve(other), step(other.step), m_values(other.m_values) {
    // qDebug() << "DrawCurve() copy from: #id" << other.id() << "start:" << other.start;
}
const QList<int> &DrawCurve::values() const {
    return m_values;
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
void DrawCurve::removeValueRange(int i, int n) {
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
void DrawCurve::mergeWith(const DrawCurve &other) {
    int curStart = start;
    int otherStart = other.start;
    auto curEnd = endTick();
    auto otherEnd = other.endTick();
    if (otherStart > curStart && otherEnd < curEnd)
        return;

    if (otherStart > curStart) {
        auto startIndex = (curEnd - otherStart) / step;
        for (int i = startIndex; i < other.values().count(); i++)
            appendValue(other.values().at(i));
    } else {
        QList<int> earlyCurve;
        auto earlyCurvePointCount = (curStart - otherStart) / step;
        for (int i = 0; i < earlyCurvePointCount; i++)
            earlyCurve.append(other.values().at(i));
        insertValues(0, earlyCurve);
        start = otherStart;
        if (curEnd < otherEnd) {
            auto startIndex = (otherEnd - curEnd) / step;
            for (int i = startIndex; i < other.values().count(); i++)
                appendValue(other.values().at(i));
        }
    }
}
void DrawCurve::overlayMergeWith(const DrawCurve &other) {
    int curStart = start;
    int otherStart = other.start;
    auto curEnd = endTick();
    auto otherEnd = other.endTick();
    // qDebug() << "DrawCurve::overlayMergeWith" << otherStart << otherEnd;
    // Q_ASSERT_X(otherStart <= curEnd && curStart >= otherEnd, "DrawCurve::drawCurve",
    //            "other curve is not overlapped with current");

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
            start = otherStart;
            for (int i = 0; i < other.values().count(); i++)
                appendValue(other.values().at(i));
        } else { // otherEnd<curEnd
            auto removeEndIndex = (otherEnd - curStart) / step;
            removeValueRange(0, removeEndIndex);
            start = otherStart;
            insertValues(0, other.values());
        }
    }
}
void DrawCurve::eraseWith(const DrawCurve &other) {
    // TODO:
}
int DrawCurve::endTick() const {
    return start + step * m_values.count();
}