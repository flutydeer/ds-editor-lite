//
// Created by FlutyDeer on 2026/7/5.
//

#include "MusicTimeConverter.h"
#include "Global/AppGlobal.h"

namespace MusicTimeConverter {

double tickToMs(const double tick, const double tempo) {
    return tick * 60 / tempo / AppGlobal::ticksPerQuarterNote * 1000;
}

double msToTick(const double ms, const double tempo) {
    return ms * AppGlobal::ticksPerQuarterNote * tempo / 60000;
}

double tickToSec(const double tick, const double tempo) {
    return tickToMs(tick, tempo) / 1000.0;
}

double secToTick(const double sec, const double tempo) {
    return msToTick(sec * 1000.0, tempo);
}

QString getBarBeatTickTime(const int ticks, const int numerator, const int denominator) {
    const int barTicks = AppGlobal::ticksPerWholeNote * numerator / denominator;
    const int beatTicks = AppGlobal::ticksPerWholeNote / denominator;
    const auto bar = ticks / barTicks + 1;
    const auto beat = ticks % barTicks / beatTicks + 1;
    const auto tick = ticks % barTicks % beatTicks;
    auto str = QString::asprintf("%03d", bar) + ":" + QString::asprintf("%02d", beat) + ":" +
               QString::asprintf("%03d", tick);
    return str;
}

} // namespace MusicTimeConverter