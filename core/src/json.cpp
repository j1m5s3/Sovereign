#include "sovereign/json.h"

namespace sov {

class JsonParser {
public:
    explicit JsonParser(std::string_view t) : t_(t) {}

    bool parseDocument(Json& out, std::string* error) {
        skipWs();
        if (!parseValue(out, 0)) {
            if (error) *error = error_ + " at offset " + std::to_string(pos_);
            return false;
        }
        skipWs();
        if (pos_ != t_.size()) {
            if (error) *error = "trailing characters at offset " + std::to_string(pos_);
            return false;
        }
        return true;
    }

private:
    bool fail(const char* msg) {
        error_ = msg;
        return false;
    }

    void skipWs() {
        while (pos_ < t_.size()) {
            char c = t_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++pos_;
            } else if (c == '/' && pos_ + 1 < t_.size() && t_[pos_ + 1] == '/') {
                // Line comments are allowed so data files can cite their source.
                while (pos_ < t_.size() && t_[pos_] != '\n') ++pos_;
            } else {
                break;
            }
        }
    }

    bool literal(std::string_view word) {
        if (t_.substr(pos_, word.size()) != word) return false;
        pos_ += word.size();
        return true;
    }

    bool parseValue(Json& out, int depth) {
        if (depth > 64) return fail("nesting too deep");
        if (pos_ >= t_.size()) return fail("unexpected end");
        char c = t_[pos_];
        if (c == '{') return parseObject(out, depth);
        if (c == '[') return parseArray(out, depth);
        if (c == '"') {
            out.type_ = Json::Type::String;
            return parseString(out.text_);
        }
        if (c == 't' && literal("true")) {
            out.type_ = Json::Type::Bool;
            out.bool_ = true;
            return true;
        }
        if (c == 'f' && literal("false")) {
            out.type_ = Json::Type::Bool;
            out.bool_ = false;
            return true;
        }
        if (c == 'n' && literal("null")) {
            out.type_ = Json::Type::Null;
            return true;
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            size_t start = pos_;
            ++pos_;
            while (pos_ < t_.size()) {
                char d = t_[pos_];
                if ((d >= '0' && d <= '9') || d == '.' || d == 'e' || d == 'E' || d == '+' || d == '-') {
                    ++pos_;
                } else {
                    break;
                }
            }
            out.type_ = Json::Type::Number;
            out.text_ = std::string(t_.substr(start, pos_ - start));
            return true;
        }
        return fail("unexpected character");
    }

    bool parseString(std::string& out) {
        ++pos_;  // opening quote
        while (pos_ < t_.size()) {
            char c = t_[pos_++];
            if (c == '"') return true;
            if (c == '\\') {
                if (pos_ >= t_.size()) return fail("bad escape");
                char e = t_[pos_++];
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        if (pos_ + 4 > t_.size()) return fail("bad unicode escape");
                        uint32_t cp = 0;
                        for (int i = 0; i < 4; ++i) {
                            char h = t_[pos_++];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= h - '0';
                            else if (h >= 'a' && h <= 'f') cp |= h - 'a' + 10;
                            else if (h >= 'A' && h <= 'F') cp |= h - 'A' + 10;
                            else return fail("bad unicode escape");
                        }
                        // UTF-8 encode (BMP only; data files are plain text).
                        if (cp < 0x80) {
                            out += static_cast<char>(cp);
                        } else if (cp < 0x800) {
                            out += static_cast<char>(0xC0 | (cp >> 6));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        } else {
                            out += static_cast<char>(0xE0 | (cp >> 12));
                            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        }
                        break;
                    }
                    default: return fail("bad escape");
                }
            } else {
                out += c;
            }
        }
        return fail("unterminated string");
    }

    bool parseArray(Json& out, int depth) {
        out.type_ = Json::Type::Array;
        ++pos_;
        skipWs();
        if (pos_ < t_.size() && t_[pos_] == ']') {
            ++pos_;
            return true;
        }
        for (;;) {
            skipWs();
            Json v;
            if (!parseValue(v, depth + 1)) return false;
            out.array_.push_back(std::move(v));
            skipWs();
            if (pos_ >= t_.size()) return fail("unterminated array");
            if (t_[pos_] == ',') { ++pos_; continue; }
            if (t_[pos_] == ']') { ++pos_; return true; }
            return fail("expected , or ]");
        }
    }

    bool parseObject(Json& out, int depth) {
        out.type_ = Json::Type::Object;
        ++pos_;
        skipWs();
        if (pos_ < t_.size() && t_[pos_] == '}') {
            ++pos_;
            return true;
        }
        for (;;) {
            skipWs();
            if (pos_ >= t_.size() || t_[pos_] != '"') return fail("expected key");
            std::string key;
            if (!parseString(key)) return false;
            skipWs();
            if (pos_ >= t_.size() || t_[pos_] != ':') return fail("expected :");
            ++pos_;
            skipWs();
            Json v;
            if (!parseValue(v, depth + 1)) return false;
            out.object_.emplace_back(std::move(key), std::move(v));
            skipWs();
            if (pos_ >= t_.size()) return fail("unterminated object");
            if (t_[pos_] == ',') { ++pos_; continue; }
            if (t_[pos_] == '}') { ++pos_; return true; }
            return fail("expected , or }");
        }
    }

    std::string_view t_;
    size_t pos_ = 0;
    std::string error_;
};

Json Json::parse(std::string_view text, std::string* error) {
    Json out;
    JsonParser p(text);
    if (!p.parseDocument(out, error)) return Json();
    if (error) error->clear();
    return out;
}

const std::string& Json::emptyString() {
    static const std::string s;
    return s;
}

const Json& Json::operator[](std::string_view key) const {
    static const Json null;
    if (type_ != Type::Object) return null;
    for (const auto& kv : object_) {
        if (kv.first == key) return kv.second;
    }
    return null;
}

Json Json::overlay(const Json& base, const Json& over) {
    if (!base.isObject() || !over.isObject()) return over;
    Json out = base;
    for (const auto& [key, value] : over.object_) {
        bool replaced = false;
        for (auto& [k, v] : out.object_) {
            if (k == key) {
                v = value;
                replaced = true;
            }
        }
        if (!replaced) out.object_.emplace_back(key, value);
    }
    return out;
}

bool Json::has(std::string_view key) const {
    if (type_ != Type::Object) return false;
    for (const auto& kv : object_) {
        if (kv.first == key) return true;
    }
    return false;
}

const std::string& Json::str(const std::string& def) const {
    return type_ == Type::String ? text_ : def;
}

int64_t Json::integer(int64_t def) const {
    if (type_ != Type::Number) return def;
    Fixed f;
    if (!Fixed::parse(text_, f)) return def;
    return f.toInt();
}

Fixed Json::fixed(Fixed def) const {
    if (type_ != Type::Number) return def;
    Fixed f;
    return Fixed::parse(text_, f) ? f : def;
}

bool Json::boolean(bool def) const {
    return type_ == Type::Bool ? bool_ : def;
}

}  // namespace sov
