// The rules core's front door. UI, AI, network and tests all change the game
// the same way: submit(Command). Queries never change state.
#pragma once

#include "sovereign/api.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "sovereign/commands.h"
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
    int attackerStrength = 0, defenderStrength = 0;
    int damageToDefenderMin = 0, damageToDefenderMax = 0;
    int damageToAttackerMin = 0, damageToAttackerMax = 0;  // melee only
};

struct PathStep {
    Hex pos;
    int turn = 0;       // 0 = reached this turn
    Fixed movesLeft;    // after entering this plot
};

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
    const std::vector<Command>& log() const { return log_; }
    // Hash of the full state (not the log); compared between peers each turn.
    uint64_t stateHash() const;

    // Path for a unit from where it stands, planned on its owner's knowledge.
    // overland: a land unit does not embark on the way (it may still leave the water).
    std::optional<std::vector<PathStep>> findPath(UnitId unit, Hex target, bool overland = false) const;
    // Movement points needed for this unit to enter `to` from adjacent `from`;
    // nullopt when it cannot enter.
    std::optional<Fixed> moveCost(const Unit& unit, Hex from, Hex to) const;
    // ---- naval play and embarkation (05-units-and-combat.md, Embarkation)
    bool canEmbark(PlayerId player, TypeIndex unitType) const;
    bool canEnterOcean(PlayerId player) const;
    // A land unit standing on water.
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
    int productionCost(PlayerId player, ProductionItem item) const;
    // Gold price, or -1 when the item cannot be bought with gold.
    int purchaseCost(PlayerId player, ProductionItem item) const;
    // Gold price of a plot, or -1 when this city cannot buy it.
    int plotPurchaseCost(CityId city, Hex plot) const;
    bool canProduce(const City& city, ProductionItem item, CommandError* why = nullptr) const;
    // Where a unit trained in this city appears (ships: the port or the water beside it); nullopt when full.
    std::optional<Hex> unitSpawnPlot(const City& city, TypeIndex unitType) const;
    std::vector<ProductionItem> buildableItems(CityId city) const;
    std::vector<CityId> citiesNeedingProduction(PlayerId player) const;
    // Net gold per turn: city gold minus building and unit maintenance.
    Fixed goldPerTurn(PlayerId player) const;

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

    // ---- research and government (04-tech-civics-government.md)
    int techCost(TypeIndex tech) const;    // scaled by game speed
    int civicCost(TypeIndex civic) const;  // scaled by game speed
    bool hasUnlocked(PlayerId player, Unlock u) const;
    bool canResearch(PlayerId player, TypeIndex tech) const;
    bool canStudyCivic(PlayerId player, TypeIndex civic) const;
    std::vector<TypeIndex> availableTechs(PlayerId player) const;
    std::vector<TypeIndex> availableCivics(PlayerId player) const;
    // Science and culture the player earns this turn (zero in anarchy).
    Fixed sciencePerTurn(PlayerId player) const;
    Fixed culturePerTurn(PlayerId player) const;
    // Whether the player meets a boost's condition right now.
    bool boostMet(PlayerId player, const Boost& boost) const;
    bool canAdoptGovernment(PlayerId player, TypeIndex government, CommandError* why = nullptr) const;
    // Policy may be slotted under the player's government (unlocked, not obsolete, allowed).
    bool policyAvailable(PlayerId player, TypeIndex policy) const;
    bool canSetPolicy(PlayerId player, int slot, TypeIndex policy, CommandError* why = nullptr) const;
    // Slot type of slot `slot` of a government: Military slots first, then
    // Economic, Diplomatic and Wildcard.
    static PolicySlot slotType(const GovernmentType& government, int slot);
    // ---- improvements and resources (01-map-and-terrain.md, 02-cities.md)
    bool resourceVisible(PlayerId player, Hex plot) const;
    // The plot's resource is worked by a matching improvement or a city center.
    bool resourceImproved(Hex plot) const;
    bool canImproveAt(PlayerId player, Hex plot, TypeIndex improvement) const;
    std::vector<TypeIndex> improvementsAt(PlayerId player, Hex plot) const;
    bool canHarvestAt(PlayerId player, Hex plot) const;
    // Yields the plot's improvement adds for its owner (base, tech bonuses, adjacency).
    Yields improvementYields(Hex plot, PlayerId owner) const;
    Fixed improvementHousing(const City& city) const;
    int luxuryAmenities(const City& city) const;
    bool hasStrategicFor(PlayerId player, TypeIndex unitType) const;
    bool unitObsolete(PlayerId player, TypeIndex unitType) const;
    // Plots the player owns with this improvement (kNone: any), optionally only on a resource it works.
    int countImprovedPlots(PlayerId player, TypeIndex improvement, bool onResourceOnly) const;

    // ---- war and combat (05-units-and-combat.md)
    bool atWar(PlayerId a, PlayerId b) const;
    bool canDeclareWar(PlayerId player, PlayerId target) const;
    bool canMakePeace(PlayerId player, PlayerId target) const;
    // Abilities in force on a unit: innate ones plus those its owner's modifiers grant.
    std::vector<TypeIndex> unitAbilities(const Unit& unit) const;
    // Sum of `amount` over the unit's promotion and ability effects of this kind
    // (conditions ignored); an effect without an amount counts 1.
    int unitEffectTotal(const Unit& unit, UnitEffectKind kind) const;
    bool unitHas(const Unit& unit, UnitEffectKind kind) const { return unitEffectTotal(unit, kind) > 0; }
    int maxMoves(const Unit& unit) const;
    int unitRange(const Unit& unit) const;
    int unitSight(const Unit& unit) const;
    int maxAttacks(const Unit& unit) const;
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
    bool canRazeCity(PlayerId player, CityId city) const;
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

    // Sizes a player's per-rules vectors (trees, government uses, units trained).
    static void fitPlayerToRules(Player& p, const Rules& rules);

private:
    void apply(const Command& c);
    CommandError validateCity(const Command& c) const;
    void applyCity(const Command& c);
    void processCities(PlayerId p);
    CommandError validateResearch(const Command& c) const;
    void applyResearch(const Command& c);
    void processResearch(PlayerId p, Fixed science, Fixed culture);
    void completeNode(PlayerId p, bool civic, TypeIndex node);
    void updateBoosts(PlayerId p);
    CommandError validateBuilder(const Command& c) const;
    void applyBuilder(const Command& c);
    void accumulateStrategics(PlayerId p);
    CommandError validateCombat(const Command& c) const;
    void applyCombat(const Command& c);
    std::optional<Fixed> terrainCost(const Unit& unit, Hex from, Hex to) const;
    bool lineOfSight(Hex from, Hex to) const;
    void gainXp(Unit& unit, int ownBase, int enemyBase, bool ranged, bool attacker, bool killed, bool vsBarbarian);
    void awardXp(Unit& unit, int xp, bool vsBarbarian);
    int unitStrength(const Unit& unit, const Unit* oppUnit, const City* oppCity, bool attacking, bool ranged) const;
    // Percent of a hit on this city that lands on its walls; -1 when it lands on the city.
    int wallDamagePercent(const Unit& attacker, const City& city, bool ranged) const;
    void attackCity(const Command& c, City& city);
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
    // A military unit entered this plot: clears a barbarian camp there.
    void enterPlot(Unit& unit);
    void linkBarbarians();
    void processBarbarians();
    void placeCamps(PlayerId barbarian);
    void releaseUnit(Camp& camp, PlayerId barbarian);
    void barbarianAct(UnitId id);
    void afterAttack(Unit& unit);
    // A captured civilian changes hands as its capture type, or is destroyed.
    void seizeCivilian(UnitId id, PlayerId captor);
    void removeUnit(UnitId id);
    bool exertsZoc(const Unit& unit) const;
    // Plots in enemy ZOC for this mover (empty: none, or it ignores ZOC).
    std::vector<uint8_t> zocMap(const Unit& mover) const;
    void payUnitFuel(PlayerId p);
    void healAndFortify(PlayerId p);
    void assignCitizens(City& city);
    bool completeItem(City& city, ProductionItem item);  // false when it cannot complete now
    bool growBorders(City& city);  // false when no plot was available
    void placeDistrict(City& city, TypeIndex district, Hex plot);
    CommandError validateLeader(const Command& c) const;
    void applyLeader(const Command& c);
    CommandError validateGreatPeople(const Command& c) const;
    void applyGreatPeople(const Command& c);
    // A player's turn: earn points, then recruit whoever they can afford.
    void processGreatPeople(PlayerId player);
    void recruitGreatPerson(PlayerId player, TypeIndex person);
    void applyGreatPersonEffect(Unit& unit, const GreatPersonEffect& fx);
    void spawnLeader(PlayerId p, Hex at);
    // The leader was beaten: captured (melee, city capture) or killed (ranged, its own failed attack).
    void leaderLost(UnitId leader, PlayerId by, bool captured);
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
    void beginPlayerTurn(PlayerId p, bool runCities = true);
    void beginGlobalTurn();
    void refreshVisibility(PlayerId p);
    Unit& spawnUnit(TypeIndex type, PlayerId owner, Hex pos);

    const Rules* rules_;
    GameState state_;
    std::vector<Command> log_;
};

}  // namespace sov
