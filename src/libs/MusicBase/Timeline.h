//
// Created by FlutyDeer on 2025/11/27.
//

#ifndef DS_EDITOR_LITE_TIMELINE_H
#define DS_EDITOR_LITE_TIMELINE_H

#include "Tempo.h"
#include "TimeSignature.h"

#include <QList>

class Timeline {
public:
    QList<Tempo> tempos;
    QList<TimeSignature> timeSignatures;

    [[nodiscard]] double tickToMs(double tick) const;
    [[nodiscard]] double msToTick(double ms) const;

    [[nodiscard]] double tickToSec(double tick) const;
    [[nodiscard]] double secToTick(double ms) const;
    [[nodiscard]] QString getBarBeatTickTime(int ticks) const;

    friend bool operator==(const Timeline &lhs, const Timeline &rhs);

    friend bool operator!=(const Timeline &lhs, const Timeline &rhs);
};


#endif // DS_EDITOR_LITE_TIMELINE_H