//
// Created by fluty on 2024/1/27.
//

#include "Curve.h"

#include <QDebug>

int Curve::start() const {
    return m_start;
}

void Curve::setStart(int offset) {
    m_start = offset;
}
int Curve::compareTo(Curve *obj) const {
    auto otherStart = obj->start();
    if (m_start < otherStart)
        return -1;
    if (m_start > otherStart)
        return 1;
    return 0;
}
bool Curve::isOverlappedWith(Curve *obj) const {
    auto otherStart = obj->start();
    auto otherEnd = obj->endTick();
    if (otherEnd <= start() || endTick() <= otherStart)
        return false;
    return true;
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
    auto curStart = start();
    auto otherStart = other.start();
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
        setStart(otherStart);
        if (curEnd < otherEnd) {
            auto startIndex = (otherEnd - curEnd) / step;
            for (int i = startIndex; i < other.values().count(); i++)
                appendValue(other.values().at(i));
        }
    }
}
void DrawCurve::overlayMergeWith(const DrawCurve &other) {
    auto curStart = start();
    auto otherStart = other.start();
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
int DrawCurve::endTick() const {
    return start() + step * m_values.count();
}
void ProbeLine::setEndTick(int tick) {
    m_endTick = tick;
}
int ProbeLine::endTick() const {
    return m_endTick;
}
int AnchorNode::pos() const {
    return m_pos;
}
void AnchorNode::setPos(int pos) {
    m_pos = pos;
}
int AnchorNode::value() const {
    return m_value;
}
void AnchorNode::setValue(int value) {
    m_value = value;
}
AnchorNode::InterpMode AnchorNode::interpMode() const {
    return m_interpMode;
}
void AnchorNode::setInterpMode(InterpMode mode) {
    m_interpMode = mode;
}
int AnchorNode::compareTo(AnchorNode *obj) const {
    auto otherPos = obj->pos();
    if (pos() < otherPos)
        return -1;
    if (pos() > otherPos)
        return 1;
    return 0;
}
bool AnchorNode::isOverlappedWith(AnchorNode *obj) const {
    return pos() == obj->pos();
}
const QList<AnchorNode *> &AnchorCurve::nodes() const {
    return m_nodes;
}
void AnchorCurve::insertNode(AnchorNode *node) {
    m_nodes.append(node);
}
void AnchorCurve::removeNode(AnchorNode *node) {
    m_nodes.removeOne(node);
}
int AnchorCurve::endTick() const {
    return m_nodes.at(m_nodes.count() - 1)->pos(); // TODO: fix
}