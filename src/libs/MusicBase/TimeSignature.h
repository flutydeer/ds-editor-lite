//
// Created by fluty on 24-7-29.
//

#ifndef TIMESIGNATURE_H
#define TIMESIGNATURE_H

#include <lite/Support/ISerializable.h>

class TimeSignature : ISerializable {
public:
    TimeSignature() = default;

    TimeSignature(int num, int deno);

    TimeSignature(int barIndex, int num, int deno);

    [[nodiscard]] QJsonObject serialize() const override;

    bool deserialize(const QJsonObject &obj) override;

    [[nodiscard]] bool isValid() const;

    [[nodiscard]] int ticksPerBar() const;

    [[nodiscard]] int ticksPerBeat() const;

    // The measure number (0-based) this signature takes effect at. Signatures
    // are indexed by measure, not tick, matching opendspx::TimeSignature::index.
    int barIndex = 0;
    int numerator = 4;
    int denominator = 4;

    friend bool operator==(const TimeSignature &lhs, const TimeSignature &rhs);

    friend bool operator!=(const TimeSignature &lhs, const TimeSignature &rhs);
};


#endif // TIMESIGNATURE_H
