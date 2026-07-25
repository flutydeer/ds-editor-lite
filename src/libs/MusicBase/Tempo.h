//
// Created by FlutyDeer on 2025/11/27.
//

#ifndef DS_EDITOR_LITE_TEMPO_H
#define DS_EDITOR_LITE_TEMPO_H

class Tempo {
public:
    int pos = 0;
    double value = 120;

    friend bool operator==(const Tempo &lhs, const Tempo &rhs);

    friend bool operator!=(const Tempo &lhs, const Tempo &rhs);
};

#endif //DS_EDITOR_LITE_TEMPO_H