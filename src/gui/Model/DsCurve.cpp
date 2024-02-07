//
// Created by fluty on 2024/1/27.
//

#include "DsCurve.h"

DsCurve::~DsCurve() {
}

int DsCurve::start() const {
    return m_start;
}

void DsCurve::setStart(int offset) {
    m_start = offset;
}
int DsCurve::compareTo(DsCurve *obj) const {
    auto otherStart = obj->start();
    if (m_start < otherStart)
        return -1;
    if (m_start > otherStart)
        return 1;
    return 0;
}
bool DsCurve::isOverlappedWith(DsCurve *obj) const {
    auto otherStart = obj->start();
    auto otherEnd = obj->endTick();
    if (otherEnd <= start() || endTick() <= otherStart)
        return false;
    return true;
}
const QList<int> &DsDrawCurve::values() const {
    return m_values;
}
void DsDrawCurve::setValues(const QList<int> &values) {
    m_values = values;
}
void DsDrawCurve::insertValue(int value) {
    m_values.append(value);
}
int DsDrawCurve::endTick() const {
    return start() + m_step * m_values.count();
}
int DsAnchorNode::pos() const {
    return m_pos;
}
void DsAnchorNode::setPos(int pos) {
    m_pos = pos;
}
int DsAnchorNode::value() const {
    return m_value;
}
void DsAnchorNode::setValue(int value) {
    m_value = value;
}
DsAnchorNode::InterpMode DsAnchorNode::interpMode() const {
    return m_interpMode;
}
void DsAnchorNode::setInterpMode(InterpMode mode) {
    m_interpMode = mode;
}
int DsAnchorNode::compareTo(DsAnchorNode *obj) const {
    auto otherPos = obj->pos();
    if (pos() < otherPos)
        return -1;
    if (pos() > otherPos)
        return 1;
    return 0;
}
bool DsAnchorNode::isOverlappedWith(DsAnchorNode *obj) const {
    return pos() == obj->pos();
}
const QList<DsAnchorNode *> &DsAnchorCurve::nodes() const {
    return m_nodes;
}
void DsAnchorCurve::insertNode(DsAnchorNode *node) {
    m_nodes.append(node);
}
void DsAnchorCurve::removeNode(DsAnchorNode *node) {
    m_nodes.removeOne(node);
}
int DsAnchorCurve::endTick() const {
    return m_nodes.at(m_nodes.count() - 1)->pos(); // TODO: fix
}