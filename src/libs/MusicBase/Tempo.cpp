//
// Created by FlutyDeer on 2025/11/27.
//

#include "Tempo.h"

#include <QLocale>
#include <QtNumeric>

QString Tempo::formatValue(const double value) {
    const QLocale locale;
    const auto decimalPoint = locale.decimalPoint();
    auto s = locale.toString(value, 'f', 3);
    while (s.contains(decimalPoint) && s.endsWith('0'))
        s.chop(1);
    if (s.endsWith(decimalPoint))
        s.chop(decimalPoint.size());
    return s;
}

bool operator==(const Tempo &lhs, const Tempo &rhs) {
    return lhs.pos == rhs.pos && qFuzzyCompare(lhs.value, rhs.value);
}

bool operator!=(const Tempo &lhs, const Tempo &rhs) {
    return !(lhs == rhs);
}
