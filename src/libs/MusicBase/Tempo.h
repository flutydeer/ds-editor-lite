//
// Created by FlutyDeer on 2025/11/27.
//

#ifndef DS_EDITOR_LITE_TEMPO_H
#define DS_EDITOR_LITE_TEMPO_H

#include <QString>

class Tempo {
public:
    int pos = 0;
    double value = 120;

    // Format a tempo value for display: 3 decimal places, trailing zeros stripped,
    // no unit suffix (the value is quarter-notes per minute, not "BPM").
    [[nodiscard]] static QString formatValue(double value);

    friend bool operator==(const Tempo &lhs, const Tempo &rhs);

    friend bool operator!=(const Tempo &lhs, const Tempo &rhs);
};

#endif //DS_EDITOR_LITE_TEMPO_H