#include "sovereign/state.h"

#include <algorithm>

#include "sovereign/commands.h"

namespace sov {

bool cityHasBuilding(const City& city, const Rules& rules, TypeIndex building) {
    if (city.has(building)) return true;
    for (TypeIndex b : city.buildings) {
        if (rules.buildings[static_cast<size_t>(b)].replaces == building) return true;
    }
    return false;
}

namespace {
template <typename T, typename Id>
T* findById(std::vector<T>& v, Id id) {
    auto it = std::lower_bound(v.begin(), v.end(), id, [](const T& a, Id b) { return a.id < b; });
    return (it != v.end() && it->id == id) ? &*it : nullptr;
}
template <typename T, typename Id>
const T* findById(const std::vector<T>& v, Id id) {
    auto it = std::lower_bound(v.begin(), v.end(), id, [](const T& a, Id b) { return a.id < b; });
    return (it != v.end() && it->id == id) ? &*it : nullptr;
}
// The city's place in the list when it lies as far past the first city as its id is past the first's, where
// lower_bound finds it too; else the list's size. Ids rise along the list and cities are seldom lost, so it mostly does.
size_t cityGuess(const std::vector<City>& v, CityId id) {
    if (v.empty() || id < v.front().id) return v.size();
    const size_t at = static_cast<size_t>(std::min<int64_t>(static_cast<int64_t>(id) - v.front().id, static_cast<int64_t>(v.size()) - 1));
    return v[at].id == id && (at == 0 || v[at - 1].id < id) ? at : v.size();
}
}  // namespace

Unit* GameState::unit(UnitId id) { return findById(units, id); }
const Unit* GameState::unit(UnitId id) const { return findById(units, id); }
City* GameState::city(CityId id) {
    const size_t at = cityGuess(cities, id);
    return at < cities.size() ? &cities[at] : findById(cities, id);
}
const City* GameState::city(CityId id) const {
    const size_t at = cityGuess(cities, id);
    return at < cities.size() ? &cities[at] : findById(cities, id);
}

const City* GameState::cityAt(Hex h) const {
    for (const City& c : cities) {
        if (c.pos == h) return &c;
    }
    return nullptr;
}

TypeIndex GameState::wonderAt(Hex h) const {
    for (const City& c : cities) {
        for (const CityWonder& w : c.wonders) {
            if (w.pos == h) return w.building;
        }
    }
    return kNone;
}

const CityDistrict* GameState::districtAt(Hex h) const {
    for (const City& c : cities) {
        for (const CityDistrict& d : c.districts) {
            if (d.pos == h) return &d;
        }
    }
    return nullptr;
}

const Unit* GameState::unitAt(Hex h, UnitLayer layer, const Rules& rules) const {
    for (const Unit& u : units) {
        if (u.pos == h && rules.units[static_cast<size_t>(u.type)].layer == layer) return &u;
    }
    return nullptr;
}

const Unit* GameState::foreignUnitAt(Hex h, PlayerId player) const {
    for (const Unit& u : units) {
        if (u.pos == h && u.owner != player) return &u;
    }
    return nullptr;
}

const char* commandErrorName(CommandError e) {
    switch (e) {
        case CommandError::Ok: return "Ok";
        case CommandError::NotYourTurn: return "NotYourTurn";
        case CommandError::BadPlayer: return "BadPlayer";
        case CommandError::BadUnit: return "BadUnit";
        case CommandError::NotYourUnit: return "NotYourUnit";
        case CommandError::BadTarget: return "BadTarget";
        case CommandError::NoPath: return "NoPath";
        case CommandError::CannotFoundHere: return "CannotFoundHere";
        case CommandError::TooCloseToCity: return "TooCloseToCity";
        case CommandError::NotASettler: return "NotASettler";
        case CommandError::BadActivity: return "BadActivity";
        case CommandError::UnitsNeedOrders: return "UnitsNeedOrders";
        case CommandError::GameOver: return "GameOver";
        case CommandError::BadCity: return "BadCity";
        case CommandError::NotYourCity: return "NotYourCity";
        case CommandError::CannotBuild: return "CannotBuild";
        case CommandError::QueueFull: return "QueueFull";
        case CommandError::NotEnoughGold: return "NotEnoughGold";
        case CommandError::CannotBuyPlot: return "CannotBuyPlot";
        case CommandError::CannotWorkPlot: return "CannotWorkPlot";
        case CommandError::ProductionNeeded: return "ProductionNeeded";
        case CommandError::CannotResearch: return "CannotResearch";
        case CommandError::ResearchNeeded: return "ResearchNeeded";
        case CommandError::CivicNeeded: return "CivicNeeded";
        case CommandError::CannotAdoptGovernment: return "CannotAdoptGovernment";
        case CommandError::CannotSetPolicy: return "CannotSetPolicy";
        case CommandError::ChangesLocked: return "ChangesLocked";
        case CommandError::CannotImprove: return "CannotImprove";
        case CommandError::CannotHarvest: return "CannotHarvest";
        case CommandError::NotEnoughResources: return "NotEnoughResources";
        case CommandError::CannotDeclareWar: return "CannotDeclareWar";
        case CommandError::CannotMakePeace: return "CannotMakePeace";
        case CommandError::CannotAttack: return "CannotAttack";
        case CommandError::CannotPromote: return "CannotPromote";
        case CommandError::CannotStrike: return "CannotStrike";
        case CommandError::CannotRaze: return "CannotRaze";
        case CommandError::CannotEquip: return "CannotEquip";
        case CommandError::CannotEscort: return "CannotEscort";
        case CommandError::LeaderNeeded: return "LeaderNeeded";
        case CommandError::CannotSucceed: return "CannotSucceed";
        case CommandError::CannotSendAgent: return "CannotSendAgent";
        case CommandError::CannotTakeStance: return "CannotTakeStance";
        case CommandError::BattlePending: return "BattlePending";
        case CommandError::NoBattle: return "NoBattle";
        case CommandError::NoGreatPerson: return "NoGreatPerson";
        case CommandError::NotEnoughFaith: return "NotEnoughFaith";
        case CommandError::CannotActivate: return "CannotActivate";
        case CommandError::CannotFoundReligion: return "CannotFoundReligion";
        case CommandError::CannotSpread: return "CannotSpread";
        case CommandError::CannotTrade: return "CannotTrade";
        case CommandError::CannotSendEnvoy: return "CannotSendEnvoy";
        case CommandError::CannotDeal: return "CannotDeal";
        case CommandError::NoDeal: return "NoDeal";
        case CommandError::CannotDenounce: return "CannotDenounce";
        case CommandError::CannotGovern: return "CannotGovern";
        case CommandError::CannotSpy: return "CannotSpy";
        case CommandError::CannotVote: return "CannotVote";
        case CommandError::CannotUpgrade: return "CannotUpgrade";
        case CommandError::CannotTreatWithClan: return "CannotTreatWithClan";
    }
    return "Unknown";
}

std::string describe(const Command& c) {
    std::string s = "p" + std::to_string(c.player) + " ";
    switch (c.type) {
        case CommandType::MoveUnit:
            return s + "MoveUnit u" + std::to_string(c.id) + " -> (" + std::to_string(c.target.x) + "," +
                   std::to_string(c.target.y) + ")";
        case CommandType::FoundCity: return s + "FoundCity u" + std::to_string(c.id);
        case CommandType::SetActivity:
            return s + "SetActivity u" + std::to_string(c.id) + " " + std::to_string(c.arg);
        case CommandType::EndTurn: return s + "EndTurn";
        case CommandType::SetProduction:
        case CommandType::QueueProduction:
        case CommandType::Purchase:
            return s + (c.type == CommandType::SetProduction ? "SetProduction" : c.type == CommandType::QueueProduction ? "QueueProduction" : "Purchase") +
                   " city" + std::to_string(c.id) + " kind" + std::to_string(c.arg) + " type" + std::to_string(c.arg2);
        case CommandType::BuyPlot:
        case CommandType::LockPlot:
            return s + (c.type == CommandType::BuyPlot ? "BuyPlot" : "LockPlot") + " city" + std::to_string(c.id) +
                   " (" + std::to_string(c.target.x) + "," + std::to_string(c.target.y) + ") " + std::to_string(c.arg);
        case CommandType::ChooseResearch: return s + "ChooseResearch tech" + std::to_string(c.id);
        case CommandType::ChooseCivic: return s + "ChooseCivic civic" + std::to_string(c.id);
        case CommandType::ChangeGovernment: return s + "ChangeGovernment gov" + std::to_string(c.id);
        case CommandType::SetPolicy:
            return s + "SetPolicy slot" + std::to_string(c.id) + " policy" + std::to_string(c.arg);
        case CommandType::BuildImprovement:
            return s + "BuildImprovement u" + std::to_string(c.id) + " improvement" + std::to_string(c.arg);
        case CommandType::Harvest: return s + "Harvest u" + std::to_string(c.id);
        case CommandType::DeclareWar: return s + "DeclareWar p" + std::to_string(c.arg);
        case CommandType::MakePeace: return s + "MakePeace p" + std::to_string(c.arg);
        case CommandType::Attack:
            return s + "Attack u" + std::to_string(c.id) + " (" + std::to_string(c.target.x) + "," + std::to_string(c.target.y) + ")";
        case CommandType::RangedAttack:
            return s + "RangedAttack u" + std::to_string(c.id) + " (" + std::to_string(c.target.x) + "," + std::to_string(c.target.y) + ")";
        case CommandType::Promote: return s + "Promote u" + std::to_string(c.id) + " promotion" + std::to_string(c.arg);
        case CommandType::CityStrike:
            return s + "CityStrike c" + std::to_string(c.id) + " (" + std::to_string(c.target.x) + "," + std::to_string(c.target.y) + ")";
        case CommandType::RazeCity: return s + "RazeCity c" + std::to_string(c.id);
        case CommandType::EquipGear:
            return s + "EquipGear u" + std::to_string(c.id) + " gear" + std::to_string(c.arg) + " slot" + std::to_string(c.arg2);
        case CommandType::LinkEscort: return s + "LinkEscort u" + std::to_string(c.id) + " leader" + std::to_string(c.arg);
        case CommandType::ChooseSuccessor: return s + "ChooseSuccessor kind" + std::to_string(c.arg) + " u" + std::to_string(c.id);
        case CommandType::AbandonLeader: return s + "AbandonLeader";
        case CommandType::BattleResult:
            return s + "BattleResult def" + std::to_string(c.arg) + " att" + std::to_string(c.arg2) + " leader" + std::to_string(c.target.x);
        case CommandType::AutoResolveBattle: return s + "AutoResolveBattle";
        case CommandType::CityStance: return s + "CityStance city" + std::to_string(c.id) + " stance" + std::to_string(c.arg);
        case CommandType::SendAssassin: return s + "SendAssassin agent" + std::to_string(c.id) + " -> p" + std::to_string(c.arg);
        case CommandType::PatronizeGreatPerson:
            return s + "PatronizeGreatPerson class" + std::to_string(c.arg) + (c.arg2 ? " faith" : " gold");
        case CommandType::PassGreatPerson: return s + "PassGreatPerson class" + std::to_string(c.arg);
        case CommandType::ActivateGreatPerson: return s + "ActivateGreatPerson u" + std::to_string(c.id);
        case CommandType::FoundPantheon: return s + "FoundPantheon belief" + std::to_string(c.arg);
        case CommandType::FoundReligion:
            return s + "FoundReligion u" + std::to_string(c.id) + " religion" + std::to_string(c.arg) + " beliefs " + std::to_string(c.arg2) + "," +
                   std::to_string(c.target.x);
        case CommandType::EvangelizeBelief: return s + "EvangelizeBelief u" + std::to_string(c.id) + " belief" + std::to_string(c.arg);
        case CommandType::SpreadReligion: return s + "SpreadReligion u" + std::to_string(c.id);
        case CommandType::StartTradeRoute: return s + "StartTradeRoute u" + std::to_string(c.id) + " -> city" + std::to_string(c.arg);
        case CommandType::SendEnvoy: return s + "SendEnvoy -> p" + std::to_string(c.arg);
        case CommandType::LevyMilitary: return s + "LevyMilitary p" + std::to_string(c.arg);
        case CommandType::BribeCamp: return s + "BribeCamp camp" + std::to_string(c.id);
        case CommandType::HireFromCamp: return s + "HireFromCamp camp" + std::to_string(c.id);
        case CommandType::BuildIndustry: return s + "BuildIndustry u" + std::to_string(c.id);
        case CommandType::LiberateCity: return s + "LiberateCity city" + std::to_string(c.id);
        case CommandType::Airlift: return s + "Airlift " + std::to_string(c.id) + " -> " + std::to_string(c.target.x) + "," + std::to_string(c.target.y);
        case CommandType::BuyPolicyChanges: return s + "BuyPolicyChanges";
        case CommandType::LaunchInquisition: return s + "LaunchInquisition u" + std::to_string(c.id);
        case CommandType::HealReligious: return s + "HealReligious u" + std::to_string(c.id);
        case CommandType::Paradrop: return s + "Paradrop " + std::to_string(c.id) + " -> " + std::to_string(c.target.x) + "," + std::to_string(c.target.y);
        case CommandType::InciteCamp: return s + "InciteCamp camp" + std::to_string(c.id) + " -> p" + std::to_string(c.arg);
        case CommandType::ProposeDeal: return s + "ProposeDeal -> p" + std::to_string(c.arg) + " (" + std::to_string(c.data.size() / 4) + " items)";
        case CommandType::AnswerDeal: return s + (c.arg ? "AcceptDeal " : "RejectDeal ") + std::to_string(c.id);
        case CommandType::Denounce: return s + "Denounce p" + std::to_string(c.arg);
        case CommandType::RecordTalk: return s + "RecordTalk p" + std::to_string(c.arg) + ": " + c.text;
        case CommandType::AppointGovernor: return s + "AppointGovernor " + std::to_string(c.arg);
        case CommandType::PromoteGovernor: return s + "PromoteGovernor " + std::to_string(c.arg) + " " + std::to_string(c.arg2);
        case CommandType::AssignGovernor: return s + "AssignGovernor " + std::to_string(c.arg) + " -> city" + std::to_string(c.id);
        case CommandType::CongressVote: return s + "CongressVote item" + std::to_string(c.id) + " option" + std::to_string(c.arg) + " candidate" + std::to_string(c.arg2);
        case CommandType::UpgradeUnit: return s + "UpgradeUnit " + std::to_string(c.id);
        case CommandType::RebaseUnit: return s + "RebaseUnit " + std::to_string(c.id) + " -> " + std::to_string(c.target.x) + "," + std::to_string(c.target.y);
        case CommandType::SendDelegation: return s + "SendDelegation " + std::to_string(c.arg) + (c.arg2 ? " embassy" : "");
        case CommandType::AskPromise: return s + "AskPromise " + std::to_string(c.arg) + " " + std::to_string(c.arg2);
        case CommandType::Excavate: return s + "Excavate " + std::to_string(c.id);
        case CommandType::ChooseDedication: return s + "ChooseDedication " + std::to_string(c.arg);
        case CommandType::DesignatePark: return s + "DesignatePark u" + std::to_string(c.id);
        case CommandType::PerformConcert: return s + "PerformConcert u" + std::to_string(c.id);
        case CommandType::MoveGreatWork:
            return s + "MoveGreatWork city" + std::to_string(c.id) + " #" + std::to_string(c.arg) + " to city" + std::to_string(c.arg2) + " b" + std::to_string(c.target.x);
        case CommandType::FormUnit: return s + "FormUnit " + std::to_string(c.id) + " + " + std::to_string(c.arg);
        case CommandType::Pillage: return s + "Pillage " + std::to_string(c.id);
        case CommandType::RepairImprovement: return s + "RepairImprovement " + std::to_string(c.id);
        case CommandType::BuildRailroad: return s + "BuildRailroad " + std::to_string(c.id);
        case CommandType::BuildRoad: return s + "BuildRoad " + std::to_string(c.id);
        case CommandType::ContributeCharge: return s + "ContributeCharge " + std::to_string(c.id);
        case CommandType::PromoteSpy: return s + "PromoteSpy " + std::to_string(c.id) + " " + std::to_string(c.arg);
        case CommandType::JoinEmergency: return s + "JoinEmergency " + std::to_string(c.arg);
        case CommandType::LaunchWmd: return s + "LaunchWmd " + std::to_string(c.arg) + " by " + std::to_string(c.id) + " -> " + std::to_string(c.target.x) + "," + std::to_string(c.target.y);
        case CommandType::SpyMission: return s + "SpyMission agent" + std::to_string(c.id) + " " + std::to_string(c.arg) + " -> city" + std::to_string(c.arg2);
    }
    return s + "?";
}

}  // namespace sov
