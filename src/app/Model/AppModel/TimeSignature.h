//
// Created by fluty on 24-7-29.
//

#ifndef TIMESIGNATURE_H
#define TIMESIGNATURE_H

#include "Interface/ISerializable.h"

class TimeSignature : ISerializable {
public:
    TimeSignature() = default;

    TimeSignature(int num, int deno);

    [[nodiscard]] QJsonObject serialize() const override;

    bool deserialize(const QJsonObject &obj) override;

    int pos = 0;
    int numerator = 4;
    int denominator = 4;

    friend bool operator==(const TimeSignature &lhs, const TimeSignature &rhs);

    friend bool operator!=(const TimeSignature &lhs, const TimeSignature &rhs);
};


#endif // TIMESIGNATURE_H
