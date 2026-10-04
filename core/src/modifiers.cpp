#include "sovereign/modifiers.h"

#include <algorithm>

namespace sov {

namespace {
bool testOne(const Requirement& q, const ReqContext& c) {
    bool ok = false;
    switch (q.type) {
        case ReqType::PlotHasResource: ok = c.plot && c.plot->resource == q.ref; break;
        case ReqType::PlotHasFeature: ok = c.plot && c.plot->feature == q.ref; break;
        case ReqType::PlotHasTerrain: ok = c.plot && c.plot->terrain == q.ref; break;
        case ReqType::CityHasBuilding: ok = c.city && c.city->has(q.ref); break;
        case ReqType::CityIsCapital: ok = c.city && c.city->capital; break;
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
const City* holderFor(const Modifier& m, const GameState& s, const City& subject, const Player& owner) {
    const bool ownerOnly = m.collection == ModCollection::OwnerCity || m.collection == ModCollection::OwnerCityPlots;
    if (m.collection == ModCollection::PlayerCapital && !subject.capital) return nullptr;
    switch (m.sourceKind) {
        case ModSource::Building:
            if (ownerOnly) return subject.has(m.sourceIndex) ? &subject : nullptr;
            for (const City& c : s.cities) {
                if (c.owner == owner.id && c.has(m.sourceIndex)) return &c;
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
        const City* holder = holderFor(m, s, city, owner);
        if (!holder) continue;
        ReqContext ownerCtx{&s, &r, &owner, holder, nullptr};
        if (!testRequirements(m.ownerReqs, ownerCtx)) continue;
        ReqContext subjectCtx{&s, &r, &owner, &city, plot};
        if (!testRequirements(m.subjectReqs, subjectCtx)) continue;
        fn(m);
    }
}
}  // namespace

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
        total += m.amount;
    });
    return total;
}

Fixed sumPlayerModifiers(const GameState& s, const Rules& r, const Player& player, ModEffect effect) {
    Fixed total;
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
        }
        if (!applies) continue;
        ReqContext ownerCtx{&s, &r, &player, holder, nullptr};
        ReqContext subjectCtx{&s, &r, &player, nullptr, nullptr};
        if (!testRequirements(m.ownerReqs, ownerCtx) || !testRequirements(m.subjectReqs, subjectCtx)) continue;
        total += m.amount;
    }
    return total;
}

}  // namespace sov
