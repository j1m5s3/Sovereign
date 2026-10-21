// What the battle scene shows of a fight (plan G step 5): who strikes, who is hit and who falls, read from the men as a
// frame shows them and never fed back, so it cannot change a battle. The machine that runs the battle sees every swing
// (each man's cooldown); a replay or the other player's view sees only positions, hit points and who stands, and fills
// the swings in. Unreal's battle scene plays what it says on the men's rigs (SovBattleScene).
#pragma once

#include <cstdint>
#include <vector>

#include "sovereign/api.h"
#include "sovereign_battle/sim.h"

namespace sov::battle {

// One man as a frame shows him (indexed as the sim's men, which never reorder).
struct Seen {
    Vec2 pos;
    float hp = 0.f;
    float cooldown = -1.f;  // the sim's swing cooldown; below 0 when the frame does not carry it (a snapshot)
    int side = 0;
    bool alive = true;
};

enum class CueKind : uint8_t {
    Strike,  // he strikes (his blow lands, or misses, impact seconds in)
    Hit,     // he is hit and flinches
    Fall,    // he is killed and falls
    Lie,     // he is dead when first seen: he lies already, with no fall
};

struct Cue {
    int man = -1;
    CueKind kind = CueKind::Strike;
    float delay = 0.f;      // seconds from now (a hit or fall waits for the blow that lands it)
    int other = -1;         // a strike: at whom; a hit or fall: by whom (-1 not known)
    float weight = 1.f;     // a hit: how hard, 0.5 to 1
    bool backward = false;  // a fall or a lie: onto his back
};

// The clips' timing (tools/art/README.md, One-shot actions; SovGait.h) and how near a fight is.
struct ShowTiming {
    float impact = 10.f / 24.f;          // a strike's blow lands this long after it starts
    float strike = 1.f;                  // and it is over this long after
    float engage = Sim::kReach + 35.f;   // an enemy this near is being fought
    float cadence = Sim::kSwingSeconds;  // an engaged man swings this often
};

class SOV_API Show {
public:
    explicit Show(const ShowTiming& t = ShowTiming());

    // The men as this frame shows them, at `now` seconds on a clock that only runs forward; appends what to show.
    // Where the frame carries the cooldowns (the machine running the battle), a strike is a man whose cooldown rose,
    // and a hit or fall is credited only to a man who struck in that frame. Where it does not (a snapshot), a hit or
    // fall is credited to the strike already on its way to him, else to an enemy in reach, who strikes then if he is
    // free; and an engaged man who has not struck for a cadence strikes on his own, which stands in for a miss.
    void update(const std::vector<Seen>& men, double now, std::vector<Cue>& out);

    // Whom he fights: the nearest living enemy within engage at the last update, else -1.
    int target(int man) const;

private:
    // One man as the last frame showed him, and his strike.
    struct Man {
        float hp = 0.f;
        float cooldown = -1.f;
        bool alive = true;
        bool seen = false;
        bool engaged = false;
        int target = -1;           // whom he fought
        double struck = -1e9;      // when his last strike started
        double impactAt = -1e9;    // when its blow lands
        int at = -1;               // whom it is at
    };
    void strike(int man, int at, double now);
    bool fallsBackward(const std::vector<Seen>& men, int man, int by) const;

    ShowTiming t_;
    std::vector<Man> mem_;
    std::vector<int> target_;     // per man this frame
    std::vector<uint8_t> fresh_;  // per man: first seen (or seen alive again) this frame
};

}  // namespace sov::battle
