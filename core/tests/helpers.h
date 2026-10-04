#pragma once

#include <cstdio>
#include <cstdlib>
#include <ostream>
#include <string>

#include "sovereign/game.h"
#include "sovereign/rules.h"
#include "test.h"

namespace sovtest {

// Real rules from data/rules, loaded once.
inline const sov::Rules& rules() {
    static sov::Rules r = [] {
        sov::Rules x;
        std::string err;
        if (!x.load({SOVEREIGN_RULES_DIR}, &err)) {
            std::printf("cannot load rules: %s\n", err.c_str());
            std::abort();
        }
        return x;
    }();
    return r;
}

// A hand-made all-grassland map with `players` players and no units.
inline sov::GameState flatState(int w, int h, int players, bool wrap = false) {
    using namespace sov;
    const Rules& r = rules();
    GameState s;
    s.setup.mapSize = "MAPSIZE_DUEL";
    s.setup.wrapX = wrap;
    s.grid = HexGrid(w, h, wrap);
    s.plots.assign(static_cast<size_t>(s.grid.size()), Plot{});
    for (Plot& p : s.plots) p.terrain = r.terrain("TERRAIN_GRASS");
    s.rng.seed(7);
    for (int i = 0; i < players; ++i) {
        Player p;
        p.id = static_cast<PlayerId>(i);
        p.civ = static_cast<TypeIndex>(i);
        s.setup.players.push_back({r.civs[static_cast<size_t>(i)].id, true});
        s.players.push_back(p);
    }
    return s;
}

inline sov::UnitId addUnit(sov::GameState& s, const char* type, sov::PlayerId owner, sov::Hex pos) {
    sov::Unit u;
    u.id = s.nextUnitId++;
    u.type = rules().unit(type);
    u.owner = owner;
    u.pos = pos;
    u.movesLeft = sov::Fixed::fromInt(rules().units[static_cast<size_t>(u.type)].moves);
    s.units.push_back(u);
    return u.id;
}

// One player on flat grassland with a capital founded at (6,6).
struct CityScenario {
    std::unique_ptr<sov::Game> game;
    sov::CityId city = sov::kNoCity;
};

inline CityScenario capitalScenario(sov::GameState s = flatState(20, 14, 1)) {
    using namespace sov;
    UnitId settler = addUnit(s, "UNIT_SETTLER", 0, {6, 6});
    CityScenario sc;
    sc.game = Game::fromScenario(rules(), std::move(s));
    if (sc.game->submit(Command::foundCity(0, settler)) == CommandError::Ok) sc.city = sc.game->state().cities[0].id;
    return sc;
}

// Ends `n` turns, keeping research going with the first available choice.
inline void endTurns(sov::Game& g, int n) {
    using namespace sov;
    for (int i = 0; i < n; ++i) {
        const PlayerId me = g.state().currentPlayer;
        CommandError e = g.submit(Command::endTurn(me));
        for (int k = 0; k < 2 && (e == CommandError::ResearchNeeded || e == CommandError::CivicNeeded); ++k) {
            if (e == CommandError::ResearchNeeded) g.submit(Command::chooseResearch(me, g.availableTechs(me).front()));
            else g.submit(Command::chooseCivic(me, g.availableCivics(me).front()));
            e = g.submit(Command::endTurn(me));
        }
        if (e != CommandError::Ok) {
            std::printf("  endTurn failed: %s\n", commandErrorName(e));
            return;
        }
    }
}

inline sov::GameSetup duelSetup(uint64_t seed) {
    sov::GameSetup g;
    g.seed = seed;
    g.mapSize = "MAPSIZE_DUEL";
    g.players = {{"CIVILIZATION_ROME", true}, {"CIVILIZATION_EGYPT", false}};
    return g;
}

}  // namespace sovtest

namespace sov {  // printers for CHECK_EQ, found by argument-dependent lookup
inline std::ostream& operator<<(std::ostream& os, const Fixed& f) { return os << f.toString(); }
inline std::ostream& operator<<(std::ostream& os, const Hex& h) { return os << "(" << h.x << "," << h.y << ")"; }
inline std::ostream& operator<<(std::ostream& os, CommandError e) { return os << commandErrorName(e); }
}  // namespace sov
