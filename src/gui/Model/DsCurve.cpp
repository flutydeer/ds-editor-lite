//
// Created by fluty on 2024/1/27.
//

#include "DsCurve.h"

DsCurve::~DsCurve() {
}

int DsCurve::start() {
    return m_start;
}

void DsCurve::setStart(int offset) {
    m_start = offset;
}