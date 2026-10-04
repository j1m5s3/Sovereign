// Tiny dependency-free test harness: TEST(name) { CHECK(...); REQUIRE(...); }
#pragma once

#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace sovtest {

struct Case {
    const char* name;
    std::function<void()> fn;
};
std::vector<Case>& registry();
void fail(const char* file, int line, const std::string& msg, bool fatal);

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};
struct Fatal {};

template <typename A, typename B>
std::string showPair(const A& a, const B& b) {
    std::ostringstream ss;
    ss << a << " vs " << b;
    return ss.str();
}

}  // namespace sovtest

#define SOV_CAT2(a, b) a##b
#define SOV_CAT(a, b) SOV_CAT2(a, b)
#define TEST(name)                                                                  \
    static void SOV_CAT(test_, name)();                                             \
    static sovtest::Registrar SOV_CAT(reg_, name)(#name, &SOV_CAT(test_, name));    \
    static void SOV_CAT(test_, name)()

#define CHECK(cond) \
    do { if (!(cond)) sovtest::fail(__FILE__, __LINE__, #cond, false); } while (0)
#define REQUIRE(cond) \
    do { if (!(cond)) sovtest::fail(__FILE__, __LINE__, #cond, true); } while (0)
#define CHECK_EQ(a, b)                                                                          \
    do {                                                                                        \
        const auto sov_a = (a);                                                                  \
        const auto sov_b = (b);                                                                  \
        if (!(sov_a == sov_b))                                                                  \
            sovtest::fail(__FILE__, __LINE__, std::string(#a " == " #b " (") +                  \
                                                  sovtest::showPair(sov_a, sov_b) + ")", false); \
    } while (0)
