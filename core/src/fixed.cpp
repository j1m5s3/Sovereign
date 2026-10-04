#include "sovereign/fixed.h"

namespace sov {

bool Fixed::parse(std::string_view text, Fixed& out) {
    size_t i = 0;
    bool neg = false;
    if (i < text.size() && (text[i] == '-' || text[i] == '+')) {
        neg = text[i] == '-';
        ++i;
    }
    if (i >= text.size()) return false;
    int64_t whole = 0;
    bool digits = false;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
        whole = whole * 10 + (text[i] - '0');
        if (whole > INT64_MAX / kScale / 10) return false;
        digits = true;
        ++i;
    }
    int64_t frac = 0;
    int64_t place = kScale / 10;
    if (i < text.size() && text[i] == '.') {
        ++i;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
            int d = text[i] - '0';
            if (place == 0) {
                if (d != 0) return false;  // more precision than we can hold
            } else {
                frac += d * place;
                place /= 10;
            }
            digits = true;
            ++i;
        }
    }
    if (!digits || i != text.size()) return false;
    int64_t raw = whole * kScale + frac;
    out = fromRaw(neg ? -raw : raw);
    return true;
}

namespace {
// Q32.32 helpers. Rules values stay far below 2^31, so int64 holds them.
constexpr int64_t kOne = int64_t(1) << 32;

// 2^(2^-i) for i = 1..32 in Q32.32, rounded (generated with Python's decimal).
constexpr uint64_t kExp2Steps[32] = {
    0x000000016A09E668ULL, 0x00000001306FE0A3ULL, 0x00000001172B83C8ULL, 0x000000010B5586D0ULL,
    0x00000001059B0D31ULL, 0x0000000102C9A3E7ULL, 0x000000010163DAA0ULL, 0x0000000100B1AFA6ULL,
    0x000000010058C86EULL, 0x00000001002C605EULL, 0x0000000100162F39ULL, 0x00000001000B175FULL,
    0x0000000100058BA0ULL, 0x000000010002C5CCULL, 0x00000001000162E5ULL, 0x000000010000B172ULL,
    0x00000001000058B9ULL, 0x0000000100002C5DULL, 0x000000010000162EULL, 0x0000000100000B17ULL,
    0x000000010000058CULL, 0x00000001000002C6ULL, 0x0000000100000163ULL, 0x00000001000000B1ULL,
    0x0000000100000059ULL, 0x000000010000002CULL, 0x0000000100000016ULL, 0x000000010000000BULL,
    0x0000000100000006ULL, 0x0000000100000003ULL, 0x0000000100000001ULL, 0x0000000100000001ULL,
};

int64_t mulQ(int64_t a, int64_t b) { return Fixed::mulDiv(a, b, kOne); }

int64_t log2Q(int64_t x) {  // x > 0
    int64_t k = 0;
    while (x >= 2 * kOne) { x >>= 1; ++k; }
    while (x < kOne) { x <<= 1; --k; }
    int64_t frac = 0;
    for (int i = 1; i <= 32; ++i) {
        x = mulQ(x, x);
        if (x >= 2 * kOne) {
            x >>= 1;
            frac |= int64_t(1) << (32 - i);
        }
    }
    return k * kOne + frac;
}

int64_t exp2Q(int64_t y) {
    int64_t ip = y / kOne;
    if (y % kOne != 0 && y < 0) --ip;  // floor
    const int64_t f = y - ip * kOne;
    int64_t r = kOne;
    for (int i = 1; i <= 32; ++i) {
        if (f & (int64_t(1) << (32 - i))) r = mulQ(r, static_cast<int64_t>(kExp2Steps[i - 1]));
    }
    if (ip >= 30) return INT64_MAX / 4;  // saturate; far beyond any rules value
    return ip >= 0 ? r << ip : (ip <= -63 ? 0 : r >> -ip);
}
}  // namespace

Fixed Fixed::pow(Fixed base, Fixed exponent) {
    if (base.raw_ <= 0) return Fixed();
    if (exponent.raw_ == 0) return fromInt(1);
    const int64_t bq = mulDiv(base.raw_, kOne, kScale);
    const int64_t eq = mulDiv(exponent.raw_, kOne, kScale);
    const int64_t rq = exp2Q(mulQ(log2Q(bq), eq));
    int64_t raw = mulDiv(rq, kScale, kOne) + (mulDiv(rq, kScale * 2, kOne) & 1);  // round to nearest
    // log2/exp2 leave ~1e-9 relative error; snap results that are an integer
    // within that error so floors in rules formulas (n^1.5 for square n) stay exact.
    const int64_t nearest = (raw + kScale / 2) / kScale * kScale;
    const int64_t tolerance = 3 + raw / 50000000;  // ~2e-9 relative
    if (raw - nearest <= tolerance && nearest - raw <= tolerance) raw = nearest;
    return fromRaw(raw);
}

std::string Fixed::toString() const {
    int64_t r = raw_;
    std::string s;
    if (r < 0) {
        s += '-';
        r = -r;
    }
    s += std::to_string(r / kScale);
    int64_t frac = r % kScale;
    if (frac != 0) {
        std::string f = std::to_string(frac);
        while (f.size() < 4) f.insert(f.begin(), '0');
        while (!f.empty() && f.back() == '0') f.pop_back();
        s += '.' + f;
    }
    return s;
}

}  // namespace sov
