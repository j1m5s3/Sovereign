#include "sovereign/state.h"

#include <algorithm>

#include "sovereign/commands.h"

namespace sov {

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
}  // namespace

Unit* GameState::unit(UnitId id) { return findById(units, id); }
const Unit* GameState::unit(UnitId id) const { return findById(units, id); }
City* GameState::city(CityId id) { return findById(cities, id); }
const City* GameState::city(CityId id) const { return findById(cities, id); }

const City* GameState::cityAt(Hex h) const {
    for (const City& c : cities) {
        if (c.pos == h) return &c;
    }
    return nullptr;
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
    }
    return s + "?";
}

}  // namespace sov
