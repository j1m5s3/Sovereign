// Eras, ages and tourism (09-civs-eras-victory-climate.md, Era score and Ages [R&F];
// 07-economy-trade-great-people.md, Tourism and Culture Victory). Historic moments earn era
// score; when the world moves to its next era each civ's score against its thresholds sets a
// Dark, Normal, Golden or Heroic Age. Tourism from Great Works, wonders and Holy Cities draws
// visiting tourists; the culture victory goes to a civ whose visitors outnumber every other
// civ's domestic tourists.
#include <algorithm>

#include "sovereign/game.h"
#include "sovereign/modifiers.h"

namespace sov {

namespace {
size_t at(TypeIndex i) { return static_cast<size_t>(i); }
bool isMajor(const Player& p) { return p.alive && !p.barbarian && !p.freeCity && p.cityState == kNone; }
int speedPercent(const GameState& s, const Rules& r) { return r.speeds[at(r.speed(s.setup.speed))].costPercent; }
}  // namespace

void Game::awardMoment(PlayerId pid, const char* id) {
    Player& p = state_.players[at(pid)];
    const TypeIndex m = rules_->moment(id);
    if (!isMajor(p) || m == kNone) return;
    const MomentType& mt = rules_->moments[at(m)];
    if (mt.obsoleteEra >= 0 && state_.gameEra >= mt.obsoleteEra) return;
    if ((mt.eraMin >= 0 && state_.gameEra < mt.eraMin) || (mt.eraMax >= 0 && state_.gameEra > mt.eraMax)) return;  // its era window
    if (p.momentEras.size() < rules_->moments.size()) p.momentEras.resize(rules_->moments.size(), 0);
    p.momentEras[at(m)] = static_cast<int8_t>(state_.gameEra + 1);
    // Taj Mahal (03: Wonders): +1 era score for each moment worth 2 or more.
    const int gained = mt.eraScore + (mt.eraScore >= 2 && buildingsOwned(pid, "BUILDING_TAJ_MAHAL") > 0 ? 1 : 0);
    p.eraScore += gained;
    p.eraScoreTotal += gained;
    pushEvent(EventKind::HistoricMoment, pid, kNoPlayer, m);
}

// Dedications [R&F] (09: Dedications; data: CommemorationTypes' era windows). At each new era a civ chooses
// one (three in a Heroic Age) among those whose window holds the world's new era. Outside a Golden Age a
// dedication earns era score for its deeds (dedicationScore); in one, its bonus applies (goldenDedication).
std::vector<TypeIndex> Game::availableDedications(PlayerId player) const {
    std::vector<TypeIndex> out;
    const Player& p = state_.players[at(player)];
    if (!isMajor(p) || p.dedicationsPending <= 0) return out;
    for (size_t d = 0; d < rules_->dedications.size(); ++d) {
        const DedicationType& dt = rules_->dedications[d];
        if (state_.gameEra < dt.eraMin || (dt.eraMax >= 0 && state_.gameEra > dt.eraMax)) continue;
        if (std::find(p.dedications.begin(), p.dedications.end(), static_cast<TypeIndex>(d)) != p.dedications.end()) continue;
        out.push_back(static_cast<TypeIndex>(d));
    }
    return out;
}

CommandError Game::dedicationProblem(PlayerId player, TypeIndex dedication) const {
    const std::vector<TypeIndex> open = availableDedications(player);
    return std::find(open.begin(), open.end(), dedication) != open.end() ? CommandError::Ok : CommandError::BadTarget;
}

void Game::chooseDedication(PlayerId player, TypeIndex dedication) {
    Player& p = state_.players[at(player)];
    p.dedications.push_back(dedication);
    --p.dedicationsPending;
    // Automaton Warfare in a Golden Age: one Giant Death Robot in the capital.
    if (goldenDedication(player, "DEDICATION_AUTOMATON_WARFARE")) {
        const TypeIndex gdr = rules_->unit("UNIT_GIANT_DEATH_ROBOT");
        for (const City& c : state_.cities) {
            if (c.owner != player || !c.capital || gdr == kNone) continue;
            if (auto spot = unitSpawnPlot(c, gdr)) spawnUnit(gdr, player, *spot);
            break;
        }
    }
}

bool Game::dedicated(PlayerId player, const char* id) const {
    if (player < 0 || at(player) >= state_.players.size()) return false;
    // One of the player's few dedications has that name (ids are unique), found without a search of them all.
    const std::vector<TypeIndex>& mine = state_.players[at(player)].dedications;
    return std::any_of(mine.begin(), mine.end(), [&](TypeIndex d) { return d >= 0 && at(d) < rules_->dedications.size() && rules_->dedications[at(d)].id == id; });
}

bool Game::goldenDedication(PlayerId player, const char* id) const {
    if (!dedicated(player, id)) return false;
    const Age a = state_.players[at(player)].age;
    return a == Age::Golden || a == Age::Heroic;
}

void Game::dedicationScore(PlayerId player, const char* id, int amount) {
    if (!dedicated(player, id) || goldenDedication(player, id)) return;
    state_.players[at(player)].eraScore += amount;
    state_.players[at(player)].eraScoreTotal += amount;
}

void Game::awardFirst(PlayerId pid, const char* worldId, const char* ownId, int key) {
    const Player& p = state_.players[at(pid)];
    if (!isMajor(p)) return;
    // A world's first goes to the first civ for each key (an era, a government tier...); later
    // civs earn the ordinary moment once per key.
    const TypeIndex w = rules_->moment(worldId);
    if (state_.worldMoments.size() < rules_->moments.size()) state_.worldMoments.resize(rules_->moments.size(), 0);
    if (w != kNone && state_.worldMoments[at(w)] < key + 1) {
        state_.worldMoments[at(w)] = static_cast<int8_t>(key + 1);
        awardMoment(pid, worldId);
        return;
    }
    const TypeIndex o = ownId ? rules_->moment(ownId) : kNone;
    if (o == kNone) return;
    if (at(o) < p.momentEras.size() && p.momentEras[at(o)] >= key + 1) return;
    awardMoment(pid, ownId);
    state_.players[at(pid)].momentEras[at(o)] = static_cast<int8_t>(key + 1);
}

void Game::awardOnce(PlayerId pid, const char* id) {
    const TypeIndex m = rules_->moment(id);
    const Player& p = state_.players[at(pid)];
    if (m == kNone || (at(m) < p.momentEras.size() && p.momentEras[at(m)] > 0)) return;
    awardMoment(pid, id);
}

// Circumnavigation (09: Historic moments): the civ has seen a plot in every column of a map that wraps
// east-west (Sovereign reading of the engine's test). The first civ earns the world's first.
void Game::circumnavigationMoment(PlayerId pid) {
    const Player& p = state_.players[at(pid)];
    const TypeIndex first = rules_->moment("MOMENT_WORLD_S_FIRST_CIRCUMNAVIGATION"), later = rules_->moment("MOMENT_WORLD_CIRCUMNAVIGATED");
    if (!isMajor(p) || !state_.grid.wrapX() || first == kNone || later == kNone || p.visibility.size() < static_cast<size_t>(state_.grid.size())) return;
    for (TypeIndex m : {first, later}) {
        if (at(m) < p.momentEras.size() && p.momentEras[at(m)] > 0) return;
    }
    const int w = state_.grid.width(), h = state_.grid.height();
    for (int x = 0; x < w; ++x) {
        bool seen = false;
        for (int y = 0; y < h && !seen; ++y) seen = p.visibility[static_cast<size_t>(y * w + x)] != static_cast<uint8_t>(Visibility::Unrevealed);
        if (!seen) return;
    }
    awardFirst(pid, "MOMENT_WORLD_S_FIRST_CIRCUMNAVIGATION", "MOMENT_WORLD_CIRCUMNAVIGATED");
}

// A railroad connection (09: Historic moments): the track through the plot just laid or mended reaches two
// of the civ's City Centers, on it or next to it (Sovereign reading). The first civ earns the world's first.
void Game::railroadMoment(PlayerId pid, Hex laid) {
    const Player& p = state_.players[at(pid)];
    const TypeIndex first = rules_->moment("MOMENT_FIRST_RAILROAD_CONNECTION_IN_WORLD"), own = rules_->moment("MOMENT_FIRST_RAILROAD_CONNECTION");
    const TypeIndex rr = railroad();
    if (!isMajor(p) || rr == kNone || first == kNone || own == kNone) return;
    if (state_.plot(laid).route != rr || state_.plot(laid).routePillaged) return;
    for (TypeIndex m : {first, own}) {
        if (at(m) < p.momentEras.size() && p.momentEras[at(m)] > 0) return;
    }
    std::vector<uint8_t> visited(static_cast<size_t>(state_.grid.size()), 0);
    std::vector<Hex> open{laid};
    visited[static_cast<size_t>(state_.grid.index(laid))] = 1;
    std::vector<CityId> reached;
    while (!open.empty()) {
        const Hex cur = open.back();
        open.pop_back();
        for (const Hex& n : state_.grid.within(cur, 1)) {
            const City* c = state_.cityAt(n);
            if (c && c->owner == pid && std::find(reached.begin(), reached.end(), c->id) == reached.end()) reached.push_back(c->id);
            const Plot& np = state_.plot(n);
            uint8_t& v = visited[static_cast<size_t>(state_.grid.index(n))];
            if (v || np.route != rr || np.routePillaged) continue;
            v = 1;
            open.push_back(n);
        }
    }
    if (reached.size() >= 2) awardFirst(pid, "MOMENT_FIRST_RAILROAD_CONNECTION_IN_WORLD", "MOMENT_FIRST_RAILROAD_CONNECTION");
}

// Units (09: Historic moments): a civ's unique unit, its first aircraft and its first ship.
void Game::unitMoments(PlayerId pid, TypeIndex unitType) {
    const UnitType& u = rules_->units[at(unitType)];
    if (u.uniqueTo != kNone) awardOnce(pid, "MOMENT_UNIQUE_UNIT_MARCHES");
    if (u.domain == Domain::Air) awardFirst(pid, "MOMENT_WORLD_S_FIRST_FLIGHT", "MOMENT_TAKING_FLIGHT", 0);
    if (u.domain == Domain::Sea && u.combat > 0) awardFirst(pid, "MOMENT_WORLD_S_FIRST_SEAFARING", "MOMENT_ON_THE_WAVES", 0);
}

// Buildings (09): a unique building; the first district of a type with every building ("Splendid ...", "Fully
// developed"); the Flood Barrier.
void Game::buildingMoments(City& city, TypeIndex building) {
    const BuildingType& b = rules_->buildings[at(building)];
    if (b.uniqueTo != kNone) awardOnce(city.owner, "MOMENT_UNIQUE_BUILDING_CONSTRUCTED");
    if (b.id == "BUILDING_FLOOD_BARRIER") awardMoment(city.owner, "MOMENT_COASTAL_FLOOD_MITIGATED");
    if (b.districtType == kNone || b.wonder) return;
    const TypeIndex civ = state_.players[at(city.owner)].civ;
    for (size_t i = 0; i < rules_->buildings.size(); ++i) {
        const BuildingType& o = rules_->buildings[i];
        if (o.districtType != b.districtType || o.wonder || o.faithOnly || o.granted || (o.uniqueTo != kNone && o.uniqueTo != civ)) continue;
        bool replaced = false;  // a civ's unique building stands in for the one it replaces
        for (const BuildingType& u : rules_->buildings) replaced = replaced || (u.uniqueTo == civ && civ != kNone && u.replaces == static_cast<TypeIndex>(i));
        if (replaced) continue;
        if (!city.has(static_cast<TypeIndex>(i))) return;
    }
    static const std::pair<const char*, const char*> kDeveloped[] = {
        {"DISTRICT_CAMPUS", "MOMENT_SPLENDID_CAMPUS_COMPLETED"}, {"DISTRICT_COMMERCIAL_HUB", "MOMENT_SPLENDID_COMMERCIAL_HUB_COMPLETED"},
        {"DISTRICT_HARBOR", "MOMENT_SPLENDID_HARBOR_COMPLETED"}, {"DISTRICT_HOLY_SITE", "MOMENT_SPLENDID_HOLY_SITE_COMPLETED"},
        {"DISTRICT_INDUSTRIAL_ZONE", "MOMENT_SPLENDID_INDUSTRIAL_ZONE_COMPLETED"}, {"DISTRICT_THEATER_SQUARE", "MOMENT_SPLENDID_THEATER_SQUARE_COMPLETED"},
        {"DISTRICT_AERODROME", "MOMENT_FIRST_AERODROME_FULLY_DEVELOPED"}, {"DISTRICT_ENCAMPMENT", "MOMENT_FIRST_ENCAMPMENT_FULLY_DEVELOPED"},
        {"DISTRICT_ENTERTAINMENT_COMPLEX", "MOMENT_FIRST_ENTERTAINMENT_COMPLEX_FULLY_DEVELOPED"},
        {"DISTRICT_WATER_PARK", "MOMENT_FIRST_WATER_PARK_FULLY_DEVELOPED"}};
    const std::string& district = rules_->districts[at(b.districtType)].id;
    for (const auto& [id, moment] : kDeveloped) {
        if (district == id) awardOnce(city.owner, moment);
    }
}

std::pair<int, int> Game::ageThresholds(PlayerId pid) const {
    const Player& p = state_.players[at(pid)];
    int shift = rules_->globalInt("THRESHOLD_SHIFT_PER_PAST_GOLDEN_AGE") * p.pastGoldenAges +
                rules_->globalInt("THRESHOLD_SHIFT_PER_PAST_DARK_AGE") * p.pastDarkAges;
    shift += rules_->eras[at(static_cast<TypeIndex>(std::clamp(state_.gameEra, 0, static_cast<int>(rules_->eras.size()) - 1)))].eraScoreShift;
    for (const City& c : state_.cities) shift += c.owner == pid ? rules_->globalInt("THRESHOLD_SHIFT_PER_CITY") : 0;
    shift += rules_->globalInt("ERA_SCORE_THRESHOLD_ADJUST");  // Sovereign: fewer moments are modelled yet
    return {rules_->globalInt("DARK_AGE_SCORE_BASE_THRESHOLD") + shift, rules_->globalInt("GOLDEN_AGE_SCORE_BASE_THRESHOLD") + shift};
}

void Game::processEras() {
    if (state_.gameEra + 1 >= static_cast<int>(rules_->eras.size())) return;
    const EraType& era = rules_->eras[at(static_cast<TypeIndex>(state_.gameEra))];
    const int speed = speedPercent(state_, *rules_);
    const int turns = state_.turn - state_.gameEraStart;
    const int minTurns = era.minTurns * speed / 100, maxTurns = era.maxTurns * speed / 100;
    int majors = 0, ahead = 0;
    for (const Player& p : state_.players) {
        if (!isMajor(p)) continue;
        ++majors;
        ahead += playerEra(p.id) > state_.gameEra ? 1 : 0;
    }
    // Between the era's minimum and maximum length, the world moves on once half the civs have
    // (the trigger between those bounds is engine; Sovereign's reading).
    const bool advance = majors > 0 && ((minTurns > 0 && turns >= minTurns && ahead * 2 >= majors) || (maxTurns > 0 && turns >= maxTurns));
    if (!advance) return;
    for (Player& p : state_.players) {
        if (!isMajor(p)) continue;
        const auto [dark, golden] = ageThresholds(p.id);
        const Age before = p.age;
        if (p.eraScore < dark) {
            p.age = Age::Dark;
            ++p.pastDarkAges;
        } else if (p.eraScore >= golden) {
            p.age = before == Age::Dark ? Age::Heroic : Age::Golden;
            ++p.pastGoldenAges;
        } else {
            p.age = Age::Normal;
        }
        p.eraScore = 0;
        pushEvent(EventKind::NewAge, p.id, kNoPlayer, static_cast<int>(p.age));
    }
    ++state_.gameEra;
    // Dark Age cards leave their slots once the age, or their era window, is over (09: Ages).
    for (Player& p : state_.players) {
        for (TypeIndex& pol : p.policies) {
            if (pol != kNone && rules_->policies[at(pol)].darkAge && !policyAvailable(p.id, pol)) pol = kNone;
        }
    }
    for (Player& p : state_.players) {
        if (!isMajor(p)) continue;
        // Dedications for the new era: one, or three in a Heroic Age (COMMEMORATE_*).
        p.dedications.clear();
        p.dedicationsPending = rules_->globalInt(p.age == Age::Heroic ? "COMMEMORATE_OPTIONS_MAX" : "COMMEMORATE_BASE_CHOICES_ALLOWED");
    }
    state_.gameEraStart = state_.turn;
    for (Player& p : state_.players) p.killsThisEra = 0;
    // Difficulty: AI civs at Immortal and Deity get free Eurekas and Inspirations in the new era's trees.
    const int free = difficulty().aiFreeBoosts;
    for (Player& p : state_.players) {
        if (free <= 0 || !difficultyAi(p.id)) continue;
        for (int civic = 0; civic < 2; ++civic) {
            const std::vector<TreeNode>& nodes = civic ? rules_->civics : rules_->techs;
            TreeProgress& t = civic ? p.civics : p.techs;
            int given = 0;
            for (size_t i = 0; i < nodes.size() && given < free; ++i) {
                if (nodes[i].era != state_.gameEra || i >= t.done.size() || t.done[i] || t.boosted[i]) continue;
                const int cost = civic ? civicCost(static_cast<TypeIndex>(i)) : techCost(static_cast<TypeIndex>(i));
                const int pct = nodes[i].boost.percent > 0 ? nodes[i].boost.percent : 40;
                t.boosted[i] = 1;
                t.progress[i] += Fixed::fromInt(cost) * pct / 100;
                ++given;
            }
        }
    }
}


int Game::tourismPerTurn(PlayerId pid) const {
    int total = tourismBase(pid);
    // A Cultural alliance at level 3 (08): 20% of the ally's tourism.
    for (const Player& ally : state_.players) {
        if (alliance(pid, ally.id) == AllianceType::Cultural && allianceLevel(pid, ally.id) >= 3) total += tourismBase(ally.id) / 5;
    }
    return total;
}

int Game::tourismBase(PlayerId pid) const {
    const Player& p = state_.players[at(pid)];
    if (!isMajor(p)) return 0;
    int total = 0;
    const int era = playerEra(pid);
    const bool wish = goldenDedication(pid, "DEDICATION_WISH_YOU_WERE_HERE");
    total += 2 * monopolySources(pid);  // 07: Monopolies
    // 07: resorts, improvements after Flight, National Parks; the Golden Gate Bridge (03) doubles them.
    total += (improvementTourism(pid) + parkTourism(pid)) * (holdsWonder(pid, W::GoldenGate) ? 2 : 1);
    const bool technocracy = governmentIs(pid, "GOVERNMENT_SYNTHETIC_TECHNOCRACY");
    const bool biosphere = holdsWonder(pid, W::Biosphere);
    for (const City& c : state_.cities) {
        if (c.owner != pid) continue;
        const int before = total;
        const int curator = cityGovernorHas(c, "GOVERNOR_PROMOTION_CURATOR") ? 2 : 1;  // Pingala
        for (const GreatWork& w : c.greatWorks) {
            const int pct = themed(c, w.building) ? 100 + rules_->buildings[at(w.building)].theming->tourismPercent : 100;  // 07: Theming
            // Heritage Tourism doubles art and artifacts; Satellite Broadcasts triples music (04).
            const std::string& kind = rules_->greatWorkTypes[at(w.type)].id;
            int scale = 1;
            if (policyIs(pid, "POLICY_HERITAGE_TOURISM") && (kind == "SCULPTURE" || kind == "PORTRAIT" || kind == "LANDSCAPE" || kind == "RELIGIOUS" || kind == "ARTIFACT")) scale = 2;
            if (policyIs(pid, "POLICY_SATELLITE_BROADCASTS") && kind == "MUSIC") scale = 3;
            // Mary Leakey (07): artifacts triple their tourism.
            if (kind == "ARTIFACT") scale = std::max(scale, greatPersonEffectTotal(pid, GreatPersonEffectKind::ArtifactTourism) / 100);
            if (kind == "RELIC" && cityFollows(c, Bf::Reliquaries)) scale *= 3;  // Reliquaries (06)
            // Heritage Organization (World Congress): option A doubles the kind's tourism, B silences it.
            if (resolutionHits(ResolutionKind::HeritageOrganization, 0, w.type)) scale *= 2;
            if (resolutionHits(ResolutionKind::HeritageOrganization, 1, w.type)) scale = 0;
            total += rules_->greatWorkTypes[at(w.type)].tourism * curator * pct / 100 * scale;
        }
        for (TypeIndex b : c.buildings) {
            const BuildingType& bt = rules_->buildings[at(b)];
            if (!bt.wonder) continue;
            const int wonderEra = bt.unlock.none() ? 0 : (bt.unlock.civic ? rules_->civics : rules_->techs)[at(bt.unlock.index)].era;
            total += rules_->globalInt("TOURISM_BASE_FROM_WONDER") + rules_->globalInt("TOURISM_ADVANCED_ERA_WONDER") * std::max(0, era - wonderEra);
        }
        total += religiousTourism(c);
        if (biosphere) total += renewablePower(c) * 3;  // the Biosphère (03): tourism from its tripled renewable Power
        for (const CityDistrict& d : c.districts) {
            if (d.complete && d.pillagedTurns == 0) total += districtTourism(state_, *rules_, p, d.type);  // Masaru Ibuka, Jamsetji Tata (07)
        }
        total += static_cast<int>(sumCityModifiers(state_, *rules_, c, ModEffect::CityTourism).toInt());  // Shopping Mall, Ferris Wheel
        // Kenzo Tange (07): tourism from the city's districts' adjacency (Culture, Production and Science in full, Faith and Gold at half).
        if (const int tange = c.greatPeopleHere.empty() ? 0 : usedHere(c, Gp::KenzoTange); tange > 0) {
            for (const CityDistrict& d : c.districts) {
                if (!d.complete) continue;
                const Yields adj = districtAdjacency(pid, d.type, d.pos);
                const Fixed t = adj[static_cast<size_t>(YieldType::Culture)] + adj[static_cast<size_t>(YieldType::Production)] + adj[static_cast<size_t>(YieldType::Science)] +
                                (adj[static_cast<size_t>(YieldType::Faith)] + adj[static_cast<size_t>(YieldType::Gold)]) / 2;
                total += tange * static_cast<int>(t.toInt());
            }
        }
        // Wish You Were Here (Golden Age): +50% tourism from cities with an established governor.
        PlayerId holder = kNoPlayer;
        if (wish && establishedGovernor(c, &holder) && holder == pid) total += (total - before) / 2;
    }
    return technocracy ? total * 90 / 100 : total;  // 04: Synthetic Technocracy, -10% Tourism
}

// Religious tourism (07): the Holy City of the religion its owner founded. St. Basil's Cathedral doubles its
// city's (03).
int Game::religiousTourism(const City& city) const {
    const Player& p = state_.players[at(city.owner)];
    if (p.religion < 0 || static_cast<size_t>(p.religion) >= state_.religions.size() || state_.religions[at(p.religion)].holyCity != city.id) return 0;
    const TypeIndex basil = rules_->building("BUILDING_ST_BASIL_S_CATHEDRAL");
    return rules_->globalInt("TOURISM_FROM_HOLY_CITY") * (basil != kNone && city.has(basil) ? 2 : 1);
}

void Game::processTourism(PlayerId pid) {
    Player& p = state_.players[at(pid)];
    if (!isMajor(p)) return;
    // Meeting a civ (seeing one of its cities or units) is a moment (09: Met New Civilization).
    if (p.met.size() < state_.players.size()) p.met.resize(state_.players.size(), 0);
    for (const Player& x : state_.players) {
        if (x.id == pid || !isMajor(x) || state_.players[at(pid)].met[at(x.id)]) continue;
        bool seen = false;
        for (const City& c : state_.cities) seen = seen || (c.owner == x.id && visibility(pid, c.pos) == Visibility::Visible);
        for (const Unit& u : state_.units) seen = seen || (u.owner == x.id && visibility(pid, u.pos) == Visibility::Visible);
        if (!seen) continue;
        state_.players[at(pid)].met[at(x.id)] = 1;
        awardMoment(pid, "MOMENT_MET_NEW_CIVILIZATION");
        bool all = true;
        for (const Player& y : state_.players) all = all && (y.id == pid || !isMajor(y) || state_.players[at(pid)].met[at(y.id)]);
        if (all) awardFirst(pid, "MOMENT_WORLD_S_FIRST_TO_MEET_ALL_CIVILIZATIONS", "MOMENT_MET_ALL_CIVILIZATIONS", 0);
    }
    // The world's largest civilization: three cities more than any other (Sovereign reading).
    {
        int mine = 0, other = 0;
        std::vector<int> counts(state_.players.size(), 0);
        for (const City& c : state_.cities) ++counts[at(c.owner)];
        for (const Player& x : state_.players) {
            if (!isMajor(x)) continue;
            if (x.id == pid) mine = counts[at(x.id)];
            else other = std::max(other, counts[at(x.id)]);
        }
        if (mine >= 6 && mine >= other + 3) awardOnce(pid, "MOMENT_WORLD_S_LARGEST_CIVILIZATION");
    }
    if (p.tourismTo.size() < state_.players.size()) p.tourismTo.resize(state_.players.size(), 0);
    const int t = tourismPerTurn(pid);
    if (t <= 0) return;
    // Religious tourism (06): halved after The Enlightenment (never with Cristo Redentor, 03), and halved again toward
    // a civ whose cities mostly follow another religion (TOURISM_DIFFERENT_RELIGION_REDUCTION).
    int religious = 0;
    if (p.religion >= 0 && static_cast<size_t>(p.religion) < state_.religions.size()) {
        const City* holy = state_.city(state_.religions[static_cast<size_t>(p.religion)].holyCity);
        if (holy && holy->owner == pid) religious = religiousTourism(*holy);
    }
    const TypeIndex enlightenment = rules_->civic("CIVIC_THE_ENLIGHTENMENT");
    const int religiousLost = enlightenment != kNone && p.civics.has(enlightenment) && !holdsWonder(pid, W::Cristo) ? religious / 2 : 0;
    auto mainReligion = [&](PlayerId who) {
        std::vector<int> n(state_.religions.size(), 0);
        for (const City& c : state_.cities) {
            const int maj = c.owner == who ? cityMajorityReligion(c) : -1;
            if (maj >= 0) ++n[static_cast<size_t>(maj)];
        }
        const auto best = std::max_element(n.begin(), n.end());
        return best == n.end() || *best == 0 ? -1 : static_cast<int>(best - n.begin());
    };
    for (const Player& x : state_.players) {
        if (x.id == pid || !isMajor(x)) continue;
        int base = t - religiousLost;
        if (religious > 0 && mainReligion(x.id) != p.religion)
            base -= (religious - religiousLost) * rules_->globalInt("TOURISM_DIFFERENT_RELIGION_REDUCTION") / 100;
        // +25% toward a civ we run a trade route to (TOURISM_TRADE_ROUTE_BONUS).
        const bool route = std::any_of(state_.tradeRoutes.begin(), state_.tradeRoutes.end(), [&](const TradeRoute& r) {
            const City* d = state_.city(r.destination);
            return r.owner == pid && d && d->owner == x.id;
        });
        // +25% toward a civ that opens its borders to us (08: Open Borders).
        const int borders = grantsOpenBorders(x.id, pid) ? 25 : 0;
        // Online Communities (04): +50% more toward civs we run a route to. Space Tourism: theirs shields them by 20%.
        const int online = route ? (policyIs(pid, "POLICY_ONLINE_COMMUNITIES") ? 50 : 0) +
                                       static_cast<int>(sumPlayerModifiers(state_, *rules_, p, ModEffect::RouteTourismPercent).toInt())
                                 : 0;  // + Sarah Breedlove, Melitta Bentz (07)
        int toward = std::max(0, base) * (100 + (route ? rules_->globalInt("TOURISM_TRADE_ROUTE_BONUS") : 0) + borders + online) / 100;
        if (policyIs(x.id, "POLICY_SPACE_TOURISM")) toward = toward * 80 / 100;
        p.tourismTo[at(x.id)] += toward;
    }
}

int Game::visitingTourists(PlayerId pid, PlayerId from) const {
    const Player& p = state_.players[at(pid)];
    const int majors = std::max(1, state_.majorsAtStart);
    const int per = touristTourism_ * majors;
    return at(from) < p.tourismTo.size() ? p.tourismTo[at(from)] / per : 0;
}

int Game::visitingTourists(PlayerId pid) const {
    int total = 0;
    for (const Player& x : state_.players) {
        if (x.id != pid && isMajor(x)) total += visitingTourists(pid, x.id);
    }
    return total;
}

int Game::domesticTourists(PlayerId pid) const {
    int n = static_cast<int>((state_.players[at(pid)].lifetimeCulture / touristCulture_).toInt());
    for (const Player& x : state_.players) {
        if (x.id != pid && isMajor(x)) n -= visitingTourists(x.id, pid);  // our people visiting them
    }
    return std::max(0, n);
}

PlayerId Game::cultureVictor() const {
    // Sovereign floor: at least kMinTouristsPerRival visiting tourists per rival major as well. With few
    // civs one early tourist (a tribal village's relic) otherwise wins, as each tourist both counts for
    // the host and comes off the rival's domestic tourists (07: Tourism).
    constexpr int kMinTouristsPerRival = 5;
    for (const Player& p : state_.players) {
        if (!isMajor(p)) continue;
        const int visitors = visitingTourists(p.id);
        if (visitors <= 0) continue;
        bool all = true;
        int rivals = 0;
        for (const Player& x : state_.players) {
            if (x.id == p.id || !isMajor(x)) continue;
            ++rivals;
            if (visitors <= domesticTourists(x.id)) all = false;
        }
        if (all && rivals > 0 && visitors >= kMinTouristsPerRival * rivals) return p.id;
    }
    return kNoPlayer;
}

}  // namespace sov
