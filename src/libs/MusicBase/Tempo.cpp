//
// Created by FlutyDeer on 2025/11/27.
//

#include "Tempo.h"

#include <QtNumeric>

bool operator==(const Tempo &lhs, const Tempo &rhs) {
    return lhs.pos == rhs.pos && qFuzzyCompare(lhs.value, rhs.value);
}

bool operator!=(const Tempo &lhs, const Tempo &rhs) {
    return !(lhs == rhs);
}