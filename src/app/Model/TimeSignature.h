//
// Created by fluty on 24-7-29.
//

#ifndef TIMESIGNATURE_H
#define TIMESIGNATURE_H

class TimeSignature {
public:
    TimeSignature() = default;
    TimeSignature(int num, int deno) : numerator(num), denominator(deno) {
    }
    int pos = 0;
    int numerator = 4;
    int denominator = 4;
};

#endif //TIMESIGNATURE_H
