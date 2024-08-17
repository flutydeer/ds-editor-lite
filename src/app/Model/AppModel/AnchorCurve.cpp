//
// Created by fluty on 24-8-17.
//

#include "AnchorCurve.h"

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
int AnchorNode::compareTo(const AnchorNode *obj) const {
    auto otherPos = obj->pos();
    if (pos() < otherPos)
        return -1;
    if (pos() > otherPos)
        return 1;
    return 0;
}
bool AnchorNode::isOverlappedWith(const AnchorNode *obj) const {
    return pos() == obj->pos();
}
std::tuple<qsizetype, qsizetype> AnchorNode::interval() const {
    return std::make_tuple(pos(), pos());
}
const OverlappableSerialList<AnchorNode> &AnchorCurve::nodes() const {
    return m_nodes;
}
void AnchorCurve::insertNode(AnchorNode *node) {
    m_nodes.add(node);
}
void AnchorCurve::removeNode(AnchorNode *node) {
    m_nodes.remove(node);
}
int AnchorCurve::endTick() const {
    return m_nodes.toList().last()->pos(); // TODO: fix
}