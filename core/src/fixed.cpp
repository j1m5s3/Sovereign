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
