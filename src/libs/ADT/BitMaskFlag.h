#ifndef BITMASK_FLAG_H
#define BITMASK_FLAG_H

#include <cstdint>
#include <type_traits>

template <typename T, typename Underlying = uint32_t>
class BitMaskFlag {
    static_assert(std::is_integral_v<Underlying>, "Underlying must be integral");
    Underlying _value;

public:
    constexpr BitMaskFlag() noexcept : _value(0) {
    }

    constexpr explicit BitMaskFlag(Underlying v) noexcept : _value(v) {
    }

    constexpr explicit operator Underlying() const noexcept {
        return _value;
    }

    constexpr explicit operator bool() const noexcept {
        return _value != 0;
    }

    constexpr BitMaskFlag operator|(BitMaskFlag rhs) const noexcept {
        return BitMaskFlag(_value | rhs._value);
    }

    constexpr BitMaskFlag operator&(BitMaskFlag rhs) const noexcept {
        return BitMaskFlag(_value & rhs._value);
    }

    constexpr BitMaskFlag operator^(BitMaskFlag rhs) const noexcept {
        return BitMaskFlag(_value ^ rhs._value);
    }

    constexpr BitMaskFlag operator~() const noexcept {
        return BitMaskFlag(~_value);
    }

    BitMaskFlag &operator|=(BitMaskFlag rhs) noexcept {
        _value |= rhs._value;
        return *this;
    }

    BitMaskFlag &operator&=(BitMaskFlag rhs) noexcept {
        _value &= rhs._value;
        return *this;
    }

    BitMaskFlag &operator^=(BitMaskFlag rhs) noexcept {
        _value ^= rhs._value;
        return *this;
    }

    constexpr bool operator==(BitMaskFlag rhs) const noexcept {
        return _value == rhs._value;
    }

    constexpr bool operator!=(BitMaskFlag rhs) const noexcept {
        return _value != rhs._value;
    }

    constexpr bool has(BitMaskFlag flag) const noexcept {
        return (_value & flag._value) == flag._value;
    }

    void set(BitMaskFlag flag) noexcept {
        _value |= flag._value;
    }

    void unset(BitMaskFlag flag) noexcept {
        _value &= ~flag._value;
    }

    void toggle(BitMaskFlag flag) noexcept {
        _value ^= flag._value;
    }

    constexpr bool empty() const noexcept {
        return _value == 0;
    }

    void reset() noexcept {
        _value = 0;
    }
};


#endif // BITMASK_FLAG_H