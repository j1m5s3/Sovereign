#include <cstdio>
#include <cstring>

#include "test.h"

namespace sovtest {

static int gFailures = 0;

std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

void fail(const char* file, int line, const std::string& msg, bool fatal) {
    ++gFailures;
    std::printf("  FAIL %s:%d: %s\n", file, line, msg.c_str());
    if (fatal) throw Fatal{};
}

}  // namespace sovtest

// Usage: sovereign_tests [substring]  runs the tests whose name contains it.
int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0, failedCases = 0;
    for (const auto& c : sovtest::registry()) {
        if (filter && !std::strstr(c.name, filter)) continue;
        ++run;
        int before = sovtest::gFailures;
        try {
            c.fn();
        } catch (const sovtest::Fatal&) {
        } catch (const std::exception& e) {
            sovtest::fail(__FILE__, __LINE__, std::string("exception: ") + e.what(), false);
        }
        bool ok = sovtest::gFailures == before;
        if (!ok) ++failedCases;
        std::printf("%s %s\n", ok ? "[ ok ]" : "[FAIL]", c.name);
    }
    std::printf("%d tests, %d failed\n", run, failedCases);
    return failedCases == 0 && run > 0 ? 0 : 1;
}
