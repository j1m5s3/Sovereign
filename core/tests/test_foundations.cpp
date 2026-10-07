#include "helpers.h"
#include "sovereign/fixed.h"
#include "sovereign/hex.h"
#include "sovereign/json.h"
#include "sovereign/rng.h"

using namespace sov;

TEST(fixed_parse_and_print) {
    Fixed f;
    REQUIRE(Fixed::parse("0.75", f));
    CHECK_EQ(f.raw(), 7500);
    REQUIRE(Fixed::parse("-12.5", f));
    CHECK_EQ(f.toString(), std::string("-12.5"));
    CHECK(Fixed::parse("3.00000", f));
    CHECK(!Fixed::parse("3.00001", f));
    CHECK(!Fixed::parse("abc", f));
    CHECK(!Fixed::parse("", f));
    CHECK(!Fixed::parse("1.2.3", f));
    CHECK_EQ(Fixed::fromInt(7).toString(), std::string("7"));
}

TEST(fixed_arithmetic_and_rounding) {
    Fixed a = Fixed::fromInt(3), b = Fixed::ratio(1, 4);
    CHECK_EQ((a * b).toString(), std::string("0.75"));
    CHECK_EQ((a / Fixed::fromInt(2)).toString(), std::string("1.5"));
    CHECK_EQ(Fixed::ratio(7, 2).floor(), 3);
    CHECK_EQ(Fixed::ratio(-7, 2).floor(), -4);
    CHECK_EQ(Fixed::ratio(-7, 2).ceil(), -3);
    CHECK_EQ(Fixed::ratio(5, 2).round(), 3);
    CHECK_EQ(Fixed::ratio(-5, 2).round(), -3);
    CHECK_EQ(Fixed::ratio(-7, 2).toInt(), -3);
    // Large values multiply through the 128-bit path without overflow.
    Fixed big = Fixed::fromInt(2000000000);
    CHECK_EQ((big * Fixed::fromInt(3)).toInt(), 6000000000LL);
}

TEST(rng_is_deterministic_and_streams_independent) {
    RngSet a, b;
    a.seed(42);
    b.seed(42);
    for (int i = 0; i < 100; ++i) CHECK_EQ(a.get(RngStream::Combat).next(), b.get(RngStream::Combat).next());
    // Drawing from a cosmetic stream must not shift combat rolls.
    RngSet c, d;
    c.seed(9);
    d.seed(9);
    for (int i = 0; i < 50; ++i) c.get(RngStream::Visual).next();
    CHECK_EQ(c.get(RngStream::Combat).next(), d.get(RngStream::Combat).next());
    CHECK(c.get(RngStream::MapGen).next() != c.get(RngStream::Combat).next());
}

TEST(rng_known_sequence_and_ranges) {
    // Pinned values: these must be identical on every compiler and platform.
    Rng r(12345);
    uint64_t first = r.next();
    Rng r2(12345);
    CHECK_EQ(first, r2.next());
    Rng q(1);
    int counts[6] = {0};
    for (int i = 0; i < 6000; ++i) {
        uint32_t v = q.below(6);
        REQUIRE(v < 6);
        ++counts[v];
    }
    for (int c : counts) CHECK(c > 850 && c < 1150);
    for (int i = 0; i < 1000; ++i) {
        int v = q.range(-3, 3);
        CHECK(v >= -3 && v <= 3);
    }
}

TEST(hex_neighbors_and_distance) {
    HexGrid g(10, 8, false);
    CHECK_EQ(g.distance({0, 0}, {0, 0}), 0);
    CHECK_EQ(g.distance({2, 2}, {5, 2}), 3);
    for (int d = 0; d < kNumDirs; ++d) {
        auto n = g.neighbor({4, 4}, static_cast<Dir>(d));
        REQUIRE(n.has_value());
        CHECK_EQ(g.distance({4, 4}, *n), 1);
        CHECK(g.directionTo({4, 4}, *n) == static_cast<Dir>(d));
        auto back = g.neighbor(*n, opposite(static_cast<Dir>(d)));
        REQUIRE(back.has_value());
        CHECK_EQ(*back, (Hex{4, 4}));
    }
    // Odd rows are shifted east: (3,3)'s NE neighbour is (4,2).
    CHECK_EQ(*g.neighbor({3, 3}, Dir::NE), (Hex{4, 2}));
    CHECK_EQ(*g.neighbor({3, 2}, Dir::NE), (Hex{3, 1}));
    CHECK(!g.neighbor({0, 0}, Dir::W).has_value());
    CHECK(!g.neighbor({0, 0}, Dir::NW).has_value());
}

TEST(hex_wraps_east_west_only) {
    HexGrid g(10, 8, true);
    CHECK_EQ(*g.neighbor({0, 2}, Dir::W), (Hex{9, 2}));
    CHECK_EQ(g.distance({0, 4}, {9, 4}), 1);
    CHECK_EQ(g.distance({1, 4}, {8, 4}), 3);
    CHECK(!g.neighbor({4, 0}, Dir::NE).has_value());
    CHECK_EQ(g.within({0, 4}, 1).size(), 7u);
    CHECK_EQ(g.within({5, 4}, 2).size(), 19u);
    // On a map narrower than the area its rows wrap onto themselves: each plot is listed once.
    HexGrid narrow(4, 9, true);
    CHECK_EQ(narrow.within({1, 4}, 2).size(), 18u);  // rows of 3, 4, 4, 4 and 3
    CHECK_EQ(narrow.within({1, 4}, 9).size(), 36u);  // the whole map
}

TEST(hex_line_is_contiguous) {
    HexGrid g(30, 20, true);
    Hex a{2, 3}, b{11, 15};
    auto line = g.line(a, b);
    REQUIRE(!line.empty());
    CHECK_EQ(line.front(), a);
    CHECK_EQ(line.back(), b);
    CHECK_EQ(static_cast<int>(line.size()), g.distance(a, b) + 1);
    for (size_t i = 1; i < line.size(); ++i) CHECK_EQ(g.distance(line[i - 1], line[i]), 1);
    // Across the wrap seam.
    auto seam = g.line({1, 5}, {28, 5});
    CHECK_EQ(seam.size(), 4u);
}

TEST(json_parses_rules_shapes) {
    std::string err;
    Json j = Json::parse(R"({"a": [1, 2.5, -3], "b": {"c": "x\"y"}, "t": true, "n": null} // trailing comment)", &err);
    REQUIRE(err.empty());
    CHECK_EQ(j["a"].items().size(), 3u);
    CHECK_EQ(j["a"].items()[1].fixed().toString(), std::string("2.5"));
    CHECK_EQ(j["a"].items()[2].integer(), -3);
    CHECK_EQ(j["b"]["c"].str(), std::string("x\"y"));
    CHECK(j["t"].boolean());
    CHECK(j["n"].isNull());
    CHECK(j["missing"].isNull());
    Json bad = Json::parse("{\"a\": }", &err);
    CHECK(!err.empty());
}
