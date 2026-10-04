// Minimal JSON reader for rules data. Numbers stay as text and are converted
// to integers or Fixed without ever passing through a float. Objects keep
// their key order so loading is deterministic.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sovereign/fixed.h"

namespace sov {

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() = default;
    static Json parse(std::string_view text, std::string* error);

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isObject() const { return type_ == Type::Object; }
    bool isArray() const { return type_ == Type::Array; }
    bool isString() const { return type_ == Type::String; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isBool() const { return type_ == Type::Bool; }

    // Object access; returns a shared null value when missing.
    const Json& operator[](std::string_view key) const;
    bool has(std::string_view key) const;
    const std::vector<std::pair<std::string, Json>>& members() const { return object_; }

    // Array access.
    const std::vector<Json>& items() const { return array_; }
    size_t size() const { return type_ == Type::Array ? array_.size() : object_.size(); }

    // Typed reads with defaults when the value is missing or of another type.
    const std::string& str(const std::string& def = emptyString()) const;
    int64_t integer(int64_t def = 0) const;
    Fixed fixed(Fixed def = Fixed()) const;
    bool boolean(bool def = false) const;

private:
    friend class JsonParser;
    static const std::string& emptyString();

    Type type_ = Type::Null;
    bool bool_ = false;
    std::string text_;  // string value, or the number's literal text
    std::vector<Json> array_;
    std::vector<std::pair<std::string, Json>> object_;
};

}  // namespace sov
