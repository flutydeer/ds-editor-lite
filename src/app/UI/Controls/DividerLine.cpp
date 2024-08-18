//
// Created by fluty on 24-3-18.
//

#include "DividerLine.h"

DividerLine::DividerLine(QWidget *parent) : QFrame(parent) {
    initUi();
}

DividerLine::DividerLine(Qt::Orientation orientation, QWidget *parent)
    : QFrame(parent), m_orientation(orientation) {
    initUi();
}

void DividerLine::setOrientation(Qt::Orientation orientation) {
    m_orientation = orientation;
    setFrameShape(m_orientation == Qt::Horizontal ? HLine : VLine);
}

void DividerLine::initUi() {
    setFrameShadow(Plain);
    setLineWidth(0);
    setFrameShape(m_orientation == Qt::Horizontal ? HLine : VLine);
}