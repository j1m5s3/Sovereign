// The live battle simulation (leader doc §9): a real-time melee between two Civ units,
// one leader fighting in person. Plain C++17 with no Unreal and no rules core, so the
// trainer and tests run it headless and Unreal draws it. A live battle is an input to the
// core, never part of its deterministic simulation, so floats are fine here; the
// simulation still has its own seeded RNG so training and tests reproduce.
//
// Civ math stays the backbone: soldiers follow their unit's HP, every blow follows the Civ
// strength difference (the core's e^(0.04 x diff) shape), and the core clamps the result to
// its band. Tactics add to it: each unit's soldiers form three squads that a commander
// orders (battle/include/sovereign_battle/commander.h); blows from the flank or rear hit
// harder, braced soldiers are harder to hit, and soldiers near their leader fight better.
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "sovereign/api.h"

namespace sov::battle {

struct Vec2 {
    float x = 0.f, y = 0.f;
    Vec2() = default;
    Vec2(float ax, float ay) : x(ax), y(ay) {}
    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
    float length() const { return std::sqrt(x * x + y * y); }
    float lengthSq() const { return x * x + y * y; }
    Vec2 normalized() const { const float l = length(); return l > 1e-4f ? Vec2(x / l, y / l) : Vec2(); }
    Vec2 left() const { return {-y, x}; }  // a quarter turn toward +y from +x
};
inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float dist(Vec2 a, Vec2 b) { return (a - b).length(); }

// xorshift64*: small, fast and the same on every compiler.
class Rng {
public:
    explicit Rng(uint64_t seed = 1) { reseed(seed); }
    void reseed(uint64_t seed) { s_ = seed * 0x9E3779B97F4A7C15ull + 0x2545F4914F6CDD1Dull; if (!s_) s_ = 1; next(); }
    uint64_t next() { s_ ^= s_ >> 12; s_ ^= s_ << 25; s_ ^= s_ >> 27; return s_ * 0x2545F4914F6CDD1Dull; }
    float unit() { return static_cast<float>(next() >> 40) / 16777216.f; }  // [0, 1)
    float range(float lo, float hi) { return lo + (hi - lo) * unit(); }
    int below(int n) { return n <= 0 ? 0 : static_cast<int>(next() % static_cast<uint64_t>(n)); }
    float normal();  // standard normal (Box-Muller)

private:
    uint64_t s_ = 1;
};

constexpr int kSquads = 3;  // left, centre, right, from the side's own point of view
enum class Order : uint8_t { Advance, Hold, FlankLeft, FlankRight, FallBack, HuntLeader };
constexpr int kOrders = 6;
SOV_API const char* orderName(Order o);

struct UnitSpec {
    std::string name;
    int owner = 0;
    int strength = 20;          // Civ combat strength in this fight (attacking or defending)
    int hp = 100;
    bool leaderIsUnit = false;  // an unescorted leader: the unit is the leader alone
};

struct Spec {
    UnitSpec attacker, defender;
    int leaderSide = 0;         // side with an escorted leader riding with its men (-1: none)
    int humanSide = 0;          // side whose leader a human moves by hand (-1: nobody)
    int leaderStrength = 20;
    int leaderHp = 100;
    uint32_t seed = 1;
    float timeLimit = 180.f;    // LIVE_BATTLE_SECONDS: settles from the current state when reached
};

struct Soldier {
    Vec2 pos, facing;
    float hp = 10.f, maxHp = 10.f;
    float cooldown = 0.f;
    Vec2 anchor;                // where a holding soldier stands its ground
    int side = 0;               // 0 attacker, 1 defender
    int squad = -1;             // -1: an escorted leader
    bool leader = false;
    bool alive = true;
};

struct Result {
    int toAttacker = 0;   // HP damage to the attacking unit
    int toDefender = 0;   // HP damage to the defending unit
    int leaderWound = 0;  // HP the escorted leader lost fighting in person
    bool timedOut = false;
    int winner = -1;      // side that held the field (-1: neither)
};

// What the human does with the leader this frame.
struct LeaderInput {
    Vec2 move;            // unit vector or zero
    bool strike = false;
};

class SOV_API Sim {
public:
    static constexpr float kReach = 140.f;
    static constexpr float kSpeed = 260.f;
    static constexpr float kSwingSeconds = 1.6f;
    static constexpr float kFieldX = 4300.f;
    static constexpr float kFieldY = 2800.f;
    static constexpr int kFlankBonus = 6;                  // Civ flanking is +2 per extra unit; men are not units
    static constexpr int kRearBonus = 10;
    static constexpr int kBracedBonus = 5;                 // a holding line, as fortifying
    static constexpr float kHoldZone = 450.f;              // holders step out to meet enemies this close
    static constexpr int kAuraBonus = 3;                   // near the side's own leader
    static constexpr float kAuraRange = 600.f;

    void start(const Spec& spec);
    // Advances the fight; the human's leader follows `human` when given.
    void step(float dt, const LeaderInput* human = nullptr);
    bool finished() const { return finished_; }
    Result result() const;

    void setOrder(int side, int squad, Order o);
    void setAllOrders(int side, Order o);
    Order order(int side, int squad) const { return orders_[side][squad]; }

    const Spec& spec() const { return spec_; }
    const std::vector<Soldier>& soldiers() const { return men_; }
    int alive(int side) const;                 // soldiers, not counting an escorted leader
    int started(int side) const { return initial_[side]; }
    int squadAlive(int side, int squad) const;
    float elapsed() const { return elapsed_; }
    float timeLeft() const { return spec_.timeLimit > elapsed_ ? spec_.timeLimit - elapsed_ : 0.f; }
    // The leader soldier of a side (escorted or the lone unit), or -1.
    int leaderOf(int side) const { return leader_[side]; }
    // The leader the human moves, or -1.
    int humanLeader() const { return spec_.humanSide >= 0 ? leader_[spec_.humanSide] : -1; }

    // The commander's view of the battle for one side, in that side's own frame (it always
    // faces +x and its left is +y), so one policy plays either side.
    static constexpr int kObservation = 49;
    void observe(int side, float* out) const;

private:
    int nearestEnemy(int from) const;
    void swing(int from, int to);
    int strengthOf(int i) const;
    int bonusAgainst(int attacker, int defender) const;
    Vec2 centroid(int side, int squad = -1) const;  // squad -1: the whole side
    Vec2 forward(int side) const;                   // from this side toward the enemy
    void moveToward(Soldier& s, Vec2 target, float speed, float dt);

    Spec spec_;
    std::vector<Soldier> men_;
    Rng rng_;
    std::array<std::array<Order, kSquads>, 2> orders_{};
    std::array<std::array<bool, kSquads>, 2> flanked_{};  // a flanking squad reached its waypoint
    std::array<std::array<Vec2, kSquads>, 2> lastCentroid_{}, velocity_{};  // how each squad is moving
    std::array<std::array<int, kSquads>, 2> squadInitial_{};
    int initial_[2] = {0, 0};
    int leader_[2] = {-1, -1};
    bool leaderIsUnit_[2] = {false, false};
    float elapsed_ = 0.f;
    bool finished_ = false, timedOut_ = false;
};

}  // namespace sov::battle
