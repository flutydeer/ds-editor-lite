//
// Created by FlutyDeer on 2025/11/27.
//

#include "Tempo.h"

#include <QtNumeric>

QString Tempo::formatValue(const double value) {
    auto s = QString::number(value, 'f', 3);
    while (s.contains('.') && s.endsWith('0'))
        s.chop(1);
    if (s.endsWith('.'))
        s.chop(1);
    return s;
}

bool operator==(const Tempo &lhs, const Tempo &rhs) {
    return lhs.pos == rhs.pos && qFuzzyCompare(lhs.value, rhs.value);
}

bool operator!=(const Tempo &lhs, const Tempo &rhs) {
    return !(lhs == rhs);
}