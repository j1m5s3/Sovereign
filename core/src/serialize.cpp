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
    w.boolean(s.barbarians);
    w.boolean(s.dominationVictory);
    w.boolean(s.scoreVictory);
    w.boolean(s.religiousVictory);
    w.boolean(s.cultureVictory);
    w.boolean(s.diplomaticVictory);
    w.i32(s.turnLimit);
    w.boolean(s.regicide);
    w.boolean(s.liveBattles);
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
    s.barbarians = r.boolean();
    s.dominationVictory = r.boolean();
    s.scoreVictory = r.boolean();
    s.religiousVictory = r.boolean();
    s.cultureVictory = r.boolean();
    s.diplomaticVictory = r.boolean();
    s.turnLimit = r.i32();
    s.regicide = r.boolean();
    s.liveBattles = r.boolean();
}

void writeCommand(ByteWriter& w, const Command& c) {
    w.u8(static_cast<uint8_t>(c.type));
    w.i8(c.player);
    w.i32(c.id);
    writeHex(w, c.target);
    w.i32(c.arg);
    w.i32(c.arg2);
    writeI32s(w, c.data);
    w.str(c.text);
}
Command readCommand(ByteReader& r) {
    Command c;
    c.type = static_cast<CommandType>(r.u8());
    c.player = r.i8();
    c.id = r.i32();
    c.target = readHex(r);
    c.arg = r.i32();
    c.arg2 = r.i32();
    readI32s(r, c.data);
    c.text = r.str();
    return c;
}
// Guards against out-of-range type indices before any rules code runs.
bool stateMatchesRules(const GameState& s, const Rules& rules) {
    auto inRange = [](TypeIndex t, size_t n, bool allowNone) {
        return (allowNone && t == kNone) || (t >= 0 && static_cast<size_t>(t) < n);
    };
    for (const Plot& p : s.plots) {
        if (!inRange(p.terrain, rules.terrains.size(), false) || !inRange(p.feature, rules.features.size(), true) ||
            !inRange(p.resource, rules.resources.size(), true) || !inRange(p.improvement, rules.improvements.size(), true))
            return false;
        if (p.route < -1 || p.route >= static_cast<int>(rules.routes.size())) return false;
        if (p.owner != kNoPlayer && (p.owner < 0 || static_cast<size_t>(p.owner) >= s.players.size())) return false;
    }
    if (s.players.empty() || s.currentPlayer < 0 || static_cast<size_t>(s.currentPlayer) >= s.players.size()) return false;
    if (s.winner != kNoPlayer && (s.winner < 0 || static_cast<size_t>(s.winner) >= s.players.size())) return false;
    if (static_cast<uint8_t>(s.victory) > static_cast<uint8_t>(Victory::Diplomatic)) return false;
    for (const CongressItem& it : s.congress) {
        if (!inRange(it.resolution, rules.resolutions.size(), false)) return false;
        for (const CongressVote& v : it.votes) if (v.target < 0 || static_cast<size_t>(v.target) >= it.candidates.size()) return false;
    }
    for (const PassedResolution& pr : s.passedResolutions) if (!inRange(pr.resolution, rules.resolutions.size(), false)) return false;
    if ((s.winner == kNoPlayer) != (s.victory == Victory::None) || s.setup.turnLimit < 0) return false;
    for (const Player& p : s.players) {
        if (!inRange(p.civ, rules.civs.size(), p.barbarian || p.cityState != kNone)) return false;
        if (!inRange(p.cityState, rules.cityStates.size(), true)) return false;
    }
    for (size_t i = 0; i < s.units.size(); ++i) {
        const Unit& u = s.units[i];
        if (!inRange(u.type, rules.units.size(), false) || !inRange(u.greatPerson, rules.greatPeople.size(), true)) return false;
        if (u.religion < -1 || u.religion >= static_cast<int>(s.religions.size())) return false;
        if (u.owner < 0 || static_cast<size_t>(u.owner) >= s.players.size() || !s.grid.valid(u.pos)) return false;
        if (i > 0 && s.units[i - 1].id >= u.id) return false;
        for (TypeIndex pr : u.promotions) if (!inRange(pr, rules.promotions.size(), false)) return false;
        for (size_t slot = 0; slot < u.gear.size(); ++slot) {
            if (!inRange(u.gear[slot], rules.gear.size(), true)) return false;
            if (u.gear[slot] != kNone && static_cast<size_t>(rules.gear[static_cast<size_t>(u.gear[slot])].slot) != slot) return false;
        }
    }
    for (size_t i = 0; i < s.cities.size(); ++i) {
        const City& c = s.cities[i];
        if (c.owner < 0 || static_cast<size_t>(c.owner) >= s.players.size() || !s.grid.valid(c.pos)) return false;
        if (i > 0 && s.cities[i - 1].id >= c.id) return false;
        if (c.originalOwner < 0 || static_cast<size_t>(c.originalOwner) >= s.players.size()) return false;
        for (TypeIndex b : c.buildings) if (!inRange(b, rules.buildings.size(), false)) return false;
        auto itemOk = [&](const ProductionItem& it) {
            return it.kind == ProductionKind::Unit ? inRange(it.type, rules.units.size(), false)
                 : it.kind == ProductionKind::Building ? inRange(it.type, rules.buildings.size(), false)
                 : it.kind == ProductionKind::District ? inRange(it.type, rules.districts.size(), false) : false;
        };
        for (const ProductionItem& it : c.queue) if (!itemOk(it)) return false;
        for (const ProductionProgress& pp : c.progress) if (!itemOk(pp.item)) return false;
        for (int32_t pi : c.worked) if (pi < 0 || pi >= s.grid.size()) return false;
        for (int32_t pi : c.locked) if (pi < 0 || pi >= s.grid.size()) return false;
        for (const CityDistrict& d : c.districts) {
            if (!inRange(d.type, rules.districts.size(), false) || !s.grid.valid(d.pos)) return false;
        }
        if (c.pressure.size() > s.religions.size()) return false;
        for (const CityWonder& cw : c.wonders) if (!inRange(cw.building, rules.buildings.size(), false) || !s.grid.valid(cw.pos)) return false;
        for (const GreatWork& g : c.greatWorks) {
            if (!inRange(g.type, rules.greatWorkTypes.size(), false) || !inRange(g.building, rules.buildings.size(), false) ||
                !inRange(g.creator, rules.greatPeople.size(), true))
                return false;
        }
    }
    for (const TradeRoute& tr : s.tradeRoutes) {
        if (tr.owner < 0 || static_cast<size_t>(tr.owner) >= s.players.size() || !inRange(tr.traderType, rules.units.size(), false)) return false;
        for (int32_t pi : tr.path) if (pi < 0 || pi >= s.grid.size()) return false;
    }
    auto playerOk = [&](PlayerId p) { return p >= 0 && static_cast<size_t>(p) < s.players.size(); };
    for (const Player& p : s.players) {
        for (const Governor& g : p.governors) {
            if (!inRange(g.type, rules.governors.size(), false)) return false;
            for (TypeIndex pr : g.promotions) if (!inRange(pr, rules.governorPromotions.size(), false)) return false;
        }
        for (const OpinionMemory& m : p.memories) if (!playerOk(m.about) || m.duration <= 0) return false;
    }
    for (const Deal& d : s.deals) {
        if (!playerOk(d.from) || !playerOk(d.to)) return false;
        for (const DealItem& i : d.items) if (!playerOk(i.from) || !inRange(i.resource, rules.resources.size(), true)) return false;
    }
    for (const TalkRecord& t : s.talks) if (!playerOk(t.speaker) || !playerOk(t.leader)) return false;
    for (const Agreement& a : s.agreements) {
        if (!playerOk(a.from) || !playerOk(a.to) || !inRange(a.resource, rules.resources.size(), true)) return false;
    }
    for (const FoundedReligion& rel : s.religions) {
        if (!inRange(rel.type, rules.religions.size(), false) || rel.founder < 0 || static_cast<size_t>(rel.founder) >= s.players.size()) return false;
        for (TypeIndex b : rel.beliefs) if (!inRange(b, rules.beliefs.size(), false)) return false;
    }
    for (size_t i = 0; i < s.camps.size(); ++i) {
        if (!s.grid.valid(s.camps[i].pos) || (i > 0 && s.camps[i - 1].id >= s.camps[i].id)) return false;
        if (!inRange(s.camps[i].tribe, rules.barbarianTribes.size(), false)) return false;
    }
    for (const Player& p : s.players) {
        if (p.unitsTrained.size() > rules.units.size()) return false;
        if (p.stockpile.size() != rules.resources.size()) return false;
        if (p.fuelShort.size() != rules.resources.size()) return false;
        if (p.greatPersonPoints.size() > rules.greatPersonClasses.size() || p.greatPeopleRecruited.size() > rules.greatPersonClasses.size())
            return false;
        for (TypeIndex g : p.greatPeoplePassed) if (!inRange(g, rules.greatPeople.size(), true)) return false;
        for (TypeIndex g : p.greatPeopleActivated) if (!inRange(g, rules.greatPeople.size(), false)) return false;
        if (!inRange(p.pantheon, rules.beliefs.size(), true) || p.religion < -1 || p.religion >= static_cast<int>(s.religions.size())) return false;
        if (p.relations.size() != s.players.size()) return false;
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
        if (p.dynastyNext < 0 || p.interregnumTurns < 0) return false;
        if (p.captor != kNoPlayer && (p.captor < 0 || static_cast<size_t>(p.captor) >= s.players.size())) return false;
        for (TypeIndex g : p.savedGear) if (!inRange(g, rules.gear.size(), true)) return false;
        for (TypeIndex pr : p.savedPromotions) if (!inRange(pr, rules.promotions.size(), false)) return false;
    }
    for (size_t i = 0; i < s.agents.size(); ++i) {
        const Agent& a = s.agents[i];
        if (i > 0 && s.agents[i - 1].id >= a.id) return false;
        if (a.owner < 0 || static_cast<size_t>(a.owner) >= s.players.size()) return false;
        if (a.target != kNoPlayer && (a.target < 0 || static_cast<size_t>(a.target) >= s.players.size())) return false;
        if (a.level < 1 || a.travel < 0) return false;
    }
    return true;
}
}  // namespace

void encodeCommand(ByteWriter& w, const Command& c) { writeCommand(w, c); }
Command decodeCommand(ByteReader& r) { return readCommand(r); }

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
        w.i16(p.improvement);
        w.u8(p.resourceAmount);
        w.u8(p.riverEdges);
        w.i8(p.owner);
        w.i32(p.city);
        w.i16(p.continent);
        w.i8(p.route);
    }
    w.u32(static_cast<uint32_t>(s.players.size()));
    for (const Player& p : s.players) {
        w.i8(p.id);
        w.i16(p.civ);
        w.boolean(p.human);
        w.boolean(p.alive);
        w.boolean(p.barbarian);
        w.boolean(p.freeCity);
        w.i32(p.reputation);
        w.i32(p.strongestUnit);
        w.i32(p.citiesFounded);
        writeFixed(w, p.gold);
        writeFixed(w, p.faith);
        writeI32s(w, std::vector<int32_t>(p.unitsTrained.begin(), p.unitsTrained.end()));
        writeI32s(w, std::vector<int32_t>(p.stockpile.begin(), p.stockpile.end()));
        w.bytes(p.fuelShort);
        writeI32s(w, std::vector<int32_t>(p.greatPersonPoints.begin(), p.greatPersonPoints.end()));
        writeI32s(w, std::vector<int32_t>(p.greatPeopleRecruited.begin(), p.greatPeopleRecruited.end()));
        writeI32s(w, std::vector<int32_t>(p.greatPeoplePassed.begin(), p.greatPeoplePassed.end()));
        writeI32s(w, std::vector<int32_t>(p.greatPeopleActivated.begin(), p.greatPeopleActivated.end()));
        w.i16(p.pantheon);
        w.i16(p.religion);
        w.i16(p.cityState);
        writeI32s(w, std::vector<int32_t>(p.envoys.begin(), p.envoys.end()));
        w.i32(p.envoyTokens);
        w.i32(p.influence);
        w.i8(p.firstMetBy);
        w.boolean(p.hadSuzerain);
        w.i32(p.eraScore);
        w.u8(static_cast<uint8_t>(p.age));
        w.i32(p.pastGoldenAges);
        w.i32(p.pastDarkAges);
        w.bytes(std::vector<uint8_t>(p.momentEras.begin(), p.momentEras.end()));
        w.bytes(p.met);
        writeFixed(w, p.lifetimeCulture);
        writeI32s(w, p.tourismTo);
        w.u32(static_cast<uint32_t>(p.relations.size()));
        for (const Relation& rel : p.relations) {
            w.boolean(rel.war);
            w.i32(rel.since);
            w.boolean(rel.peaceOffered);
            w.i32(rel.denouncedOn);
            w.i32(rel.friendsUntil);
            w.i32(rel.openBordersUntil);
            w.i32(rel.lastProposal);
        }
        w.u32(static_cast<uint32_t>(p.memories.size()));
        for (const OpinionMemory& m : p.memories) {
            w.i8(m.about);
            w.u8(static_cast<uint8_t>(m.kind));
            w.i16(m.amount);
            w.i16(m.duration);
            w.i32(m.turn);
        }
        writeI32s(w, {p.warsDeclared, p.surpriseWars, p.citiesCaptured, p.citiesRazed, p.tradersPlundered, p.assassinsSent});
        w.u32(static_cast<uint32_t>(p.governors.size()));
        for (const Governor& g : p.governors) {
            w.i16(g.type);
            w.i32(g.city);
            w.i32(g.establishTurns);
            writeI32s(w, std::vector<int32_t>(g.promotions.begin(), g.promotions.end()));
        }
        w.i32(p.governorTitlesSpent);
        writeI32s(w, p.grievances);
        w.i32(p.favor);
        w.i32(p.diplomaticVictoryPoints);
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
        w.str(p.leaderName);
        w.i32(p.dynastyNext);
        w.boolean(p.successionPending);
        w.i32(p.interregnumTurns);
        w.i8(p.captor);
        for (TypeIndex g : p.savedGear) w.i32(g);
        writeI32s(w, std::vector<int32_t>(p.savedPromotions.begin(), p.savedPromotions.end()));
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
        w.boolean(u.moveOverland);
        w.i16(u.greatPerson);
        w.i16(u.religion);
        w.i32(u.xp);
        w.i32(u.charges);
        writeI32s(w, std::vector<int32_t>(u.promotions.begin(), u.promotions.end()));
        w.i32(u.fortifyTurns);
        w.i32(u.attacks);
        w.boolean(u.moved);
        w.boolean(u.attacked);
        w.i32(u.camp);
        for (TypeIndex g : u.gear) w.i32(g);
        w.i32(u.escorting);
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
        w.i32(c.hp);
        w.i32(c.wallHp);
        w.i32(c.lastAttackedTurn);
        w.boolean(c.struck);
        w.i8(c.originalOwner);
        w.boolean(c.originalCapital);
        w.i32(c.capturedTurn);
        w.u32(static_cast<uint32_t>(c.districts.size()));
        for (const CityDistrict& d : c.districts) {
            w.i16(d.type);
            writeHex(w, d.pos);
            w.boolean(d.complete);
        }
        w.i32(c.loyalty);
        w.i32(c.stanceTurn);
        w.i32(c.benevolenceUntil);
        w.i32(c.fearUntil);
        w.i32(c.fearAfterUntil);
        w.u32(static_cast<uint32_t>(c.greatWorks.size()));
        for (const GreatWork& g : c.greatWorks) {
            w.i16(g.type);
            w.i16(g.building);
            w.i16(g.creator);
        }
        writeI32s(w, c.pressure);
        w.u32(static_cast<uint32_t>(c.wonders.size()));
        for (const CityWonder& cw : c.wonders) {
            w.i16(cw.building);
            writeHex(w, cw.pos);
        }
    }
    w.bytes(s.greatPeopleClaimed);
    w.u32(static_cast<uint32_t>(s.tradeRoutes.size()));
    for (const TradeRoute& tr : s.tradeRoutes) {
        w.i32(tr.id);
        w.i8(tr.owner);
        w.i32(tr.origin);
        w.i32(tr.destination);
        w.i16(tr.traderType);
        writeI32s(w, tr.path);
        w.i32(tr.turnsLeft);
    }
    w.i32(s.nextTradeRouteId);
    w.u32(static_cast<uint32_t>(s.deals.size()));
    for (const Deal& d : s.deals) {
        w.i32(d.id);
        w.i8(d.from);
        w.i8(d.to);
        w.i32(d.turn);
        w.u32(static_cast<uint32_t>(d.items.size()));
        for (const DealItem& i : d.items) {
            w.u8(static_cast<uint8_t>(i.kind));
            w.i8(i.from);
            w.i32(i.amount);
            w.i16(i.resource);
        }
    }
    w.u32(static_cast<uint32_t>(s.agreements.size()));
    for (const Agreement& a : s.agreements) {
        w.u8(static_cast<uint8_t>(a.kind));
        w.i8(a.from);
        w.i8(a.to);
        w.i32(a.amount);
        w.i16(a.resource);
        w.i32(a.until);
    }
    w.i32(s.nextDealId);
    w.i32(s.nextCongressTurn);
    w.i32(s.congressOpenedTurn);
    w.u32(static_cast<uint32_t>(s.congress.size()));
    for (const CongressItem& it : s.congress) {
        w.i16(it.resolution);
        writeI32s(w, it.candidates);
        w.u32(static_cast<uint32_t>(it.votes.size()));
        for (const CongressVote& v : it.votes) {
            w.i8(v.player);
            w.u8(v.option);
            w.i32(v.target);
            w.i32(v.votes);
        }
    }
    w.u32(static_cast<uint32_t>(s.passedResolutions.size()));
    for (const PassedResolution& pr : s.passedResolutions) {
        w.i16(pr.resolution);
        w.u8(pr.option);
        w.i32(pr.target);
    }
    w.u32(static_cast<uint32_t>(s.talks.size()));
    for (const TalkRecord& t : s.talks) {
        w.i32(t.turn);
        w.i8(t.speaker);
        w.i8(t.leader);
        w.str(t.text);
    }
    w.i32(s.gameEra);
    w.i32(s.gameEraStart);
    w.bytes(std::vector<uint8_t>(s.worldMoments.begin(), s.worldMoments.end()));
    w.i32(s.majorsAtStart);
    w.u32(static_cast<uint32_t>(s.religions.size()));
    for (const FoundedReligion& rel : s.religions) {
        w.i16(rel.type);
        w.i8(rel.founder);
        w.i32(rel.holyCity);
        writeI32s(w, std::vector<int32_t>(rel.beliefs.begin(), rel.beliefs.end()));
    }
    w.u32(static_cast<uint32_t>(s.agents.size()));
    for (const Agent& a : s.agents) {
        w.i32(a.id);
        w.i8(a.owner);
        w.i32(a.level);
        w.i8(a.target);
        w.i32(a.travel);
        w.boolean(a.spy);
        w.i32(a.city);
        w.u8(static_cast<uint8_t>(a.mission));
        w.i32(a.missionTurns);
        w.i32(a.sourcesCity);
        w.i32(a.sourcesUntil);
    }
    w.i32(s.nextAgentId);
    const PendingBattle& pb = s.pendingBattle;
    w.boolean(pb.active);
    w.i32(pb.attacker);
    w.i32(pb.defender);
    w.i32(pb.city);
    writeHex(w, pb.target);
    w.i8(pb.liveFor);
    w.i32(pb.leader);
    w.i32(pb.expectedToDefender);
    w.i32(pb.expectedToAttacker);
    w.u32(static_cast<uint32_t>(s.events.size()));
    for (const GameEvent& e : s.events) {
        w.i32(e.turn);
        w.u8(static_cast<uint8_t>(e.kind));
        w.i8(e.actor);
        w.i8(e.target);
        w.i32(e.value);
    }
    w.u32(static_cast<uint32_t>(s.camps.size()));
    for (const Camp& k : s.camps) {
        w.i32(k.id);
        writeHex(w, k.pos);
        w.i32(k.boldness);
        w.i32(k.spawnTimer);
        w.i16(k.tribe);
    }
    w.i32(s.nextCampId);
    w.i32(s.nextUnitId);
    w.i32(s.nextCityId);
    w.i8(s.winner);
    w.u8(static_cast<uint8_t>(s.victory));
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
        p.improvement = r.i16();
        p.resourceAmount = r.u8();
        p.riverEdges = r.u8();
        p.owner = r.i8();
        p.city = r.i32();
        p.continent = r.i16();
        p.route = r.i8();
    }
    uint32_t np = r.u32();
    if (!r.checkCount(np, 16)) return false;
    s.players.resize(np);
    for (Player& p : s.players) {
        p.id = r.i8();
        p.civ = r.i16();
        p.human = r.boolean();
        p.alive = r.boolean();
        p.barbarian = r.boolean();
        p.freeCity = r.boolean();
        p.reputation = r.i32();
        p.strongestUnit = r.i32();
        p.citiesFounded = r.i32();
        p.gold = readFixed(r);
        p.faith = readFixed(r);
        std::vector<int32_t> trained;
        if (!readI32s(r, trained)) return false;
        p.unitsTrained.assign(trained.begin(), trained.end());
        if (!readI32s(r, trained)) return false;
        p.stockpile.assign(trained.begin(), trained.end());
        p.fuelShort = r.bytes();
        if (!readI32s(r, trained)) return false;
        p.greatPersonPoints.assign(trained.begin(), trained.end());
        if (!readI32s(r, trained)) return false;
        p.greatPeopleRecruited.assign(trained.begin(), trained.end());
        if (!readI32s(r, trained)) return false;
        p.greatPeoplePassed.clear();
        for (int32_t v : trained) p.greatPeoplePassed.push_back(static_cast<TypeIndex>(v));
        if (!readI32s(r, trained)) return false;
        p.greatPeopleActivated.clear();
        for (int32_t v : trained) p.greatPeopleActivated.push_back(static_cast<TypeIndex>(v));
        p.pantheon = r.i16();
        p.religion = r.i16();
        p.cityState = r.i16();
        if (!readI32s(r, trained)) return false;
        p.envoys.assign(trained.begin(), trained.end());
        p.envoyTokens = r.i32();
        p.influence = r.i32();
        p.firstMetBy = r.i8();
        p.hadSuzerain = r.boolean();
        p.eraScore = r.i32();
        p.age = static_cast<Age>(r.u8());
        p.pastGoldenAges = r.i32();
        p.pastDarkAges = r.i32();
        {
            const std::vector<uint8_t> me = r.bytes();
            p.momentEras.assign(me.begin(), me.end());
        }
        p.met = r.bytes();
        p.lifetimeCulture = readFixed(r);
        if (!readI32s(r, p.tourismTo)) return false;
        uint32_t nrel = r.u32();
        if (!r.checkCount(nrel, 6)) return false;
        p.relations.resize(nrel);
        for (Relation& rel : p.relations) {
            rel.war = r.boolean();
            rel.since = r.i32();
            rel.peaceOffered = r.boolean();
            rel.denouncedOn = r.i32();
            rel.friendsUntil = r.i32();
            rel.openBordersUntil = r.i32();
            rel.lastProposal = r.i32();
        }
        uint32_t nmem = r.u32();
        if (!r.checkCount(nmem, 10)) return false;
        p.memories.resize(nmem);
        for (OpinionMemory& m : p.memories) {
            m.about = r.i8();
            const uint8_t kind = r.u8();
            if (kind > static_cast<uint8_t>(MemoryKind::SpyCaught)) return false;
            m.kind = static_cast<MemoryKind>(kind);
            m.amount = r.i16();
            m.duration = r.i16();
            m.turn = r.i32();
        }
        {
            std::vector<int32_t> deeds;
            if (!readI32s(r, deeds) || deeds.size() != 6) return false;
            p.warsDeclared = deeds[0];
            p.surpriseWars = deeds[1];
            p.citiesCaptured = deeds[2];
            p.citiesRazed = deeds[3];
            p.tradersPlundered = deeds[4];
            p.assassinsSent = deeds[5];
        }
        uint32_t ngov = r.u32();
        if (!r.checkCount(ngov, 14)) return false;
        p.governors.resize(ngov);
        for (Governor& g : p.governors) {
            g.type = r.i16();
            g.city = r.i32();
            g.establishTurns = r.i32();
            std::vector<int32_t> promos;
            if (!readI32s(r, promos)) return false;
            g.promotions.clear();
            for (int32_t v : promos) g.promotions.push_back(static_cast<TypeIndex>(v));
        }
        p.governorTitlesSpent = r.i32();
        if (!readI32s(r, p.grievances)) return false;
        p.favor = r.i32();
        p.diplomaticVictoryPoints = r.i32();
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
        p.leaderName = r.str();
        p.dynastyNext = r.i32();
        p.successionPending = r.boolean();
        p.interregnumTurns = r.i32();
        p.captor = r.i8();
        for (TypeIndex& g : p.savedGear) g = static_cast<TypeIndex>(r.i32());
        std::vector<int32_t> saved;
        if (!readI32s(r, saved)) return false;
        p.savedPromotions.clear();
        for (int32_t v : saved) p.savedPromotions.push_back(static_cast<TypeIndex>(v));
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
        u.moveOverland = r.boolean();
        u.greatPerson = r.i16();
        u.religion = r.i16();
        u.xp = r.i32();
        u.charges = r.i32();
        std::vector<int32_t> promos;
        if (!readI32s(r, promos)) return false;
        u.promotions.clear();
        for (int32_t v : promos) u.promotions.push_back(static_cast<TypeIndex>(v));
        u.fortifyTurns = r.i32();
        u.attacks = r.i32();
        u.moved = r.boolean();
        u.attacked = r.boolean();
        u.camp = r.i32();
        for (TypeIndex& g : u.gear) g = static_cast<TypeIndex>(r.i32());
        u.escorting = r.i32();
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
        c.hp = r.i32();
        c.wallHp = r.i32();
        c.lastAttackedTurn = r.i32();
        c.struck = r.boolean();
        c.originalOwner = r.i8();
        c.originalCapital = r.boolean();
        c.capturedTurn = r.i32();
        uint32_t nd = r.u32();
        if (!r.checkCount(nd, 11)) return false;
        c.districts.resize(nd);
        for (CityDistrict& d : c.districts) {
            d.type = r.i16();
            d.pos = readHex(r);
            d.complete = r.boolean();
        }
        c.loyalty = r.i32();
        c.stanceTurn = r.i32();
        c.benevolenceUntil = r.i32();
        c.fearUntil = r.i32();
        c.fearAfterUntil = r.i32();
        uint32_t ngw = r.u32();
        if (!r.checkCount(ngw, 6)) return false;
        c.greatWorks.resize(ngw);
        for (GreatWork& g : c.greatWorks) {
            g.type = r.i16();
            g.building = r.i16();
            g.creator = r.i16();
        }
        if (!readI32s(r, c.pressure)) return false;
        uint32_t nw = r.u32();
        if (!r.checkCount(nw, 10)) return false;
        c.wonders.resize(nw);
        for (CityWonder& cw : c.wonders) {
            cw.building = r.i16();
            cw.pos = readHex(r);
        }
    }
    s.greatPeopleClaimed = r.bytes();
    uint32_t ntr = r.u32();
    if (!r.checkCount(ntr, 23)) return false;
    s.tradeRoutes.resize(ntr);
    for (TradeRoute& tr : s.tradeRoutes) {
        tr.id = r.i32();
        tr.owner = r.i8();
        tr.origin = r.i32();
        tr.destination = r.i32();
        tr.traderType = r.i16();
        if (!readI32s(r, tr.path)) return false;
        tr.turnsLeft = r.i32();
    }
    s.nextTradeRouteId = r.i32();
    uint32_t ndeal = r.u32();
    if (!r.checkCount(ndeal, 14)) return false;
    s.deals.resize(ndeal);
    for (Deal& d : s.deals) {
        d.id = r.i32();
        d.from = r.i8();
        d.to = r.i8();
        d.turn = r.i32();
        uint32_t ni = r.u32();
        if (!r.checkCount(ni, 8)) return false;
        d.items.resize(ni);
        for (DealItem& i : d.items) {
            const uint8_t kind = r.u8();
            if (kind >= kNumDealItemKinds) return false;
            i.kind = static_cast<DealItemKind>(kind);
            i.from = r.i8();
            i.amount = r.i32();
            i.resource = r.i16();
        }
    }
    uint32_t nag = r.u32();
    if (!r.checkCount(nag, 14)) return false;
    s.agreements.resize(nag);
    for (Agreement& a : s.agreements) {
        const uint8_t kind = r.u8();
        if (kind >= kNumDealItemKinds) return false;
        a.kind = static_cast<DealItemKind>(kind);
        a.from = r.i8();
        a.to = r.i8();
        a.amount = r.i32();
        a.resource = r.i16();
        a.until = r.i32();
    }
    s.nextDealId = r.i32();
    s.nextCongressTurn = r.i32();
    s.congressOpenedTurn = r.i32();
    uint32_t ncong = r.u32();
    if (!r.checkCount(ncong, 10)) return false;
    s.congress.resize(ncong);
    for (CongressItem& it : s.congress) {
        it.resolution = r.i16();
        if (!readI32s(r, it.candidates)) return false;
        uint32_t nv = r.u32();
        if (!r.checkCount(nv, 10)) return false;
        it.votes.resize(nv);
        for (CongressVote& v : it.votes) {
            v.player = r.i8();
            v.option = r.u8();
            v.target = r.i32();
            v.votes = r.i32();
        }
    }
    uint32_t npass = r.u32();
    if (!r.checkCount(npass, 7)) return false;
    s.passedResolutions.resize(npass);
    for (PassedResolution& pr : s.passedResolutions) {
        pr.resolution = r.i16();
        pr.option = r.u8();
        pr.target = r.i32();
    }
    uint32_t ntalk = r.u32();
    if (!r.checkCount(ntalk, 10)) return false;
    s.talks.resize(ntalk);
    for (TalkRecord& t : s.talks) {
        t.turn = r.i32();
        t.speaker = r.i8();
        t.leader = r.i8();
        t.text = r.str();
        if (t.text.size() > kMaxTalkText) return false;
    }
    s.gameEra = r.i32();
    s.gameEraStart = r.i32();
    {
        const std::vector<uint8_t> wm = r.bytes();
        s.worldMoments.assign(wm.begin(), wm.end());
    }
    s.majorsAtStart = r.i32();
    uint32_t nrel = r.u32();
    if (!r.checkCount(nrel, 11)) return false;
    s.religions.resize(nrel);
    for (FoundedReligion& rel : s.religions) {
        rel.type = r.i16();
        rel.founder = r.i8();
        rel.holyCity = r.i32();
        std::vector<int32_t> beliefs;
        if (!readI32s(r, beliefs)) return false;
        rel.beliefs.clear();
        for (int32_t v : beliefs) rel.beliefs.push_back(static_cast<TypeIndex>(v));
    }
    uint32_t na = r.u32();
    if (!r.checkCount(na, 14)) return false;
    s.agents.resize(na);
    for (Agent& a : s.agents) {
        a.id = r.i32();
        a.owner = r.i8();
        a.level = r.i32();
        a.target = r.i8();
        a.travel = r.i32();
        a.spy = r.boolean();
        a.city = r.i32();
        const uint8_t mission = r.u8();
        if (mission >= kNumSpyMissions) return false;
        a.mission = static_cast<SpyMission>(mission);
        a.missionTurns = r.i32();
        a.sourcesCity = r.i32();
        a.sourcesUntil = r.i32();
    }
    s.nextAgentId = r.i32();
    PendingBattle& pb = s.pendingBattle;
    pb.active = r.boolean();
    pb.attacker = r.i32();
    pb.defender = r.i32();
    pb.city = r.i32();
    pb.target = readHex(r);
    pb.liveFor = r.i8();
    pb.leader = r.i32();
    pb.expectedToDefender = r.i32();
    pb.expectedToAttacker = r.i32();
    uint32_t ne = r.u32();
    if (!r.checkCount(ne, 11)) return false;
    s.events.resize(ne);
    for (GameEvent& e : s.events) {
        e.turn = r.i32();
        e.kind = static_cast<EventKind>(r.u8());
        e.actor = r.i8();
        e.target = r.i8();
        e.value = r.i32();
    }
    uint32_t nk = r.u32();
    if (!r.checkCount(nk, 16)) return false;
    s.camps.resize(nk);
    for (Camp& k : s.camps) {
        k.id = r.i32();
        k.pos = readHex(r);
        k.boldness = r.i32();
        k.spawnTimer = r.i32();
        k.tribe = r.i16();
    }
    s.nextCampId = r.i32();
    s.nextUnitId = r.i32();
    s.nextCityId = r.i32();
    s.winner = r.i8();
    s.victory = static_cast<Victory>(r.u8());
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
