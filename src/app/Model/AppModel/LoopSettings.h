//
// Created for loop section feature
//

#ifndef LOOPSETTINGS_H
#define LOOPSETTINGS_H

#include "Interface/ISerializable.h"

class LoopSettings : public ISerializable {
public:
    LoopSettings() = default;
    LoopSettings(bool enabled, int start, int length);

    [[nodiscard]] QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;

    bool enabled = false;
    int start = 0;      // Start position in ticks
    int length = 0;     // Length in ticks (0 = uninitialized)

    [[nodiscard]] int end() const { return start + length; }

    friend bool operator==(const LoopSettings &lhs, const LoopSettings &rhs);
    friend bool operator!=(const LoopSettings &lhs, const LoopSettings &rhs);
};

#endif // LOOPSETTINGS_H
