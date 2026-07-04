//
// Created by fluty on 24-8-17.
//

#include "AnchorCurve.h"

#include "DrawCurve.h"

AnchorCurve::AnchorCurve(const AnchorCurve &other) : Curve() {
    setLocalStart(other.localStart());
    for (const auto *node : other.nodes().toList()) {
        auto *copy = new AnchorNode(node->pos(), node->value());
        copy->setInterpMode(node->interpMode());
        insertNode(copy);
    }
}

int AnchorNode::pos() const {
    return m_pos;
}

void AnchorNode::setPos(const int pos) {
    m_pos = pos;
}

int AnchorNode::value() const {
    return m_value;
}

void AnchorNode::setValue(const int value) {
    m_value = value;
}

AnchorNode::InterpMode AnchorNode::interpMode() const {
    return m_interpMode;
}

void AnchorNode::setInterpMode(const InterpMode mode) {
    m_interpMode = mode;
}

int AnchorNode::compareTo(const AnchorNode *obj) const {
    const auto otherPos = obj->pos();
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

opendspx::Interpolator<double> AnchorCurve::createInterpolator(
    const AnchorNode *n1, const AnchorNode *n2,
    const AnchorNode *ref1, const AnchorNode *ref2) {
    const double x1 = n1->pos();
    const double y1 = n1->value();
    const double x2 = n2->pos();
    const double y2 = n2->value();

    if (n1->interpMode() == AnchorNode::Linear)
        return opendspx::Interpolator<double>::createLinear(x1, y1, x2, y2);

    if (ref1 && ref2)
        return opendspx::Interpolator<double>::create(
            x1, y1, x2, y2, ref1->pos(), ref1->value(), ref2->pos(), ref2->value());
    if (ref1)
        return opendspx::Interpolator<double>::createWithRef1Only(
            x1, y1, x2, y2, ref1->pos(), ref1->value());
    if (ref2)
        return opendspx::Interpolator<double>::createWithRef2Only(
            x1, y1, x2, y2, ref2->pos(), ref2->value());
    return opendspx::Interpolator<double>::createLinear(x1, y1, x2, y2);
}

DrawCurve *AnchorCurve::toDrawCurve() const {
    const auto &nodeList = m_nodes.toList();
    if (nodeList.size() < 2)
        return nullptr;

    constexpr int step = 5;
    const int startTick = nodeList.first()->pos() / step * step;
    const int endTick = nodeList.last()->pos();

    QList<int> values;
    int segIdx = 0;
    auto interp = createInterpolator(
        nodeList[0], nodeList[1],
        nullptr,
        nodeList.size() > 2 ? nodeList[2] : nullptr);

    for (int tick = startTick; tick <= endTick; tick += step) {
        while (segIdx < nodeList.size() - 2 && tick >= nodeList[segIdx + 1]->pos()) {
            segIdx++;
            auto *ref1 = segIdx > 0 ? nodeList[segIdx - 1] : nullptr;
            auto *ref2 = segIdx + 2 < nodeList.size() ? nodeList[segIdx + 2] : nullptr;
            interp = createInterpolator(nodeList[segIdx], nodeList[segIdx + 1], ref1, ref2);
        }
        values.append(static_cast<int>(interp.evaluate(tick)));
    }

    auto *dc = new DrawCurve;
    dc->setLocalStart(startTick);
    dc->setValues(values);
    return dc;
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

int AnchorCurve::localEndTick() const {
    const auto list = m_nodes.toList();
    if (list.isEmpty())
        qFatal("AnchorCurve::localEndTick: nodes list is empty");
    return list.last()->pos();
}