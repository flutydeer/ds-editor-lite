//
// Created by fluty on 2024/1/27.
//

#include "Curve.h"

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
void DrawCurve::insertValue(int value) {
    m_values.append(value);
}
int DrawCurve::endTick() const {
    return start() + m_step * m_values.count();
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