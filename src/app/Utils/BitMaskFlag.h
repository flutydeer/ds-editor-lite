#ifndef BITMASK_FLAG_H
#define BITMASK_FLAG_H

#include <cstdint>
#include <type_traits>

template <typename T, typename Underlying = uint32_t>
class BitMaskFlag {
private:
    static_assert(std::is_integral_v<Underlying>, "Underlying must be integral");
    Underlying _value;

public:
    inline constexpr BitMaskFlag() noexcept : _value(0) {
    }

    inline constexpr explicit BitMaskFlag(Underlying v) noexcept : _value(v) {
    }

    inline constexpr explicit operator Underlying() const noexcept {
        return _value;
    }

    inline constexpr explicit operator bool() const noexcept {
        return _value != 0;
    }

    inline constexpr BitMaskFlag operator|(BitMaskFlag rhs) const noexcept {
        return BitMaskFlag(_value | rhs._value);
    }

    inline constexpr BitMaskFlag operator&(BitMaskFlag rhs) const noexcept {
        return BitMaskFlag(_value & rhs._value);
    }

    inline constexpr BitMaskFlag operator^(BitMaskFlag rhs) const noexcept {
        return BitMaskFlag(_value ^ rhs._value);
    }

    inline constexpr BitMaskFlag operator~() const noexcept {
        return BitMaskFlag(~_value);
    }

    inline BitMaskFlag &operator|=(BitMaskFlag rhs) noexcept {
        _value |= rhs._value;
        return *this;
    }

    inline BitMaskFlag &operator&=(BitMaskFlag rhs) noexcept {
        _value &= rhs._value;
        return *this;
    }

    inline BitMaskFlag &operator^=(BitMaskFlag rhs) noexcept {
        _value ^= rhs._value;
        return *this;
    }

    inline constexpr bool operator==(BitMaskFlag rhs) const noexcept {
        return _value == rhs._value;
    }

    inline constexpr bool operator!=(BitMaskFlag rhs) const noexcept {
        return _value != rhs._value;
    }

    inline constexpr bool has(BitMaskFlag flag) const noexcept {
        return (_value & flag._value) == flag._value;
    }

    inline void set(BitMaskFlag flag) noexcept {
        _value |= flag._value;
    }

    inline void unset(BitMaskFlag flag) noexcept {
        _value &= ~flag._value;
    }

    inline void toggle(BitMaskFlag flag) noexcept {
        _value ^= flag._value;
    }

    inline constexpr bool empty() const noexcept {
        return _value == 0;
    }

    inline void reset() noexcept {
        _value = 0;
    }
};


#endif // BITMASK_FLAG_H