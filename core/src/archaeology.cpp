// Archaeology (07-great-people-great-works-tourism.md, Archaeology; data: ARCHAEOLOGY_*). While the
// world era is at most ARCHAEOLOGY_MAX_ERA, fought-over plots are remembered. Once any civ knows Natural
// History, ARCHAEOLOGY_SITES_PER_CIV_LAND antiquity sites and ARCHAEOLOGY_SITES_PER_CIV_SEA shipwrecks per
// major civ appear: on those battle plots first, then on other open plots (Sovereign reading of "similar
// history"). Only civs that know Natural History see them. An Archaeologist (three excavations) on a
// site in its civ's land, unowned land, or land whose owner opened its borders to it digs an Artifact
// into a free artifact slot (an Archaeological Museum's) of one of its cities.
#include <algorithm>

#include "sovereign/game.h"
#include "sovereign/mapgen.h"

namespace sov {

namespace {
size_t at(int i) { return static_cast<size_t>(i); }
}  // namespace

bool Game::seesAntiquity(PlayerId player) const {
    const TypeIndex nh = rules_->civic("CIVIC_NATURAL_HISTORY");
    return nh != kNone && player >= 0 && state_.players[at(player)].civics.has(nh);
}

void Game::noteBattle(Hex plot, PlayerId attacker) {
    if (state_.antiquityPlaced || state_.gameEra > rules_->globalInt("ARCHAEOLOGY_MAX_ERA") || state_.battleSites.size() >= 400) return;
    const int32_t i = state_.grid.index(plot);
    if (std::find(state_.battleSites.begin(), state_.battleSites.end(), i) != state_.battleSites.end()) return;
    state_.battleSites.push_back(i);
    // The Artifact found here later belongs to the attacker's civilization and the era of the fight.
    const TypeIndex civ = attacker >= 0 ? state_.players[at(attacker)].civ : kNone;
    state_.battleHistory.push_back(state_.gameEra * 4096 + civ + 1);
}

// Theming (07: Theming bonuses; data: buildings' theming flags): every slot of the building full, and its
// works by different people and of one object type (Art Museum), or of one era and from different
// civilizations (Archaeological Museum).
bool Game::themed(const City& city, TypeIndex building) const {
    const BuildingType& b = rules_->buildings[at(building)];
    if (!b.theming) return false;
    int slots = 0;
    for (const auto& [slot, n] : b.greatWorkSlots) slots += n;
    std::vector<const GreatWork*> works;
    for (const GreatWork& w : city.greatWorks) {
        if (w.building == building) works.push_back(&w);
    }
    if (slots <= 1 || static_cast<int>(works.size()) < slots) return false;
    for (size_t i = 0; i < works.size(); ++i) {
        for (size_t k = i + 1; k < works.size(); ++k) {
            const GreatWork& x = *works[i];
            const GreatWork& y = *works[k];
            if (b.theming->sameObject && x.type != y.type) return false;
            if (b.theming->uniquePerson && (x.creator == kNone || x.creator == y.creator)) return false;
            if (b.theming->sameEra && (x.era < 0 || x.era != y.era)) return false;
            if (b.theming->uniqueCivs && (x.civ == kNone || x.civ == y.civ)) return false;
        }
    }
    return true;
}

void Game::placeAntiquity() {
    if (state_.antiquityPlaced) return;
    bool known = false;
    int majors = 0;
    for (const Player& p : state_.players) {
        if (!isMajorCiv(p.id)) continue;
        ++majors;
        known = known || seesAntiquity(p.id);
    }
    if (!known) return;
    state_.antiquityPlaced = true;
    const auto open = [&](Hex h, bool sea) {
        const Plot& p = state_.plot(h);
        const TerrainType& t = rules_->terrains[at(p.terrain)];
        if (state_.cityAt(h) || state_.districtAt(h) || state_.wonderAt(h) != kNone || p.village || p.antiquity) return false;
        if (p.feature != kNone && rules_->features[at(p.feature)].naturalWonder) return false;
        return sea ? t.water : isLandPassable(state_, *rules_, h);
    };
    Rng& rng = state_.rng.get(RngStream::Gameplay);
    for (int sea = 0; sea < 2; ++sea) {
        int left = majors * rules_->globalInt(sea ? "ARCHAEOLOGY_SITES_PER_CIV_SEA" : "ARCHAEOLOGY_SITES_PER_CIV_LAND");
        for (int32_t i : state_.battleSites) {
            if (left <= 0) break;
            const Hex h = state_.grid.at(i);
            if (!open(h, sea != 0)) continue;
            state_.plot(h).antiquity = static_cast<uint8_t>(sea ? 2 : 1);
            --left;
        }
        for (int tries = 0; tries < 4000 && left > 0; ++tries) {
            const Hex h = state_.grid.at(static_cast<int>(rng.below(static_cast<uint32_t>(state_.grid.size()))));
            if (!open(h, sea != 0)) continue;
            if (sea && !rules_->terrains[at(state_.plot(h).terrain)].shallowWater) continue;  // shipwrecks near the coast
            state_.plot(h).antiquity = static_cast<uint8_t>(sea ? 2 : 1);
            --left;
        }
    }
}

CommandError Game::excavateProblem(PlayerId player, UnitId id) const {
    const Unit* u = state_.unit(id);
    if (!u || u->owner != player) return CommandError::NotYourUnit;
    if (rules_->units[at(u->type)].excavations <= 0 || u->charges <= 0 || u->movesLeft <= Fixed()) return CommandError::BadUnit;
    const Plot& p = state_.plot(u->pos);
    if (p.antiquity == 0 || !seesAntiquity(player)) return CommandError::BadTarget;
    if (p.owner != kNoPlayer && p.owner != player && !grantsOpenBorders(p.owner, player)) return CommandError::BadTarget;
    TypeIndex artifact = kNone;
    for (size_t w = 0; w < rules_->greatWorkTypes.size(); ++w) artifact = rules_->greatWorkTypes[w].id == "ARTIFACT" ? static_cast<TypeIndex>(w) : artifact;
    if (artifact == kNone) return CommandError::BadTarget;
    for (const City& c : state_.cities) {
        if (c.owner == player && freeGreatWorkSlot(c, artifact) != kNone) return CommandError::Ok;
    }
    return CommandError::CannotImprove;  // nowhere to put it
}

void Game::excavate(UnitId id) {
    Unit& u = *state_.unit(id);
    const PlayerId landOwner = state_.plot(u.pos).owner;
    if (landOwner != kNoPlayer && landOwner != u.owner) breakPromises(u.owner, landOwner, PromiseKind::NoDigging);  // 08 [GS]
    state_.plot(u.pos).antiquity = 0;
    TypeIndex artifact = kNone;
    for (size_t w = 0; w < rules_->greatWorkTypes.size(); ++w) artifact = rules_->greatWorkTypes[w].id == "ARTIFACT" ? static_cast<TypeIndex>(w) : artifact;
    // Its history: the battle fought here, or (a site placed on open ground) a civilization in the game and an
    // era before the present one, drawn at random.
    int8_t era = -1;
    TypeIndex civ = kNone;
    const int32_t here = state_.grid.index(u.pos);
    for (size_t i = 0; i < state_.battleSites.size() && i < state_.battleHistory.size(); ++i) {
        if (state_.battleSites[i] != here) continue;
        era = static_cast<int8_t>(state_.battleHistory[i] / 4096);
        civ = static_cast<TypeIndex>(state_.battleHistory[i] % 4096 - 1);
    }
    if (civ == kNone) {
        Rng& rng = state_.rng.get(RngStream::Gameplay);
        std::vector<TypeIndex> civs;
        for (const Player& p : state_.players) {
            if (isMajorCiv(p.id) && p.civ != kNone) civs.push_back(p.civ);
        }
        if (!civs.empty()) civ = civs[rng.below(static_cast<uint32_t>(civs.size()))];
        era = static_cast<int8_t>(rng.below(static_cast<uint32_t>(std::max(1, std::min(state_.gameEra, rules_->globalInt("ARCHAEOLOGY_MAX_ERA") + 1)))));
    }
    for (City& c : state_.cities) {
        if (c.owner != u.owner) continue;
        const TypeIndex slot = freeGreatWorkSlot(c, artifact);
        if (slot == kNone) continue;
        GreatWork gw;
        gw.type = artifact;
        gw.building = slot;
        gw.era = era;
        gw.civ = civ;
        c.greatWorks.push_back(gw);
        break;
    }
    u.movesLeft = Fixed();
    if (--u.charges <= 0) removeUnit(id);
}

}  // namespace sov
