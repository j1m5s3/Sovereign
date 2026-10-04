#include "sovereign/game.h"

#include <algorithm>
#include <queue>

#include "sovereign/mapgen.h"
#include "sovereign/serialize.h"

namespace sov {

namespace {
const TerrainType& terrainOf(const Rules& r, const Plot& p) { return r.terrains[static_cast<size_t>(p.terrain)]; }
const UnitType& typeOf(const Rules& r, const Unit& u) { return r.units[static_cast<size_t>(u.type)]; }
}  // namespace

std::unique_ptr<Game> Game::create(const Rules& rules, const GameSetup& setup, std::string* error) {
    std::string localError;
    if (!error) error = &localError;
    GameState s;
    s.setup = setup;
    TypeIndex size = rules.mapSize(setup.mapSize);
    if (size == kNone) {
        *error = "unknown map size " + setup.mapSize;
        return nullptr;
    }
    if (rules.speed(setup.speed) == kNone) {
        *error = "unknown game speed " + setup.speed;
        return nullptr;
    }
    if (setup.players.empty() || setup.players.size() > 64) {
        *error = "a game needs 1 to 64 players";
        return nullptr;
    }
    const MapSizeType& ms = rules.mapSizes[static_cast<size_t>(size)];
    s.grid = HexGrid(ms.width, ms.height, setup.wrapX);
    s.rng.seed(setup.seed);
    for (size_t i = 0; i < setup.players.size(); ++i) {
        Player p;
        p.id = static_cast<PlayerId>(i);
        p.civ = rules.civ(setup.players[i].civ);
        if (p.civ == kNone) {
            *error = "unknown civilization " + setup.players[i].civ;
            return nullptr;
        }
        p.human = setup.players[i].human;
        fitPlayerToRules(p, rules);
        p.visibility.assign(static_cast<size_t>(s.grid.size()), 0);
        s.players.push_back(std::move(p));
    }
    generateMap(s, rules);
    if (!chooseStartPositions(s, rules, error)) return nullptr;

    auto game = std::make_unique<Game>(rules, std::move(s), std::vector<Command>{});
    GameState& st = game->state_;
    for (Player& p : st.players) {
        for (const std::string& unitId : rules.startingUnits) {
            TypeIndex t = rules.unit(unitId);
            UnitLayer layer = rules.units[static_cast<size_t>(t)].layer;
            std::optional<Hex> spot;
            if (!st.unitAt(p.startPos, layer, rules)) spot = p.startPos;
            for (const Hex& n : st.grid.within(p.startPos, 2)) {
                if (spot) break;
                if (isLandPassable(st, rules, n) && !st.unitAt(n, layer, rules) && !st.foreignUnitAt(n, p.id)) spot = n;
            }
            if (!spot) {
                *error = "no room for starting unit " + unitId;
                return nullptr;
            }
            game->spawnUnit(t, p.id, *spot);
        }
    }
    for (const Player& p : st.players) game->refreshVisibility(p.id);
    game->beginPlayerTurn(0);
    return game;
}

std::unique_ptr<Game> Game::replay(const Rules& rules, const GameSetup& setup, const std::vector<Command>& log,
                                   std::string* error) {
    auto game = create(rules, setup, error);
    if (!game) return nullptr;
    for (size_t i = 0; i < log.size(); ++i) {
        CommandError e = game->submit(log[i]);
        if (e != CommandError::Ok) {
            if (error) *error = "command " + std::to_string(i) + " (" + describe(log[i]) + ") failed: " + commandErrorName(e);
            return nullptr;
        }
    }
    return game;
}

std::unique_ptr<Game> Game::fromScenario(const Rules& rules, GameState state) {
    auto game = std::make_unique<Game>(rules, std::move(state), std::vector<Command>{});
    for (Player& p : game->state_.players) {
        fitPlayerToRules(p, rules);
        if (p.visibility.size() != static_cast<size_t>(game->state_.grid.size()))
            p.visibility.assign(static_cast<size_t>(game->state_.grid.size()), 0);
    }
    for (const Player& p : game->state_.players) game->refreshVisibility(p.id);
    game->state_.currentPlayer = 0;
    game->beginPlayerTurn(0, false);
    return game;
}

Game::Game(const Rules& rules, GameState state, std::vector<Command> log)
    : rules_(&rules), state_(std::move(state)), log_(std::move(log)) {}

uint64_t Game::stateHash() const {
    std::vector<uint8_t> bytes = serializeState(state_);
    return fnv1a(bytes.data(), bytes.size());
}

Visibility Game::visibility(PlayerId player, Hex h) const {
    if (player < 0 || static_cast<size_t>(player) >= state_.players.size()) return Visibility::Unrevealed;
    return static_cast<Visibility>(state_.players[static_cast<size_t>(player)].visibility[static_cast<size_t>(state_.grid.index(h))]);
}

// ---------------------------------------------------------------- validation

CommandError Game::validate(const Command& c) const {
    if (c.player < 0 || static_cast<size_t>(c.player) >= state_.players.size()) return CommandError::BadPlayer;
    if (!state_.players[static_cast<size_t>(c.player)].alive) return CommandError::BadPlayer;
    if (c.player != state_.currentPlayer) return CommandError::NotYourTurn;

    if (c.type == CommandType::EndTurn) {
        if (!unitsNeedingOrders(c.player).empty()) return CommandError::UnitsNeedOrders;
        if (!citiesNeedingProduction(c.player).empty()) return CommandError::ProductionNeeded;
        // A player with a city must keep a tech and a civic in progress while any is left.
        const Player& p = state_.players[static_cast<size_t>(c.player)];
        const bool hasCity = std::any_of(state_.cities.begin(), state_.cities.end(),
                                         [&](const City& city) { return city.owner == c.player; });
        if (hasCity && p.techs.current == kNone && !availableTechs(c.player).empty()) return CommandError::ResearchNeeded;
        if (hasCity && p.civics.current == kNone && !availableCivics(c.player).empty()) return CommandError::CivicNeeded;
        return CommandError::Ok;
    }
    switch (c.type) {
        case CommandType::SetProduction:
        case CommandType::QueueProduction:
        case CommandType::Purchase:
        case CommandType::BuyPlot:
        case CommandType::LockPlot:
            return validateCity(c);
        case CommandType::ChooseResearch:
        case CommandType::ChooseCivic:
        case CommandType::ChangeGovernment:
        case CommandType::SetPolicy:
            return validateResearch(c);
        default: break;
    }
    const Unit* u = state_.unit(c.id);
    if (!u) return CommandError::BadUnit;
    if (u->owner != c.player) return CommandError::NotYourUnit;

    switch (c.type) {
        case CommandType::MoveUnit: {
            auto t = state_.grid.normalize(c.target);
            if (!t || *t != c.target || *t == u->pos) return CommandError::BadTarget;
            const Unit* own = state_.unitAt(*t, typeOf(*rules_, *u).layer, *rules_);
            if (own && own->owner == c.player) return CommandError::BadTarget;
            return findPath(c.id, *t) ? CommandError::Ok : CommandError::NoPath;
        }
        case CommandType::FoundCity: {
            if (!typeOf(*rules_, *u).foundCity) return CommandError::NotASettler;
            if (u->movesLeft <= Fixed()) return CommandError::CannotFoundHere;
            CommandError why = CommandError::Ok;
            canFoundCityAt(c.player, u->pos, &why);
            return why;
        }
        case CommandType::BuildImprovement:
        case CommandType::Harvest:
            return validateBuilder(c);
        case CommandType::SetActivity: {
            if (c.arg < 0 || c.arg > static_cast<int32_t>(Activity::Skip)) return CommandError::BadActivity;
            auto a = static_cast<Activity>(c.arg);
            if (a == Activity::Fortify && typeOf(*rules_, *u).layer != UnitLayer::Military) return CommandError::BadActivity;
            return CommandError::Ok;
        }
        default: break;
    }
    return CommandError::BadTarget;
}

bool Game::canFoundCityAt(PlayerId player, Hex at, CommandError* why) const {
    auto set = [&](CommandError e) {
        if (why) *why = e;
        return e == CommandError::Ok;
    };
    if (!isLandPassable(state_, *rules_, at)) return set(CommandError::CannotFoundHere);
    const Plot& p = state_.plot(at);
    if (p.owner != kNoPlayer && p.owner != player) return set(CommandError::CannotFoundHere);
    const int minRange = rules_->globalInt("CITY_MIN_RANGE");
    for (const City& c : state_.cities) {
        if (state_.grid.distance(c.pos, at) <= minRange) return set(CommandError::TooCloseToCity);
    }
    return set(CommandError::Ok);
}

std::vector<UnitId> Game::unitsNeedingOrders(PlayerId player) const {
    std::vector<UnitId> out;
    for (const Unit& u : state_.units) {
        if (u.owner == player && u.activity == Activity::Awake && !u.moveTarget && u.movesLeft > Fixed())
            out.push_back(u.id);
    }
    return out;
}

// ------------------------------------------------------------------ movement

std::optional<Fixed> Game::moveCost(const Unit& unit, Hex from, Hex to) const {
    const UnitType& ut = typeOf(*rules_, unit);
    if (ut.domain != Domain::Land) return std::nullopt;  // naval and air movement arrive later
    if (!isLandPassable(state_, *rules_, to)) return std::nullopt;
    if (state_.foreignUnitAt(to, unit.owner)) return std::nullopt;  // no war yet: foreign units block
    const City* c = state_.cityAt(to);
    if (c && c->owner != unit.owner) return std::nullopt;
    const Plot& p = state_.plot(to);
    int cost = terrainOf(*rules_, p).moveCost;
    if (p.feature != kNone) cost += rules_->features[static_cast<size_t>(p.feature)].moveChange;
    auto d = state_.grid.directionTo(from, to);
    if (!d) return std::nullopt;
    if (hasRiver(state_, from, *d)) cost += rules_->globalInt("MOVEMENT_RIVER_COST");
    return Fixed::fromInt(std::max(cost, 1));
}

std::optional<std::vector<PathStep>> Game::findPath(UnitId id, Hex target) const {
    const Unit* u = state_.unit(id);
    if (!u) return std::nullopt;
    auto t = state_.grid.normalize(target);
    if (!t) return std::nullopt;
    const Player& owner = state_.players[static_cast<size_t>(u->owner)];
    auto known = [&](Hex h) {
        return owner.visibility[static_cast<size_t>(state_.grid.index(h))] != static_cast<uint8_t>(Visibility::Unrevealed);
    };
    if (!known(*t)) return std::nullopt;
    const Fixed maxMoves = Fixed::fromInt(typeOf(*rules_, *u).moves);

    struct Node { int turn = INT32_MAX; Fixed moves; int prev = -1; };
    std::vector<Node> best(static_cast<size_t>(state_.grid.size()));
    auto better = [](int t1, Fixed m1, int t2, Fixed m2) { return t1 != t2 ? t1 < t2 : m1 > m2; };
    struct QItem { int turn; int64_t negMoves; int index; };
    auto cmp = [](const QItem& a, const QItem& b) {
        if (a.turn != b.turn) return a.turn > b.turn;
        if (a.negMoves != b.negMoves) return a.negMoves > b.negMoves;
        return a.index > b.index;
    };
    std::priority_queue<QItem, std::vector<QItem>, decltype(cmp)> open(cmp);
    const int start = state_.grid.index(u->pos);
    best[static_cast<size_t>(start)] = {0, u->movesLeft, -1};
    open.push({0, -u->movesLeft.raw(), start});
    const int goal = state_.grid.index(*t);
    while (!open.empty()) {
        QItem q = open.top();
        open.pop();
        const Node cur = best[static_cast<size_t>(q.index)];
        if (cur.turn != q.turn || -cur.moves.raw() != q.negMoves) continue;  // stale
        if (q.index == goal) break;
        Hex from = state_.grid.at(q.index);
        for (int d = 0; d < kNumDirs; ++d) {
            auto n = state_.grid.neighbor(from, static_cast<Dir>(d));
            if (!n || !known(*n)) continue;
            auto cost = moveCost(*u, from, *n);
            if (!cost) continue;
            int turn = cur.turn;
            Fixed mp = cur.moves;
            if (mp <= Fixed()) {
                ++turn;
                mp = maxMoves;
            }
            if (mp < *cost && mp != maxMoves) {
                ++turn;
                mp = maxMoves;
            }
            mp = mp >= *cost ? mp - *cost : Fixed();
            Node& nb = best[static_cast<size_t>(state_.grid.index(*n))];
            if (better(turn, mp, nb.turn, nb.moves)) {
                nb = {turn, mp, q.index};
                open.push({turn, -mp.raw(), state_.grid.index(*n)});
            }
        }
    }
    if (best[static_cast<size_t>(goal)].turn == INT32_MAX) return std::nullopt;
    std::vector<PathStep> path;
    for (int i = goal; i != -1; i = best[static_cast<size_t>(i)].prev) {
        path.push_back({state_.grid.at(i), best[static_cast<size_t>(i)].turn, best[static_cast<size_t>(i)].moves});
    }
    std::reverse(path.begin(), path.end());
    return path;
}

void Game::advanceUnit(UnitId id) {
    for (int guard = 0; guard < 64; ++guard) {
        Unit* u = state_.unit(id);
        if (!u || !u->moveTarget) return;
        if (u->pos == *u->moveTarget) {
            u->moveTarget.reset();
            return;
        }
        auto path = findPath(id, *u->moveTarget);
        if (!path || path->size() < 2) {
            u->moveTarget.reset();
            return;
        }
        const Hex next = (*path)[1].pos;
        const Fixed cost = *moveCost(*u, u->pos, next);
        const Fixed maxMoves = Fixed::fromInt(typeOf(*rules_, *u).moves);
        if (u->movesLeft <= Fixed()) return;
        if (u->movesLeft < cost && u->movesLeft != maxMoves) return;  // continue next turn
        const Fixed after = u->movesLeft >= cost ? u->movesLeft - cost : Fixed();
        const Unit* own = state_.unitAt(next, typeOf(*rules_, *u).layer, *rules_);
        if (own && own->id != u->id) {
            // Friendly units may be passed through but not shared at the end of a move.
            if (next == *u->moveTarget) {
                u->moveTarget.reset();
                return;
            }
            if (after <= Fixed()) return;
        }
        u->pos = next;
        u->movesLeft = after;
        u->activity = Activity::Awake;
        refreshVisibility(u->owner);
    }
}

// ---------------------------------------------------------------- visibility

void Game::refreshVisibility(PlayerId pid) {
    Player& p = state_.players[static_cast<size_t>(pid)];
    for (uint8_t& v : p.visibility) {
        if (v == static_cast<uint8_t>(Visibility::Visible)) v = static_cast<uint8_t>(Visibility::Revealed);
    }
    auto see = [&](Hex from, int range) {
        const Plot& vp = state_.plot(from);
        const int viewerHeight = terrainOf(*rules_, vp).sightThrough;
        range += terrainOf(*rules_, vp).sightModifier;
        for (const Hex& target : state_.grid.within(from, range)) {
            bool blocked = false;
            std::vector<Hex> line = state_.grid.line(from, target);
            for (size_t i = 1; i + 1 < line.size() && !blocked; ++i) {
                const Plot& op = state_.plot(line[i]);
                int obstacle = terrainOf(*rules_, op).sightThrough;
                if (op.feature != kNone) obstacle += rules_->features[static_cast<size_t>(op.feature)].sightThrough;
                blocked = obstacle > viewerHeight;
            }
            if (!blocked) p.visibility[static_cast<size_t>(state_.grid.index(target))] = static_cast<uint8_t>(Visibility::Visible);
        }
    };
    for (const Unit& u : state_.units) {
        if (u.owner == pid) see(u.pos, typeOf(*rules_, u).sight);
    }
    for (const City& c : state_.cities) {
        if (c.owner == pid) see(c.pos, rules_->globalInt("CITY_SIGHT_RANGE"));
    }
}

// ------------------------------------------------------------------ applying

CommandError Game::submit(const Command& c) {
    CommandError e = validate(c);
    if (e != CommandError::Ok) return e;
    log_.push_back(c);
    apply(c);
    updateBoosts(c.player);
    return CommandError::Ok;
}

void Game::apply(const Command& c) {
    switch (c.type) {
        case CommandType::MoveUnit: applyMove(c); break;
        case CommandType::FoundCity: applyFoundCity(c); break;
        case CommandType::SetActivity: {
            Unit* u = state_.unit(c.id);
            u->activity = static_cast<Activity>(c.arg);
            u->moveTarget.reset();
            break;
        }
        case CommandType::EndTurn: applyEndTurn(c); break;
        case CommandType::SetProduction:
        case CommandType::QueueProduction:
        case CommandType::Purchase:
        case CommandType::BuyPlot:
        case CommandType::LockPlot: applyCity(c); break;
        case CommandType::ChooseResearch:
        case CommandType::ChooseCivic:
        case CommandType::ChangeGovernment:
        case CommandType::SetPolicy: applyResearch(c); break;
        case CommandType::BuildImprovement:
        case CommandType::Harvest: applyBuilder(c); break;
    }
}

void Game::applyMove(const Command& c) {
    Unit* u = state_.unit(c.id);
    u->moveTarget = c.target;
    u->activity = Activity::Awake;
    advanceUnit(c.id);
}

void Game::applyFoundCity(const Command& c) {
    const Unit* u = state_.unit(c.id);
    const Hex at = u->pos;
    const PlayerId owner = u->owner;
    Player& p = state_.players[static_cast<size_t>(owner)];
    const CivType& civ = rules_->civs[static_cast<size_t>(p.civ)];

    City city;
    city.id = state_.nextCityId++;
    city.owner = owner;
    city.pos = at;
    city.foundedTurn = state_.turn;
    city.capital = p.citiesFounded == 0;
    city.name = static_cast<size_t>(p.citiesFounded) < civ.cityNames.size()
                    ? civ.cityNames[static_cast<size_t>(p.citiesFounded)]
                    : civ.name + " " + std::to_string(p.citiesFounded + 1);
    ++p.citiesFounded;
    for (const Hex& h : state_.grid.within(at, 1)) {
        Plot& plot = state_.plot(h);
        if (plot.owner == kNoPlayer || h == at) {
            plot.owner = owner;
            plot.city = city.id;
        }
    }
    // Founding clears removable features (woods, rainforest, marsh) from the center.
    Plot& center = state_.plot(at);
    if (center.feature != kNone && rules_->features[static_cast<size_t>(center.feature)].removable) center.feature = kNone;
    center.improvement = kNone;
    if (city.capital) {
        for (size_t b = 0; b < rules_->buildings.size(); ++b) {
            if (rules_->buildings[b].granted) city.buildings.push_back(static_cast<TypeIndex>(b));
        }
    }
    const CityId newId = city.id;
    state_.cities.push_back(std::move(city));
    assignCitizens(*state_.city(newId));

    state_.units.erase(std::remove_if(state_.units.begin(), state_.units.end(),
                                      [&](const Unit& x) { return x.id == c.id; }),
                       state_.units.end());
    refreshVisibility(owner);
}

void Game::applyEndTurn(const Command& c) {
    state_.players[static_cast<size_t>(c.player)].freeChanges = false;
    const size_t n = state_.players.size();
    bool wrapped = false;
    for (size_t step = 1; step <= n; ++step) {
        size_t next = (static_cast<size_t>(c.player) + step) % n;
        if (!wrapped && next <= static_cast<size_t>(c.player)) {
            wrapped = true;
            // Wrapped past the last player: the world takes its turn.
            beginGlobalTurn();
        }
        if (state_.players[next].alive) {
            state_.currentPlayer = static_cast<PlayerId>(next);
            beginPlayerTurn(state_.currentPlayer);
            return;
        }
    }
}

void Game::beginGlobalTurn() {
    ++state_.turn;
}

void Game::beginPlayerTurn(PlayerId pid, bool runCities) {
    if (runCities) {
        processCities(pid);
        Player& p = state_.players[static_cast<size_t>(pid)];
        if (p.anarchyTurns > 0 && --p.anarchyTurns == 0) p.freeChanges = true;  // set up the new government
    }
    for (Unit& u : state_.units) {
        if (u.owner != pid) continue;
        u.movesLeft = Fixed::fromInt(typeOf(*rules_, u).moves);
        if (u.activity == Activity::Skip) u.activity = Activity::Awake;
    }
    std::vector<UnitId> moving;
    for (const Unit& u : state_.units) {
        if (u.owner == pid && u.moveTarget) moving.push_back(u.id);
    }
    for (UnitId id : moving) advanceUnit(id);
    refreshVisibility(pid);
    updateBoosts(pid);
}

Unit& Game::spawnUnit(TypeIndex type, PlayerId owner, Hex pos) {
    Unit u;
    u.id = state_.nextUnitId++;
    u.type = type;
    u.owner = owner;
    u.pos = pos;
    u.hp = rules_->globalInt("COMBAT_MAX_HIT_POINTS");
    u.movesLeft = Fixed::fromInt(rules_->units[static_cast<size_t>(type)].moves);
    u.charges = rules_->units[static_cast<size_t>(type)].buildCharges;
    state_.units.push_back(u);  // ids only grow, so the vector stays sorted
    return state_.units.back();
}

}  // namespace sov
