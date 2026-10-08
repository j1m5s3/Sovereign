// The rules core's front door. UI, AI, network and tests all change the game
// the same way: submit(Command). Queries never change state.
#pragma once

#include "sovereign/api.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "sovereign/commands.h"
#include "sovereign/modifiers.h"
#include "sovereign/rules.h"
#include "sovereign/state.h"

namespace sov {

// A city's derived numbers this turn (02-cities.md). Computed, never stored.
struct CityReport {
    Yields yields{};          // after percentage modifiers
    Fixed foodConsumption;
    Fixed housing;
    int amenities = 0;
    int amenitiesNeeded = 0;
    int happiness = 0;        // index into Rules::happiness
    int defense = 0;          // bonus defense from modifiers (part of the city's combat strength)
};

// What an attack would do, before the dice (05-units-and-combat.md, Combat resolution).
struct CombatPreview {
    bool valid = false;
    bool ranged = false;
    bool capture = false;          // melee into civilians only: they are captured or destroyed
    UnitId defender = kNoUnit;
    CityId city = kNoCity;         // the target is this city (damage figures are to its walls or HP)
    bool hitsWalls = false;        // the damage lands on the city's walls
    bool captureCity = false;      // a melee move into a city at 0 HP: taken without a fight
    bool encampment = false;       // the target is `city`'s Encampment (damage to its walls or HP)
    int attackerStrength = 0, defenderStrength = 0;
    int damageToDefenderMin = 0, damageToDefenderMax = 0;
    int damageToAttackerMin = 0, damageToAttackerMax = 0;  // melee only
};

struct PathStep {
    Hex pos;
    int turn = 0;       // 0 = reached this turn
    Fixed movesLeft;    // after entering this plot
};

// A personal aim for the leader, shown at turn end beside Civ's "X turns to Y" (player-retention §4).
enum class LeaderGoalKind : uint8_t {
    Promotion = 0,    // a promotion waits to be chosen
    PromotionSoon,    // value: XP still to earn for the next one
    AssassinNear,     // an assassin is reported in place to strike; value: 1 when the leader is exposed
    CityUnrest,       // id: a city of ours that may revolt or rebel; value: its loyalty
    RivalLeaderNear,  // id: the rival; value: plots between the leaders; at: the rival leader's plot
};
struct LeaderGoal {
    LeaderGoalKind kind = LeaderGoalKind::Promotion;
    int32_t value = 0;
    int32_t id = -1;
    Hex at;
};

// Places the map size's city-states at the start (08: City-States); used when a game is created.
void placeCityStates(GameState& state, const Rules& rules);

class SOV_API Game {
public:
    // Builds a new game: map, start positions, starting units, first turn.
    static std::unique_ptr<Game> create(const Rules& rules, const GameSetup& setup, std::string* error);
    // Rebuilds a game from its setup and command log. Every command must apply.
    static std::unique_ptr<Game> replay(const Rules& rules, const GameSetup& setup,
                                        const std::vector<Command>& log, std::string* error);
    // Starts play from a hand-made state (scenario maps and tests): computes
    // every player's visibility and begins player 0's turn without running
    // start-of-turn city processing (the state is taken as already processed).
    static std::unique_ptr<Game> fromScenario(const Rules& rules, GameState state);
    // Wraps an already-loaded state (see serialize.h).
    Game(const Rules& rules, GameState state, std::vector<Command> log);

    CommandError validate(const Command& c) const;
    // Validates, applies and logs. Nothing changes when it returns an error.
    CommandError submit(const Command& c);

    const Rules& rules() const { return *rules_; }
    const GameState& state() const { return state_; }
    // Changes state outside the rules: only for tests that need a broken game (a forced desync).
    GameState& stateMutForTests() { return state_; }
    const std::vector<Command>& log() const { return log_; }
    // Hash of the full state (not the log); compared between peers each turn.
    uint64_t stateHash() const;

    // Path for a unit from where it stands, planned on its owner's knowledge.
    // overland: a land unit does not embark on the way (it may still leave the water).
    std::optional<std::vector<PathStep>> findPath(UnitId unit, Hex target, bool overland = false) const;
    // The plots a move order for the unit finds a path to (1, the rest 0), planned as the order is (a linked escort's
    // for its leader): an order to any other plot fails.
    std::vector<uint8_t> moveReach(UnitId unit, bool overland = false) const;
    // Movement points needed for this unit to enter `to` from adjacent `from`, also where it may only pass (05:
    // Stacking); nullopt when it cannot enter.
    std::optional<Fixed> moveCost(const Unit& unit, Hex from, Hex to) const;
    // ---- naval play and embarkation (05-units-and-combat.md, Embarkation)
    bool canEmbark(PlayerId player, TypeIndex unitType) const;
    bool canEnterOcean(PlayerId player) const;
    // A water plot carrying a road: the Golden Gate Bridge (03), a land bridge that land units cross dry.
    bool bridgeAt(Hex plot) const;
    // A land unit standing on water (not on a land bridge).
    bool isEmbarked(const Unit& unit) const;
    // Embarking or disembarking: allowed with any movement left, which it then uses up.
    bool isEmbarkTransition(const Unit& unit, Hex from, Hex to) const;
    // A city next to Coast or Lake: it trains ships.
    bool isCoastalCity(const City& city) const;
    // Units that block ending the turn (05/00: "Units need orders").
    std::vector<UnitId> unitsNeedingOrders(PlayerId player) const;
    bool canFoundCityAt(PlayerId player, Hex at, CommandError* why = nullptr) const;
    Visibility visibility(PlayerId player, Hex h) const;

    // ---- cities (02-cities.md)
    CityReport cityReport(CityId city) const;
    // Yields of a plot as worked by `city` (city center rules when it is the center).
    Yields plotYields(Hex plot, const City& city) const;
    // Plots this city's citizens may work: owned by it, within 3, workable terrain.
    std::vector<Hex> workablePlots(const City& city) const;
    int growthThreshold(int population) const;
    int borderGrowthCost(int plotsAcquired) const;
    // `city`: where it is built, for costs that depend on it (the Flood Barrier; 09).
    int productionCost(PlayerId player, ProductionItem item, const City* city = nullptr) const;
    // Gold price, or -1 when the item cannot be bought with gold; `city`: where it is bought (Ngazargamu, 08);
    // `currency`: what pays that price, for the World Congress's Mercenary Companies (Faith buys some units at it).
    int purchaseCost(PlayerId player, ProductionItem item, const City* city = nullptr, YieldType currency = YieldType::Gold) const;
    bool canTrainFormation(const City& city, TypeIndex unit, int formation) const;  // a Corps (1) or Army (2) whole (05)
    // A placed, unfinished district bought outright with Gold (Reyna's Contractor) or Faith (Moksha's
    // Divine Architect) (08: Governors); -1 when it cannot be.
    int districtPurchaseCost(const City& city, TypeIndex district, bool faith) const;
    // Gold price of a plot, or -1 when this city cannot buy it.
    int plotPurchaseCost(CityId city, Hex plot) const;
    bool canProduce(const City& city, ProductionItem item, CommandError* why = nullptr, bool purchase = false) const;
    // Where a unit trained in this city appears (ships: the port or the water beside it); nullopt when full.
    std::optional<Hex> unitSpawnPlot(const City& city, TypeIndex unitType) const;
    // The most advanced unit of a class the player can train, its civ's unique one where it has one. With none unlocked
    // yet: kNone, or with orFirst the class's first unit.
    TypeIndex bestUnitOfClass(PlayerId player, const std::string& unitClass, bool orFirst = false) const;
    std::vector<ProductionItem> buildableItems(CityId city) const;
    std::vector<CityId> citiesNeedingProduction(PlayerId player) const;
    // Net gold per turn: city gold minus building and unit maintenance.
    Fixed goldPerTurn(PlayerId player) const;
    // Puts its citizens to work: the best plots and specialist slots (02: Citizens and specialists).
    void assignCitizens(City& city);

    // ---- districts (03-districts-buildings-wonders.md)
    // Districts needing population this city may hold: 1 + (pop - 1) / DISTRICT_POPULATION_REQUIRED_PER.
    int districtLimit(const City& city) const;
    int districtCost(PlayerId player, TypeIndex district) const;
    bool canPlaceDistrict(const City& city, TypeIndex district, Hex plot, CommandError* why = nullptr) const;
    // Plots where this city could place the district now.
    std::vector<Hex> districtPlots(CityId city, TypeIndex district) const;
    // Adjacency yields a district of this type would earn on the plot (after policy bonuses).
    Yields districtAdjacency(PlayerId player, TypeIndex district, Hex plot) const;

    // ---- great people and Great Works (07-economy-trade-great-people.md)
    // The era most living major civs have reached: great people older than it drop out.
    int worldEra() const;
    // The individual each class offers to everyone now; kNone when its roster is spent.
    TypeIndex currentGreatPerson(TypeIndex cls) const;
    // Points to recruit this individual (by its era, +30% per era ahead of the world, scaled by speed).
    int greatPersonCost(TypeIndex person) const;
    // Gold (or faith) to buy the class's current individual now; -1 when the player cannot.
    int patronageCost(PlayerId player, TypeIndex cls, bool faith) const;
    int greatPersonPointsPerTurn(PlayerId player, TypeIndex cls) const;
    bool canActivateGreatPerson(UnitId unit, CommandError* why = nullptr) const;
    // Slots of a type in a city's buildings, and the building with a free one that takes this work.
    int greatWorkSlots(const City& city, const std::string& slot) const;
    TypeIndex freeGreatWorkSlot(const City& city, TypeIndex workType) const;
    // Great General / Admiral auras on a unit (+strength, +movement).
    int greatPersonAuraStrength(const Unit& unit) const;
    int greatPersonAuraMoves(const Unit& unit) const;

    // ---- eras, ages and tourism (09: Era score and Ages; 07: Tourism and Culture Victory)
    std::pair<int, int> ageThresholds(PlayerId player) const;  // (Dark below, Golden at or above) for this era's score
    int tourismPerTurn(PlayerId player) const;
    int visitingTourists(PlayerId player, PlayerId from) const;
    int visitingTourists(PlayerId player) const;  // from every other civ
    int domesticTourists(PlayerId player) const;
    PlayerId cultureVictor() const;

    // ---- city-states and envoys (08: City-States)
    bool isCityState(PlayerId player) const;
    int envoysAt(PlayerId player, PlayerId cityState) const;
    PlayerId suzerainOf(PlayerId cityState) const;  // kNoPlayer: none
    bool isSuzerain(PlayerId player, PlayerId cityState) const;  // suzerainOf(cityState) == player
    int suzeraintiesOf(PlayerId player) const;       // living city-states it is suzerain of
    bool suzerainBonus(PlayerId player, const char* cityStateId) const;  // enjoys that city-state's suzerain bonus (08)
    // Levy Military (08): the Gold to take a city-state's military units for LEVY_MILITARY_TURN_DURATION
    // (LEVY_MILITARY_PERCENT_OF_UNIT_PURCHASE_COST of their purchase cost; -1: not possible).
    int levyCost(PlayerId player, PlayerId cityState) const;
    // Monopolies and Corporations mode (07): why an Industry (or a Corporation) may not be made at the plot (Ok when it
    // may), the luxuries the player holds a monopoly of, and how many of a luxury's improved sources it owns.
    CommandError industryProblem(PlayerId player, Hex plot) const;
    bool hasMonopoly(PlayerId player, TypeIndex luxury) const;
    int monopolySources(PlayerId player) const;  // improved sources of all its monopolies
    // Barbarian Clans mode (01: Barbarians): what dealing with a camp costs, and why it may not be done (Ok when it may).
    int clanCost(PlayerId player, int32_t camp, CommandType action) const;  // gold, or -1
    CommandError clanProblem(PlayerId player, int32_t camp, CommandType action, PlayerId against) const;
    bool campLeavesAlone(const Camp& camp, PlayerId player) const;  // bribed by it, or incited against someone else
    bool levied(const Unit& unit) const;             // serving a suzerain under a levy
    void processLevies(PlayerId player);             // levies running out send their units home
    bool canSendEnvoy(PlayerId player, PlayerId cityState) const;
    // Yields a city earns from its owner's envoys (capital and building tiers), and production toward an item.
    Yields envoyYields(const City& city) const;
    int envoyProduction(const City& city, ProductionItem item) const;

    // ---- world wonders (03: Wonders)
    bool wonderBuilt(TypeIndex building) const;
    bool canPlaceWonder(const City& city, TypeIndex building, Hex plot) const;
    std::vector<Hex> wonderPlots(CityId city, TypeIndex building) const;
    void wonderCompleted(CityId city, TypeIndex building);  // its completion effects (scenarios, tests)
    // The city lies within `range` of a wonder its owner holds (Colosseum, Statue of Liberty).
    bool nearOwnWonder(const City& city, TypeIndex wonder, int range) const;
    // Kilwa Kisiwani: +15% in its city with one suzerainty of the kind, +15% in every city with two (0, 15 or 30).
    int kilwaPercent(const City& city, CityStateKind kind) const;

    // ---- trade routes and roads (07: Trade routes; 01: Routes)
    int tradeRouteCapacity(PlayerId player) const;
    int tradeRoutesOf(PlayerId player) const;
    // What a route from origin to destination pays its origin each turn.
    Yields tradeRouteYields(const City& origin, const City& destination) const;
    Yields tradeRouteDestinationYields(const City& origin, const City& destination) const;  // what the destination city gets
    // The plots a Trader would follow to a destination, within range (15 tiles, 30 when it sails); empty: out of reach.
    std::vector<Hex> tradePath(PlayerId player, TypeIndex traderType, const City& origin, const City& destination) const;
    bool canStartTradeRoute(UnitId trader, CityId destination) const;
    const City* tradeOrigin(UnitId trader) const;  // the city a Trader would start from (in it or beside it), or null
    std::vector<CityId> tradeDestinations(UnitId trader) const;
    int tradeRouteLength() const;  // turns, scaled by game speed, longer in later world eras
    // The road a player lays now (its era's tier).
    TypeIndex roadFor(PlayerId player) const;

    // ---- religion (06-religion.md)
    bool canFoundPantheon(PlayerId player, TypeIndex belief) const;
    // Whether the core applies this belief's effect yet (the rest wait for their systems).
    bool beliefModelled(TypeIndex belief) const;
    // Beliefs of a class no player or religion has taken yet.
    std::vector<TypeIndex> availableBeliefs(BeliefClass cls) const;
    int maxReligions() const;  // by map size
    bool canFoundReligion(UnitId prophet, TypeIndex religion, TypeIndex founder, TypeIndex follower, CommandError* why = nullptr) const;
    bool canEvangelize(UnitId apostle, TypeIndex belief) const;
    bool canSpreadReligion(UnitId unit) const;
    // Launch Inquisition (an unused Apostle of the player's own religion, once) and a Guru's heal (06).
    bool canLaunchInquisition(UnitId apostle) const;
    bool canHealReligious(UnitId guru) const;
    int cityMajorityReligion(const City& city) const;
    int civReligion(PlayerId player) const;  // the religion it founded, else its capital's (-1: none)
    int cityFollowers(const City& city, int religion) const;
    // Faith to buy this item here (religious units, worship buildings); -1 when it cannot be bought with Faith.
    int faithPurchaseCost(PlayerId player, const City& city, ProductionItem item) const;
    int religiousStrength(const Unit& unit, bool defending) const;
    // Yields a founder earns from its religion's spread (Tithe, Pilgrimage, World Church...).
    Yields founderYields(PlayerId player) const;
    // The player whose religion is the majority in every living major civ (06: Religious victory), or kNoPlayer.
    PlayerId religiousVictor() const;

    // ---- research and government (04-tech-civics-government.md)
    int techCost(TypeIndex tech) const;    // scaled by game speed
    int civicCost(TypeIndex civic) const;  // scaled by game speed
    bool hasUnlocked(PlayerId player, Unlock u) const;
    bool canResearch(PlayerId player, TypeIndex tech) const;
    bool canStudyCivic(PlayerId player, TypeIndex civic) const;
    std::vector<TypeIndex> availableTechs(PlayerId player) const;
    std::vector<TypeIndex> availableCivics(PlayerId player) const;
    // Science and culture the player earns this turn (zero in anarchy), and its cities' Faith: one report per city.
    struct Output {
        Fixed science, culture, faith;
    };
    Output outputPerTurn(PlayerId player) const;
    Fixed sciencePerTurn(PlayerId player) const { return outputPerTurn(player).science; }
    Fixed culturePerTurn(PlayerId player) const { return outputPerTurn(player).culture; }
    // Whether the player meets a boost's condition right now.
    bool boostMet(PlayerId player, const Boost& boost) const;
    bool canAdoptGovernment(PlayerId player, TypeIndex government, CommandError* why = nullptr) const;
    // Policy may be slotted under the player's government (unlocked, not obsolete, allowed).
    bool policyAvailable(PlayerId player, TypeIndex policy) const;
    bool canSetPolicy(PlayerId player, int slot, TypeIndex policy, CommandError* why = nullptr) const;
    // Gold to open government and policy changes on a turn without a new civic (04: POLICY_COST_*).
    int policyChangeCost(PlayerId player) const;
    // Slot type of slot `slot` of a government: Military slots first, then
    // Economic, Diplomatic and Wildcard.
    static PolicySlot slotType(const GovernmentType& government, int slot);
    // A player's slots are the government's, then those of the wonders they hold (Alhambra, Forbidden City...).
    PolicySlot policySlotType(PlayerId player, int slot) const;
    void syncPolicySlots(PlayerId player);  // sizes the player's slots to their government and wonders
    // ---- improvements and resources (01-map-and-terrain.md, 02-cities.md)
    bool resourceVisible(PlayerId player, Hex plot) const;
    // The plot's resource is worked by a matching improvement or a city center.
    bool resourceImproved(Hex plot) const;
    // ownUnit: built by a unit's own ability (the Legionary's Fort), which needs no unlock.
    bool canImproveAt(PlayerId player, Hex plot, TypeIndex improvement, bool ownUnit = false) const;
    std::vector<TypeIndex> improvementsAt(PlayerId player, Hex plot) const;
    // improvementsAt holds one a Builder makes (no builtBy unit), found with no more checks than it takes.
    bool builderCanImprove(PlayerId player, Hex plot) const;
    bool canHarvestAt(PlayerId player, Hex plot) const;
    // Yields the plot's improvement adds for its owner (base, tech bonuses, adjacency).
    Yields improvementYields(Hex plot, PlayerId owner) const;
    Fixed improvementHousing(const City& city) const;
    // Appeal of a plot: what its neighbours contribute (01: Appeal).
    int plotAppeal(Hex plot) const;
    // Housing and amenities from the city's finished districts (Aqueduct, Neighborhood, Dam, ...).
    Fixed districtHousing(const City& city) const;
    int districtAmenities(const City& city) const;
    // A finished district of the city that prevents droughts (or floods) on its plots [GS].
    bool cityPrevents(CityId city, bool floods) const;
    int luxuryAmenities(const City& city) const;
    bool hasStrategicFor(PlayerId player, TypeIndex unitType, const City* city = nullptr) const;
    // Strategic resources to train the unit in the city (Black Marketeer: 80% less; 08).
    int strategicCostIn(const City* city, TypeIndex unitType) const;
    // Units of the city's owner in its territory, with the city's established governor holding the promotion.
    bool territoryGovernorHas(Hex at, PlayerId owner, const char* promotionId) const;
    bool unitObsolete(PlayerId player, TypeIndex unitType) const;

    // ---- war and combat (05-units-and-combat.md)
    bool atWar(PlayerId a, PlayerId b) const;
    bool canDeclareWar(PlayerId player, PlayerId target) const;
    bool canMakePeace(PlayerId player, PlayerId target) const;

    // ---- player modelling (leader doc §10, AI layer 2)
    const PlayerProfile* profile(PlayerId player) const;  // nullptr before the first world turn
    // ---- rivals who remember you (player-retention §1): an AI civ's memory of a human from earlier
    // games (nullptr: none, or the setup turned it off), the grudge (0..30) and respect (0..15) it
    // brings to opinions and targeting, and the memories to write back: earlier games plus this one.
    const RivalMemory* rivalMemory(PlayerId ai, PlayerId human) const;
    int rivalGrudge(PlayerId ai, PlayerId human) const;
    int rivalRespect(PlayerId ai, PlayerId human) const;
    std::vector<RivalMemory> rivalMemories(PlayerId human) const;
    // ---- the chronicle of a reign (player-retention §2): its key events in plain lines ("Turn 40: ..."),
    // those the player took part in, ending with how the game ended for them; and its Hall of Sovereigns
    // record (the ruler, its loadout and level, victory or fall, rivals made).
    static bool chronicleWorthy(EventKind kind);
    std::vector<std::string> chronicleLines(PlayerId viewer) const;
    std::string hallEntry(PlayerId player) const;
    // The achievements (Rules::achievements ids) the player holds as the game stands (player-retention §7).
    std::vector<std::string> achievementsEarned(PlayerId player) const;

    // ---- city projects (03: Projects)
    void completeProject(City& city, TypeIndex project);  // its completion effects (the production queue calls it)
    bool completeItem(City& city, ProductionItem item);    // finish an item now (false when it cannot complete now)
    // The Science victory (09 [GS]): light-years a turn of the player's exoplanet expedition (0: not launched).
    int expeditionSpeed(PlayerId player) const;

    // ---- climate and disasters (09: Climate and Disasters [GS])
    int climateChangePoints() const;   // one per half degree of warming
    int temperatureTenths() const;     // degrees of warming, x10
    // Coastal lowland band of a plot: 1-3 meters, 0 when it is not lowland.
    int lowlandBand(Hex plot) const;
    bool inDrought(Hex plot) const;
    // Strikes a disaster at a plot (the world turn's roll picks both; tests call it directly).
    // `follow`: a storm moving on or a fire spreading, not a new disaster.
    void strikeDisaster(TypeIndex disaster, Hex center, bool follow = false);
    // Deforestation's effect on CO2 [GS] (09): -20% .. +50% by the share of the world's woods lost.
    int deforestationPercent() const;

    // ---- grievances, favor and the World Congress (08 [GS])
    int grievances(PlayerId holder, PlayerId against) const;
    int favorPerTurn(PlayerId player) const;
    bool congressInSession() const { return state_.congressOpenedTurn > 0; }
    // Favor for `extra` votes beyond the free one: 10, 30, 60, 100... (10 more for each).
    static int extraVoteCost(int extra) { return 5 * extra * (extra + 1); }
    bool hasVoted(PlayerId player, int item) const;
    // The resolution of this kind in force, or null.
    const PassedResolution* passed(ResolutionKind kind) const;
    // A passed resolution of this kind with this option and target (World Congress [GS]).
    bool resolutionHits(ResolutionKind kind, uint8_t option, int32_t target) const;
    std::string candidateName(const CongressItem& item, int candidate) const;
    // Mercenary Companies: the % of the usual cost a military unit has in this currency (Production, Gold or Faith).
    int mercenaryPercent(PlayerId player, TypeIndex unit, YieldType currency) const;
    // Arms Control: the most devices of this weapon the player may hold, or -1 for no limit.
    int wmdCap(PlayerId player, TypeIndex weapon) const;

    // ---- espionage (08: Espionage)
    int spyCapacity(PlayerId player) const;   // from civics and techs
    int buildingsOwned(PlayerId pid, const char* buildingId) const;  // in all its cities
    bool governmentIs(PlayerId pid, const char* governmentId) const;  // in force (not in anarchy)
    bool policyIs(PlayerId pid, const char* policyId) const;          // slotted and in force
    int stockpileCap(PlayerId pid, TypeIndex resource) const;        // the resource's cap plus its buildings' raises (01)
    int spiesOf(PlayerId player) const;
    const SpyOperationType* spyOperationFor(SpyMission mission) const;
    bool canSpyMission(PlayerId player, int32_t spy, SpyMission mission, CityId city, CommandError* why = nullptr) const;
    // The chance (in percent) an operation succeeds now; 100 for passive ones.
    int spySuccessPercent(int32_t spy, SpyMission mission, CityId city) const;

    // ---- governors (08: Governors [R&F])
    int governorTitles(PlayerId player) const;          // earned from civics
    int governorTitlesLeft(PlayerId player) const;      // earned minus spent
    const Governor* governor(PlayerId player, TypeIndex type) const;  // appointed, or null
    // The owner's governor established in this city (a city-state's: the major whose Amani serves there).
    const Governor* establishedGovernor(const City& city, PlayerId* owner = nullptr) const;
    bool canAppointGovernor(PlayerId player, TypeIndex type) const;
    bool canPromoteGovernor(PlayerId player, TypeIndex type, TypeIndex promotion) const;
    bool canAssignGovernor(PlayerId player, TypeIndex type, CityId city) const;
    int governorEstablishTurns(TypeIndex type) const;
    // Envoys a player's Amani adds at this city-state (2, doubled by Puppeteer).
    int governorEnvoys(PlayerId player, PlayerId cityState) const;
    bool governorHasPromotion(const Governor& g, const char* promotionId) const;
    // The city's own established governor holds this promotion.
    bool cityGovernorHas(const City& city, const char* promotionId) const;

    // ---- diplomacy (08-diplomacy-city-states-governors.md; leader doc §10). Rules decide
    // every outcome; the dialogue layer only turns words into these deals.
    bool isMajorCiv(PlayerId player) const;
    bool hasMet(PlayerId a, PlayerId b) const;
    bool denouncing(PlayerId by, PlayerId target) const;  // within DIPLOMACY_DENOUNCE_TIME_LIMIT
    bool friends(PlayerId a, PlayerId b) const;            // a declaration of friendship in force
    bool grantsOpenBorders(PlayerId owner, PlayerId to) const;
    // How `holder` feels about `about`: the sum of its reasons, clamped to -100..100.
    int opinionOf(PlayerId holder, PlayerId about) const;
    std::vector<OpinionReason> opinionReasons(PlayerId holder, PlayerId about) const;
    int agendaOpinion(PlayerId holder, PlayerId about) const;
    Relationship relationship(PlayerId holder, PlayerId about) const;
    bool canDenounce(PlayerId by, PlayerId target) const;
    // Why a deal cannot be made now (CommandError::Ok when it can).
    CommandError dealProblem(const Deal& deal) const;
    // The deal's worth to `judge` in gold (positive: it gains), before its opinion is weighed.
    int dealValue(PlayerId judge, const Deal& deal) const;
    // Rules-only acceptance: value against a bar its opinion of the proposer raises or lowers.
    bool wouldAccept(PlayerId judge, const Deal& deal) const;
    // Items `from` could put into a deal with `to` now (amounts at their most).
    std::vector<DealItem> offerableItems(PlayerId from, PlayerId to) const;
    // Luxuries: improved copies owned, and access after deals (luxury amenities follow access).
    int luxuryCopies(PlayerId player, TypeIndex resource) const;
    std::vector<int> resourceCopies(PlayerId player) const;  // luxuryCopies for every resource, in one pass
    int luxuryCopiesTraded(PlayerId player, TypeIndex resource) const;  // given away by running deals
    bool hasLuxury(PlayerId player, TypeIndex resource) const;
    std::vector<uint8_t> luxuriesHeld(PlayerId player) const;  // hasLuxury for every resource, in one pass
    const Deal* deal(int32_t id) const;
    // Past conversations between two civs (either side speaking), oldest first.
    std::vector<const TalkRecord*> talksBetween(PlayerId a, PlayerId b) const;
    // Abilities in force on a unit: innate ones plus those its owner's modifiers grant.
    std::vector<TypeIndex> unitAbilities(const Unit& unit) const;
    // Sum of `amount` over the unit's promotion and ability effects of this kind
    // (conditions ignored); an effect without an amount counts 1.
    int unitEffectTotal(const Unit& unit, UnitEffectKind kind) const;
    int plunderPercent(const Unit& unit) const;  // + % to its pillage and plunder yields
    bool unitHas(const Unit& unit, UnitEffectKind kind) const { return unitEffectTotal(unit, kind) > 0; }
    int maxMoves(const Unit& unit) const;
    int unitRange(const Unit& unit) const;
    int unitSight(const Unit& unit) const;
    int maxAttacks(const Unit& unit) const;
    // ---- air power (05: air units and air combat; 03: Aerodrome)
    bool isAircraft(const Unit& unit) const;
    int airSlots(PlayerId player, Hex base) const;
    // Airlift and paradrop (05): why a land unit may not be flown or dropped there (Ok when it may).
    CommandError airliftProblem(UnitId unit, Hex to) const;
    // Whether the viewer sees the unit: on a plot in sight, and for a hidden one (Submarine, Privateer; Camouflage and
    // Twilight Veil promotions) only next to the viewer's units or cities, or within sight of one of its units that
    // reveals stealth (05).
    bool unitVisibleTo(PlayerId viewer, const Unit& unit) const;
    CommandError paradropProblem(UnitId unit, Hex to) const;   // aircraft the player can base there (0: not a base)
    int baseAirSlots(PlayerId player, Hex base) const;  // city and Aerodrome slots alone
    int aircraftAt(Hex base) const;
    std::optional<Hex> freeAirBase(const City& city) const;  // the city's center or Aerodrome with room
    int rebaseRange(const Unit& aircraft) const;
    CommandError rebaseProblem(UnitId aircraft, Hex to) const;
    // The strongest defender covering `target` against this aircraft: (strength, unit).
    std::pair<int, UnitId> interception(const Unit& aircraft, Hex target) const;
    void groundAircraft(const City& city);  // enemy aircraft taken with a city or its Aerodrome are lost
    void checkAirBases();                   // aircraft left without a base slot are lost

    // ---- nuclear weapons (05-units-and-combat.md: Nuclear weapons)
    int wmdsHeld(PlayerId player) const;     // devices of every kind

    // ---- war weariness (08: War weariness)
    // ---- specialists (02: Citizens and specialists)
    int specialistSlots(const City& city, const CityDistrict& district) const;  // from its buildings' citizen slots
    Yields specialistYield(const City& city, const CityDistrict& district) const;  // one specialist there

    // ---- formations (05: Corps and Armies)
    // Why `unit` cannot absorb `with` (Ok: it can): both the player's, the same military type, side by
    // side, with moves; a single and a single make a Corps (Nationalism), a Corps and a single an Army (Mobilization).
    CommandError formationProblem(PlayerId player, UnitId unit, UnitId with) const;

    // ---- pillage and repair (05: Pillage)
    CommandError pillageProblem(PlayerId player, UnitId unit) const;
    CommandError coastalRaidProblem(PlayerId player, UnitId unit, Hex at) const;  // naval melee or raider, a neighbouring plot
    CommandError repairProblem(PlayerId player, UnitId builder) const;
    void pillage(UnitId unit, std::optional<Hex> at = std::nullopt);  // its own plot, or the plot a coastal raid hits
    bool districtPillaged(Hex plot) const;   // a district there, pillaged

    // ---- Military Engineers [GS] (01: Routes, Mountain tunnels)
    TypeIndex railroad() const;                        // the unit-only route (kNone: none in the rules)
    CommandError railroadProblem(PlayerId player, UnitId engineer) const;
    CommandError roadProblem(PlayerId player, UnitId unit) const;  // a road by hand for a charge (01: Routes)
    bool freeRoad(PlayerId player, UnitId unit) const;  // a Builder of Qin's laying one for no charge (Standardization)
    // Spending a charge (03): a Military Engineer's on the district it stands on, or, with the Royal Society, a Builder's
    // on the project its city is building, standing where the project runs (its district, or the City Center).
    CommandError chargeProblem(PlayerId player, UnitId unit) const;
    std::optional<ProductionItem> chargedProject(const Unit& builder) const;  // that project (Royal Society)
    int projectChargePercent(PlayerId player) const;  // the share of a project each charge completes (Royal Society)
    std::vector<Hex> tunnelSites(PlayerId player, UnitId engineer) const;  // neighbouring mountains it may tunnel

    // ---- spy promotions (08: Espionage)
    int spyPromotionTotal(const Agent& spy, int SpyPromotionType::*field) const;  // summed over its promotions
    int spyOperationLevels(const Agent& spy, SpyMission m) const;                 // extra levels its promotions give

    // ---- natural wonders (01: Natural wonders)
    bool nextToNaturalWonder(Hex plot, const char* featureId) const;  // a neighbouring plot holds it

    // ---- tourism from the land: improvements, National Parks, Rock Bands (07: Tourism sources)
    int improvementTourism(PlayerId player) const;
    std::optional<std::array<Hex, 4>> parkPlots(UnitId naturalist) const;  // the park it would make here
    std::optional<std::array<Hex, 4>> parkPlotsAt(PlayerId player, Hex plot) const;
    CommandError parkProblem(PlayerId player, UnitId naturalist) const;
    void designatePark(UnitId naturalist);
    int parkTourism(PlayerId player) const;
    int parkAmenities(const City& city) const;
    CommandError concertProblem(PlayerId player, UnitId band) const;
    void grantBandPromotion(Unit& band);  // a random Rock Band promotion it lacks (under Hallyu, one to choose)
    void grantApostlePromotion(Unit& apostle);  // one random Apostle promotion (06)
    void performConcert(UnitId band);

    // ---- dedications [R&F] (09: Dedications)
    std::vector<TypeIndex> availableDedications(PlayerId player) const;
    CommandError dedicationProblem(PlayerId player, TypeIndex dedication) const;
    void chooseDedication(PlayerId player, TypeIndex dedication);
    bool dedicated(PlayerId player, const char* id) const;        // chosen this era
    bool goldenDedication(PlayerId player, const char* id) const; // chosen, and in a Golden (or Heroic) Age: its bonus
    void dedicationScore(PlayerId player, const char* id, int amount);  // its era score, outside a Golden Age

    // ---- archaeology (07: Archaeology)
    void noteBattle(Hex plot, PlayerId attacker);  // remembered as a future site while the world is young enough
    bool themed(const City& city, TypeIndex building) const;  // its Great Works earn the theming bonus (07)
    // The work would give the player a museum's theme it cannot reach without it (07; for trading).
    bool workCompletesTheme(PlayerId player, const GreatWork& work) const;
    const GreatWork* dealWork(const DealItem& item) const;
    void lockArt(GreatWork& work) const;  // the Great Work a GreatWork deal item names
    const CapturedSpy* dealCaptive(const DealItem& item) const;  // the spy a Captive deal item names, held by its giver
    int freeSlotsFor(const City& city, TypeIndex building, TypeIndex workType) const;
    CommandError moveGreatWorkProblem(PlayerId player, CityId from, int index, CityId to, TypeIndex building) const;
    void moveGreatWork(CityId from, int index, CityId to, TypeIndex building);
    // Moves that gather the player's works into a theming building (07): empty when it cannot be themed
    // from works the player has outside other themed buildings.
    std::vector<Command> themingMoves(PlayerId player, CityId city, TypeIndex building) const;
    void placeAntiquity();                  // once any civ has Natural History
    CommandError excavateProblem(PlayerId player, UnitId archaeologist) const;
    void excavate(UnitId archaeologist);
    bool seesAntiquity(PlayerId player) const;  // it knows Natural History

    // ---- tribal villages (01: Tribal Villages)
    void enterVillage(Unit& unit);   // the reward: a category, then a reward in it, by weight

    // ---- city-state quests (08: Quests)
    void assignQuests();                                         // each city-state, each major that met it: one open quest
    void questDone(PlayerId major, QuestKind kind, int32_t arg);  // fulfils matching quests: an envoy in each
    void checkQuests();                                          // the quests the world's state fulfils (conversion, routes)
    const Quest* questFor(PlayerId cityState, PlayerId major) const;
    std::string questText(const Quest& q) const;

    // ---- scored competitions [GS] (08: Scored Competitions)
    void startCompetition();                                       // at a World Congress session
    void requestAid(PlayerId victim, bool military = false);       // a disaster (or, military, a grievous war): an Aid Request
    bool specialSessionDue() const;                                // WORLD_CONGRESS_MIN_TIME_BETWEEN_SPECIAL_SESSIONS since the last
    void checkMilitaryAid();                                       // the world turn: a civ at war with one it holds grievances against
    const Competition* runningAidRequest() const;
    void competitionScore(PlayerId player, CompetitionKind kind, int amount);
    int competitionStanding(const Competition& c, PlayerId player) const;  // its score now (state-based ones counted live)
    void processCompetitions();                                    // settles the ones whose time is up (the world turn)

    // ---- emergencies [R&F/GS] (08: Emergencies)
    // Whether the player may join this running emergency: a living major civ that has met the target,
    // is not the target, its ally or its declared friend, and has not joined yet.
    bool canJoinEmergency(PlayerId player, int emergency) const;
    bool inEmergencyAgainst(PlayerId member, PlayerId target) const;  // a running emergency it joined
    void triggerEmergency(EmergencyKind kind, PlayerId target, CityId city, PlayerId victim);
    void processEmergencies();               // goals met, expiry, rewards (the world turn)

    // ---- delegations, embassies and diplomatic access (08: Access level, Diplomatic actions)
    CommandError delegationProblem(PlayerId from, PlayerId to, bool embassy) const;
    bool wouldReceive(PlayerId to, PlayerId from) const;  // an AI lets the delegation in
    void sendDelegation(PlayerId from, PlayerId to, bool embassy);
    int accessLevel(PlayerId viewer, PlayerId target) const;  // 0 None, 1 Limited, 2 Open, 3 Secret, 4 Top Secret
    static const char* accessName(int level);
    static int gossipLevel(EventKind kind);               // the access an outsider needs to hear of it
    bool hearsOf(PlayerId viewer, const GameEvent& e) const;

    // ---- promises [GS] (08: Ask Promise)
    CommandError askPromiseProblem(PlayerId asker, PlayerId of, PromiseKind kind) const;
    bool wouldPromise(PlayerId of, PlayerId asker) const;            // an AI's answer
    void askPromise(PlayerId asker, PlayerId of, PromiseKind kind);
    void breakPromises(PlayerId by, PlayerId to, PromiseKind kind);  // the deed was done
    bool promised(PlayerId by, PlayerId to, PromiseKind kind) const;  // a promise in force

    // ---- casus belli (08: War types)
    bool hasCasusBelli(PlayerId player, PlayerId target, CasusBelli why) const;  // civic and condition met
    int casusBelliGrievancePercent(CasusBelli why) const;                        // of a formal war's grievances
    CasusBelli bestCasusBelli(PlayerId player, PlayerId target) const;          // the cheapest it holds (None: none)

    // ---- alliances [R&F] (08: Alliance)
    AllianceType alliance(PlayerId a, PlayerId b) const;  // None when not allied
    int allianceLevel(PlayerId a, PlayerId b) const;      // 0 not allied, else 1..3 by alliance points
    // The highest level of an alliance of this type the player holds with anyone (0: none).
    int bestAllianceLevel(PlayerId player, AllianceType type) const;
    bool militaryAllianceAtWar(PlayerId player) const;  // a level-2 Military ally, and a war on either side
    int tourismBase(PlayerId player) const;              // its own tourism, before an ally's share
    int religiousTourism(const City& city) const;       // its owner's Holy City: TOURISM_FROM_HOLY_CITY, doubled by St. Basil's (03)
    Fixed allianceShare(PlayerId player, YieldType yield) const;  // Research/Cultural level 3: 10% of the ally's yield
    int warWeariness(PlayerId player) const;            // points against every opponent together
    int warWearinessAmenities(PlayerId player) const;   // amenities each of its cities loses (1 per 400 points)
    void addWarWeariness(PlayerId player, PlayerId against, int points);  // scaled by policies and grievances
    void processWarWeariness(PlayerId player);          // decays as the player's turn begins
    // Why a launch cannot happen (Ok: it can): a held device, a delivery in range with moves (a bomber's
    // strike range, or the device's ICBM range from a Nuclear Submarine or the player's Missile Silo),
    // and nothing in the blast belonging to a civ the launcher is at peace with.
    CommandError wmdProblem(const Command& c) const;
    std::vector<Hex> wmdBlast(Hex target, TypeIndex weapon) const;
    void launchWmd(const Command& c);
    void processFallout();                   // contamination runs down; units on it take damage

    // The game's difficulty level, and whether its AI (or human) bonuses apply to this player.
    const DifficultyType& difficulty() const;
    // The player's civ and leader abilities together (empty for city-states, barbarians and Free Cities).
    const CivAbility& civAbility(PlayerId player) const;
    bool difficultyAi(PlayerId player) const;     // an AI-run major civ
    bool difficultyHuman(PlayerId player) const;  // a human-run major civ
    // Unit upgrades (05: Upgrades): gold to turn the unit into the next in its line, -1 when it has none.
    int upgradeCost(const Unit& unit) const;
    // Lasting great person effects of a kind: the player's (those it used), or a city's (those used there).
    int greatPersonEffectTotal(PlayerId player, GreatPersonEffectKind kind, TypeIndex ref = kNone) const;
    int cityGreatPersonEffectTotal(const City& city, GreatPersonEffectKind kind) const;
    // Tesla, Paxton (07): their lasting bonus to the regional buildings of this city's district of that type.
    int regionalBonus(const City& city, TypeIndex district, GreatPersonEffectKind kind, YieldType yield = YieldType::Production) const;
    int upgradeResourceCost(const Unit& unit) const;  // strategic resources the upgrade spends
    TypeIndex upgradeTarget(const Unit& unit) const;  // the next unit in its line (the civ's unique if it has one)
    CommandError upgradeProblem(UnitId unit) const;  // Ok when the upgrade can be bought now
    // A unit entering this plot loses its remaining moves (enemy unit or city next to it).
    bool inEnemyZoc(const Unit& mover, Hex plot) const;
    // Combat strength of `unit` fighting `opponent` (05: Strength calculation).
    int combatStrength(const Unit& unit, const Unit& opponent, bool attacking, bool ranged) const;
    // Damage dealt for a strength difference and a roll in 0..COMBAT_MAX_EXTRA_DAMAGE.
    int combatDamage(int strengthDifference, int roll) const;
    CombatPreview previewAttack(UnitId attacker, Hex target, bool ranged) const;
    // The human whose leader stack is in this melee (so it goes live), or kNoPlayer (leader doc §9).
    PlayerId liveBattleSide(const Unit& attacker, const Unit& defender) const;
    // The same for a melee assault on a city: the attacker's leader stack, or a human leader inside.
    PlayerId liveAssaultSide(const Unit& attacker, const City& city) const;
    bool battlePending() const { return state_.pendingBattle.active; }
    // Strength of `unit` attacking (or, for a city strike, defending against) a city.
    int combatStrengthVsCity(const Unit& unit, const City& city, bool attacking, bool ranged) const;
    // City defence and strike strength (02-cities.md, City combat).
    int cityStrength(const City& city) const;
    int cityMaxHp() const;
    int cityMaxWallHp(const City& city) const;
    // All six neighbours hold enemy units or lie in enemy ZOC: the city cannot heal.
    bool cityUnderSiege(const City& city) const;
    bool canCityStrike(CityId city, Hex target) const;
    // The city's Encampment strikes from its own plot once the city has walls (03: Defense; range 2).
    bool canEncampmentStrike(CityId city, Hex target) const;
    const CityDistrict* encampmentOf(const City& city) const;  // complete and not pillaged
    // The Encampment as a combat target (05: City combat): its own hit points (Districts.HitPoints)
    // and the city's walls as its outer defences.
    const City* encampmentTargetAt(Hex plot) const;  // the city whose standing Encampment is here
    int encampmentMaxHp(const City& city) const;
    int encampmentHp(const City& city) const;
    int encampmentWallHp(const City& city) const;
    int encampmentStrength(const City& city) const;
    bool canRazeCity(PlayerId player, CityId city) const;
    // A city captured this turn may go back to its original owner, alive and not at war with the player (02).
    bool canLiberateCity(PlayerId player, CityId city) const;
    PlayerId barbarianPlayer() const;
    // Score line items (09: Score; ScoringLineItems): 3 per civic, 2 per tech, 5 per city,
    // 2 per finished district, 1 per population. Cost scaling of techs and civics is unverified and not applied.
    int score(PlayerId player) const;
    int turnLimit() const;  // last turn played; Score decides after it
    bool gameOver() const { return state_.winner != kNoPlayer; }  // kNoPlayer when the game has no barbarians
    const Camp* campAt(Hex h) const;
    int xpForNextLevel(const Unit& unit) const;
    bool canPromote(UnitId unit, TypeIndex promotion) const;
    std::vector<TypeIndex> availablePromotions(UnitId unit) const;

    // ---- loyalty [R&F] (02-cities.md, Loyalty)
    // Net citizen pressure on a city this turn (capped at +-LOYALTY_PER_TURN_FROM_NEARBY_CITIZEN_PRESSURE_MAX_LOYALTY).
    Fixed loyaltyPressure(const City& city) const;
    // Loyalty change at the start of the owner's turn (Free Cities: in the world turn).
    Fixed loyaltyPerTurn(CityId city) const;
    const LoyaltyLevel* loyaltyLevel(const City& city) const;
    PlayerId freeCityPlayer() const;  // kNoPlayer until the first city revolts
    // ---- citizen stances and reputation (leader doc §4, §8.1-8.2)
    bool canTakeStance(PlayerId player, CityId city, Stance stance, CommandError* why = nullptr) const;
    int benevolenceCost(const City& city) const;
    bool fearActive(const City& city) const;   // order imposed: no Unrest or Revolt
    bool beloved(PlayerId player) const;
    bool feared(PlayerId player) const;

    // ---- the leader (leader doc §1, §2, §5; data in leader.json)
    bool isLeader(const Unit& unit) const;
    const Unit* leaderOf(PlayerId player) const;
    // The military unit linked to escort this leader, while they share a plot.
    const Unit* escortOf(const Unit& leader) const;
    // The unit that fights for a plot: its military unit, else a leader standing there.
    const Unit* defenderAt(Hex plot) const;
    // Melee and ranged base strength (a leader's come from its weapon).
    int meleeStrength(const Unit& unit) const;
    int rangedStrength(const Unit& unit) const;
    int gearCost(TypeIndex gear) const;  // gold, scaled by game speed
    bool gearUnlocked(PlayerId player, TypeIndex gear) const;
    bool canEquip(UnitId leader, TypeIndex gear, CommandError* why = nullptr) const;
    // Gold per turn the leader's mount costs: twice its upkeep unit's maintenance.
    int leaderUpkeep(PlayerId player) const;
    // Succession (§5): the dynasty still has an heir; units that may take the throne (level 4+).
    bool hasHeir(PlayerId player) const;
    std::vector<UnitId> successorUnits(PlayerId player) const;
    bool canSucceed(PlayerId player, Succession kind, UnitId unit, CommandError* why = nullptr) const;
    // Presence aura (§1): range in plots of the leader's strength bonus to nearby units.
    int auraRange(const Unit& leader) const;
    // ---- assassins (§6; numbers in leader.json)
    int playerEra(PlayerId player) const;  // highest era among its finished techs and civics
    int agentCapacity(PlayerId player) const;  // one per finished Encampment
    int agentsOf(PlayerId player) const;
    const Agent* agent(int32_t id) const;
    int assassinPower(const Agent& agent) const;
    // The leader's defence against an assassin: gear, terrain, wounds, promotions and guards.
    int leaderDefenseVsAssassin(const Unit& leader) const;
    // An opening: the leader is outside a city, or in one with no own military unit on or next to its plot.
    bool leaderExposed(const Unit& leader) const;
    int assassinSuccessPercent(const Agent& agent, const Unit& leader) const;
    // The leader's next personal goals, most pressing first: an assassin in place, a city near revolt, a rival
    // leader within reach, a promotion to choose or close by (player-retention §4). Empty without a leader.
    std::vector<LeaderGoal> leaderGoals(PlayerId player) const;

    // Sizes a player's per-rules vectors (trees, government uses, units trained).
    static void fitPlayerToRules(Player& p, const Rules& rules);

private:
    void apply(const Command& c);
    // canImproveAt in two parts: the plot takes any improvement of the player's, and this one fits it (`resourceSeen`:
    // resourceVisible for the plot, which the plot's improvements share).
    bool improvablePlot(PlayerId player, Hex plot) const;
    bool improvementFits(PlayerId player, Hex plot, TypeIndex improvement, bool ownUnit, bool resourceSeen) const;
    // The improvements that take the plot's land, as improvementsAt tries them (its resource's when seen).
    const std::vector<TypeIndex>& improvementsForLand(const Plot& plot, bool resourceSeen) const;
    CommandError validateCity(const Command& c) const;
    void applyCity(const Command& c);
    void processCities(PlayerId p);
    CommandError validateResearch(const Command& c) const;
    void applyResearch(const Command& c);
    void processResearch(PlayerId p, Fixed science, Fixed culture);
    void completeNode(PlayerId p, bool civic, TypeIndex node);
    void updateBoosts(PlayerId p);
    // The techs (0) and civics (1) whose boost updateBoosts looks for, by index in order: those with a percent and a
    // condition it can see (an event's boost comes as the event happens, eventBoost). Listed when the game is made.
    std::vector<uint32_t> watchedBoosts_[2];
    void listWatchedBoosts();
    // The plots a player owns with an improvement, counted in one pass for all the boosts checked together.
    struct ImprovedPlots {
        int total = 0;
        std::vector<int> byImprovement, onResource;  // per improvement; onResource: those working the plot's resource
        std::vector<int> byResource;                 // per resource: plots an improvement works it on
    };
    ImprovedPlots improvedPlots(PlayerId player) const;
    // What one civ's Eureka and Inspiration checks share, each part gathered on first need: its improved plots and
    // the indices of its cities and units.
    struct BoostScan {
        std::optional<ImprovedPlots> improved;
        std::optional<std::vector<size_t>> cities, units;
        // By building, district and unit type: how many of its cities have the building (a civ's unique counting as
        // the one it replaces) or the finished district, and how many of its units are of the type (a civ's unique
        // counting as the unit it replaces).
        std::optional<std::vector<int>> buildingCities, districtCities, unitsOfType;
        std::optional<int> alliance;  // its highest allianceLevel with another player (INT_MIN with none)
    };
    bool boostMet(PlayerId player, const Boost& boost, BoostScan& scan) const;
    // boostMet for the boosts it leaves: those that gather the player's cities, units or plots first.
    bool boostScanned(PlayerId player, const Boost& boost, BoostScan& scan) const;
    void grantBoost(PlayerId p, bool civic, size_t node);  // a boost earned now, with its dedication and quest bookkeeping
    void eventBoost(PlayerId p, BoostKind kind, TypeIndex ref = kNone);  // boosts of this event kind (a kill, a camp...)
    CommandError validateBuilder(const Command& c) const;
    void applyBuilder(const Command& c);
    void accumulateStrategics(PlayerId p);
    CommandError validateCombat(const Command& c) const;
    void applyCombat(const Command& c);
    // A bit for a kind of unit effect; kinds 64 apart share one, so a mask of them may only let in more abilities.
    static constexpr uint64_t effectBit(UnitEffectKind k) { return uint64_t{1} << (static_cast<unsigned>(k) % 64); }
    // unitAbilities, keeping only those with an effect of a kind in `kinds` (effectBits): the others add nothing to the
    // effects of those kinds, so the grants of none of them need a look.
    std::vector<TypeIndex> abilitiesWithEffects(const Unit& unit, uint64_t kinds) const;
    template <typename Want>
    std::vector<TypeIndex> abilitiesWhere(const Unit& unit, Want&& want) const;
    std::optional<Fixed> terrainCost(const Unit& unit, Hex from, Hex to) const;
    // What a unit's abilities, promotions and policies change about its movement: the same for every step of a path.
    struct MoveTraits {
        bool freeEmbark = false, ignoreHills = false, ignoreForest = false, ignoreTerrain = false, ignoreBorders = false;
        bool zeal = false;      // a religious unit under Missionary Zeal
        bool noRiver = false;   // crosses rivers at no extra cost (Amphibious, the Helicopter: 05)
        bool rockBand = false;  // kept out by Music Censorship
    };
    MoveTraits moveTraits(const Unit& unit) const;
    // What keeps a unit out of plots, the same for every step of a path.
    struct MoveLimits {
        // A plot's marks in `blocked` (valued to sit beside a path search's own): kept out of (a unit of a player at
        // war, a foreign city, a standing enemy Encampment), or held by another player's unit at peace, which a move
        // may pass but not end on (kPassOnly | kTheirs).
        enum : uint8_t { kKeepOut = 1, kPassOnly = 16, kTheirs = 32 };
        std::vector<uint8_t> blocked;  // per plot, its marks
        std::vector<uint8_t> closed;   // per player: 1 closed borders (entered only from inside), 2 no entry at all
        bool onlyBlocked = false;      // with `only`: whether that plot is kept out of (`blocked` is then left empty)
    };
    // With `only` (a plot on the grid), just what a step into it reads: whether it is blocked, and its owner's entry.
    MoveLimits moveLimits(const Unit& unit, const MoveTraits& traits, std::optional<Hex> only = std::nullopt) const;
    // A step's cost: `dir` is the direction from `from` to its neighbour `to`, `limits` the whole map's (no `only`).
    std::optional<Fixed> moveCost(const Unit& unit, const MoveTraits& traits, const MoveLimits& limits, Hex from, Hex to, Dir dir) const;
    std::optional<Fixed> terrainCost(const Unit& unit, const MoveTraits& traits, Hex from, Hex to, Dir dir) const;
    // terrainCost in three parts, so that a path search reads each plot once however many of its steps end there:
    // what a step's cost reads of the unit (the same for every step), of the plot it ends on, and of the plot it
    // starts from.
    struct StepUnit {
        const Unit* unit = nullptr;
        const MoveTraits* traits = nullptr;
        Domain domain = Domain::Land;
        int embarkCost = 0, riverCost = 0;
        int8_t embark = -1, ocean = -1;  // canEmbark and canEnterOcean, asked on first need (-1: not yet)
    };
    // How a step into the plot is costed: not at all; 1 (sailing, a canal); 1 from the water (a ship into a city);
    // embarking; a religious unit under Missionary Zeal; on land (or a land bridge) by its terrain.
    enum class StepKind : uint8_t { None, Sail, Port, Embark, Zeal, Land };
    // Plain data (a path search keeps one per plot, set as it reaches the plot): StepInto{} is StepKind::None. Its
    // first fields are those of a StepFrom read off the same plot.
    struct StepInto {
        StepKind kind;
        bool water;
        bool afloat;         // water with no land bridge
        int8_t road;         // its route, -1 with none or a pillaged one
        uint8_t riverEdges;  // Plot::riverEdges
        int32_t cost;        // Land: the terrain's cost, before roads, rivers and disembarking
    };
    struct StepFrom {
        bool water, afloat;
        int8_t road;
        uint8_t riverEdges;
    };
    StepUnit stepUnit(const Unit& unit, const MoveTraits& traits) const;
    StepInto stepInto(StepUnit& su, Hex to) const;
    StepFrom stepFrom(Hex from) const;
    // The step's cost: `dir` is the direction from the step's start to its end.
    std::optional<Fixed> stepCost(const StepUnit& su, const StepInto& to, const StepFrom& from, Dir dir) const;
    bool lineOfSight(Hex from, Hex to, bool throughFeatures = false) const;
    // A unit's sight given its Sight effects' total (unitSight); and its sight with whether it sees through woods
    // (Sentry, 05), from one look at its abilities.
    int sightFrom(const Unit& unit, int sightEffects) const;
    std::pair<int, bool> unitSightAndSentry(const Unit& unit) const;
    void gainXp(Unit& unit, int ownBase, int enemyBase, bool ranged, bool attacker, bool killed, bool vsBarbarian);
    void awardXp(Unit& unit, int xp, bool vsBarbarian);
    int unitStrength(const Unit& unit, const Unit* oppUnit, const City* oppCity, bool attacking, bool ranged) const;
    // Percent of a hit on this city that lands on its walls; -1 when it lands on the city.
    int wallDamagePercent(const Unit& attacker, const City& city, bool ranged) const;
    int wallDamagePercent(const Unit& attacker, const City& city, bool ranged, Hex at, int wallHp) const;
    void attackCity(const Command& c, City& city);
    void attackEncampment(const Command& c, City& city);
    // Applies a unit-vs-unit fight's damage and everything that follows (kills, capture, XP, advance).
    void resolveUnitFight(UnitId attackerId, UnitId defenderId, Hex target, bool ranged, int toDefender, int toAttacker);
    // Applies an assault's damage to the city (walls first) and the attacker, then capture.
    void resolveCityAssault(UnitId attackerId, CityId cityId, bool ranged, int dealt, int toAttacker);
    CommandError validateBattle(const Command& c) const;
    void applyBattle(const Command& c);
    void captureCity(City& city, UnitId attacker);
    void razeCity(CityId city);
    void checkElimination(PlayerId p);
    void checkVictory();
    void healCities(PlayerId p);
    // Barbarian bookkeeping before a unit dies in combat: camp boldness.
    void noteKill(const Unit& victim, const Unit* killer);
    void killReward(Player& to, const UnitEffect& effect, const UnitType& victim);  // a KillYield effect's reward
    // A unit entered this plot: natural wonders' abilities, a goody hut, a barbarian camp.
    void enterPlot(Unit& unit);
    void clearCamp(Unit& unit);  // a civ's military unit entering a barbarian camp clears it
    void linkBarbarians();
    void processBarbarians();
    void placeCamps(PlayerId barbarian);
    void releaseUnit(Camp& camp, PlayerId barbarian);
    bool releaseScout(Camp& camp, PlayerId bp);
    TypeIndex campUnitType(const Camp& camp, bool ranged, Domain& domain) const;  // the unit the camp would raise
    void applyClan(const Command& c);
    void applyIndustry(const Command& c);
    void convertCamp(int32_t camp);  // the camp becomes a city-state  // false when no Scout can be placed
    bool isBarbarianScout(const Unit& u) const;
    void barbarianScoutAct(UnitId id);
    void barbarianAct(UnitId id);
    void afterAttack(Unit& unit);
    // A captured civilian changes hands as its capture type, or is destroyed.
    void seizeCivilian(UnitId id, PlayerId captor);
    void removeUnit(UnitId id);
    bool exertsZoc(const Unit& unit) const;
    // Marks with `bit` the plots in enemy ZOC for this mover (none when it ignores ZOC) in `plots`, one entry per plot.
    void markZoc(const Unit& mover, std::vector<uint8_t>& plots, uint8_t bit) const;
    void payUnitFuel(PlayerId p);
    void healAndFortify(PlayerId p);
    bool growBorders(City& city);  // false when no plot was available
    void placeDistrict(City& city, TypeIndex district, Hex plot);
    CommandError validateLeader(const Command& c) const;
    void applyLeader(const Command& c);
    CommandError validateGreatPeople(const Command& c) const;
    void applyGreatPeople(const Command& c);
    void applyTradeRoute(const Command& c);
    void processSpies(PlayerId player);  // travel, operations ending and their results
    // Grievances `holder` comes to hold against `against` (Public Relations scales them).
    void addGrievance(PlayerId holder, PlayerId against, int amount);
    void processGrievances();          // world turn: decay, and grievances for cities held
    void processWorldCongress();       // world turn: convene, open sessions, count votes
    void processClimate();             // world turn: warming, climate phases, lowlands; droughts, repairs, disasters
    void processProfiles();            // world turn: update every major civ's play profile
    void processRivals();              // world turn: this game's part of each human's rival memories
    void recordChronicle(const GameEvent& e);  // keeps a chronicle-worthy event for the whole game
    void applyEraStart();  // game creation: the techs, civics, gold, faith and units of the setup's start era
    void processSpaceRace();           // world turn: exoplanet expeditions travel
    void burnPower(PlayerId player);   // power [GS]: each city's demand met by free sources, then by plants burning fuel (CO2)
    int renewablePower(const City& city) const;  // from its Hydroelectric Dam and renewable improvements, before the Biosphère
    void addCo2(PlayerId player, int64_t amount);
    void unitCo2(PlayerId player, size_t resource, int burned);  // units emit CLIMATE_CO2_PERCENT_FROM_UNITS of the CO2
    void openCongressSession();
    void closeCongressSession();
    void aiCongressVotes(PlayerId player);
    void armsControl(PlayerId player);  // Arms Control: devices over the cap are lost
    void resolveSpyOperation(Agent& spy);
    CommandError validateGovernor(const Command& c) const;
    void applyGovernor(const Command& c);
    void processGovernors(PlayerId player);  // establishing counts down; governors in lost cities come home
    CommandError validateDiplomacy(const Command& c) const;
    void applyDiplomacy(const Command& c);
    void executeDeal(const Deal& deal);
    // `holder` remembers something `about` did (no-op unless both are major civs).
    void remember(PlayerId holder, PlayerId about, MemoryKind kind, int amount, int duration);
    // War is declared: memories, deeds, friendships and running deals end.
    void onWarDeclared(PlayerId by, PlayerId target, CasusBelli why = CasusBelli::None);
    void declareWarOn(PlayerId by, PlayerId target, CasusBelli why);  // relations, grievances, memories
    void onPeace(PlayerId a, PlayerId b);
    void processDiplomacy(PlayerId player);  // running deals pay, expire or break; old memories fade
    void processEnvoys(PlayerId player);  // influence and first meetings, each turn
    // Historic moments: an ordinary one, or a world's first (per key) with the ordinary one as fallback.
    void awardMoment(PlayerId player, const char* moment);
    void awardFirst(PlayerId player, const char* worldMoment, const char* ownMoment, int key = 0);
    void awardOnce(PlayerId pid, const char* id);  // a moment a player earns only once
    void circumnavigationMoment(PlayerId pid);     // a plot seen in every column of a map that wraps (09)
    void railroadMoment(PlayerId pid, Hex laid);   // the track just laid or mended joins two of the player's cities (09)
    void unitMoments(PlayerId pid, TypeIndex unitType);  // moments for a unit trained or bought (09)
    void buildingMoments(City& city, TypeIndex building);  // moments for a building completed (09)
    void processEras();  // the world moves to the next era and every civ's age is set
    void processTourism(PlayerId player);
    void processTrade(PlayerId player);  // routes run, end or are plundered
    CommandError validateReligion(const Command& c) const;
    void applyReligion(const Command& c);
    void processReligion();  // passive pressure, each world turn
    // Adds pressure for a religion in cities within `range` of `at` (negative: removes it).
    void shiftPressure(Hex at, int range, int religion, int amount);
    void theologicalCombat(Unit& attacker, Unit& defender);
    // A player's turn: earn points, then recruit whoever they can afford.
    void processGreatPeople(PlayerId player);
    void recruitGreatPerson(PlayerId player, TypeIndex person, const City* in = nullptr);  // appears in `in`, else the capital
    void wonderProphet(City& city, TypeIndex prophet);  // Stonehenge's Great Prophet, or an Apostle (03)
    void greatLibraryEurekas(PlayerId recruiter);         // a random Eureka for each other civ with the Great Library (03)
    void allianceEurekas(PlayerId player);                 // a level-2 Research alliance's Eureka every 30 turns (08)
    TypeIndex unfinishedWonderAt(const City& city, Hex plot) const;  // the wonder the city is building on the plot (03), or kNone
    void applyGreatPersonEffect(Unit& unit, const GreatPersonEffect& fx);
    // A one-time effect for a player at a plot (city: the player's city there, or null).
    void applyEffectAt(PlayerId player, City* city, Hex at, const GreatPersonEffect& fx);
    void completeWonder(City& city, TypeIndex building);
    void spawnLeader(PlayerId p, Hex at);
    // The leader was beaten: captured (melee, city capture) or killed (ranged, its own failed attack).
    // A LeaderLost event records it unless an assassin's own event already does (inBattle false).
    void leaderLost(UnitId leader, PlayerId by, bool captured, bool inBattle = true);
    // After a barbarian fight: never below 1 HP, and home to the capital when badly hurt.
    void barbarianWound(Unit& leader);
    void startInterregnum(Player& p);
    void processLoyalty(PlayerId p);
    void rebellion(City& city);  // a pretender's rebels appear next to the city
    void processFreeCities();
    PlayerId ensureFreeCityPlayer();
    // Hands a city to a new owner (revolt or flip): plots, Palace, queue and citizens follow.
    void transferCity(CityId city, PlayerId to, int loyalty);
    void processAgents();  // world turn: agents travel and strike
    void pushEvent(EventKind kind, PlayerId actor, PlayerId target, int value);
    // Regicide: the player is out and its cities pass to whoever took the leader.
    void regicide(PlayerId loser, PlayerId by);
    Unit* escortMut(const Unit& leader);
    void applyMove(const Command& c);
    void applyFoundCity(const Command& c);
    void applyEndTurn(const Command& c);
    // Moves the unit along its move order as far as its moves allow.
    void advanceUnit(UnitId id);
    // Whether `unit` may pass `plot` but not stop there (05: Stacking): another of its owner's units of its layer, or
    // another player's unit, holds it.
    bool passOnly(const Unit& unit, Hex plot) const;
    // How far `path` takes a unit about to step onto a plot it may only pass, this turn: past such plots to a free one
    // (1), to the end of its moves among them (0), or to the path's end with it still held (-1).
    int passOn(const Unit& unit, const std::vector<PathStep>& path) const;
    // findPath's search; with `along`, one kept to the plots of `along` (and with enemy ZOC only where it ends a pass
    // over plots others hold, as elsewhere it changes only the moves a path leaves): a path it finds then shows that
    // findPath finds one too. With `reach`, a search with no goal (reading ZOC the same way) that marks in it every plot
    // a move order may go to.
    std::optional<std::vector<PathStep>> searchPath(UnitId id, Hex target, bool overland, const std::vector<PathStep>* along,
                                                    std::vector<uint8_t>* reach = nullptr) const;
    // The unit a move order for `unit` is planned for: a linked escort's moves the pair, so it is planned for the leader.
    const Unit& orderMover(const Unit& unit) const;
    void beginPlayerTurn(PlayerId p, bool runCities = true);
    void beginGlobalTurn();
    void refreshVisibility(PlayerId p);
    Unit& spawnUnit(TypeIndex type, PlayerId owner, Hex pos);

    const Rules* rules_;
    // World wonders read in hot paths, looked up once (03: Wonders).
    enum class W : uint8_t { Kilwa, Sankore, Oracle, MachuPicchu, Colosseum, Liberty, Zimbabwe, Torre, Eiffel, GoldenGate, Biosphere, Cristo, Huey, Count };
    TypeIndex wonders_[static_cast<size_t>(W::Count)] = {};
    TypeIndex wonderType(W w) const { return wonders_[static_cast<size_t>(w)]; }
    bool holdsWonder(PlayerId player, W w) const;  // one of the player's cities has it
    // Beliefs read in hot paths, looked up once (06: Religion).
    enum class Bf : uint8_t {
        DanceOfTheAurora, DesertFolklore, SacredPath, EarthGoddess, GodOfHealing, GodOfWar, InitiationRites, HolyWaters,
        DivineInspiration, JesuitEducation, ReligiousCommunity, Reliquaries, WarriorMonks, WorkEthic, SacredPlaces, PapalPrimacy, ReligiousUnity, Count
    };
    TypeIndex beliefs_[static_cast<size_t>(Bf::Count)] = {};
    bool beliefInPlay(Bf b) const;                      // some pantheon or religion has it
    bool cityFollows(const City& city, Bf b) const;     // the city's majority religion has it, or its owner's pantheon while it has none
    bool playerHasBelief(PlayerId player, Bf b) const;  // the player's pantheon or founded religion has it
    // Great people whose effects are in code, looked up once (07: Great People).
    enum class Gp : uint8_t {
        ZhengHe, ZhangQian, MarcoPolo, IbnFadlan, RajaTodarMal, Rockefeller, MimarSinan, Crassus, Hildegard, Roebling, JaneDrew, Zahrawi,
        IbnKhaldun, KenzoTange, Raskova, Spilsbury, Rubinstein, Strauss, Lauder, JamesYoung, Goddard, Medici, Count
    };
    TypeIndex oil_ = kNone;  // James Young reveals it (07)
    TypeIndex greatPeople_[static_cast<size_t>(Gp::Count)] = {};
    // City-states whose suzerain bonuses are in code, looked up once (08: City-States).
    enum class Cs : uint8_t {
        Akkad, Anshan, Antananarivo, Ayutthaya, BandarBrunei, BuenosAires, Chinguetti, Fez, Hattusa, Hunza, Jerusalem, Johannesburg, Kabul, Kandy, Kumasi,
        MexicoCity, Mogadishu, MohenjoDaro, Nalanda, NanMadol, Ngazargamu, Samarkand, Singapore, Valletta, VaticanCity, Venice, Vilnius, Wolin, Yerevan,
        Zanzibar, Count
    };
    TypeIndex cityStates_[static_cast<size_t>(Cs::Count)] = {};
    bool suzerainBonus(PlayerId player, Cs cityState) const;  // suzerainBonus(player, its id)
    // The players of each city-state type (by Rules::cityStates index) when the game was built, in player order, and
    // how many players there were then. A player's type is set before it joins, and players only ever join after the
    // others (a clan settling, a free city), so cityStateOfType looks through these lists and the later players only.
    std::vector<std::vector<size_t>> cityStatePlayers_;
    size_t playersAtStart_ = 0;
    // The first living player of a city-state type of the rules, as a look through the players in order finds it (null:
    // none).
    const Player* cityStateOfType(TypeIndex type) const;
    TypeIndex products_[4] = {};  // Toys, Cosmetics, Jeans, Perfume: the luxury corporations' products (07)
    TypeIndex spices_[2] = {kNone, kNone};  // Cinnamon and Cloves: Zanzibar's suzerain holds a copy of each (08)
    TypeIndex oceanTerrain_ = kNone;          // TERRAIN_OCEAN: sailed once the owner may enter the Ocean
    TypeIndex encampment_ = kNone;            // DISTRICT_ENCAMPMENT: exerts zone of control like a city (markZoc)
    std::vector<TypeIndex> oceanTechs_;       // the techs that open the Ocean (Cartography)
    std::vector<TypeIndex> embarkTechs_;      // the techs that let land units, or one of their types, embark
    // Each tech's and civic's era, by index (playerEra), raised to 0 where below: a player's era is never below the first.
    std::vector<uint8_t> techEras_, civicEras_;
    std::vector<TypeIndex> borderCivics_;     // the civics that close a civ's borders (Early Empire)
    int suzerainEnvoys_ = 0;                  // INFLUENCE_TOKENS_MINIMUM_FOR_SUZERAIN (isSuzerain)
    int touristTourism_ = 0;                  // TOURISM_TOURISM_TO_MOVE_CITIZEN (visitingTourists)
    int touristCulture_ = 0;                  // TOURISM_CULTURE_PER_CITIZEN (domesticTourists)
    TypeIndex pamukkale_ = kNone;             // FEATURE_PAMUKKALE: an amenity per natural wonder in its city's land (cityReport)
    // The districts whose buildings Mexico City's suzerain reaches farther from: Industrial Zone, Entertainment Complex
    // and Water Park (cityReport).
    TypeIndex fartherDistricts_[3] = {kNone, kNone, kNone};
    // Corps and Armies trained whole (canTrainFormation): the civics for each (Nationalism, Mobilization), and the
    // buildings land units and ships need (Military Academy, Seaport).
    TypeIndex formationCivics_[2] = {kNone, kNone};
    TypeIndex formationSchools_[2] = {kNone, kNone};
    // By unit type: the GrantAbility player modifiers whose ability covers its class, in modifier order (unitAbilities).
    std::vector<std::vector<uint32_t>> abilityGrants_;
    std::vector<uint64_t> abilityKinds_;  // by ability: the effectBits of its effects' kinds (abilitiesWithEffects)
    // Whether any plot is a National Park: a game starts with those its state has, designatePark makes the others and
    // none is ever lost, so while this is false the park scans have nothing to find.
    bool parks_ = false;
    // The plots of each landmass (a continent id k >= 0; only map generation and loading set them), in map order, one
    // landmass after another in the order they first appear: landmass i is landPlots_[landFirst_[i]] up to
    // landPlots_[landFirst_[i + 1]].
    std::vector<int32_t> landFirst_, landPlots_;
    // The plots that had a resource when the game was built, in map order. Play only takes resources away (a harvest,
    // a district, a submerged coast), never adds one, so every plot with a resource is among them.
    std::vector<int32_t> resourcePlots_;
    // isLake for each plot (a lakeMap). Only the sea's rise changes terrain once the map is made, and it marks them again.
    std::vector<uint8_t> lakes_;
    // The plots that have had an improvement since the game was built, in the order they first had one, and a mark
    // on each of them. Once the map is made only a Builder's work adds an improvement, and it lists the plot.
    std::vector<int32_t> improvedOnce_;
    std::vector<uint8_t> improvedOnceAt_;
    // The improvements that take each terrain, feature and resource (ImprovementType::validTerrains, validFeatures and
    // validResources), in the rules' order: those improvementsAt tries on a plot.
    std::vector<std::vector<TypeIndex>> terrainImprovements_, featureImprovements_, resourceImprovements_;
    // What the wonders of one city's list of things to make (buildableItems) look up alike, each part worked out for the
    // first wonder that needs it: the buildings standing anywhere (by building: 1 when some city has it), for
    // wonderBuilt, and the city's plots that wonderOpen allows.
    struct WonderShare {
        std::vector<uint8_t> built;
        std::optional<std::vector<Hex>> open;
    };
    // canProduce with that shared by its wonders (null: each looks it up).
    bool canProduce(const City& city, ProductionItem item, CommandError* why, bool purchase, WonderShare* shared) const;
    // purchaseCost's price for a unit or building, whether or not Gold may buy it (Valletta opens walls to Faith; 08).
    int purchasePrice(PlayerId player, ProductionItem item, const City* city, YieldType currency) const;
    // The larger share of the tech or civic tree the player has completed (GAME_PROGRESS costs).
    Fixed treeProgress(PlayerId player) const;
    // A unit's cost before game speed: + PREVIOUS_COPIES for each copy already made, or x GAME_PROGRESS.
    int unitCost(PlayerId player, TypeIndex type) const;
    // tradePath to each of the destinations (each on a plot of its own), in their order, from one search.
    std::vector<std::vector<Hex>> tradeWays(PlayerId player, TypeIndex traderType, const City& origin, const std::vector<const City*>& destinations) const;
    // canPlaceWonder's checks that do not depend on the wonder (the plot is the city's own land within 3 plots, not its
    // center, with no visible resource, natural wonder, district, wonder or barbarian camp), and those that do.
    bool wonderOpen(const City& city, Hex plot) const;
    bool wonderFits(const City& city, TypeIndex building, Hex plot) const;
    // Whether wonderPlots or districtPlots would list a plot, trying them in the same order and stopping at the first.
    // `open`: the city's plots that wonderOpen allows, listed on first use (null: each plot is looked at in full).
    bool anyWonderPlot(CityId city, TypeIndex building, std::optional<std::vector<Hex>>* open = nullptr) const;
    bool anyDistrictPlot(CityId city, TypeIndex district) const;
    // canPlaceDistrict with `cityChecks` false: the district's checks that do not depend on the plot (districtOpenIn,
    // districtUnblockedIn) are taken as passed.
    bool canPlaceDistrict(const City& city, TypeIndex type, Hex plot, CommandError* why, bool cityChecks) const;
    // canPlaceDistrict's checks that do not depend on the plot: those it makes before the plot's (the district is
    // unlocked, not yet placed here, and the city's population allows another) and after them (no district it
    // excludes here, and none of a one-per-civ kind anywhere).
    bool districtOpenIn(const City& city, TypeIndex type) const;
    bool districtUnblockedIn(const City& city, TypeIndex type) const;
    // currentGreatPerson and greatPersonCost with the world era already worked out.
    TypeIndex currentGreatPerson(TypeIndex cls, int world) const;
    int greatPersonCost(TypeIndex person, int world) const;
    // Copies of each resource the player holds, added into `n` (by resource index); `only`: just that one (kNone: all).
    void addCopies(PlayerId player, TypeIndex only, std::vector<int>& n) const;
    // The part of them not counted off its land: corporations' products, luxuries granted, Zanzibar's spices.
    void addCopiesOffMap(PlayerId player, TypeIndex only, std::vector<int>& n) const;
    // What a run of one civ's city reports shares, each part worked out on first use: the owner's luxuriesHeld and
    // luxuryShares, the National Park plots of each city, the owner's suzerainBonus by city-state kind (0 not asked
    // yet, 1 no, 2 yes), and the cities holding the buildings whose modifiers reach all its cities. Valid while no
    // city changes hands, grows or shrinks and no building is built or lost.
    struct ReportShare {
        std::optional<std::vector<uint8_t>> luxuries;
        std::optional<std::vector<std::pair<CityId, int>>> luxuryShares;
        std::optional<std::map<CityId, int>> parkPlots;
        uint8_t suzerain[static_cast<size_t>(Cs::Count)] = {};
        BuildingHolders holders;
    };
    // suzerainBonus(player, cityState) for the owner of a run of city reports, asked once per kind for the run.
    bool suzerainBonus(PlayerId player, Cs cityState, ReportShare& shared) const;
    CityReport cityReport(const City& city, ReportShare& shared) const;
    // plotYields, told whether the city follows Earth Goddess (cityFollows), for a pass over many of its plots.
    Yields plotYields(Hex plot, const City& city, bool earthGoddess) const;
    // `report`, when given, gets the report the change was worked out from (none for a city-state's or a Free City's).
    Fixed loyaltyPerTurn(const City& city, ReportShare& shared, std::optional<CityReport>* report = nullptr) const;
    int luxuryAmenities(const City& city, ReportShare& shared) const;
    // The Amenities each of the player's cities gets from luxuries, in state order.
    std::vector<std::pair<CityId, int>> luxuryShares(PlayerId player, ReportShare& shared) const;
    int parkAmenities(const City& city, ReportShare& shared) const;
    // goldPerTurn from the player's city reports, in city order, when the caller has made them already.
    Fixed goldPerTurn(PlayerId player, const std::vector<CityReport>* reports) const;
    int usedHere(const City& city, Gp g) const;  // times it was used on the city's land
    bool usedBy(PlayerId player, Gp g) const;    // the player has used it
    bool codedGreatPerson(TypeIndex person) const;
    int extraPalaceSlots(const City& city, TypeIndex building) const;  // Giovanni de' Medici: +2 in each Bank (07)
    static uint32_t bit(W w) { return 1u << static_cast<unsigned>(w); }
    uint32_t heldWonders(PlayerId player, uint32_t which) const;  // bit(w) for each of the wonders in `which` the player holds
    void grantTorreBuildings(PlayerId player);     // Torre de Belém's one-time buildings
    void bridgeRoads(const City& city, TypeIndex building);  // the Golden Gate Bridge's roads (03)
    GameState state_;
    std::vector<Command> log_;
    std::vector<std::pair<PlayerId, Hex>> captures_;  // Builders owed by Jaguar-style kills this command
    void spawnCaptures();
    // The path a MoveUnit command's check found, and the unit it was found for (an escort's order plans for its leader).
    struct CheckedPath {
        UnitId unit = kNoUnit;
        std::vector<PathStep> steps;
    };
    // validate, also handing back the path a MoveUnit command's check found.
    CommandError validate(const Command& c, std::optional<CheckedPath>* movePath) const;
    // That path while submit applies the move: the unit's first step takes it rather than search again (advanceUnit).
    std::optional<CheckedPath> checkedPath_;
};

// Plain-English deal terms ("England gives 100 Gold; France gives Wine for 30 turns"), for the
// HUD, logs and the dialogue layer's prompts.
SOV_API std::string describeDealItem(const Rules& rules, const GameState& state, const DealItem& item);
SOV_API std::string describeDeal(const Rules& rules, const GameState& state, const Deal& deal);
SOV_API const char* relationshipName(Relationship r);
SOV_API const char* opinionReasonName(OpinionReasonKind k);

}  // namespace sov
