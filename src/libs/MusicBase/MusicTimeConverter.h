//
// Created by FlutyDeer on 2026/7/5.
//

#ifndef DS_EDITOR_LITE_MUSICTIMECONVERTER_H
#define DS_EDITOR_LITE_MUSICTIMECONVERTER_H

#include <QString>

namespace MusicTimeConverter {

[[nodiscard]] double tickToMs(double tick, double tempo);
[[nodiscard]] double msToTick(double ms, double tempo);
[[nodiscard]] double tickToSec(double tick, double tempo);
[[nodiscard]] double secToTick(double sec, double tempo);
[[nodiscard]] QString getBarBeatTickTime(int ticks, int numerator, int denominator);

} // namespace MusicTimeConverter

#endif // DS_EDITOR_LITE_MUSICTIMECONVERTER_H