#include "sovereign/modifiers.h"

#include <algorithm>

#include "sovereign/mapgen.h"

namespace sov {

namespace {
}  // namespace

int envoysAt(const GameState& s, const Rules& r, PlayerId player, PlayerId cs) {
    const Player& p = s.players[static_cast<size_t>(player)];
    int n = static_cast<size_t>(cs) < p.envoys.size() ? p.envoys[static_cast<size_t>(cs)] : 0;
    // Amani serving there counts as envoys (08: Governors, Messenger and Puppeteer).
    for (const Governor& g : p.governors) {
        const City* c = s.city(g.city);
        if (!c || c->owner != cs || g.establishTurns > 0) continue;
        const bool puppeteer = std::any_of(g.promotions.begin(), g.promotions.end(), [&](TypeIndex pr) {
            return r.governorPromotions[static_cast<size_t>(pr)].id == "GOVERNOR_PROMOTION_PUPPETEER";
        });
        n += puppeteer ? 4 : 2;
        break;
    }
    return n;
}

PlayerId suzerainOf(const GameState& s, const Rules& r, PlayerId cs) {
    // The most envoys, at least INFLUENCE_TOKENS_MINIMUM_FOR_SUZERAIN, and more than anyone else.
    PlayerId best = kNoPlayer;
    int most = 0;
    bool tie = false;
    for (const Player& p : s.players) {
        const int n = envoysAt(s, r, p.id, cs);
        if (n > most) {
            most = n;
            best = p.id;
            tie = false;
        } else if (n == most && n > 0) {
            tie = true;
        }
    }
    return !tie && most >= r.globalInt("INFLUENCE_TOKENS_MINIMUM_FOR_SUZERAIN") ? best : kNoPlayer;
}

bool enjoysSuzerainBonus(const GameState& s, const Rules& r, PlayerId player, TypeIndex type) {
    for (const Player& cs : s.players) {
        if (cs.cityState != type || !cs.alive) continue;
        const auto& rels = s.players[static_cast<size_t>(player)].relations;
        if (static_cast<size_t>(cs.id) < rels.size() && rels[static_cast<size_t>(cs.id)].war) return false;
        const PlayerId suz = suzerainOf(s, r, cs.id);
        if (suz == player) return true;
        // A level-3 Economic alliance shares the ally's suzerain bonuses (08: alliance levels).
        if (suz == kNoPlayer || static_cast<size_t>(suz) >= rels.size()) return false;
        const Relation& rel = rels[static_cast<size_t>(suz)];
        return rel.alliance == AllianceType::Economic && rel.allianceUntil >= s.turn && rel.alliancePoints >= r.globalInt("ALLIANCE_LEVEL_THREE_XP");
    }
    return false;
}

namespace {

bool testOne(const Requirement& q, const ReqContext& c) {
    bool ok = false;
    switch (q.type) {
        case ReqType::PlotHasResource: ok = c.plot && (q.ref == kNone ? c.plot->resource != kNone : c.plot->resource == q.ref); break;  // no ref: any
        case ReqType::PlotHasFeature: ok = c.plot && (q.ref == kNone ? c.plot->feature != kNone : c.plot->feature == q.ref); break;
        case ReqType::PlotHasTerrain: ok = c.plot && c.plot->terrain == q.ref; break;
        case ReqType::PlotHasImprovement: ok = c.plot && (q.ref == kNone ? c.plot->improvement != kNone : c.plot->improvement == q.ref); break;
        case ReqType::PlotNextToRiver: {
            // The plot's index in the grid from its address in the plot array.
            ok = false;
            if (c.plot && c.state && c.plot >= c.state->plots.data() && c.plot < c.state->plots.data() + c.state->plots.size())
                ok = isRiverAdjacent(*c.state, c.state->grid.at(static_cast<int32_t>(c.plot - c.state->plots.data())));
            break;
        }
        case ReqType::CityHasBuilding: ok = c.city && (c.rules ? cityHasBuilding(*c.city, *c.rules, q.ref) : c.city->has(q.ref)); break;
        case ReqType::CityIsCapital: ok = c.city && c.city->capital; break;
        case ReqType::CityHasGarrison: {
            const Unit* g = c.city && c.state && c.rules ? c.state->unitAt(c.city->pos, UnitLayer::Military, *c.rules) : nullptr;
            ok = g && g->owner == c.city->owner;
            break;
        }
        case ReqType::CityHasGovernor:
            ok = c.city && c.player && std::any_of(c.player->governors.begin(), c.player->governors.end(), [&](const Governor& g) {
                     return g.city == c.city->id && g.establishTurns == 0 && static_cast<int>(g.promotions.size()) >= q.value;
                 });
            break;
        case ReqType::CityMinSpecialtyDistricts: {
            int n = 0;
            if (c.city && c.rules) {
                for (const CityDistrict& d : c.city->districts) n += d.complete && c.rules->districts[static_cast<size_t>(d.type)].needsPopulation ? 1 : 0;
            }
            ok = c.city && n >= q.value;
            break;
        }
        case ReqType::CityCaptured: ok = c.city && c.city->originalOwner != c.city->owner; break;
        case ReqType::CityHasImprovedResource:
            if (c.city && c.state) {
                for (size_t i = 0; i < c.state->plots.size() && !ok; ++i) {
                    const Plot& p = c.state->plots[i];
                    ok = p.city == c.city->id && p.resource == q.ref && p.improvement != kNone && p.pillagedTurns == 0;
                }
            }
            break;
        case ReqType::PlayerAtPeace:
            ok = c.player && c.state;
            for (size_t i = 0; ok && i < c.player->relations.size() && i < c.state->players.size(); ++i) {
                const Player& o = c.state->players[i];
                ok = !(c.player->relations[i].war && o.cityState == kNone && !o.barbarian);
            }
            break;
        case ReqType::WorldMinEra: ok = c.state && c.state->gameEra >= q.value; break;
        case ReqType::CityOnCapitalContinent: {
            ok = false;
            if (c.city && c.state) {
                for (const City& cap : c.state->cities) {
                    if (cap.owner == c.city->owner && cap.capital) ok = c.state->plot(cap.pos).continent == c.state->plot(c.city->pos).continent;
                }
            }
            break;
        }
        case ReqType::CityHasDistrict:
            ok = c.city && std::any_of(c.city->districts.begin(), c.city->districts.end(), [&](const CityDistrict& d) { return d.complete && d.type == q.ref; });
            break;
        case ReqType::CityMinPopulation: ok = c.city && c.city->population >= q.value; break;
        case ReqType::PlayerIsHuman: ok = c.player && c.player->human; break;
    }
    return q.negate ? !ok : ok;
}

bool isPlotCollection(ModCollection c) {
    return c == ModCollection::OwnerCityPlots || c == ModCollection::PlayerCityPlots;
}

// Whether a policy or government source is in force for this player. Anarchy
// switches both off (04-tech-civics-government.md, Governments).
bool playerHasSource(const Modifier& m, const Player& owner) {
    if (owner.anarchyTurns > 0) return false;
    if (m.sourceKind == ModSource::Government) return owner.government == m.sourceIndex;
    return std::find(owner.policies.begin(), owner.policies.end(), m.sourceIndex) != owner.policies.end();
}

// The city that "holds" a modifier for this subject city, or nullptr if the
// modifier does not reach it. For player-wide sources the subject's player
// must carry the source.
const City* holderFor(const Modifier& m, const GameState& s, const Rules& r, const City& subject, const Player& owner) {
    const bool ownerOnly = m.collection == ModCollection::OwnerCity || m.collection == ModCollection::OwnerCityPlots;
    if (m.collection == ModCollection::PlayerCapital && !subject.capital) return nullptr;
    switch (m.sourceKind) {
        case ModSource::Building:
            // A civ's unique building carries the modifiers of the building it replaces.
            if (ownerOnly) return cityHasBuilding(subject, r, m.sourceIndex) ? &subject : nullptr;
            for (const City& c : s.cities) {
                if (c.owner == owner.id && cityHasBuilding(c, r, m.sourceIndex)) return &c;
            }
            return nullptr;
        case ModSource::Civ:
            return owner.civ == m.sourceIndex ? &subject : nullptr;
        case ModSource::Everyone:
            return &subject;
        case ModSource::Policy:
            return playerHasSource(m, owner) ? &subject : nullptr;
        case ModSource::Government:
            return playerHasSource(m, owner) ? &subject : nullptr;
        case ModSource::Belief: {
            // A city follows the beliefs of its majority religion, or its owner's pantheon while it has none.
            const int maj = majorityReligion(s, r, subject);
            if (maj >= 0) return religionHas(s, maj, m.sourceIndex) ? &subject : nullptr;
            return owner.pantheon == m.sourceIndex ? &subject : nullptr;
        }
        case ModSource::GreatPerson: {
            // Its city effects where it was used; its player effects once its owner has used it (07).
            if (m.collection == ModCollection::OwnerCity || m.collection == ModCollection::OwnerCityPlots)
                return std::find(subject.greatPeopleHere.begin(), subject.greatPeopleHere.end(), m.sourceIndex) != subject.greatPeopleHere.end() ? &subject : nullptr;
            return std::find(owner.greatPeopleActivated.begin(), owner.greatPeopleActivated.end(), m.sourceIndex) != owner.greatPeopleActivated.end() ? &subject : nullptr;
        }
        case ModSource::CityState:
            return enjoysSuzerainBonus(s, r, owner.id, m.sourceIndex) ? &subject : nullptr;
        case ModSource::Governor:
            // The owner's governor established in this city holds the promotion (08: Governors).
            for (const Governor& g : owner.governors) {
                if (g.city != subject.id || g.establishTurns > 0) continue;
                return std::find(g.promotions.begin(), g.promotions.end(), m.sourceIndex) != g.promotions.end() ? &subject : nullptr;
            }
            return nullptr;
    }
    return nullptr;
}

template <typename Fn>
void forEachApplying(const GameState& s, const Rules& r, const City& city, bool plotEffect, const Plot* plot,
                     Fn&& fn) {
    const Player& owner = s.players[static_cast<size_t>(city.owner)];
    for (const Modifier& m : r.modifiers) {
        if (m.collection == ModCollection::Player) continue;
        if (isPlotCollection(m.collection) != plotEffect) continue;
        const City* holder = holderFor(m, s, r, city, owner);
        if (!holder) continue;
        ReqContext ownerCtx{&s, &r, &owner, holder, nullptr};
        if (!testRequirements(m.ownerReqs, ownerCtx)) continue;
        ReqContext subjectCtx{&s, &r, &owner, &city, plot};
        if (!testRequirements(m.subjectReqs, subjectCtx)) continue;
        fn(m);
    }
}
}  // namespace

int religionFollowers(const GameState& s, const Rules& r, const City& city, int religion) {
    if (religion < 0 || static_cast<size_t>(religion) >= city.pressure.size() || city.population <= 0) return 0;
    int64_t total = static_cast<int64_t>(r.globalInt("RELIGION_SPREAD_ATHEISM_PRESSURE_PER_POP")) * city.population;
    for (int32_t p : city.pressure) total += std::max<int32_t>(0, p);
    const int64_t mine = std::max<int32_t>(0, city.pressure[static_cast<size_t>(religion)]);
    (void)s;
    return total <= 0 ? 0 : static_cast<int>((mine * city.population * 2 + total) / (total * 2));  // rounded
}

int majorityReligion(const GameState& s, const Rules& r, const City& city) {
    for (size_t i = 0; i < city.pressure.size(); ++i) {
        if (religionFollowers(s, r, city, static_cast<int>(i)) * 2 > city.population) return static_cast<int>(i);
    }
    return -1;
}

bool religionHas(const GameState& s, int religion, TypeIndex belief) {
    if (religion < 0 || static_cast<size_t>(religion) >= s.religions.size()) return false;
    const std::vector<TypeIndex>& b = s.religions[static_cast<size_t>(religion)].beliefs;
    return std::find(b.begin(), b.end(), belief) != b.end();
}

bool testRequirements(const RequirementSet& set, const ReqContext& ctx) {
    if (set.reqs.empty()) return true;
    for (const Requirement& q : set.reqs) {
        bool ok = testOne(q, ctx);
        if (set.any && ok) return true;
        if (!set.any && !ok) return false;
    }
    return !set.any;
}

Fixed sumCityModifiers(const GameState& s, const Rules& r, const City& city, ModEffect effect,
                       std::optional<YieldType> yield) {
    Fixed total;
    forEachApplying(s, r, city, false, nullptr, [&](const Modifier& m) {
        if (m.effect != effect) return;
        if (yield && m.yield != *yield) return;
        total += m.amount;
    });
    return total;
}

Fixed sumPlotModifiers(const GameState& s, const Rules& r, const City& city, Hex plot, YieldType yield) {
    Fixed total;
    const Plot& p = s.plot(plot);
    forEachApplying(s, r, city, true, &p, [&](const Modifier& m) {
        if (m.effect == ModEffect::PlotYield && m.yield == yield) total += m.amount;
    });
    return total;
}

Fixed sumUnitProductionPercent(const GameState& s, const Rules& r, const City& city, TypeIndex unitType) {
    const UnitType& u = r.units[static_cast<size_t>(unitType)];
    Fixed total;
    forEachApplying(s, r, city, false, nullptr, [&](const Modifier& m) {
        if (m.effect != ModEffect::UnitProductionPercent) return;
        if (!m.unitClass.empty() && m.unitClass != u.unitClass) return;
        if (m.unit != kNone && m.unit != unitType) return;
        if (m.maxEra >= 0 && u.era > m.maxEra) return;
        if (m.minEra >= 0 && u.era < m.minEra) return;
        if (m.military && u.layer != UnitLayer::Military) return;
        total += m.amount;
    });
    return total;
}

namespace {
// Calls fn for every player-collection modifier with this effect that applies to the player.
template <typename Fn>
void forEachPlayerModifier(const GameState& s, const Rules& r, const Player& player, ModEffect effect, Fn&& fn) {
    for (const Modifier& m : r.modifiers) {
        if (m.collection != ModCollection::Player || m.effect != effect) continue;
        const City* holder = nullptr;
        bool applies = false;
        switch (m.sourceKind) {
            case ModSource::Building:
                for (const City& c : s.cities) {
                    if (c.owner == player.id && c.has(m.sourceIndex)) {
                        holder = &c;
                        break;
                    }
                }
                applies = holder != nullptr;
                break;
            case ModSource::Civ: applies = player.civ == m.sourceIndex; break;
            case ModSource::Everyone: applies = true; break;
            case ModSource::Policy:
            case ModSource::Government: applies = playerHasSource(m, player); break;
            case ModSource::Governor: applies = false; break;  // city effects only
            case ModSource::GreatPerson:
                applies = std::find(player.greatPeopleActivated.begin(), player.greatPeopleActivated.end(), m.sourceIndex) != player.greatPeopleActivated.end();
                break;
            case ModSource::CityState: applies = enjoysSuzerainBonus(s, r, player.id, m.sourceIndex); break;
            case ModSource::Belief:
                applies = player.pantheon == m.sourceIndex || (player.religion >= 0 && religionHas(s, player.religion, m.sourceIndex));
                break;
        }
        if (!applies) continue;
        ReqContext ownerCtx{&s, &r, &player, holder, nullptr};
        ReqContext subjectCtx{&s, &r, &player, nullptr, nullptr};
        if (!testRequirements(m.ownerReqs, ownerCtx) || !testRequirements(m.subjectReqs, subjectCtx)) continue;
        fn(m);
    }
}
}  // namespace

Fixed sumPlayerModifiers(const GameState& s, const Rules& r, const Player& player, ModEffect effect) {
    Fixed total;
    forEachPlayerModifier(s, r, player, effect, [&](const Modifier& m) { total += m.amount; });
    return total;
}

std::vector<TypeIndex> grantedAbilities(const GameState& s, const Rules& r, const Player& player) {
    std::vector<TypeIndex> out;
    forEachPlayerModifier(s, r, player, ModEffect::GrantAbility, [&](const Modifier& m) { out.push_back(m.ability); });
    return out;
}

int sumUnitStrength(const GameState& s, const Rules& r, const Player& player, const std::string& unitClass,
                    bool vsBarbarian) {
    Fixed total;
    forEachPlayerModifier(s, r, player, ModEffect::UnitStrength, [&](const Modifier& m) {
        if ((m.unitClass.empty() || m.unitClass == unitClass) && (!m.vsBarbarians || vsBarbarian)) total += m.amount;
    });
    return static_cast<int>(total.toInt());
}

int sumDistrictAdjacencyPercent(const GameState& s, const Rules& r, const Player& player, TypeIndex district) {
    Fixed total;
    forEachPlayerModifier(s, r, player, ModEffect::DistrictAdjacencyPercent, [&](const Modifier& m) {
        if (m.district == district) total += m.amount;
    });
    return static_cast<int>(total.toInt());
}

Fixed sumItemProductionPercent(const GameState& s, const Rules& r, const City& city, ProductionItem item) {
    Fixed total;
    forEachApplying(s, r, city, false, nullptr, [&](const Modifier& m) {
        if (m.effect != ModEffect::ItemProductionPercent) return;
        bool hit = false;
        if (item.kind == ProductionKind::Building && item.type >= 0 && static_cast<size_t>(item.type) < r.buildings.size()) {
            const BuildingType& b = r.buildings[static_cast<size_t>(item.type)];
            if (m.scope == "BUILDING") hit = m.building == item.type;
            if (m.scope == "DISTRICT_BUILDINGS") hit = m.district != kNone && !b.wonder && b.district == r.districts[static_cast<size_t>(m.district)].id;
            if (m.scope == "WONDERS" && b.wonder) {
                const int era = b.unlock.none() ? 0 : (b.unlock.civic ? r.civics : r.techs)[static_cast<size_t>(b.unlock.index)].era;
                hit = (m.minEra < 0 || era >= m.minEra) && (m.maxEra < 0 || era <= m.maxEra);
            }
        } else if (item.kind == ProductionKind::District) {
            hit = m.scope == "DISTRICT" && m.district == item.type;
        } else if (item.kind == ProductionKind::Project && item.type >= 0 && static_cast<size_t>(item.type) < r.projects.size()) {
            hit = (m.scope == "SPACE_RACE" && r.projects[static_cast<size_t>(item.type)].spaceRace) || m.scope == "PROJECTS";
        }
        if (hit) total += m.amount;
    });
    return total;
}

Fixed sumCityGreatPersonPoints(const GameState& s, const Rules& r, const City& city, TypeIndex gpClass) {
    Fixed total;
    forEachApplying(s, r, city, false, nullptr, [&](const Modifier& m) {
        if (m.effect == ModEffect::CityGreatPersonPoints && m.gpClass == gpClass) total += m.amount;
    });
    return total;
}

Fixed sumPlayerGreatPersonPoints(const GameState& s, const Rules& r, const Player& player, TypeIndex gpClass) {
    Fixed total;
    forEachPlayerModifier(s, r, player, ModEffect::GreatPersonPoints, [&](const Modifier& m) {
        if (m.gpClass == gpClass) total += m.amount;
    });
    return total;
}

Yields tradeRouteModifierYields(const GameState& s, const Rules& r, const Player& owner, bool domestic, bool ally, bool cityState,
                                bool suzerain, bool toDestination) {
    Yields out{};
    forEachPlayerModifier(s, r, owner, ModEffect::TradeRouteYield, [&](const Modifier& m) {
        if (m.toDestination != toDestination) return;
        const bool hit = m.scope == "ALL" || (m.scope == "DOMESTIC" && domestic) || (m.scope == "INTERNATIONAL" && !domestic) ||
                         (m.scope == "ALLY" && ally) || (m.scope == "CITY_STATE" && cityState) || (m.scope == "SUZERAIN" && suzerain);
        if (hit) out[static_cast<size_t>(m.yield)] += m.amount;
    });
    return out;
}

int districtTourism(const GameState& s, const Rules& r, const Player& player, TypeIndex district) {
    Fixed total;
    forEachPlayerModifier(s, r, player, ModEffect::DistrictTourism, [&](const Modifier& m) {
        if (m.district == district) total += m.amount;
    });
    return static_cast<int>(total.toInt());
}

Fixed sumUnitXpPercent(const GameState& s, const Rules& r, const Player& player, const std::string& unitClass) {
    Fixed total;
    forEachPlayerModifier(s, r, player, ModEffect::UnitXpPercent, [&](const Modifier& m) {
        if (m.unitClass.empty() || m.unitClass == unitClass) total += m.amount;
    });
    return total;
}

}  // namespace sov
