// Emergencies [R&F/GS] (08: Emergencies; data: world-congress-emergencies). A civ that takes a major
// civ's city (Military), a city-state (City-State), turns another religion's Holy City (Religious),
// uses a nuclear weapon (Nuclear) or declares war on a friend or ally (Betrayal) becomes the target.
// The civ it wronged joins at once; other civs that have met the target (not its friends or allies)
// may join. Members win by undoing the deed before it ends (30 turns; Nuclear and Betrayal 60): the
// city taken back from the target, the Holy City following its own religion again, or, for Nuclear
// and Betrayal, any city of the target taken by a member. Success gives each member 100 Diplomatic
// Favor; failure gives the target 200. Members declare war on the target without grievances; while a
// Betrayal emergency runs the target's war weariness against members grows by half.
#include <algorithm>

#include "sovereign/game.h"
#include "sovereign/modifiers.h"

namespace sov {

namespace {
size_t at(int i) { return static_cast<size_t>(i); }
}  // namespace

bool Game::canJoinEmergency(PlayerId player, int emergency) const {
    if (emergency < 0 || at(emergency) >= state_.emergencies.size() || player < 0 || at(player) >= state_.players.size()) return false;
    const Emergency& e = state_.emergencies[at(emergency)];
    if (e.outcome != 0 || player == e.target || !isMajorCiv(player) || !state_.players[at(player)].alive) return false;
    if (at(player) < e.members.size() && e.members[at(player)]) return false;
    if (!hasMet(player, e.target) || friends(player, e.target) || alliance(player, e.target) != AllianceType::None) return false;
    return true;
}

bool Game::inEmergencyAgainst(PlayerId member, PlayerId target) const {
    for (const Emergency& e : state_.emergencies) {
        if (e.outcome == 0 && e.target == target && at(member) < e.members.size() && e.members[at(member)]) return true;
    }
    return false;
}

void Game::triggerEmergency(EmergencyKind kind, PlayerId target, CityId city, PlayerId victim) {
    if (!isMajorCiv(target)) return;
    // An emergency is a special session of the World Congress, at most one per
    // WORLD_CONGRESS_MIN_TIME_BETWEEN_SPECIAL_SESSIONS turns (08: Special sessions).
    if (!specialSessionDue()) return;
    // One running emergency of a kind against a target at a time.
    for (const Emergency& e : state_.emergencies) {
        if (e.outcome == 0 && e.kind == kind && e.target == target) return;
    }
    Emergency e;
    e.kind = kind;
    e.target = target;
    e.city = city;
    e.endTurn = state_.turn + (kind == EmergencyKind::Nuclear || kind == EmergencyKind::Betrayal ? 60 : 30);
    e.members.assign(state_.players.size(), 0);
    if (kind == EmergencyKind::Religious) {
        for (size_t r = 0; r < state_.religions.size(); ++r) {
            if (state_.religions[r].holyCity == city) e.religion = static_cast<int32_t>(r);
        }
    }
    if (victim != kNoPlayer && victim != target && isMajorCiv(victim)) e.members[at(victim)] = 1;
    state_.emergencies.push_back(std::move(e));
    state_.lastSpecialSession = state_.turn;
}

void Game::processEmergencies() {
    // Religious: a Holy City now following a religion founded by another major civ.
    for (const FoundedReligion& fr : state_.religions) {
        const City* holy = state_.city(fr.holyCity);
        if (!holy) continue;
        const int majority = cityMajorityReligion(*holy);
        if (majority < 0 || at(majority) >= state_.religions.size()) continue;
        const PlayerId converter = state_.religions[at(majority)].founder;
        // Once per Holy City and converter: a settled emergency is not called again.
        const bool called = std::any_of(state_.emergencies.begin(), state_.emergencies.end(), [&](const Emergency& e) {
            return e.kind == EmergencyKind::Religious && e.city == holy->id && e.target == converter;
        });
        if (!called && converter != fr.founder && converter != kNoPlayer) triggerEmergency(EmergencyKind::Religious, converter, holy->id, fr.founder);
    }
    for (Emergency& e : state_.emergencies) {
        if (e.outcome != 0) continue;
        bool met = false;
        switch (e.kind) {
            case EmergencyKind::Military:
            case EmergencyKind::CityState: {
                const City* c = state_.city(e.city);
                met = !c || c->owner != e.target;
                break;
            }
            case EmergencyKind::Religious: {
                const City* c = state_.city(e.city);
                met = !c || cityMajorityReligion(*c) == e.religion;
                break;
            }
            case EmergencyKind::Nuclear:
            case EmergencyKind::Betrayal:
                break;  // a member taking one of the target's cities settles it (captureCity)
        }
        if (met) e.outcome = 1;
        else if (state_.turn >= e.endTurn) e.outcome = 2;
        if (e.outcome == 1) {
            for (size_t m = 0; m < e.members.size() && m < state_.players.size(); ++m) {
                if (e.members[m]) state_.players[m].favor += 100;
            }
        } else if (e.outcome == 2) {
            state_.players[at(e.target)].favor += 200;
        }
    }
}

}  // namespace sov
