// Fixed-point number used for every fractional rules value. The core never
// uses float or double (engine doc, Core foundations): results must be
// bit-identical on every compiler and CPU for lockstep multiplayer.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

namespace sov {

class Fixed {
public:
    // Four decimal places: exact for the data's percentages and route costs
    // (0.75, 0.5, 0.25) and parseable from decimal text without floats.
    static constexpr int64_t kScale = 10000;

    constexpr Fixed() = default;
    static constexpr Fixed fromRaw(int64_t raw) { Fixed f; f.raw_ = raw; return f; }
    static constexpr Fixed fromInt(int64_t v) { return fromRaw(v * kScale); }
    // n / d, truncated toward zero.
    static constexpr Fixed ratio(int64_t n, int64_t d) { return fromRaw(n * kScale / d); }
    // Parses "12", "-0.75", "3.5". Returns false on malformed text or more than
    // four decimals of precision that is not zero.
    static bool parse(std::string_view text, Fixed& out);

    constexpr int64_t raw() const { return raw_; }
    // Truncates toward zero, like Civ's integer conversions.
    constexpr int64_t toInt() const { return raw_ / kScale; }
    constexpr int64_t floor() const {
        return raw_ >= 0 ? raw_ / kScale : -((-raw_ + kScale - 1) / kScale);
    }
    constexpr int64_t ceil() const { return -Fixed::fromRaw(-raw_).floor(); }
    // Round half away from zero.
    constexpr int64_t round() const {
        return raw_ >= 0 ? (raw_ + kScale / 2) / kScale : -((-raw_ + kScale / 2) / kScale);
    }
    std::string toString() const;

    constexpr Fixed operator-() const { return fromRaw(-raw_); }
    constexpr Fixed operator+(Fixed o) const { return fromRaw(raw_ + o.raw_); }
    constexpr Fixed operator-(Fixed o) const { return fromRaw(raw_ - o.raw_); }
    Fixed operator*(Fixed o) const { return fromRaw(mulDiv(raw_, o.raw_, kScale)); }
    Fixed operator/(Fixed o) const { return fromRaw(mulDiv(raw_, kScale, o.raw_)); }
    constexpr Fixed operator*(int64_t v) const { return fromRaw(raw_ * v); }
    constexpr Fixed operator/(int64_t v) const { return fromRaw(raw_ / v); }
    Fixed& operator+=(Fixed o) { raw_ += o.raw_; return *this; }
    Fixed& operator-=(Fixed o) { raw_ -= o.raw_; return *this; }

    constexpr bool operator==(Fixed o) const { return raw_ == o.raw_; }
    constexpr bool operator!=(Fixed o) const { return raw_ != o.raw_; }
    constexpr bool operator<(Fixed o) const { return raw_ < o.raw_; }
    constexpr bool operator<=(Fixed o) const { return raw_ <= o.raw_; }
    constexpr bool operator>(Fixed o) const { return raw_ > o.raw_; }
    constexpr bool operator>=(Fixed o) const { return raw_ >= o.raw_; }

    // base ^ exponent for base >= 0, through fixed-point log2/exp2 (bit-exact on
    // every platform), rounded to the nearest representable value. Used for
    // Civ's growth and border cost curves (n^1.5, (6n)^1.3).
    static Fixed pow(Fixed base, Fixed exponent);

    // a * b / d with a 128-bit intermediate, truncated toward zero.
    static int64_t mulDiv(int64_t a, int64_t b, int64_t d);

private:
    int64_t raw_ = 0;
};

inline int64_t Fixed::mulDiv(int64_t a, int64_t b, int64_t d) {
#if defined(__SIZEOF_INT128__)
    __extension__ typedef __int128 int128;
    return static_cast<int64_t>(static_cast<int128>(a) * b / d);
#else
    // MSVC: signed 128-bit multiply and divide intrinsics (x64).
    int64_t hi = 0;
    int64_t lo = _mul128(a, b, &hi);
    int64_t rem = 0;
    return _div128(hi, lo, d, &rem);
#endif
}

}  // namespace sov
