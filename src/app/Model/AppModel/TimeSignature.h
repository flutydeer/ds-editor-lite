//
// Created by fluty on 24-7-29.
//

#ifndef TIMESIGNATURE_H
#define TIMESIGNATURE_H

#include "Interface/ISerializable.h"

class TimeSignature : ISerializable {
public:
    TimeSignature() = default;

    TimeSignature(const int num, const int deno) : numerator(num), denominator(deno) {
    }

    [[nodiscard]] QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;

    int pos = 0;
    int numerator = 4;
    int denominator = 4;
};

inline QJsonObject TimeSignature::serialize() const {
    return QJsonObject{
        {"pos",         pos        },
        {"numerator",   numerator  },
        {"denominator", denominator}
    };
}

inline bool TimeSignature::deserialize(const QJsonObject &obj) {
    pos = obj["pos"].toInt();
    numerator = obj["numerator"].toInt();
    denominator = obj["denominator"].toInt();
    return true;
}

#endif // TIMESIGNATURE_H
