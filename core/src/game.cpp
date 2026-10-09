#include "sovereign/game.h"

#include <algorithm>
#include <cstdlib>
#include <queue>

#include "sovereign/mapgen.h"
#include "sovereign/modifiers.h"
#include "sovereign/serialize.h"

namespace sov {

namespace {
const TerrainType& terrainOf(const Rules& r, const Plot& p) { return r.terrains[static_cast<size_t>(p.terrain)]; }

// hasRiver(from, d) where `to` is from's neighbour in direction d, from the two plots' riverEdges: a plot keeps the
// rivers on its E, SE and SW edges, so the edge toward the other three directions is the neighbour's, facing back.
bool riverEdgeBetween(uint8_t fromEdges, uint8_t toEdges, Dir d) {
    // Looked up rather than switched on: a path search's steps run every way in turn, which a jump mispredicts.
    static constexpr uint8_t kEdge[kNumDirs] = {kRiverSW, kRiverE, kRiverSE, kRiverSW, kRiverE, kRiverSE};  // NE, E, SE, SW, W, NW
    const unsigned i = static_cast<unsigned>(d);
    if (i >= static_cast<unsigned>(kNumDirs)) return false;
    const bool own = i - 1 < 3u;  // E, SE, SW
    return ((own ? fromEdges : toEdges) & kEdge[i]) != 0;
}
const UnitType& typeOf(const Rules& r, const Unit& u) { return r.units[static_cast<size_t>(u.type)]; }
// How high a plot stands in the way of sight (01: Visibility): its terrain, and its feature unless seen through.
int sightObstacle(const GameState& s, const Rules& r, Hex h, bool throughFeatures) {
    const Plot& op = s.plot(h);
    int obstacle = terrainOf(r, op).sightThrough;
    if (op.feature != kNone && !throughFeatures) obstacle += r.features[static_cast<size_t>(op.feature)].sightThrough;
    return obstacle;
}
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
    for (Player& p : s.players) p.relations.resize(s.players.size());
    generateMap(s, rules);
    if (!chooseStartPositions(s, rules, error)) return nullptr;
    addStartBonuses(s, rules);
    placeCityStates(s, rules);
    placeNaturalWonders(s, rules);
    if (setup.tribalVillages) placeVillages(s, rules);
    if (setup.barbarians) {
        // The barbarians: one extra player, at war with all, who moves in the world turn.
        Player b;
        b.id = static_cast<PlayerId>(s.players.size());
        b.barbarian = true;
        fitPlayerToRules(b, rules);
        b.visibility.assign(static_cast<size_t>(s.grid.size()), 0);
        s.players.push_back(std::move(b));
        for (Player& p : s.players) p.relations.resize(s.players.size());
    }

    for (const Player& p : s.players) s.majorsAtStart += !p.barbarian && p.cityState == kNone ? 1 : 0;
    auto game = std::make_unique<Game>(rules, std::move(s), std::vector<Command>{});
    GameState& st = game->state_;
    game->linkBarbarians();
    for (Player& p : st.players) {
        if (p.barbarian) continue;
        if (p.cityState != kNone) {
            // A city-state starts with its city and two Warriors (game-setup.md, Starting units (city-states)).
            Unit& settler = game->spawnUnit(rules.unit("UNIT_SETTLER"), p.id, p.startPos);
            game->applyFoundCity(Command::foundCity(p.id, settler.id));
            for (int k = 0; k < 2; ++k) {
                if (auto spot = game->unitSpawnPlot(*st.cityAt(p.startPos), rules.unit("UNIT_WARRIOR"))) game->spawnUnit(rules.unit("UNIT_WARRIOR"), p.id, *spot);
            }
            continue;
        }
        for (const std::string& unitId : rules.startingUnits) {
            TypeIndex t = rules.unit(unitId);
            UnitLayer layer = rules.units[static_cast<size_t>(t)].layer;
            std::optional<Hex> spot;
            if (!st.unitAt(p.startPos, layer, rules)) spot = p.startPos;
            for (const Hex& n : st.grid.nearestFirst(p.startPos, 2)) {
                if (spot) break;
                if (isLandPassable(st, rules, n) && !st.unitAt(n, layer, rules) && !st.foreignUnitAt(n, p.id)) spot = n;
            }
            if (!spot) {
                *error = "no room for starting unit " + unitId;
                return nullptr;
            }
            game->spawnUnit(t, p.id, *spot);
        }
        // AI civs at Immortal and Deity start with extra units (00-overview: AI starting units).
        if (game->difficultyAi(p.id)) {
            const DifficultyType& d = game->difficulty();
            const std::pair<const char*, int> extras[] = {{"UNIT_SETTLER", d.aiExtraSettlers}, {"UNIT_WARRIOR", d.aiExtraWarriors}, {"UNIT_BUILDER", d.aiExtraBuilders}};
            for (const auto& [unitId, count] : extras) {
                const TypeIndex t = rules.unit(unitId);
                if (t == kNone) continue;
                const UnitLayer layer = rules.units[static_cast<size_t>(t)].layer;
                for (int k = 0; k < count; ++k) {
                    for (const Hex& n : st.grid.nearestFirst(p.startPos, 2)) {
                        if (isLandPassable(st, rules, n) && !st.unitAt(n, layer, rules) && !st.foreignUnitAt(n, p.id)) {
                            game->spawnUnit(t, p.id, n);
                            break;
                        }
                    }
                }
            }
        }
        // The leader starts on the Settler's tile with the normal starting units (leader doc §1).
        if (rules.leaderUnit != kNone) game->spawnLeader(p.id, p.startPos);
    }
    if (setup.startEra > 0) game->applyEraStart();
    for (const Player& p : st.players) game->refreshVisibility(p.id);
    // Profiles carried from earlier games seed this one (leader doc §10); this game's conquests start at zero.
    st.profiles.resize(st.players.size());
    for (size_t i = 0; i < setup.players.size() && i < st.players.size(); ++i) {
        if (!setup.players[i].hasProfile) continue;
        st.profiles[i] = setup.players[i].profile;
        st.profiles[i].citiesHeld = 0;
    }
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
        p.relations.resize(game->state_.players.size());
        if (p.visibility.size() != static_cast<size_t>(game->state_.grid.size()))
            p.visibility.assign(static_cast<size_t>(game->state_.grid.size()), 0);
        // Hand-made units count toward the strongest unit a player has had (city strength).
        for (const Unit& u : game->state_.units) {
            const UnitType& ut = rules.units[static_cast<size_t>(u.type)];
            if (u.owner == p.id && ut.layer == UnitLayer::Military) p.strongestUnit = std::max(p.strongestUnit, ut.combat);
        }
    }
    // Hand-made cities start at full HP, as founded, unless the scenario set their HP.
    for (City& c : game->state_.cities) {
        if (c.hp <= 0) c.hp = game->cityMaxHp();
        if (c.originalOwner == kNoPlayer) {
            c.originalOwner = c.owner;
            c.originalCapital = c.capital;
        }
    }
    game->linkBarbarians();
    for (const Player& p : game->state_.players) game->refreshVisibility(p.id);
    game->state_.currentPlayer = 0;
    game->beginPlayerTurn(0, false);
    return game;
}

Game::Game(const Rules& rules, GameState state, std::vector<Command> log)
    : rules_(&rules), state_(std::move(state)), log_(std::move(log)) {
    static const char* const kWonders[] = {"BUILDING_KILWA_KISIWANI", "BUILDING_UNIVERSITY_OF_SANKORE", "BUILDING_ORACLE",          "BUILDING_MACHU_PICCHU",
                                           "BUILDING_COLOSSEUM",      "BUILDING_STATUE_OF_LIBERTY",     "BUILDING_GREAT_ZIMBABWE",  "BUILDING_TORRE_DE_BEL_M",
                                           "BUILDING_EIFFEL_TOWER",   "BUILDING_GOLDEN_GATE_BRIDGE",    "BUILDING_BIOSPH_RE",       "BUILDING_CRISTO_REDENTOR",
                                           "BUILDING_HUEY_TEOCALLI"};
    static_assert(sizeof(kWonders) / sizeof(kWonders[0]) == static_cast<size_t>(W::Count), "one id per wonder");
    for (size_t i = 0; i < static_cast<size_t>(W::Count); ++i) wonders_[i] = rules_->building(kWonders[i]);
    static const char* const kBeliefs[] = {"BELIEF_DANCE_OF_THE_AURORA", "BELIEF_DESERT_FOLKLORE",   "BELIEF_SACRED_PATH",         "BELIEF_EARTH_GODDESS",
                                           "BELIEF_GOD_OF_HEALING",      "BELIEF_GOD_OF_WAR",        "BELIEF_INITIATION_RITES",    "BELIEF_HOLY_WATERS",
                                           "BELIEF_DIVINE_INSPIRATION",  "BELIEF_JESUIT_EDUCATION",  "BELIEF_RELIGIOUS_COMMUNITY", "BELIEF_RELIQUARIES",
                                           "BELIEF_WARRIOR_MONKS",       "BELIEF_WORK_ETHIC",        "BELIEF_SACRED_PLACES",       "BELIEF_PAPAL_PRIMACY",
                                           "BELIEF_RELIGIOUS_UNITY"};
    static_assert(sizeof(kBeliefs) / sizeof(kBeliefs[0]) == static_cast<size_t>(Bf::Count), "one id per belief");
    for (size_t i = 0; i < static_cast<size_t>(Bf::Count); ++i) beliefs_[i] = rules_->belief(kBeliefs[i]);
    static const char* const kPeople[] = {
        "GREAT_PERSON_ZHENG_HE",    "GREAT_PERSON_ZHANG_QIAN",        "GREAT_PERSON_MARCO_POLO",          "GREAT_PERSON_IBN_FADLAN",
        "GREAT_PERSON_RAJA_TODAR_MAL", "GREAT_PERSON_JOHN_ROCKEFELLER", "GREAT_PERSON_MIMAR_SINAN",         "GREAT_PERSON_MARCUS_LICINIUS_CRASSUS",
        "GREAT_PERSON_HILDEGARD_OF_BINGEN", "GREAT_PERSON_JOHN_ROEBLING", "GREAT_PERSON_JANE_DREW",       "GREAT_PERSON_ABU_AL_QASIM_AL_ZAHRAWI",
        "GREAT_PERSON_IBN_KHALDUN", "GREAT_PERSON_KENZO_TANGE",       "GREAT_PERSON_MARINA_RASKOVA",      "GREAT_PERSON_JOHN_SPILSBURY",
        "GREAT_PERSON_HELENA_RUBINSTEIN", "GREAT_PERSON_LEVI_STRAUSS", "GREAT_PERSON_EST_E_LAUDER",       "GREAT_PERSON_JAMES_YOUNG",
        "GREAT_PERSON_MARY_KATHERINE_GODDARD", "GREAT_PERSON_GIOVANNI_DE_MEDICI"};
    static_assert(sizeof(kPeople) / sizeof(kPeople[0]) == static_cast<size_t>(Gp::Count), "one id per great person");
    for (size_t i = 0; i < static_cast<size_t>(Gp::Count); ++i) greatPeople_[i] = rules_->greatPerson(kPeople[i]);
    static const char* const kCityStates[] = {
        "CITYSTATE_AKKAD",      "CITYSTATE_ANSHAN",       "CITYSTATE_ANTANANARIVO", "CITYSTATE_AYUTTHAYA",    "CITYSTATE_BANDAR_BRUNEI", "CITYSTATE_BUENOS_AIRES",
        "CITYSTATE_CHINGUETTI", "CITYSTATE_FEZ",          "CITYSTATE_HATTUSA",      "CITYSTATE_HUNZA",        "CITYSTATE_JERUSALEM",     "CITYSTATE_JOHANNESBURG",
        "CITYSTATE_KABUL",      "CITYSTATE_KANDY",        "CITYSTATE_KUMASI",       "CITYSTATE_MEXICO_CITY",  "CITYSTATE_MOGADISHU",     "CITYSTATE_MOHENJO_DARO",
        "CITYSTATE_NALANDA",    "CITYSTATE_NAN_MADOL",    "CITYSTATE_NGAZARGAMU",   "CITYSTATE_SAMARKAND",    "CITYSTATE_SINGAPORE",     "CITYSTATE_VALLETTA",
        "CITYSTATE_VATICAN_CITY", "CITYSTATE_VENICE",     "CITYSTATE_VILNIUS",      "CITYSTATE_WOLIN",        "CITYSTATE_YEREVAN",       "CITYSTATE_ZANZIBAR"};
    static_assert(sizeof(kCityStates) / sizeof(kCityStates[0]) == static_cast<size_t>(Cs::Count), "one id per city-state");
    for (size_t i = 0; i < static_cast<size_t>(Cs::Count); ++i) cityStates_[i] = rules_->cityState(kCityStates[i]);
    static const char* const kProducts[] = {"RESOURCE_TOYS", "RESOURCE_COSMETICS", "RESOURCE_JEANS", "RESOURCE_PERFUME"};
    for (size_t i = 0; i < 4; ++i) products_[i] = rules_->resource(kProducts[i]);
    oil_ = rules_->resource("RESOURCE_OIL");
    spices_[0] = rules_->resource("RESOURCE_CINNAMON");
    spices_[1] = rules_->resource("RESOURCE_CLOVES");
    oceanTerrain_ = rules_->terrain("TERRAIN_OCEAN");
    encampment_ = rules_->district("DISTRICT_ENCAMPMENT");
    for (size_t i = 0; i < rules_->techs.size(); ++i) {
        const TreeNode& t = rules_->techs[i];
        if (t.ocean) oceanTechs_.push_back(static_cast<TypeIndex>(i));
        if (t.embarkAll || t.embarkUnit != kNone) embarkTechs_.push_back(static_cast<TypeIndex>(i));
    }
    for (const TreeNode& t : rules_->techs) techEras_.push_back(static_cast<uint8_t>(std::clamp(t.era, 0, 255)));
    for (const TreeNode& t : rules_->civics) civicEras_.push_back(static_cast<uint8_t>(std::clamp(t.era, 0, 255)));
    for (size_t i = 0; i < rules_->civics.size(); ++i) {
        if (rules_->civics[i].enforceBorders) borderCivics_.push_back(static_cast<TypeIndex>(i));
    }
    suzerainEnvoys_ = rules_->globalInt(HotGlobal::InfluenceTokensMinimumForSuzerain);
    touristTourism_ = rules_->globalInt("TOURISM_TOURISM_TO_MOVE_CITIZEN");
    touristCulture_ = rules_->globalInt("TOURISM_CULTURE_PER_CITIZEN");
    listWatchedBoosts();
    pamukkale_ = rules_->feature("FEATURE_PAMUKKALE");
    fartherDistricts_[0] = rules_->district("DISTRICT_INDUSTRIAL_ZONE");
    fartherDistricts_[1] = rules_->district("DISTRICT_ENTERTAINMENT_COMPLEX");
    fartherDistricts_[2] = rules_->district("DISTRICT_WATER_PARK");
    formationCivics_[0] = rules_->civic("CIVIC_NATIONALISM");
    formationCivics_[1] = rules_->civic("CIVIC_MOBILIZATION");
    formationSchools_[0] = rules_->building("BUILDING_MILITARY_ACADEMY");
    formationSchools_[1] = rules_->building("BUILDING_SEAPORT");
    abilityGrants_.resize(rules_->units.size());
    for (uint32_t i : rules_->playerModifiers(ModEffect::GrantAbility)) {
        const std::vector<std::string>& classes = rules_->abilities[static_cast<size_t>(rules_->modifiers[i].ability)].classes;
        for (size_t t = 0; t < rules_->units.size(); ++t) {
            if (hasClassIn(rules_->units[t], classes)) abilityGrants_[t].push_back(i);
        }
    }
    for (const AbilityType& a : rules_->abilities) {
        uint64_t kinds = 0;
        for (const UnitEffect& e : a.effects) kinds |= effectBit(e.kind);
        abilityKinds_.push_back(kinds);
    }
    parks_ = std::any_of(state_.plots.begin(), state_.plots.end(), [](const Plot& p) { return p.park; });
    // The landmasses in the order they first appear (each one's place in it, by landmass, + 1) and their plot counts;
    // then each plot goes after the earlier plots of its landmass.
    std::vector<int32_t> place, count;
    for (const Plot& p : state_.plots) {
        if (p.continent < 0) continue;
        const size_t k = static_cast<size_t>(p.continent);
        if (k >= place.size()) place.resize(k + 1, 0);
        if (place[k] == 0) {
            count.push_back(0);
            place[k] = static_cast<int32_t>(count.size());
        }
        ++count[static_cast<size_t>(place[k] - 1)];
    }
    landFirst_.assign(count.size() + 1, 0);
    for (size_t g = 0; g < count.size(); ++g) landFirst_[g + 1] = landFirst_[g] + count[g];
    landPlots_.resize(static_cast<size_t>(landFirst_.back()));
    std::vector<int32_t> next(landFirst_.begin(), landFirst_.end() - 1);
    for (size_t i = 0; i < state_.plots.size(); ++i) {
        const int16_t k = state_.plots[i].continent;
        if (k >= 0) landPlots_[static_cast<size_t>(next[static_cast<size_t>(place[static_cast<size_t>(k)] - 1)]++)] = static_cast<int32_t>(i);
    }
    for (size_t i = 0; i < state_.plots.size(); ++i) {
        if (state_.plots[i].resource != kNone) resourcePlots_.push_back(static_cast<int32_t>(i));
    }
    lakes_ = lakeMap(state_, *rules_);
    terrainImprovements_.resize(rules_->terrains.size());
    featureImprovements_.resize(rules_->features.size());
    resourceImprovements_.resize(rules_->resources.size());
    for (size_t i = 0; i < rules_->improvements.size(); ++i) {
        const ImprovementType& im = rules_->improvements[i];
        const auto takes = [&](std::vector<std::vector<TypeIndex>>& by, TypeIndex land) {
            if (land < 0 || static_cast<size_t>(land) >= by.size()) return;
            std::vector<TypeIndex>& list = by[static_cast<size_t>(land)];
            if (list.empty() || list.back() != static_cast<TypeIndex>(i)) list.push_back(static_cast<TypeIndex>(i));
        };
        for (const TypeIndex t : im.validTerrains) takes(terrainImprovements_, t);
        for (const TypeIndex f : im.validFeatures) takes(featureImprovements_, f);
        for (const TypeIndex r : im.validResources) takes(resourceImprovements_, r);
    }
    improvedOnceAt_.assign(state_.plots.size(), 0);
    for (size_t i = 0; i < state_.plots.size(); ++i) {
        if (state_.plots[i].improvement == kNone) continue;
        improvedOnceAt_[i] = 1;
        improvedOnce_.push_back(static_cast<int32_t>(i));
    }
    cityStatePlayers_.resize(rules_->cityStates.size());
    for (size_t i = 0; i < state_.players.size(); ++i) {
        const TypeIndex type = state_.players[i].cityState;
        if (type >= 0 && static_cast<size_t>(type) < cityStatePlayers_.size()) cityStatePlayers_[static_cast<size_t>(type)].push_back(i);
    }
    playersAtStart_ = state_.players.size();
}

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
    return validate(c, nullptr);
}

CommandError Game::validate(const Command& c, std::optional<CheckedPath>* movePath) const {
    if (c.player < 0 || static_cast<size_t>(c.player) >= state_.players.size()) return CommandError::BadPlayer;
    if (!state_.players[static_cast<size_t>(c.player)].alive) return CommandError::BadPlayer;
    // A pending live battle stops the world until it is settled, whoever's turn it is (leader doc §9).
    if (state_.pendingBattle.active || c.type == CommandType::BattleResult || c.type == CommandType::AutoResolveBattle)
        return validateBattle(c);
    if (c.player != state_.currentPlayer) return CommandError::NotYourTurn;
    if (state_.winner != kNoPlayer) return CommandError::GameOver;

    if (c.type == CommandType::EndTurn) {
        if (!unitsNeedingOrders(c.player).empty()) return CommandError::UnitsNeedOrders;
        if (state_.players[static_cast<size_t>(c.player)].successionPending) return CommandError::LeaderNeeded;
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
        case CommandType::SetCityFocus:
            return validateCity(c);
        case CommandType::ChooseResearch:
        case CommandType::ChooseCivic:
        case CommandType::ChangeGovernment:
        case CommandType::SetPolicy:
        case CommandType::BuyPolicyChanges:
            return validateResearch(c);
        case CommandType::DeclareWar:
        case CommandType::MakePeace:
        case CommandType::Attack:
        case CommandType::RangedAttack:
        case CommandType::Promote:
        case CommandType::CityStrike:
        case CommandType::RazeCity:
        case CommandType::LiberateCity:
            return validateCombat(c);
        case CommandType::EquipGear:
        case CommandType::LinkEscort:
        case CommandType::ChooseSuccessor:
        case CommandType::AbandonLeader:
        case CommandType::SendAssassin:
        case CommandType::CityStance:
            return validateLeader(c);
        case CommandType::PatronizeGreatPerson:
        case CommandType::PassGreatPerson:
        case CommandType::ActivateGreatPerson:
            return validateGreatPeople(c);
        case CommandType::FoundPantheon:
        case CommandType::FoundReligion:
        case CommandType::EvangelizeBelief:
        case CommandType::SpreadReligion:
        case CommandType::LaunchInquisition:
        case CommandType::HealReligious:
            return validateReligion(c);
        case CommandType::ProposeDeal:
        case CommandType::AnswerDeal:
        case CommandType::Denounce:
        case CommandType::RecordTalk:
            return validateDiplomacy(c);
        case CommandType::AppointGovernor:
        case CommandType::PromoteGovernor:
        case CommandType::AssignGovernor:
            return validateGovernor(c);
        case CommandType::UpgradeUnit: {
            const Unit* u = state_.unit(c.id);
            if (!u || u->owner != c.player) return CommandError::NotYourUnit;
            return upgradeProblem(c.id);
        }
        case CommandType::RebaseUnit: {
            const Unit* u = state_.unit(c.id);
            if (!u || u->owner != c.player) return CommandError::NotYourUnit;
            return rebaseProblem(c.id, c.target);
        }
        case CommandType::Airlift:
        case CommandType::Paradrop: {
            const Unit* u = state_.unit(c.id);
            if (!u || u->owner != c.player) return CommandError::NotYourUnit;
            return c.type == CommandType::Airlift ? airliftProblem(c.id, c.target) : paradropProblem(c.id, c.target);
        }
        case CommandType::LaunchWmd: return wmdProblem(c);
        case CommandType::JoinEmergency: return canJoinEmergency(c.player, c.arg) ? CommandError::Ok : CommandError::CannotDeal;
        case CommandType::BuildRailroad: return railroadProblem(c.player, c.id);
        case CommandType::BuildRoad: return roadProblem(c.player, c.id);
        case CommandType::ContributeCharge: return chargeProblem(c.player, c.id);
        case CommandType::Pillage: return c.arg == 1 ? coastalRaidProblem(c.player, c.id, c.target) : c.arg == 0 ? pillageProblem(c.player, c.id) : CommandError::BadTarget;
        case CommandType::FormUnit: return formationProblem(c.player, c.id, c.arg);
        case CommandType::Excavate: return excavateProblem(c.player, c.id);
        case CommandType::DesignatePark: return parkProblem(c.player, c.id);
        case CommandType::PerformConcert: return concertProblem(c.player, c.id);
        case CommandType::ChooseDedication: return dedicationProblem(c.player, static_cast<TypeIndex>(c.arg));
        case CommandType::MoveGreatWork: return moveGreatWorkProblem(c.player, c.id, c.arg, c.arg2, static_cast<TypeIndex>(c.target.x));
        case CommandType::SendDelegation:
            if (c.arg < 0 || static_cast<size_t>(c.arg) >= state_.players.size()) return CommandError::CannotDeal;
            return delegationProblem(c.player, static_cast<PlayerId>(c.arg), c.arg2 != 0);
        case CommandType::AskPromise:
            if (c.arg < 0 || static_cast<size_t>(c.arg) >= state_.players.size() || c.arg2 < 0 || c.arg2 >= kNumPromiseKinds) return CommandError::CannotDeal;
            return askPromiseProblem(c.player, static_cast<PlayerId>(c.arg), static_cast<PromiseKind>(c.arg2));
        case CommandType::RepairImprovement: return repairProblem(c.player, c.id);
        case CommandType::PromoteSpy: {
            const Agent* a = agent(c.id);
            if (!a || !a->spy || a->owner != c.player || a->promotionsPending <= 0 || c.arg < 0 || static_cast<size_t>(c.arg) >= rules_->spyPromotions.size() ||
                std::find(a->promotions.begin(), a->promotions.end(), static_cast<TypeIndex>(c.arg)) != a->promotions.end())
                return CommandError::CannotSpy;
            return CommandError::Ok;
        }
        case CommandType::CongressVote: {
            const int item = c.id;
            if (!congressInSession() || item < 0 || static_cast<size_t>(item) >= state_.congress.size() || hasVoted(c.player, item) ||
                !isMajorCiv(c.player))
                return CommandError::CannotVote;
            const CongressItem& it = state_.congress[static_cast<size_t>(item)];
            if ((c.arg != 0 && c.arg != 1) || c.arg2 < 0 || static_cast<size_t>(c.arg2) >= it.candidates.size() || c.target.x < 0 || c.target.x > 10)
                return CommandError::CannotVote;
            return extraVoteCost(c.target.x) <= state_.players[static_cast<size_t>(c.player)].favor ? CommandError::Ok : CommandError::CannotVote;
        }
        case CommandType::SpyMission: {
            CommandError why = CommandError::CannotSpy;
            canSpyMission(c.player, c.id, static_cast<SpyMission>(c.arg), c.arg2, &why);
            return why;
        }
        case CommandType::StartTradeRoute:
            return canStartTradeRoute(c.id, static_cast<CityId>(c.arg)) ? CommandError::Ok : CommandError::CannotTrade;
        case CommandType::SendEnvoy:
            return c.arg >= 0 && static_cast<size_t>(c.arg) < state_.players.size() && canSendEnvoy(c.player, static_cast<PlayerId>(c.arg))
                       ? CommandError::Ok
                       : CommandError::CannotSendEnvoy;
        case CommandType::BribeCamp:
        case CommandType::HireFromCamp:
        case CommandType::InciteCamp: return clanProblem(c.player, c.id, c.type, static_cast<PlayerId>(c.arg));
        case CommandType::LevyMilitary: {
            if (c.arg < 0 || static_cast<size_t>(c.arg) >= state_.players.size()) return CommandError::CannotSendEnvoy;
            const int cost = levyCost(c.player, static_cast<PlayerId>(c.arg));
            if (cost < 0) return CommandError::CannotSendEnvoy;
            return state_.players[static_cast<size_t>(c.player)].gold >= Fixed::fromInt(cost) ? CommandError::Ok : CommandError::NotEnoughGold;
        }
        default: break;
    }
    const Unit* u = state_.unit(c.id);
    if (!u) return CommandError::BadUnit;
    if (u->owner != c.player) return CommandError::NotYourUnit;

    switch (c.type) {
        case CommandType::MoveUnit: {
            u = &orderMover(*u);
            auto t = state_.grid.normalize(c.target);
            if (!t || *t != c.target || *t == u->pos) return CommandError::BadTarget;
            if (isAircraft(*u)) return CommandError::BadTarget;  // aircraft rebase, they do not walk
            if (u->attacked && !unitHas(*u, UnitEffectKind::MoveAfterAttack)) return CommandError::BadTarget;
            const Unit* own = state_.unitAt(*t, typeOf(*rules_, *u).layer, *rules_);
            if (own && own->owner == c.player) return CommandError::BadTarget;
            std::optional<std::vector<PathStep>> path = findPath(u->id, *t, c.arg == 1);
            if (!path) return CommandError::NoPath;
            if (movePath) *movePath = CheckedPath{u->id, std::move(*path)};
            return CommandError::Ok;
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
        case CommandType::BuildIndustry: {
            if (u->charges <= 0 || !isBuilder(rules_->units[static_cast<size_t>(u->type)]) || u->movesLeft <= Fixed()) return CommandError::CannotImprove;
            return industryProblem(c.player, u->pos);
        }
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
    const Plot& p = state_.plot(at);
    if (!canHoldCity(*rules_, p)) return set(CommandError::CannotFoundHere);
    if (policyIs(player, "POLICY_ISOLATIONISM")) return set(CommandError::CannotFoundHere);  // Isolationism (09)
    if (p.owner != kNoPlayer && p.owner != player) return set(CommandError::CannotFoundHere);
    // Nor on a district or wonder (03): one across the water three plots from its city is as far as founding asks.
    if (state_.districtAt(at) || state_.wonderAt(at) != kNone) return set(CommandError::CannotFoundHere);
    const int minRange = rules_->globalInt(HotGlobal::CityMinRange);
    for (const City& c : state_.cities) {
        // A city more rows away than the range is farther than it (rows do not wrap).
        if (std::abs(c.pos.y - at.y) > minRange) continue;
        // Across water (another landmass) the cities may stand one plot closer (02: Founding).
        const bool otherLand = state_.plot(c.pos).continent != state_.plot(at).continent;
        if (state_.grid.distance(c.pos, at) <= minRange - (otherLand ? 1 : 0)) return set(CommandError::TooCloseToCity);
    }
    if (campAt(at)) return set(CommandError::CannotFoundHere);
    return set(CommandError::Ok);
}

std::vector<UnitId> Game::unitsNeedingOrders(PlayerId player) const {
    std::vector<UnitId> out;
    for (const Unit& u : state_.units) {
        if (u.owner != player || u.activity != Activity::Awake || u.moveTarget || u.movesLeft <= Fixed()) continue;
        // A linked escort follows its leader and needs no orders of its own.
        const Unit* l = u.escorting != kNoUnit ? state_.unit(u.escorting) : nullptr;
        if (l && l->pos == u.pos && l->owner == u.owner) continue;
        out.push_back(u.id);
    }
    return out;
}

// ------------------------------------------------------------------ movement

std::optional<Fixed> Game::moveCost(const Unit& unit, Hex from, Hex to) const {
    const std::optional<Dir> d = state_.grid.directionTo(from, to);
    if (!d) return std::nullopt;
    const MoveTraits traits = moveTraits(unit);
    // Just the step's limits (`to` is on the grid: a neighbour of `from`), read as the path search's moveCost reads a
    // whole map's.
    const MoveLimits limits = moveLimits(unit, traits, to);
    if (limits.onlyBlocked) return std::nullopt;
    if (const PlayerId land = state_.plot(to).owner; land != kNoPlayer) {
        const uint8_t closed = limits.closed[static_cast<size_t>(land)];
        if ((closed & 2) || ((closed & 1) && state_.plot(from).owner != land)) return std::nullopt;
    }
    return terrainCost(unit, traits, from, to, *d);
}

Game::MoveLimits Game::moveLimits(const Unit& unit, const MoveTraits& traits, std::optional<Hex> only) const {
    MoveLimits limits;
    if (!only) limits.blocked.assign(static_cast<size_t>(state_.grid.size()), 0);
    auto block = [&](Hex h, uint8_t mark) {
        if (only) limits.onlyBlocked = limits.onlyBlocked || ((mark & MoveLimits::kKeepOut) && h == *only && state_.grid.normalize(h) == h);
        else if (state_.grid.normalize(h) == h) limits.blocked[static_cast<size_t>(state_.grid.index(h))] |= mark;
    };
    // Another player's unit: one at war is kept out of (attacks and captures are their own commands), one at peace may
    // be passed but not shared (05: Stacking).
    for (const Unit& u : state_.units) {
        if (u.owner == unit.owner || (only && u.pos != *only)) continue;
        block(u.pos, static_cast<uint8_t>(atWar(unit.owner, u.owner) ? MoveLimits::kKeepOut : MoveLimits::kPassOnly | MoveLimits::kTheirs));
    }
    // An Encampment counts only on its own city's plot (below), so with `only` just that plot's city's can block it.
    const CityId onlyCity = only ? state_.plot(*only).city : kNoCity;
    for (const City& c : state_.cities) {
        if (c.owner != unit.owner) block(c.pos, MoveLimits::kKeepOut);
        if (only && c.id != onlyCity) continue;
        // Nor an enemy Encampment that still stands (05: City combat).
        const CityDistrict* camp = atWar(unit.owner, c.owner) ? encampmentOf(c) : nullptr;
        if (camp && state_.grid.normalize(camp->pos) == camp->pos && state_.plot(camp->pos).city == c.id) block(camp->pos, MoveLimits::kKeepOut);
    }
    limits.closed.assign(state_.players.size(), 0);
    const PlayerId onlyOwner = only ? state_.plot(*only).owner : kNoPlayer;
    for (const Player& p : state_.players) {
        if (p.id == unit.owner || (only && p.id != onlyOwner)) continue;
        uint8_t& closed = limits.closed[static_cast<size_t>(p.id)];
        // Music Censorship (04): no foreign Rock Band enters the territory.
        if (traits.rockBand && policyIs(p.id, "POLICY_MUSIC_CENSORSHIP")) closed |= 2;
        // Closed borders: after Early Empire only units at war (or able to ignore borders) may enter.
        if (traits.ignoreBorders || atWar(unit.owner, p.id) || grantsOpenBorders(p.id, unit.owner)) continue;
        for (TypeIndex i : borderCivics_) {
            if (p.civics.has(i)) {
                closed |= 1;
                break;
            }
        }
    }
    return limits;
}

std::optional<Fixed> Game::moveCost(const Unit& unit, const MoveTraits& traits, const MoveLimits& limits, Hex from, Hex to, Dir dir) const {
    if (limits.blocked[static_cast<size_t>(state_.grid.index(to))] & MoveLimits::kKeepOut) return std::nullopt;
    if (const PlayerId land = state_.plot(to).owner; land != kNoPlayer) {
        const uint8_t closed = limits.closed[static_cast<size_t>(land)];
        if ((closed & 2) || ((closed & 1) && state_.plot(from).owner != land)) return std::nullopt;
    }
    return terrainCost(unit, traits, from, to, dir);
}

bool Game::canEmbark(PlayerId player, TypeIndex unitType) const {
    if (player < 0 || static_cast<size_t>(player) >= state_.players.size()) return false;
    const Player& p = state_.players[static_cast<size_t>(player)];
    if (rules_->units[static_cast<size_t>(unitType)].domain != Domain::Land) return false;
    for (TypeIndex i : embarkTechs_) {
        const TreeNode& t = rules_->techs[static_cast<size_t>(i)];
        if ((t.embarkAll || t.embarkUnit == unitType) && p.techs.has(i)) return true;
    }
    return false;
}

bool Game::canEnterOcean(PlayerId player) const {
    if (player < 0 || static_cast<size_t>(player) >= state_.players.size()) return false;
    const Player& p = state_.players[static_cast<size_t>(player)];
    for (TypeIndex i : oceanTechs_) {
        if (p.techs.has(i)) return true;
    }
    return greatPersonEffectTotal(player, GreatPersonEffectKind::Ocean) > 0;  // Leif Erikson (07)
}

bool Game::bridgeAt(Hex plot) const {
    const Plot& p = state_.plot(plot);
    return p.route >= 0 && terrainOf(*rules_, p).water;
}

bool Game::isEmbarked(const Unit& unit) const {
    return typeOf(*rules_, unit).domain == Domain::Land && terrainOf(*rules_, state_.plot(unit.pos)).water && !bridgeAt(unit.pos);
}

bool Game::isEmbarkTransition(const Unit& unit, Hex from, Hex to) const {
    if (typeOf(*rules_, unit).domain != Domain::Land) return false;
    const auto afloat = [&](Hex h) { return terrainOf(*rules_, state_.plot(h)).water && !bridgeAt(h); };
    return afloat(from) != afloat(to);
}

bool Game::isCoastalCity(const City& city) const {
    for (int d = 0; d < kNumDirs; ++d) {
        auto n = state_.grid.neighbor(city.pos, static_cast<Dir>(d));
        if (n && terrainOf(*rules_, state_.plot(*n)).water) return true;
    }
    return false;
}

std::optional<Fixed> Game::terrainCost(const Unit& unit, Hex from, Hex to) const {
    const std::optional<Dir> d = state_.grid.directionTo(from, to);
    return d ? terrainCost(unit, moveTraits(unit), from, to, *d) : std::nullopt;
}

std::optional<Fixed> Game::terrainCost(const Unit& unit, const MoveTraits& traits, Hex from, Hex to, Dir dir) const {
    StepUnit su = stepUnit(unit, traits);
    const StepInto into = stepInto(su, to);
    return stepCost(su, into, stepFrom(from), dir);
}

Game::StepUnit Game::stepUnit(const Unit& unit, const MoveTraits& traits) const {
    StepUnit su;
    su.unit = &unit;
    su.traits = &traits;
    const UnitType& ut = typeOf(*rules_, unit);
    su.domain = ut.domain;
    su.embarkCost = rules_->globalInt(HotGlobal::MovementEmbarkCost);
    su.riverCost = rules_->globalInt(HotGlobal::MovementRiverCost);
    return su;
}

Game::StepInto Game::stepInto(StepUnit& su, Hex to) const {
    StepInto s{};
    const Plot& p = state_.plot(to);
    const TerrainType& tt = terrainOf(*rules_, p);
    // The Golden Gate Bridge (03): land units cross its plot dry, along its road.
    const bool bridge = tt.water && p.route >= 0;
    s.water = tt.water;
    s.afloat = tt.water && !bridge;
    s.road = p.route >= 0 && !p.routePillaged ? p.route : int8_t{-1};  // a pillaged road counts for nothing
    s.riverEdges = p.riverEdges;
    // Water: Coast and Lake for anyone afloat; Ocean once the owner has Cartography.
    auto sailable = [&] {
        if (!tt.water || tt.impassable) return false;
        if (p.feature != kNone && rules_->features[static_cast<size_t>(p.feature)].impassable) return false;
        if (p.terrain != oceanTerrain_) return true;
        if (su.ocean < 0) su.ocean = canEnterOcean(su.unit->owner) ? 1 : 0;
        return su.ocean != 0;
    };
    if (su.domain == Domain::Sea) {
        if (tt.water) {
            if (sailable()) s.kind = StepKind::Sail;
        } else if (const CityDistrict* cd = state_.districtAt(to); cd && cd->complete && rules_->districts[static_cast<size_t>(cd->type)].canal) {
            s.kind = StepKind::Sail;  // a finished Canal carries ships across the land (03: Canal [GS])
        } else if (state_.cityAt(to)) {
            s.kind = StepKind::Port;  // ships put into a city from the water and sail out again (a coastal city is a port)
        }
        return s;
    }
    if (su.domain != Domain::Land) return s;  // air units arrive later
    if (s.afloat) {
        if (!sailable()) return s;
        if (su.embark < 0) su.embark = canEmbark(su.unit->owner, su.unit->type) ? 1 : 0;
        if (su.embark != 0) s.kind = StepKind::Embark;
        return s;
    }
    if (!bridge && !isLandPassable(*rules_, p)) return s;
    const MoveTraits& traits = *su.traits;
    if (traits.zeal) {
        s.kind = StepKind::Zeal;  // Missionary Zeal: religious units ignore terrain (06)
        return s;
    }
    int cost = tt.impassable ? 1 : tt.moveCost;  // through a tunnel: as flat ground
    if ((su.unit->wonderAbilities & 1) && tt.relief == Relief::Hills) cost = std::min(cost, 1);  // Everest (01): hills as flat ground
    if (tt.relief == Relief::Hills && cost > 1 && traits.ignoreHills) cost = 1;  // Alpine (05)
    if (p.feature != kNone) {
        const FeatureType& ft = rules_->features[static_cast<size_t>(p.feature)];
        // Ranger (05): woods cost nothing extra.
        if (!(traits.ignoreForest && ft.moveChange > 0 && ft.id == "FEATURE_FOREST")) cost += ft.moveChange;
    }
    if (cost > 1 && traits.ignoreTerrain) cost = 1;
    s.kind = StepKind::Land;
    s.cost = cost;
    return s;
}

Game::StepFrom Game::stepFrom(Hex from) const {
    const Plot& p = state_.plot(from);
    StepFrom f{};
    f.water = terrainOf(*rules_, p).water;
    f.afloat = f.water && p.route < 0;  // not on a land bridge
    f.road = p.route >= 0 && !p.routePillaged ? p.route : int8_t{-1};
    f.riverEdges = p.riverEdges;
    return f;
}

std::optional<Fixed> Game::stepCost(const StepUnit& su, const StepInto& to, const StepFrom& from, Dir dir) const {
    switch (to.kind) {
        case StepKind::None: return std::nullopt;
        case StepKind::Sail: return Fixed::fromInt(1);
        case StepKind::Port: return from.water ? std::optional<Fixed>(Fixed::fromInt(1)) : std::nullopt;
        // Amphibious (05): embarking and disembarking cost nothing extra.
        case StepKind::Embark: return Fixed::fromInt(from.afloat || su.traits->freeEmbark ? 1 : su.embarkCost + 1);  // embarking: 2 plus the water tile
        case StepKind::Zeal: return Fixed::fromInt(from.afloat ? su.embarkCost + 1 : 1);
        case StepKind::Land: break;
    }
    if (from.afloat) return Fixed::fromInt((su.traits->freeEmbark ? 0 : su.embarkCost) + std::max(to.cost, 1));  // disembarking
    // Along a road the road's cost replaces the terrain's; later roads bridge rivers (01: Routes). A unit that ignores
    // rivers (Amphibious, the Helicopter) crosses them at no extra cost either way (05).
    const bool river = !su.traits->noRiver && riverEdgeBetween(from.riverEdges, to.riverEdges, dir);
    if (to.road >= 0 && from.road >= 0) {
        const RouteType& slow = rules_->routes[static_cast<size_t>(std::min(to.road, from.road))];
        Fixed rc = slow.moveCost;
        if (!slow.bridges && river) rc += Fixed::fromInt(su.riverCost);
        return rc;
    }
    int cost = to.cost;
    if (river) cost += su.riverCost;
    return Fixed::fromInt(std::max(cost, 1));
}

std::optional<std::vector<PathStep>> Game::findPath(UnitId id, Hex target, bool overland) const {
    return searchPath(id, target, overland, nullptr);
}

const Unit& Game::orderMover(const Unit& unit) const {
    if (unit.escorting != kNoUnit) {
        const Unit* l = state_.unit(unit.escorting);
        if (l && l->pos == unit.pos && l->owner == unit.owner) return *l;
    }
    return unit;
}

std::vector<uint8_t> Game::moveReach(UnitId id, bool overland) const {
    std::vector<uint8_t> reach(static_cast<size_t>(state_.grid.size()), 0);
    const Unit* u = state_.unit(id);
    // Every plot a step can take the mover to (a search's goal changes only when it stops), each one a search for it
    // would find a path to.
    if (u) searchPath(orderMover(*u).id, u->pos, overland, nullptr, &reach);
    return reach;
}

std::optional<std::vector<PathStep>> Game::searchPath(UnitId id, Hex target, bool overland, const std::vector<PathStep>* along,
                                                      std::vector<uint8_t>* reach) const {
    const Unit* u = state_.unit(id);
    if (!u) return std::nullopt;
    auto t = state_.grid.normalize(target);
    if (!t) return std::nullopt;
    const Player& owner = state_.players[static_cast<size_t>(u->owner)];
    auto known = [&](int index) { return owner.visibility[static_cast<size_t>(index)] != static_cast<uint8_t>(Visibility::Unrevealed); };
    const int start = state_.grid.index(u->pos);
    const int goal = reach ? -1 : state_.grid.index(*t);  // none for a reach
    if (goal >= 0 && !known(goal)) return std::nullopt;
    const bool keepDry = overland && typeOf(*rules_, *u).domain == Domain::Land && !isEmbarked(*u);
    const MoveTraits traits = moveTraits(*u);
    MoveLimits limits = moveLimits(*u, traits);
    StepUnit su = stepUnit(*u, traits);

    // Per plot: the best arrival found (read only where its flags have kReached): its turn, the plot it came from (-1
    // for the start) and the pass it came on (-1 for none, below), and the moves left; and the plot as a step's end
    // (worked out the first time a step reaches it, kRead): whether a step may end there at all, or only from the
    // owner's own land (closed borders), the rest of the step's cost that it decides, and its first pass. The flags are
    // moveLimits' marks (a plot kept out of, kBlocked, or another player's unit at peace holds, kPass | kTheirs), with
    // enemy ZOC (kZoc) and plots our own units of the mover's layer hold (kPass) marked in too (or, along a path, every
    // plot off it blocked).
    enum : uint8_t { kBlocked = MoveLimits::kKeepOut, kZoc = 2, kReached = 4, kRead = 8, kPass = MoveLimits::kPassOnly, kTheirs = MoveLimits::kTheirs };
    enum : uint8_t { kNever = 0, kAny, kInside };
    struct Cell {
        int turn;
        int prev;
        int prevPass;
        int firstPass;
        int64_t moves;
        StepInto into;
        uint8_t entry;
        int8_t owner;
    };
    const size_t plots = static_cast<size_t>(state_.grid.size());
    const std::unique_ptr<Cell[]> cells(new Cell[plots]);
    std::vector<uint8_t> flags = std::move(limits.blocked);
    // A unit may pass its owner's units of its layer, and other players' units at peace, but never end a turn on one
    // (05: Stacking). The order's goal is planned as any other plot where one of ours holds it (the order ends before it
    // while one does: advanceUnit), and is out of reach where another player's unit does.
    if (const UnitLayer layer = typeOf(*rules_, *u).layer; layer != UnitLayer::Air) {
        for (const Unit& o : state_.units) {
            if (o.owner != u->owner || o.id == u->id || typeOf(*rules_, o).layer != layer || state_.grid.normalize(o.pos) != o.pos) continue;
            flags[static_cast<size_t>(state_.grid.index(o.pos))] |= kPass;
        }
    }
    // A leader moves with its escort, and the pair shares no plot with another military unit (advanceUnit): it keeps
    // out of other players' units.
    if (isLeader(*u) && escortOf(*u)) {
        for (uint8_t& f : flags) {
            if (f & kTheirs) f |= kBlocked;
        }
    }
    if (goal >= 0) {
        uint8_t& g = flags[static_cast<size_t>(goal)];
        if (g & kTheirs) g |= kBlocked;
        else g &= static_cast<uint8_t>(~kPass);
    }
    if (along) {
        std::vector<uint8_t> walk(plots, kBlocked);
        for (const PathStep& s : *along) {
            const size_t i = static_cast<size_t>(state_.grid.index(s.pos));
            walk[i] = flags[i];
        }
        flags.swap(walk);
    }
    auto read = [&](int index, Hex h) -> const Cell& {
        Cell& c = cells[static_cast<size_t>(index)];
        uint8_t& f = flags[static_cast<size_t>(index)];
        if (f & kRead) return c;
        f |= kRead;
        c.firstPass = -1;
        c.entry = kNever;
        c.owner = kNoPlayer;
        // A step never ends on a plot the owner has not seen, on the water when kept dry, on a plot kept out of (a unit
        // of a player at war, a foreign city, a standing enemy Encampment), or across borders closed to it.
        if (!known(index) || (f & kBlocked)) return c;
        c.into = stepInto(su, h);
        if (c.into.kind == StepKind::None || (keepDry && c.into.afloat)) return c;
        c.entry = kAny;
        if (const PlayerId land = state_.plot(h).owner; land != kNoPlayer) {
            const uint8_t closed = limits.closed[static_cast<size_t>(land)];
            if (closed & 2) c.entry = kNever;
            else if (closed & 1) c.entry = kInside;
            c.owner = land;
        }
        return c;
    };
    // A step into the plot `to` (at `index`) from a plot read as `sf` and held by `fromOwner`, `dir` from the one to
    // the other: its cost, or none.
    auto step = [&](const StepFrom& sf, PlayerId fromOwner, int index, Hex to, Dir dir) -> std::optional<Fixed> {
        const Cell& c = read(index, to);
        if (c.entry == kNever || (c.entry == kInside && fromOwner != c.owner)) return std::nullopt;
        return stepCost(su, c.into, sf, dir);
    };
    if (goal >= 0 && goal != start) {
        // A goal no step can enter is out of reach, which the search would only learn by visiting every plot it can
        // reach. The steps into it are those from its neighbours (seen or not), each the opposite way to the one
        // the neighbour lies.
        bool enterable = false;
        for (int d = 0; d < kNumDirs && !enterable; ++d) {
            const std::optional<Hex> from = state_.grid.neighbor(*t, static_cast<Dir>(d));
            if (!from) continue;
            enterable = step(stepFrom(*from), state_.plot(*from).owner, goal, *t, opposite(static_cast<Dir>(d))).has_value();
        }
        if (!enterable) return std::nullopt;
    }
    const Fixed fullMoves = Fixed::fromInt(maxMoves(*u));
    // Enemy ZOC changes only the moves a path leaves, not whether there is one (all a search along a path or a reach
    // asks), except on plots the mover may only pass, where a move it ends could not stay: such a search marks it the
    // first time it steps onto one.
    bool zocMarked = !along && !reach;
    if (zocMarked) markZoc(*u, flags, kZoc);

    auto better = [](int turn, Fixed moves, const Cell& than) {
        return turn != than.turn ? turn < than.turn : moves.raw() > than.moves;
    };
    struct QItem { int64_t negMoves; int turn; int index; };
    auto cmp = [](const QItem& a, const QItem& b) {
        if (a.turn != b.turn) return a.turn > b.turn;
        if (a.negMoves != b.negMoves) return a.negMoves > b.negMoves;
        return a.index > b.index;
    };
    std::vector<QItem> queued;
    queued.reserve(64);
    std::priority_queue<QItem, std::vector<QItem>, decltype(cmp)> open(cmp, std::move(queued));
    {
        Cell& s = cells[static_cast<size_t>(start)];
        s.turn = 0;
        s.prev = -1;
        s.prevPass = -1;
        s.moves = u->movesLeft.raw();
        flags[static_cast<size_t>(start)] |= kReached;
    }
    open.push({-u->movesLeft.raw(), 0, start});
    const bool land = su.domain == Domain::Land;
    // A unit standing where it may only pass (passing a unit there) may not wait there either.
    const bool passing = flags[static_cast<size_t>(start)] & kPass;
    auto relax = [&](int next, int turn, Fixed mp, int prev, int prevPass) {
        uint8_t& f = flags[static_cast<size_t>(next)];
        Cell& nb = cells[static_cast<size_t>(next)];
        if (!(f & kReached) || better(turn, mp, nb)) {
            nb.turn = turn;
            nb.prev = prev;
            nb.prevPass = prevPass;
            nb.moves = mp.raw();
            f |= kReached;
            open.push({-mp.raw(), turn, next});
        }
    };
    // Passes over plots the mover may only pass, which a step crosses within one turn. One plot can hold several, as an
    // earlier arrival there does not stand for a later one with more moves: no turn ends there to wait for them. Each
    // keeps the step before it (a plot, and the pass it was on, or -1), and the next pass over the same plot.
    struct Pass {
        int plot;
        int prev;
        int prevPass;
        int nextHere;
        int turn;
        int64_t moves;
    };
    std::vector<Pass> passes;
    std::vector<int> todo;
    auto pass = [&](int at, int prev, int prevPass, int turn, Fixed mp) {
        // An order to a plot of ours gets that far (and ends there); one to another player's unit is out of reach.
        if (!(flags[static_cast<size_t>(at)] & kTheirs)) flags[static_cast<size_t>(at)] |= kReached;
        if (mp <= Fixed()) return;  // its moves would end there
        Cell& c = cells[static_cast<size_t>(at)];
        for (int k = c.firstPass; k >= 0; k = passes[static_cast<size_t>(k)].nextHere) {
            if (passes[static_cast<size_t>(k)].turn <= turn && passes[static_cast<size_t>(k)].moves >= mp.raw()) return;
        }
        passes.push_back({at, prev, prevPass, c.firstPass, turn, mp.raw()});
        c.firstPass = static_cast<int>(passes.size()) - 1;
        todo.push_back(c.firstPass);
    };
    // The steps out of the passes in `todo`: only those its moves pay for now (or that embark or disembark).
    auto crossPasses = [&] {
        while (!todo.empty()) {
            const int k = todo.back();
            todo.pop_back();
            const Pass p = passes[static_cast<size_t>(k)];  // a copy, as `passes` grows below
            const Cell& c = cells[static_cast<size_t>(p.plot)];
            const StepFrom sf{c.into.water, c.into.afloat, c.into.road, c.into.riverEdges};
            const Hex from = state_.grid.at(p.plot);
            for (int d = 0; d < kNumDirs; ++d) {
                auto n = state_.grid.neighbor(from, static_cast<Dir>(d));
                if (!n) continue;
                const int next = state_.grid.index(*n);
                auto cost = step(sf, c.owner, next, *n, static_cast<Dir>(d));
                if (!cost) continue;
                Fixed mp = Fixed::fromRaw(p.moves);
                if (mp < *cost && mp != fullMoves && !(land && sf.afloat != cells[static_cast<size_t>(next)].into.afloat)) continue;
                mp = mp >= *cost ? mp - *cost : Fixed();
                const uint8_t f = flags[static_cast<size_t>(next)];
                if (f & kZoc) mp = Fixed();
                if (f & kPass) pass(next, p.plot, k, p.turn, mp);
                else relax(next, p.turn, mp, p.plot, k);
            }
        }
    };
    while (!open.empty()) {
        QItem q = open.top();
        open.pop();
        const Cell& cur = cells[static_cast<size_t>(q.index)];
        const int curTurn = cur.turn;
        const int64_t curMoves = cur.moves;
        if (curTurn != q.turn || -curMoves != q.negMoves) continue;  // stale
        if (q.index == goal) break;
        const Hex from = state_.grid.at(q.index);
        // A plot reached by a step was read as its end, which holds what a step from it reads.
        const bool first = q.index == start;
        const StepFrom sf = first ? stepFrom(from) : StepFrom{cur.into.water, cur.into.afloat, cur.into.road, cur.into.riverEdges};
        const PlayerId fromOwner = first ? state_.plot(from).owner : cur.owner;
        for (int d = 0; d < kNumDirs; ++d) {
            auto n = state_.grid.neighbor(from, static_cast<Dir>(d));
            if (!n) continue;
            const int next = state_.grid.index(*n);
            auto cost = step(sf, fromOwner, next, *n, static_cast<Dir>(d));
            if (!cost) continue;
            int turn = curTurn;
            Fixed mp = Fixed::fromRaw(curMoves);
            if (mp <= Fixed()) {
                ++turn;
                mp = fullMoves;
            }
            // Embarking or disembarking is allowed with any movement left (it uses it up).
            if (mp < *cost && mp != fullMoves && !(land && sf.afloat != cells[static_cast<size_t>(next)].into.afloat)) {
                if (first && passing) continue;
                ++turn;
                mp = fullMoves;
            }
            mp = mp >= *cost ? mp - *cost : Fixed();
            if (!(flags[static_cast<size_t>(next)] & kPass)) {
                if (flags[static_cast<size_t>(next)] & kZoc) mp = Fixed();  // entering enemy ZOC ends the move
                relax(next, turn, mp, q.index, -1);
                continue;
            }
            if (!zocMarked) {
                markZoc(*u, flags, kZoc);
                zocMarked = true;
            }
            const bool zoc = flags[static_cast<size_t>(next)] & kZoc;
            pass(next, q.index, -1, turn, zoc ? Fixed() : mp);
            // Or first wait here for a new turn's moves, which may get it past where these do not.
            if (turn == curTurn && !(first && passing) && Fixed::fromRaw(curMoves) != fullMoves) {
                pass(next, q.index, -1, turn + 1, zoc || fullMoves < *cost ? Fixed() : fullMoves - *cost);
            }
            crossPasses();
        }
    }
    if (reach) {
        for (size_t i = 0; i < plots; ++i) (*reach)[i] = (flags[i] & kReached) ? 1 : 0;
        return std::nullopt;
    }
    if (!(flags[static_cast<size_t>(goal)] & kReached)) return std::nullopt;
    std::vector<PathStep> path;
    for (int i = goal, k = -1; i != -1;) {
        if (k >= 0) {
            const Pass& p = passes[static_cast<size_t>(k)];
            path.push_back({state_.grid.at(i), p.turn, Fixed::fromRaw(p.moves)});
            i = p.prev;
            k = p.prevPass;
        } else {
            const Cell& n = cells[static_cast<size_t>(i)];
            path.push_back({state_.grid.at(i), n.turn, Fixed::fromRaw(n.moves)});
            i = n.prev;
            k = n.prevPass;
        }
    }
    std::reverse(path.begin(), path.end());
    return path;
}

void Game::advanceUnit(UnitId id) {
    std::optional<std::vector<PathStep>> path;  // the path the unit's last step took
    for (int guard = 0; guard < 64; ++guard) {
        Unit* u = state_.unit(id);
        if (!u || !u->moveTarget) return;
        if (u->pos == *u->moveTarget) {
            u->moveTarget.reset();
            return;
        }
        // A move just checked by submit takes the path its check found for this unit: the unit's orders and activity,
        // all that changed since, do not enter the search.
        const bool checked = checkedPath_ && checkedPath_->unit == id;
        // Out of moves after a step, the unit goes on next turn unless no path is left (which ends the order): a path
        // over just the plots of the last step's path shows one is left.
        if (!checked && path && u->movesLeft <= Fixed()) {
            const std::optional<std::vector<PathStep>> rest = searchPath(id, *u->moveTarget, u->moveOverland, &*path);
            if (rest && rest->size() >= 2) {
                checkedPath_.reset();
                return;
            }
        }
        if (checked) path = std::move(checkedPath_->steps);
        else if (std::optional<std::vector<PathStep>> found = findPath(id, *u->moveTarget, u->moveOverland)) path = std::move(found);
        else if (path && path->size() > 2 && (*path)[1].pos == u->pos && passOnly(*u, u->pos) && moveCost(*u, u->pos, (*path)[2].pos)) {
            // Among units it may only pass, with no path found now, it goes on past them along the path it took: a
            // turn's full moves can hang on the plot it starts on (the Heavy Chariot's open ground), which the search
            // reads where the unit stands.
            path->erase(path->begin());
        } else path.reset();
        checkedPath_.reset();
        if (!path || path->size() < 2) {
            u->moveTarget.reset();
            return;
        }
        const Hex next = (*path)[1].pos;
        const Fixed cost = *moveCost(*u, u->pos, next);
        const Fixed fullMoves = Fixed::fromInt(maxMoves(*u));
        if (u->movesLeft <= Fixed()) return;
        if (u->movesLeft < cost && u->movesLeft != fullMoves && !isEmbarkTransition(*u, u->pos, next)) return;  // continue next turn
        const Fixed after = u->movesLeft >= cost ? u->movesLeft - cost : Fixed();
        if (passOnly(*u, next)) {
            // Our units of its layer and other players' units at peace may be passed but not shared at the end of a
            // move: the unit steps among them only to get past them this turn, as the search plans it (or waits for
            // them to move on), and an order whose end they hold ends here.
            const int past = passOn(*u, *path);
            if (next == *u->moveTarget || past < 0) {
                u->moveTarget.reset();
                return;
            }
            if (past == 0) return;
        }
        // A linked escort steps with its leader, at the slower unit's pace.
        Unit* escort = isLeader(*u) ? escortMut(*u) : nullptr;
        Fixed escortAfter;
        if (escort) {
            const auto ecost = moveCost(*escort, escort->pos, next);
            const Fixed efull = Fixed::fromInt(maxMoves(*escort));
            if (!ecost || escort->movesLeft <= Fixed() ||
                (escort->movesLeft < *ecost && escort->movesLeft != efull && !isEmbarkTransition(*escort, escort->pos, next)))
                return;
            const Unit* blocker = state_.unitAt(next, UnitLayer::Military, *rules_);
            if (blocker && blocker->id != escort->id) {
                u->moveTarget.reset();  // the pair cannot share a plot with another military unit
                return;
            }
            escortAfter = escort->movesLeft >= *ecost ? escort->movesLeft - *ecost : Fixed();
        }
        if (escort) {
            escort->pos = next;
            escort->movesLeft = inEnemyZoc(*escort, next) ? Fixed() : escortAfter;
            escort->activity = Activity::Awake;
            escort->moved = true;
            escort->fortifyTurns = 0;
            escort->moveTarget.reset();
            enterPlot(*escort);
            u = state_.unit(id);  // enterPlot may change the unit list
        }
        // A carrier takes its aircraft along (05: Naval carrier).
        if (const int slots = typeOf(*rules_, *u).airSlots; slots > 0) {
            int carried = 0;
            for (Unit& o : state_.units) {
                if (carried < slots && o.pos == u->pos && o.owner == u->owner && isAircraft(o)) {
                    o.pos = next;
                    ++carried;
                }
            }
        }
        u->pos = next;
        u->movesLeft = inEnemyZoc(*u, next) ? Fixed() : after;
        u->activity = Activity::Awake;
        u->moved = true;
        u->fortifyTurns = 0;
        const PlayerId mover = u->owner;
        enterPlot(*u);  // which may move the unit list (a goody hut's unit)
        refreshVisibility(mover);
    }
}

bool Game::passOnly(const Unit& unit, Hex plot) const {
    const UnitLayer layer = typeOf(*rules_, unit).layer;
    for (const Unit& o : state_.units) {
        if (o.pos == plot && o.id != unit.id && (o.owner != unit.owner || typeOf(*rules_, o).layer == layer)) return true;
    }
    return false;
}

int Game::passOn(const Unit& unit, const std::vector<PathStep>& path) const {
    const Fixed full = Fixed::fromInt(maxMoves(unit));
    Fixed moves = unit.movesLeft;
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        const Hex from = path[i].pos;
        const Hex to = path[i + 1].pos;
        if (i > 0 && !passOnly(unit, from)) return 1;
        const std::optional<Fixed> cost = moveCost(unit, from, to);
        if (!cost || moves <= Fixed() || (moves < *cost && moves != full && !isEmbarkTransition(unit, from, to))) return 0;
        moves = moves >= *cost ? moves - *cost : Fixed();
        if (inEnemyZoc(unit, to)) moves = Fixed();
    }
    return passOnly(unit, path.back().pos) ? -1 : 1;
}

// ---------------------------------------------------------------- visibility

void Game::refreshVisibility(PlayerId pid) {
    Player& p = state_.players[static_cast<size_t>(pid)];
    if (p.barbarian) {
        // Barbarians see the whole map (they only need it to pick targets).
        std::fill(p.visibility.begin(), p.visibility.end(), static_cast<uint8_t>(Visibility::Visible));
        return;
    }
    // Written as a select rather than a conditional store so the compiler can do many plots at once.
    for (uint8_t& v : p.visibility) v = v == static_cast<uint8_t>(Visibility::Visible) ? static_cast<uint8_t>(Visibility::Revealed) : v;
    std::vector<TypeIndex> discovered;  // natural wonders this player sees for the first time (01)
    std::vector<int16_t> landfalls;     // landmasses it sees for the first time (09: a new continent)
    std::vector<int16_t> knownLand;     // landmasses it had seen before, gathered at the first new land plot
    bool landGathered = false;
    Unit* finder = nullptr;
    // A plot seen for the first time: the landmass or natural wonder on it may be new to the player.
    const auto firstSight = [&](Hex target) {
        if (const int16_t k = state_.plot(target).continent; k >= 0) {
            if (!landGathered) {
                // Each landmass with a plot revealed, looked for until its first.
                for (size_t i = 0; i + 1 < landFirst_.size(); ++i) {
                    for (int32_t j = landFirst_[i]; j < landFirst_[i + 1]; ++j) {
                        const size_t at = static_cast<size_t>(landPlots_[static_cast<size_t>(j)]);
                        if (at >= p.visibility.size()) break;  // the rest lie further on
                        if (p.visibility[at] == static_cast<uint8_t>(Visibility::Unrevealed)) continue;
                        knownLand.push_back(state_.plots[at].continent);
                        break;
                    }
                }
                landGathered = true;
            }
            if (std::find(knownLand.begin(), knownLand.end(), k) == knownLand.end()) {
                knownLand.push_back(k);
                landfalls.push_back(k);
            }
        }
        const TypeIndex f = state_.plot(target).feature;
        if (f != kNone && rules_->features[static_cast<size_t>(f)].naturalWonder && std::find(discovered.begin(), discovered.end(), f) == discovered.end()) {
            bool known = false;
            for (int i = 0; i < state_.grid.size() && !known; ++i) {
                known = state_.plots[static_cast<size_t>(i)].feature == f && p.visibility[static_cast<size_t>(i)] != static_cast<uint8_t>(Visibility::Unrevealed);
            }
            if (!known) {
                discovered.push_back(f);
                if (finder && !typeOf(*rules_, *finder).promotionClass.empty()) finder->xp += rules_->globalInt("EXPERIENCE_REVEAL_NATURAL_WONDER");
            }
        }
    };
    // Where each look so far was from, how far it reached and whether it saw through features: a later look from the
    // same plot, reaching no further the same way, sees nothing the earlier one did not, and is skipped.
    struct Look {
        Hex from;
        int range;
        bool throughFeatures;
    };
    std::vector<Look> looks;
    auto see = [&](Hex from, int range, bool throughFeatures = false) {
        for (const Look& l : looks) {
            if (l.from == from && l.throughFeatures == throughFeatures && l.range >= range) return;
        }
        looks.push_back({from, range, throughFeatures});
        const TerrainType& viewer = terrainOf(*rules_, state_.plot(from));
        range += viewer.sightModifier;
        // lineOfSight(from, target), with the step to the target known from the walk.
        const auto clear = [&](Hex h) { return sightObstacle(state_, *rules_, h, throughFeatures) <= viewer.sightThrough; };
        state_.grid.forEachWithinStep(from, range, [&](Hex target, Axial step) {
            uint8_t& v = p.visibility[static_cast<size_t>(state_.grid.index(target))];
            if (v == static_cast<uint8_t>(Visibility::Visible)) return;  // already seen in this refresh: nothing more to learn
            if (!state_.grid.betweenStep(from, step, clear)) return;
            if (v == static_cast<uint8_t>(Visibility::Unrevealed)) firstSight(target);
            v = static_cast<uint8_t>(Visibility::Visible);
        });
    };
    // Military alliance, level 2: allies see what each other sees (08: alliance levels).
    std::vector<uint8_t> sharing(state_.players.size(), 0);  // by player, worked out once for the refresh
    for (size_t o = 0; o < sharing.size(); ++o) {
        const PlayerId other = static_cast<PlayerId>(o);
        sharing[o] = other == pid || (alliance(pid, other) == AllianceType::Military && allianceLevel(pid, other) >= 2) ? 1 : 0;
    }
    const auto shares = [&](PlayerId o) { return o == pid || (o >= 0 && static_cast<size_t>(o) < sharing.size() && sharing[static_cast<size_t>(o)] != 0); };
    for (Unit& u : state_.units) {
        if (!shares(u.owner)) continue;
        finder = u.owner == pid ? &u : nullptr;
        const std::pair<int, bool> sight = unitSightAndSentry(u);
        see(u.pos, sight.first, sight.second);  // Sentry (05)
    }
    finder = nullptr;
    for (const City& c : state_.cities) {
        if (shares(c.owner)) see(c.pos, rules_->globalInt(HotGlobal::CitySightRange));
        // An Encampment watches its strike range (Sovereign reading; 03: Defense).
        if (const CityDistrict* camp = shares(c.owner) ? encampmentOf(c) : nullptr) see(camp->pos, rules_->districts[static_cast<size_t>(camp->type)].attackRange);
    }
    // Diplomatic access (08): Secret shows a civ's capital, Top Secret all its cities. Worked out at a civ's first
    // city and kept for the rest (none of it changes here), 0 for one that is not a major civ.
    std::vector<int8_t> accessTo(state_.players.size(), -1);
    for (const City& c : state_.cities) {
        if (c.owner == pid || c.owner < 0 || static_cast<size_t>(c.owner) >= accessTo.size()) continue;
        int8_t& access = accessTo[static_cast<size_t>(c.owner)];
        if (access < 0) access = static_cast<int8_t>(isMajorCiv(c.owner) ? accessLevel(pid, c.owner) : 0);
        if (access >= 4 || (access >= 3 && c.capital)) {
            for (const Hex& h : state_.grid.within(c.pos, 1)) p.visibility[static_cast<size_t>(state_.grid.index(h))] = static_cast<uint8_t>(Visibility::Visible);
        }
    }
    for (const Agent& a : state_.agents) {
        const City* c = a.spy && a.owner == pid && a.travel == 0 ? state_.city(a.city) : nullptr;
        if (!c) continue;
        for (const Hex& h : state_.grid.within(c->pos, a.mission == SpyMission::ListeningPost ? 2 : 1))
            p.visibility[static_cast<size_t>(state_.grid.index(h))] = static_cast<uint8_t>(Visibility::Visible);
    }
    // A natural wonder discovered (01; 09: historic moments): era score (more for the world's first), and
    // the Astrology Eureka.
    for (TypeIndex f : discovered) {
        bool first = true;
        for (const Player& o : state_.players) {
            if (o.id == pid || !isMajorCiv(o.id)) continue;
            for (size_t i = 0; i < state_.plots.size() && first; ++i) {
                first = !(state_.plots[i].feature == f && i < o.visibility.size() && o.visibility[i] != static_cast<uint8_t>(Visibility::Unrevealed));
            }
        }
        awardMoment(pid, first ? "MOMENT_FIRST_DISCOVERY_OF_A_NATURAL_WONDER" : "MOMENT_DISCOVERY_OF_A_NATURAL_WONDER");
        dedicationScore(pid, "DEDICATION_HIC_SUNT_DRACONES", 3);
        // Kandy (08: suzerain): a Relic, in a free slot, for each natural wonder its suzerain discovers.
        if (suzerainBonus(pid, Cs::Kandy)) {
            GreatPersonEffect relic;
            relic.kind = GreatPersonEffectKind::Relic;
            relic.amount = 1;
            applyEffectAt(pid, nullptr, Hex{}, relic);
        }
        eventBoost(pid, BoostKind::NaturalWonder);  // 04: Astrology
    }
    // A new continent discovered (09: historic moments; Sovereign: every landmass is one). It counts once
    // the civ has a capital, whose continent it has seen by then: the world's first civ to see the land
    // earns the moment, and Hic Sunt Dracones +3 for each.
    bool capital = false;
    for (size_t i = 0; i < state_.cities.size() && !landfalls.empty() && !capital; ++i) capital = state_.cities[i].owner == pid && state_.cities[i].capital;
    if (capital && isMajorCiv(pid)) {
        for (int16_t k : landfalls) {
            bool first = true;
            for (const Player& o : state_.players) {
                if (o.id == pid || !isMajorCiv(o.id)) continue;
                for (size_t i = 0; i < state_.plots.size() && i < o.visibility.size() && first; ++i)
                    first = !(state_.plots[i].continent == k && o.visibility[i] != static_cast<uint8_t>(Visibility::Unrevealed));
            }
            if (first) awardMoment(pid, "MOMENT_FIRST_DISCOVERY_OF_A_NEW_CONTINENT");
            dedicationScore(pid, "DEDICATION_HIC_SUNT_DRACONES", 3);
        }
    }
}

// Nothing between the two plots stands higher than the viewer's plot (01: Visibility).
bool Game::lineOfSight(Hex from, Hex to, bool throughFeatures) const {
    const int viewerHeight = terrainOf(*rules_, state_.plot(from)).sightThrough;
    return state_.grid.between(from, to, [&](Hex h) { return sightObstacle(state_, *rules_, h, throughFeatures) <= viewerHeight; });
}

// ------------------------------------------------------------------ applying

CommandError Game::submit(const Command& c) {
    std::optional<CheckedPath> movePath;
    CommandError e = validate(c, &movePath);
    if (e != CommandError::Ok) return e;
    log_.push_back(c);
    checkedPath_ = std::move(movePath);  // a move's first step sees what the check saw
    apply(c);
    checkedPath_.reset();
    spawnCaptures();
    checkAirBases();
    updateBoosts(c.player);
    checkVictory();
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
            if (u->activity != Activity::Fortify) u->fortifyTurns = 0;
            break;
        }
        case CommandType::EndTurn: applyEndTurn(c); break;
        case CommandType::SetProduction:
        case CommandType::QueueProduction:
        case CommandType::Purchase:
        case CommandType::BuyPlot:
        case CommandType::LockPlot:
        case CommandType::SetCityFocus: applyCity(c); break;
        case CommandType::ChooseResearch:
        case CommandType::ChooseCivic:
        case CommandType::ChangeGovernment:
        case CommandType::SetPolicy:
        case CommandType::BuyPolicyChanges: applyResearch(c); break;
        case CommandType::BuildImprovement:
        case CommandType::Harvest: applyBuilder(c); break;
        case CommandType::BuildIndustry: applyIndustry(c); break;
        case CommandType::DeclareWar:
        case CommandType::MakePeace:
        case CommandType::Attack:
        case CommandType::RangedAttack:
        case CommandType::Promote:
        case CommandType::CityStrike:
        case CommandType::RazeCity:
        case CommandType::LiberateCity: applyCombat(c); break;
        case CommandType::EquipGear:
        case CommandType::LinkEscort:
        case CommandType::ChooseSuccessor:
        case CommandType::AbandonLeader:
        case CommandType::SendAssassin:
        case CommandType::CityStance: applyLeader(c); break;
        case CommandType::BattleResult:
        case CommandType::AutoResolveBattle: applyBattle(c); break;
        case CommandType::PatronizeGreatPerson:
        case CommandType::PassGreatPerson:
        case CommandType::ActivateGreatPerson: applyGreatPeople(c); break;
        case CommandType::FoundPantheon:
        case CommandType::FoundReligion:
        case CommandType::EvangelizeBelief:
        case CommandType::SpreadReligion:
        case CommandType::LaunchInquisition:
        case CommandType::HealReligious: applyReligion(c); break;
        case CommandType::StartTradeRoute: applyTradeRoute(c); break;
        case CommandType::ProposeDeal:
        case CommandType::AnswerDeal:
        case CommandType::Denounce:
        case CommandType::RecordTalk: applyDiplomacy(c); break;
        case CommandType::AppointGovernor:
        case CommandType::PromoteGovernor:
        case CommandType::AssignGovernor: applyGovernor(c); break;
        case CommandType::Airlift:
        case CommandType::Paradrop:
        case CommandType::RebaseUnit: {
            Unit& u = *state_.unit(c.id);
            u.pos = *state_.grid.normalize(c.target);
            u.movesLeft = Fixed();  // rebasing takes the aircraft's turn
            u.activity = Activity::Awake;
            u.moved = true;
            refreshVisibility(c.player);
            break;
        }
        case CommandType::LaunchWmd: launchWmd(c); break;
        case CommandType::Pillage:
            if (c.arg == 1) pillage(c.id, c.target);
            else pillage(c.id);
            break;
        case CommandType::Excavate: excavate(c.id); break;
        case CommandType::DesignatePark: designatePark(c.id); break;
        case CommandType::PerformConcert: performConcert(c.id); break;
        case CommandType::ChooseDedication: chooseDedication(c.player, static_cast<TypeIndex>(c.arg)); break;
        case CommandType::MoveGreatWork: moveGreatWork(c.id, c.arg, c.arg2, static_cast<TypeIndex>(c.target.x)); break;
        case CommandType::SendDelegation: sendDelegation(c.player, static_cast<PlayerId>(c.arg), c.arg2 != 0); break;
        case CommandType::AskPromise: askPromise(c.player, static_cast<PlayerId>(c.arg), static_cast<PromiseKind>(c.arg2)); break;
        case CommandType::FormUnit: {
            Unit& u = *state_.unit(c.id);
            const Unit& w = *state_.unit(c.arg);
            ++u.formation;
            {
                const bool naval = rules_->units[static_cast<size_t>(u.type)].domain == Domain::Sea;
                if (u.formation == 1) awardFirst(c.player, naval ? "MOMENT_WORLD_S_FIRST_FLEET" : "MOMENT_WORLD_S_FIRST_CORPS", naval ? "MOMENT_FIRST_FLEET" : "MOMENT_FIRST_CORPS", 0);
                else awardFirst(c.player, naval ? "MOMENT_WORLD_S_FIRST_ARMADA" : "MOMENT_WORLD_S_FIRST_ARMY", naval ? "MOMENT_FIRST_ARMADA" : "MOMENT_FIRST_ARMY", 0);
            }
            u.hp = std::max(u.hp, w.hp);  // the stronger of the two carries on
            u.xp = std::max(u.xp, w.xp);
            u.movesLeft = Fixed();        // forming takes the turn
            u.moveTarget.reset();
            removeUnit(c.arg);
            refreshVisibility(c.player);
            break;
        }
        case CommandType::RepairImprovement: {
            Unit& u = *state_.unit(c.id);
            state_.plot(u.pos).pillagedTurns = 0;
            state_.plot(u.pos).routePillaged = false;
            u.movesLeft = Fixed();  // repairing takes the Builder's turn, not a charge
            u.moveTarget.reset();
            if (City* city = state_.city(state_.plot(u.pos).city)) assignCitizens(*city);
            railroadMoment(c.player, u.pos);  // mended track can join two cities again
            break;
        }
        case CommandType::ContributeCharge: {
            // The district's share of its cost goes into the city's progress on it (03: Military Engineer); with the
            // Royal Society, a Builder's share of a project goes into the project (03).
            Unit& u = *state_.unit(c.id);
            City& city = *state_.city(state_.plot(u.pos).city);
            const std::optional<ProductionItem> project = chargedProject(u);
            const CityDistrict* d = project ? nullptr : state_.districtAt(u.pos);
            const ProductionItem item = project ? *project : ProductionItem{ProductionKind::District, d->type};
            const Fixed share = project ? Fixed::fromInt(productionCost(c.player, item, &city)) * projectChargePercent(c.player) / 100
                                        : Fixed::fromInt(productionCost(c.player, item) * rules_->districts[static_cast<size_t>(d->type)].chargePercent / 100);
            auto it = std::find_if(city.progress.begin(), city.progress.end(), [&](const ProductionProgress& pp) { return pp.item == item; });
            if (it == city.progress.end()) city.progress.push_back({item, share});
            else it->amount += share;
            u.movesLeft = Fixed();
            u.moveTarget.reset();
            if (--u.charges <= 0) removeUnit(c.id);
            break;
        }
        case CommandType::BuildRailroad: {
            Unit& u = *state_.unit(c.id);
            const RouteType& rr = rules_->routes[static_cast<size_t>(railroad())];
            Player& p = state_.players[static_cast<size_t>(c.player)];
            for (const auto& [res, n] : rr.resourceCost) p.stockpile[static_cast<size_t>(res)] -= n;
            state_.plot(u.pos).route = static_cast<int8_t>(railroad());
            state_.plot(u.pos).routePillaged = false;
            u.movesLeft = Fixed();  // laying track takes the engineer's turn
            u.moveTarget.reset();
            railroadMoment(c.player, u.pos);
            break;
        }
        case CommandType::BuildRoad: {
            Unit& u = *state_.unit(c.id);
            state_.plot(u.pos).route = static_cast<int8_t>(roadFor(c.player));
            state_.plot(u.pos).routePillaged = false;
            u.movesLeft = Fixed();  // laying a road takes the unit's turn
            u.moveTarget.reset();
            if (freeRoad(c.player, c.id)) break;  // Qin's Builders keep their charges
            if (--u.charges <= 0 && rules_->units[static_cast<size_t>(u.type)].layer != UnitLayer::Military) removeUnit(c.id);  // a Legionary stays
            break;
        }
        case CommandType::PromoteSpy:
            for (Agent& a : state_.agents) {
                if (a.id != c.id) continue;
                a.promotions.push_back(static_cast<TypeIndex>(c.arg));
                --a.promotionsPending;
            }
            break;
        case CommandType::JoinEmergency: {
            Emergency& e = state_.emergencies[static_cast<size_t>(c.arg)];
            if (e.members.size() < state_.players.size()) e.members.resize(state_.players.size(), 0);
            e.members[static_cast<size_t>(c.player)] = 1;
            break;
        }
        case CommandType::UpgradeUnit: {
            Unit& u = *state_.unit(c.id);
            Player& p = state_.players[static_cast<size_t>(c.player)];
            p.gold -= Fixed::fromInt(upgradeCost(u));
            const TypeIndex to = upgradeTarget(u);
            const UnitType& up = rules_->units[static_cast<size_t>(to)];
            if (up.strategicResource != kNone) p.stockpile[static_cast<size_t>(up.strategicResource)] -= upgradeResourceCost(u);
            u.type = to;  // keeps its health, experience and promotions; the upgrade takes its turn
            u.movesLeft = Fixed();
            u.activity = Activity::Awake;
            break;
        }
        case CommandType::CongressVote: {
            Player& p = state_.players[static_cast<size_t>(c.player)];
            p.favor -= extraVoteCost(c.target.x);
            state_.congress[static_cast<size_t>(c.id)].votes.push_back({c.player, static_cast<uint8_t>(c.arg), c.arg2, 1 + c.target.x});
            break;
        }
        case CommandType::SpyMission: {
            for (Agent& a : state_.agents) {
                if (a.id != c.id) continue;
                const SpyMission m = static_cast<SpyMission>(c.arg);
                const int speed = rules_->speeds[static_cast<size_t>(rules_->speed(state_.setup.speed))].costPercent;
                if (m == SpyMission::None) {
                    a.city = kNoCity;
                    a.travel = a.missionTurns = 0;
                } else {
                    // A new city costs the journey first (SPYOP_TRAVEL_NEW_CITY).
                    if (a.city != c.arg2) {
                        const TypeIndex travel = rules_->spyOperation("SPYOP_TRAVEL_NEW_CITY");
                        const int lies = goldenDedication(c.player, "DEDICATION_BODYGUARD_OF_LIES") ? 100 : 0;  // 09
                        a.travel = std::max(1, (travel == kNone ? 3 : rules_->spyOperations[static_cast<size_t>(travel)].turns) * speed / 100 *
                                                   100 / (100 + spyPromotionTotal(a, &SpyPromotionType::travelFaster) + lies));
                        a.city = c.arg2;
                    }
                    const SpyOperationType* op = spyOperationFor(m);
                    const TypeIndex opIndex = rules_->spyOperation(op ? op->id : "");
                    int faster = 0;
                    for (TypeIndex pr : a.promotions) faster += opIndex == kNone ? 0 : rules_->spyPromotions[static_cast<size_t>(pr)].faster[static_cast<size_t>(opIndex)];
                    if (m != SpyMission::Counterspy && m != SpyMission::ListeningPost && goldenDedication(c.player, "DEDICATION_BODYGUARD_OF_LIES")) faster += 25;  // 09
                    if (m != SpyMission::Counterspy && m != SpyMission::ListeningPost && policyIs(c.player, "POLICY_MACHIAVELLIANISM")) faster += 25;  // 04
                    a.missionTurns = std::max(1, (op ? op->turns : 8) * speed / 100 * (100 - std::min(75, faster)) / 100);
                }
                a.mission = m;
            }
            break;
        }
        case CommandType::BribeCamp:
        case CommandType::HireFromCamp:
        case CommandType::InciteCamp: applyClan(c); break;
        case CommandType::LevyMilitary: {
            const PlayerId cs = static_cast<PlayerId>(c.arg);
            Player& p = state_.players[static_cast<size_t>(c.player)];
            p.gold -= Fixed::fromInt(levyCost(c.player, cs));
            Levy lv;
            lv.player = c.player;
            lv.cityState = cs;
            lv.until = state_.turn + rules_->globalInt("LEVY_MILITARY_TURN_DURATION") * rules_->speeds[static_cast<size_t>(rules_->speed(state_.setup.speed))].costPercent / 100;
            for (Unit& u : state_.units) {
                if (u.owner != cs || rules_->units[static_cast<size_t>(u.type)].layer != UnitLayer::Military) continue;
                u.owner = c.player;
                lv.units.push_back(u.id);
            }
            // A levied unit standing in the city-state's city or among its units steps to the nearest plot it may stand
            // on (05: Stacking; Sovereign: the spec does not say where levied units go).
            for (UnitId id : lv.units) {
                Unit& u = *state_.unit(id);
                if (mayStand(u, u.pos)) continue;
                if (const std::optional<Hex> spot = standingPlotNear(u, u.pos, kLevyPlacement)) relocateUnit(u, *spot);
            }
            state_.levies.push_back(lv);
            awardMoment(c.player, "MOMENT_CITY_STATE_ARMY_LEVIED");  // 09
            refreshVisibility(c.player);
            refreshVisibility(cs);
            break;
        }
        case CommandType::SendEnvoy: {
            Player& p = state_.players[static_cast<size_t>(c.player)];
            if (p.envoys.size() < state_.players.size()) p.envoys.resize(state_.players.size(), 0);
            --p.envoyTokens;
            // Diplomatic League (04): the first envoy to a city-state counts twice. Containment: one more where a rival is suzerain.
            const PlayerId suzerain = suzerainOf(static_cast<PlayerId>(c.arg));
            int sent = 1;
            if (p.envoys[static_cast<size_t>(c.arg)] == 0 && policyIs(c.player, "POLICY_DIPLOMATIC_LEAGUE")) ++sent;
            if (suzerain != kNoPlayer && suzerain != c.player && policyIs(c.player, "POLICY_CONTAINMENT")) ++sent;
            p.envoys[static_cast<size_t>(c.arg)] += sent;
            // Papal Primacy (06): each envoy adds 200 pressure of the sender's religion in the city-state's cities.
            if (p.religion >= 0 && playerHasBelief(c.player, Bf::PapalPrimacy)) {
                for (City& city : state_.cities) {
                    if (city.owner != static_cast<PlayerId>(c.arg)) continue;
                    if (city.pressure.size() < state_.religions.size()) city.pressure.resize(state_.religions.size(), 0);
                    city.pressure[static_cast<size_t>(p.religion)] += 200;
                }
            }
            // A city-state's first suzerain is a historic moment (09).
            Player& cs = state_.players[static_cast<size_t>(c.arg)];
            if (!cs.hadSuzerain && isSuzerain(c.player, cs.id)) {
                cs.hadSuzerain = true;
                awardMoment(c.player, "MOMENT_CITY_STATE_S_FIRST_SUZERAIN");
            }
            break;
        }
    }
}

void Game::applyMove(const Command& c) {
    Unit* u = state_.unit(c.id);
    if (u->escorting != kNoUnit) {
        Unit* l = state_.unit(u->escorting);
        if (l && l->pos == u->pos && l->owner == u->owner) u = l;  // the escort's order moves the pair
    }
    u->moveTarget = c.target;
    u->moveOverland = c.arg == 1;
    u->activity = Activity::Awake;
    advanceUnit(u->id);
}

void Game::applyFoundCity(const Command& c) {
    const Unit* u = state_.unit(c.id);
    const Hex at = u->pos;
    const PlayerId owner = u->owner;
    // Settling within 6 of a civ it promised not to settle near breaks the promise (08 [GS]).
    for (const City& near : state_.cities) {
        if (near.owner != owner && state_.grid.distance(near.pos, at) <= 6) breakPromises(owner, near.owner, PromiseKind::NoSettling);
    }
    Player& p = state_.players[static_cast<size_t>(owner)];
    City city;
    city.id = state_.nextCityId++;
    city.owner = owner;
    city.pos = at;
    city.foundedTurn = state_.turn;
    city.capital = p.citiesFounded == 0;
    city.hp = cityMaxHp();
    city.originalOwner = owner;
    city.originalCapital = city.capital;
    if (p.cityState != kNone) {
        city.name = rules_->cityStates[static_cast<size_t>(p.cityState)].name;
    } else {
        const CivType& civ = rules_->civs[static_cast<size_t>(p.civ)];
        city.name = static_cast<size_t>(p.citiesFounded) < civ.cityNames.size()
                        ? civ.cityNames[static_cast<size_t>(p.citiesFounded)]
                        : civ.name + " " + std::to_string(p.citiesFounded + 1);
    }
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
    // A game begun in a later era: its cities start larger, with that era's City Center buildings (game-setup.md).
    if (p.cityState == kNone && !p.barbarian && !p.freeCity) {
        for (int e = 1; e <= state_.setup.startEra; ++e) {
            const EraStartType* es = rules_->eraStart(e);
            if (!es) continue;
            if (e == state_.setup.startEra) city.population = std::max(city.population, city.capital ? es->capitalPopulation : es->otherPopulation);
            for (const TypeIndex b : es->buildings) {
                if (std::find(city.buildings.begin(), city.buildings.end(), b) == city.buildings.end()) city.buildings.push_back(b);
            }
        }
    }
    const CityId newId = city.id;
    // Hardship settlements are historic moments (09).
    if (p.cityState == kNone && p.citiesFounded > 1) {
        const std::string& base = rules_->terrains[static_cast<size_t>(center.terrain)].base;
        if (base == "DESERT") awardMoment(owner, "MOMENT_DESERT_CITY");
        else if (base == "SNOW") awardMoment(owner, "MOMENT_SNOW_CITY");
        else if (base == "TUNDRA") awardMoment(owner, "MOMENT_TUNDRA_CITY");
        // Bold settlements (09; Sovereign readings of the distances): beside a volcano or floodplains, near a rival's
        // city, or on a continent where it has no city yet. City of Awe: within 2 tiles of a natural wonder.
        bool volcano = false, flood = false, rival = false, newContinent = true, awe = false;
        for (const Hex& h : state_.grid.within(at, 2)) {
            const TypeIndex f = state_.plot(h).feature;
            const std::string fid = f == kNone ? std::string() : rules_->features[static_cast<size_t>(f)].id;
            volcano = volcano || fid == "FEATURE_VOLCANO";
            flood = flood || (state_.grid.distance(h, at) <= 1 && fid.rfind("FEATURE_FLOODPLAINS", 0) == 0);
            awe = awe || (f != kNone && rules_->features[static_cast<size_t>(f)].naturalWonder);
        }
        for (const City& o : state_.cities) {
            rival = rival || (o.owner != owner && isMajorCiv(o.owner) && state_.grid.distance(o.pos, at) <= 6);
            newContinent = newContinent && !(o.owner == owner && state_.plot(o.pos).continent == center.continent);
        }
        if (volcano) awardMoment(owner, "MOMENT_CITY_NEAR_VOLCANO");
        if (flood) awardMoment(owner, "MOMENT_CITY_NEAR_FLOODABLE_RIVER");
        if (rival) awardMoment(owner, "MOMENT_AGGRESSIVE_CITY_PLACEMENT");
        if (newContinent) awardMoment(owner, "MOMENT_CITY_ON_NEW_CONTINENT");
        if (awe) awardMoment(owner, "MOMENT_CITY_OF_AWE");
    }
    // A city stands on a road of its founder's era (01: Routes).
    if (const TypeIndex road = roadFor(owner); road != kNone) {
        center.route = static_cast<int8_t>(road);
        center.routePillaged = false;
    }
    // Religious Colonization: new cities start following the founder's religion (06).
    city.pressure.assign(state_.religions.size(), 0);
    if (p.religion >= 0 && sumPlayerModifiers(state_, *rules_, p, ModEffect::ReligionColonizes) > Fixed())
        city.pressure[static_cast<size_t>(p.religion)] = rules_->globalInt(HotGlobal::ReligionSpreadAtheismPressurePerPop) * 2;
    state_.cities.push_back(std::move(city));
    {
        City& made = state_.cities.back();
        const CivAbility& ab = civAbility(owner);
        made.population += ab.foundPopulation;
        // Hic Sunt Dracones in a Golden Age (09): +3 Population for a city founded off the capital's continent.
        if (!made.capital && goldenDedication(owner, "DEDICATION_HIC_SUNT_DRACONES")) {
            for (const City& home : state_.cities) {
                if (home.owner != owner || !home.capital) continue;
                if (state_.plot(home.pos).continent != center.continent) made.population += 3;
                break;
            }
        }
        if (ab.foundBuilding != kNone && !made.has(ab.foundBuilding))
            made.buildings.insert(std::lower_bound(made.buildings.begin(), made.buildings.end(), ab.foundBuilding), ab.foundBuilding);
    }
    // Ancestral Hall (03): a Builder in every city it founds.
    if (buildingsOwned(owner, "BUILDING_ANCESTRAL_HALL") > 0) {
        const TypeIndex builder = rules_->unit("UNIT_BUILDER");
        if (builder != kNone) {
            if (const auto spot = unitSpawnPlot(*state_.city(newId), builder)) spawnUnit(builder, owner, *spot);
        }
    }
    assignCitizens(*state_.city(newId));

    state_.units.erase(std::remove_if(state_.units.begin(), state_.units.end(),
                                      [&](const Unit& x) { return x.id == c.id; }),
                       state_.units.end());
    refreshVisibility(owner);
}

const CivAbility& Game::civAbility(PlayerId player) const {
    static const CivAbility none;
    if (player < 0 || static_cast<size_t>(player) >= state_.players.size()) return none;
    const Player& p = state_.players[static_cast<size_t>(player)];
    const TypeIndex civ = p.civ;
    if (civ == kNone || static_cast<size_t>(civ) >= rules_->civs.size()) return none;
    // A dynasty heir on the throne adds their trait (leaders-and-art-style: Dynasties).
    if (p.rulingHeir > 0) {
        const Dynasty* d = rules_->dynastyOf(civ);
        if (d && static_cast<size_t>(p.rulingHeir) < d->combined.size()) return d->combined[static_cast<size_t>(p.rulingHeir)];
    }
    return rules_->civs[static_cast<size_t>(civ)].combined;
}

const DifficultyType& Game::difficulty() const {
    static const DifficultyType prince;
    if (rules_->difficulties.empty()) return prince;
    const int i = std::clamp(state_.setup.difficulty, 0, static_cast<int>(rules_->difficulties.size()) - 1);
    return rules_->difficulties[static_cast<size_t>(i)];
}

bool Game::difficultyAi(PlayerId player) const {
    return player >= 0 && static_cast<size_t>(player) < state_.players.size() && isMajorCiv(player) && !state_.players[static_cast<size_t>(player)].human;
}

bool Game::difficultyHuman(PlayerId player) const {
    return player >= 0 && static_cast<size_t>(player) < state_.players.size() && isMajorCiv(player) && state_.players[static_cast<size_t>(player)].human;
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
        if (state_.players[next].alive && !state_.players[next].barbarian) {
            state_.currentPlayer = static_cast<PlayerId>(next);
            beginPlayerTurn(state_.currentPlayer);
            // A player whose last city revolted as its turn began is out: the turn moves on.
            if (state_.players[next].alive) return;
        }
    }
}

void Game::beginGlobalTurn() {
    ++state_.turn;
    processEras();
    processGrievances();
    processWorldCongress();
    processClimate();
    processFallout();
    processEmergencies();
    checkMilitaryAid();  // 08: Military Aid Request
    processCompetitions();
    checkQuests();
    assignQuests();
    placeAntiquity();
    processProfiles();
    processRivals();
    processSpaceRace();
    processReligion();
    processAgents();
    processFreeCities();
    processBarbarians();
}

void Game::beginPlayerTurn(PlayerId pid, bool runCities) {
    if (runCities) {
        processCities(pid);
        processLoyalty(pid);
        Player& p = state_.players[static_cast<size_t>(pid)];
        if (p.anarchyTurns > 0 && --p.anarchyTurns == 0) p.freeChanges = true;  // set up the new government
        syncPolicySlots(pid);  // wonders won or lost since
        // The interregnum runs out only while someone sits on the throne.
        if (p.interregnumTurns > 0 && !p.successionPending && p.captor == kNoPlayer && --p.interregnumTurns == 0)
            p.freeChanges = true;
        payUnitFuel(pid);
        burnPower(pid);
        processWarWeariness(pid);
        processLevies(pid);
        armsControl(pid);  // the World Congress's cap follows the target's devices
        // Pillaged districts are repaired over their owner's turns (Sovereign reading of the repair).
        for (City& city : state_.cities) {
            if (city.owner != pid) continue;
            for (CityDistrict& d : city.districts) {
                // A repaired Encampment stands again at full strength.
                if (d.pillagedTurns > 0 && --d.pillagedTurns == 0) d.damage = d.wallDamage = 0;
            }
        }
        processGreatPeople(pid);
        processTrade(pid);
        processEnvoys(pid);
        processTourism(pid);
        circumnavigationMoment(pid);
        processDiplomacy(pid);
        processGovernors(pid);
        processSpies(pid);
        healAndFortify(pid);
        healCities(pid);
    }
    for (Unit& u : state_.units) {
        if (u.owner != pid) continue;
        u.movesLeft = Fixed::fromInt(maxMoves(u));
        // Logistics (04): +1 starting the turn in its own territory; Integrated Attack Logistics: in an enemy's.
        const PlayerId land = state_.plot(u.pos).owner;
        if (land == pid && policyIs(pid, "POLICY_LOGISTICS")) u.movesLeft += Fixed::fromInt(1);
        if (land != kNoPlayer && land != pid && atWar(pid, land) && policyIs(pid, "POLICY_INTEGRATED_ATTACK_LOGISTICS")) u.movesLeft += Fixed::fromInt(1);
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

bool Game::mayStand(const Unit& unit, Hex plot) const {
    const UnitType& ut = typeOf(*rules_, unit);
    const City* city = state_.cityAt(plot);
    if (city && city->owner != unit.owner) return false;
    const TerrainType& t = terrainOf(*rules_, state_.plot(plot));
    if (ut.domain == Domain::Sea) {
        if (!city && (!t.water || t.impassable || (t.id == "TERRAIN_OCEAN" && !canEnterOcean(unit.owner)))) return false;
    } else if (!isLandPassable(state_, *rules_, plot) && !(plot == unit.pos && isEmbarked(unit))) {
        return false;  // a land unit stands on land, or stays embarked where it is
    }
    for (const Unit& o : state_.units) {
        if (o.pos == plot && o.id != unit.id && (o.owner != unit.owner || typeOf(*rules_, o).layer == ut.layer)) return false;
    }
    return true;
}

std::optional<Hex> Game::standingPlotNear(const Unit& unit, Hex around, int radius) const {
    for (const Hex& h : state_.grid.nearestFirst(around, radius)) {
        if (mayStand(unit, h)) return h;
    }
    return std::nullopt;
}

void Game::relocateUnit(Unit& unit, Hex to) {
    if (unit.pos != to) {
        unit.pos = to;
        unit.activity = Activity::Awake;
        unit.fortifyTurns = 0;
    }
    unit.moveTarget.reset();
    unit.escorting = kNoUnit;
}

Unit& Game::spawnUnit(TypeIndex type, PlayerId owner, Hex pos) {
    Unit u;
    u.id = state_.nextUnitId++;
    u.type = type;
    u.owner = owner;
    u.pos = pos;
    u.hp = rules_->globalInt("COMBAT_MAX_HIT_POINTS");
    u.movesLeft = Fixed::fromInt(rules_->units[static_cast<size_t>(type)].moves);
    const UnitType& ut = rules_->units[static_cast<size_t>(type)];
    u.charges = ut.buildCharges > 0 ? ut.buildCharges : ut.excavations;
    Player& p = state_.players[static_cast<size_t>(owner)];
    if (ut.layer == UnitLayer::Military) p.strongestUnit = std::max(p.strongestUnit, ut.combat);
    state_.units.push_back(u);  // ids only grow, so the vector stays sorted
    return state_.units.back();
}

}  // namespace sov
