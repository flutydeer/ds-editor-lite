//
// Created by FlutyDeer on 2025/11/27.
//

#include "Timeline.h"
#include "Global/AppGlobal.h"
#include "Utils/MusicTimeConverter.h"

// TODO 支持多曲速
double Timeline::tickToMs(const double tick) const {
    return MusicTimeConverter::tickToMs(tick, tempos.first().value);
}

double Timeline::msToTick(const double ms) const {
    return MusicTimeConverter::msToTick(ms, tempos.first().value);
}

double Timeline::tickToSec(double tick) const {
    return MusicTimeConverter::tickToSec(tick, tempos.first().value);
}

double Timeline::secToTick(double ms) const {
    return MusicTimeConverter::secToTick(ms, tempos.first().value);
}

QString Timeline::getBarBeatTickTime(const int ticks) const {
    const auto ts = timeSignatures.first();
    return MusicTimeConverter::getBarBeatTickTime(ticks, ts.numerator, ts.denominator);
}

bool operator==(const Timeline &lhs, const Timeline &rhs) {
    return lhs.tempos == rhs.tempos && lhs.timeSignatures == rhs.timeSignatures;
}

bool operator!=(const Timeline &lhs, const Timeline &rhs) {
    return !(lhs == rhs);
}