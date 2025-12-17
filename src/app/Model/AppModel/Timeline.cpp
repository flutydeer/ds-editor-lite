//
// Created by FlutyDeer on 2025/11/27.
//

#include "Timeline.h"

// TODO 支持多曲速
double Timeline::tickToMs(const double tick) const {
    return tick * 60 / tempos.first().value / 480 * 1000;
}

double Timeline::msToTick(const double ms) const {
    return ms * 480 * tempos.first().value / 60000;
}

double Timeline::tickToSec(double tick) const {
    return tickToMs(tick) / 1000.0;
}

double Timeline::secToTick(double ms) const {
    return msToTick(ms * 1000.0);
}

QString Timeline::getBarBeatTickTime(const int ticks) const {
    const auto timeSignature = timeSignatures.first();
    const int barTicks = 1920 * timeSignature.numerator / timeSignature.denominator;
    const int beatTicks = 1920 / timeSignature.denominator;
    const auto bar = ticks / barTicks + 1;
    const auto beat = ticks % barTicks / beatTicks + 1;
    const auto tick = ticks % barTicks % beatTicks;
    auto str = QString::asprintf("%03d", bar) + ":" + QString::asprintf("%02d", beat) + ":" +
               QString::asprintf("%03d", tick);
    return str;
}

bool operator==(const Timeline &lhs, const Timeline &rhs) {
    return lhs.tempos == rhs.tempos && lhs.timeSignatures == rhs.timeSignatures;
}

bool operator!=(const Timeline &lhs, const Timeline &rhs) {
    return !(lhs == rhs);
}