#include "sovereign/serialize.h"

#include <algorithm>

namespace sov {

namespace {
constexpr uint8_t kMagic[4] = {'S', 'O', 'V', 'S'};

void writeHex(ByteWriter& w, Hex h) {
    w.i32(h.x);
    w.i32(h.y);
}
Hex readHex(ByteReader& r) {
    Hex h;
    h.x = r.i32();
    h.y = r.i32();
    return h;
}

void writeI32s(ByteWriter& w, const std::vector<int32_t>& v) {
    w.u32(static_cast<uint32_t>(v.size()));
    for (int32_t x : v) w.i32(x);
}
bool readI32s(ByteReader& r, std::vector<int32_t>& v) {
    uint32_t n = r.u32();
    if (!r.checkCount(n, 4)) return false;
    v.resize(n);
    for (int32_t& x : v) x = r.i32();
    return true;
}
void writeFixed(ByteWriter& w, Fixed f) { w.i64(f.raw()); }
Fixed readFixed(ByteReader& r) { return Fixed::fromRaw(r.i64()); }
void writeItem(ByteWriter& w, ProductionItem it) {
    w.u8(static_cast<uint8_t>(it.kind));
    w.i16(it.type);
}
ProductionItem readItem(ByteReader& r) {
    ProductionItem it;
    it.kind = static_cast<ProductionKind>(r.u8());
    it.type = r.i16();
    return it;
}

void writeSetup(ByteWriter& w, const GameSetup& s) {
    w.u64(s.seed);
    w.str(s.mapSize);
    w.str(s.speed);
    w.boolean(s.wrapX);
    w.u32(static_cast<uint32_t>(s.players.size()));
    for (const PlayerSetup& p : s.players) {
        w.str(p.civ);
        w.boolean(p.human);
    }
}
void readSetup(ByteReader& r, GameSetup& s) {
    s.seed = r.u64();
    s.mapSize = r.str();
    s.speed = r.str();
    s.wrapX = r.boolean();
    uint32_t n = r.u32();
    if (!r.checkCount(n, 5)) return;
    s.players.resize(n);
    for (PlayerSetup& p : s.players) {
        p.civ = r.str();
        p.human = r.boolean();
    }
}

void writeCommand(ByteWriter& w, const Command& c) {
    w.u8(static_cast<uint8_t>(c.type));
    w.i8(c.player);
    w.i32(c.id);
    writeHex(w, c.target);
    w.i32(c.arg);
    w.i32(c.arg2);
}
Command readCommand(ByteReader& r) {
    Command c;
    c.type = static_cast<CommandType>(r.u8());
    c.player = r.i8();
    c.id = r.i32();
    c.target = readHex(r);
    c.arg = r.i32();
    c.arg2 = r.i32();
    return c;
}
// Guards against out-of-range type indices before any rules code runs.
bool stateMatchesRules(const GameState& s, const Rules& rules) {
    auto inRange = [](TypeIndex t, size_t n, bool allowNone) {
        return (allowNone && t == kNone) || (t >= 0 && static_cast<size_t>(t) < n);
    };
    for (const Plot& p : s.plots) {
        if (!inRange(p.terrain, rules.terrains.size(), false) || !inRange(p.feature, rules.features.size(), true) ||
            !inRange(p.resource, rules.resources.size(), true))
            return false;
        if (p.owner != kNoPlayer && (p.owner < 0 || static_cast<size_t>(p.owner) >= s.players.size())) return false;
    }
    if (s.players.empty() || s.currentPlayer < 0 || static_cast<size_t>(s.currentPlayer) >= s.players.size()) return false;
    for (const Player& p : s.players) {
        if (!inRange(p.civ, rules.civs.size(), false)) return false;
    }
    for (size_t i = 0; i < s.units.size(); ++i) {
        const Unit& u = s.units[i];
        if (!inRange(u.type, rules.units.size(), false)) return false;
        if (u.owner < 0 || static_cast<size_t>(u.owner) >= s.players.size() || !s.grid.valid(u.pos)) return false;
        if (i > 0 && s.units[i - 1].id >= u.id) return false;
    }
    for (size_t i = 0; i < s.cities.size(); ++i) {
        const City& c = s.cities[i];
        if (c.owner < 0 || static_cast<size_t>(c.owner) >= s.players.size() || !s.grid.valid(c.pos)) return false;
        if (i > 0 && s.cities[i - 1].id >= c.id) return false;
        for (TypeIndex b : c.buildings) if (!inRange(b, rules.buildings.size(), false)) return false;
        auto itemOk = [&](const ProductionItem& it) {
            return it.kind == ProductionKind::Unit ? inRange(it.type, rules.units.size(), false)
                 : it.kind == ProductionKind::Building ? inRange(it.type, rules.buildings.size(), false) : false;
        };
        for (const ProductionItem& it : c.queue) if (!itemOk(it)) return false;
        for (const ProductionProgress& pp : c.progress) if (!itemOk(pp.item)) return false;
        for (int32_t pi : c.worked) if (pi < 0 || pi >= s.grid.size()) return false;
        for (int32_t pi : c.locked) if (pi < 0 || pi >= s.grid.size()) return false;
    }
    for (const Player& p : s.players) {
        if (p.unitsTrained.size() > rules.units.size()) return false;
        if (p.techs.done.size() != rules.techs.size() || p.civics.done.size() != rules.civics.size()) return false;
        if (!inRange(p.techs.current, rules.techs.size(), true) || !inRange(p.civics.current, rules.civics.size(), true))
            return false;
        if (p.governmentUses.size() != rules.governments.size()) return false;
        if (!inRange(p.government, rules.governments.size(), true)) return false;
        const size_t slots = p.government == kNone
                                 ? 0
                                 : static_cast<size_t>(rules.governments[static_cast<size_t>(p.government)].totalSlots());
        if (p.policies.size() != slots) return false;
        for (TypeIndex pol : p.policies) if (!inRange(pol, rules.policies.size(), true)) return false;
        if (p.anarchyTurns < 0) return false;
    }
    return true;
}
}  // namespace

std::vector<uint8_t> serializeState(const GameState& s) {
    ByteWriter w;
    writeSetup(w, s.setup);
    w.i32(s.turn);
    w.i8(s.currentPlayer);
    w.i32(s.grid.width());
    w.i32(s.grid.height());
    w.boolean(s.grid.wrapX());
    for (const Plot& p : s.plots) {
        w.i16(p.terrain);
        w.i16(p.feature);
        w.i16(p.resource);
        w.u8(p.resourceAmount);
        w.u8(p.riverEdges);
        w.i8(p.owner);
        w.i32(p.city);
        w.i16(p.continent);
    }
    w.u32(static_cast<uint32_t>(s.players.size()));
    for (const Player& p : s.players) {
        w.i8(p.id);
        w.i16(p.civ);
        w.boolean(p.human);
        w.boolean(p.alive);
        w.i32(p.citiesFounded);
        writeFixed(w, p.gold);
        writeFixed(w, p.faith);
        writeI32s(w, std::vector<int32_t>(p.unitsTrained.begin(), p.unitsTrained.end()));
        for (const TreeProgress* t : {&p.techs, &p.civics}) {
            w.bytes(t->done);
            w.bytes(t->boosted);
            w.u32(static_cast<uint32_t>(t->progress.size()));
            for (Fixed f : t->progress) writeFixed(w, f);
            w.i16(t->current);
            writeFixed(w, t->overflow);
        }
        w.i16(p.government);
        writeI32s(w, std::vector<int32_t>(p.policies.begin(), p.policies.end()));
        writeI32s(w, std::vector<int32_t>(p.governmentUses.begin(), p.governmentUses.end()));
        w.i32(p.anarchyTurns);
        w.boolean(p.freeChanges);
        writeHex(w, p.startPos);
        w.bytes(p.visibility);
    }
    w.u32(static_cast<uint32_t>(s.units.size()));
    for (const Unit& u : s.units) {
        w.i32(u.id);
        w.i16(u.type);
        w.i8(u.owner);
        writeHex(w, u.pos);
        w.i32(u.hp);
        w.i64(u.movesLeft.raw());
        w.u8(static_cast<uint8_t>(u.activity));
        w.boolean(u.moveTarget.has_value());
        writeHex(w, u.moveTarget.value_or(Hex{}));
        w.i32(u.xp);
    }
    w.u32(static_cast<uint32_t>(s.cities.size()));
    for (const City& c : s.cities) {
        w.i32(c.id);
        w.i8(c.owner);
        w.str(c.name);
        writeHex(w, c.pos);
        w.i32(c.population);
        w.i32(c.foundedTurn);
        w.boolean(c.capital);
        writeFixed(w, c.food);
        writeFixed(w, c.borderCulture);
        w.i32(c.plotsByCulture);
        writeFixed(w, c.overflow);
        w.u32(static_cast<uint32_t>(c.buildings.size()));
        for (TypeIndex b : c.buildings) w.i16(b);
        w.u32(static_cast<uint32_t>(c.queue.size()));
        for (const ProductionItem& it : c.queue) writeItem(w, it);
        w.u32(static_cast<uint32_t>(c.progress.size()));
        for (const ProductionProgress& pp : c.progress) {
            writeItem(w, pp.item);
            writeFixed(w, pp.amount);
        }
        writeI32s(w, c.worked);
        writeI32s(w, c.locked);
    }
    w.i32(s.nextUnitId);
    w.i32(s.nextCityId);
    for (size_t i = 0; i < static_cast<size_t>(RngStream::Count); ++i) {
        for (uint64_t word : s.rng.get(static_cast<RngStream>(i)).state()) w.u64(word);
    }
    return w.take();
}

bool deserializeState(ByteReader& r, GameState& s) {
    readSetup(r, s.setup);
    s.turn = r.i32();
    s.currentPlayer = r.i8();
    int32_t width = r.i32(), height = r.i32();
    bool wrap = r.boolean();
    if (!r.ok() || width <= 0 || height <= 0 || width > 4096 || height > 4096) return false;
    s.grid = HexGrid(width, height, wrap);
    s.plots.resize(static_cast<size_t>(s.grid.size()));
    for (Plot& p : s.plots) {
        p.terrain = r.i16();
        p.feature = r.i16();
        p.resource = r.i16();
        p.resourceAmount = r.u8();
        p.riverEdges = r.u8();
        p.owner = r.i8();
        p.city = r.i32();
        p.continent = r.i16();
    }
    uint32_t np = r.u32();
    if (!r.checkCount(np, 16)) return false;
    s.players.resize(np);
    for (Player& p : s.players) {
        p.id = r.i8();
        p.civ = r.i16();
        p.human = r.boolean();
        p.alive = r.boolean();
        p.citiesFounded = r.i32();
        p.gold = readFixed(r);
        p.faith = readFixed(r);
        std::vector<int32_t> trained;
        if (!readI32s(r, trained)) return false;
        p.unitsTrained.assign(trained.begin(), trained.end());
        for (TreeProgress* t : {&p.techs, &p.civics}) {
            t->done = r.bytes();
            t->boosted = r.bytes();
            uint32_t n = r.u32();
            if (!r.checkCount(n, 8) || n != t->done.size() || n != t->boosted.size()) return false;
            t->progress.resize(n);
            for (Fixed& f : t->progress) f = readFixed(r);
            t->current = r.i16();
            t->overflow = readFixed(r);
        }
        p.government = r.i16();
        std::vector<int32_t> ints;
        if (!readI32s(r, ints)) return false;
        p.policies.clear();
        for (int32_t v : ints) p.policies.push_back(static_cast<TypeIndex>(v));
        if (!readI32s(r, ints)) return false;
        p.governmentUses.assign(ints.begin(), ints.end());
        p.anarchyTurns = r.i32();
        p.freeChanges = r.boolean();
        p.startPos = readHex(r);
        p.visibility = r.bytes();
        if (p.visibility.size() != s.plots.size()) return false;
    }
    uint32_t nu = r.u32();
    if (!r.checkCount(nu, 30)) return false;
    s.units.resize(nu);
    for (Unit& u : s.units) {
        u.id = r.i32();
        u.type = r.i16();
        u.owner = r.i8();
        u.pos = readHex(r);
        u.hp = r.i32();
        u.movesLeft = Fixed::fromRaw(r.i64());
        u.activity = static_cast<Activity>(r.u8());
        bool hasTarget = r.boolean();
        Hex t = readHex(r);
        u.moveTarget = hasTarget ? std::optional<Hex>(t) : std::nullopt;
        u.xp = r.i32();
    }
    uint32_t nc = r.u32();
    if (!r.checkCount(nc, 20)) return false;
    s.cities.resize(nc);
    for (City& c : s.cities) {
        c.id = r.i32();
        c.owner = r.i8();
        c.name = r.str();
        c.pos = readHex(r);
        c.population = r.i32();
        c.foundedTurn = r.i32();
        c.capital = r.boolean();
        c.food = readFixed(r);
        c.borderCulture = readFixed(r);
        c.plotsByCulture = r.i32();
        c.overflow = readFixed(r);
        uint32_t nb = r.u32();
        if (!r.checkCount(nb, 2)) return false;
        c.buildings.resize(nb);
        for (TypeIndex& b : c.buildings) b = r.i16();
        uint32_t nq = r.u32();
        if (!r.checkCount(nq, 3)) return false;
        c.queue.resize(nq);
        for (ProductionItem& it : c.queue) it = readItem(r);
        uint32_t npg = r.u32();
        if (!r.checkCount(npg, 11)) return false;
        c.progress.resize(npg);
        for (ProductionProgress& pp : c.progress) {
            pp.item = readItem(r);
            pp.amount = readFixed(r);
        }
        if (!readI32s(r, c.worked) || !readI32s(r, c.locked)) return false;
    }
    s.nextUnitId = r.i32();
    s.nextCityId = r.i32();
    for (size_t i = 0; i < static_cast<size_t>(RngStream::Count); ++i) {
        std::array<uint64_t, 4> st{};
        for (uint64_t& word : st) word = r.u64();
        s.rng.get(static_cast<RngStream>(i)).setState(st);
    }
    return r.ok();
}

std::vector<uint8_t> saveGame(const Game& game) {
    ByteWriter w;
    for (uint8_t m : kMagic) w.u8(m);
    w.u32(kSaveVersion);
    w.u64(game.rules().checksum());
    w.bytes(serializeState(game.state()));
    w.u32(static_cast<uint32_t>(game.log().size()));
    for (const Command& c : game.log()) writeCommand(w, c);
    return w.take();
}

std::unique_ptr<Game> loadGame(const Rules& rules, const std::vector<uint8_t>& bytes, std::string* error) {
    std::string localError;
    if (!error) error = &localError;
    ByteReader r(bytes);
    for (uint8_t m : kMagic) {
        if (r.u8() != m) {
            *error = "not a Sovereign save";
            return nullptr;
        }
    }
    uint32_t version = r.u32();
    if (version != kSaveVersion) {
        // Older versions get migrated here as the format evolves.
        *error = "unsupported save version " + std::to_string(version);
        return nullptr;
    }
    if (r.u64() != rules.checksum()) {
        *error = "save was made with different rules data";
        return nullptr;
    }
    std::vector<uint8_t> stateBytes = r.bytes();
    ByteReader sr(stateBytes);
    GameState state;
    if (!r.ok() || !deserializeState(sr, state) || !sr.atEnd()) {
        *error = "corrupt game state";
        return nullptr;
    }
    if (!stateMatchesRules(state, rules)) {
        *error = "save refers to rules rows that do not exist";
        return nullptr;
    }
    uint32_t n = r.u32();
    if (!r.checkCount(n, 22)) {
        *error = "corrupt command log";
        return nullptr;
    }
    std::vector<Command> log;
    log.reserve(n);
    for (uint32_t i = 0; i < n; ++i) log.push_back(readCommand(r));
    if (!r.ok() || !r.atEnd()) {
        *error = "corrupt command log";
        return nullptr;
    }
    return std::make_unique<Game>(rules, std::move(state), std::move(log));
}

}  // namespace sov
