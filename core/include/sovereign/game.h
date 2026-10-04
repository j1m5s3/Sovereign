// The rules core's front door. UI, AI, network and tests all change the game
// the same way: submit(Command). Queries never change state.
#pragma once

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
    int defense = 0;          // bonus defense from modifiers (combat arrives in MVP-4)
};

struct PathStep {
    Hex pos;
    int turn = 0;       // 0 = reached this turn
    Fixed movesLeft;    // after entering this plot
};

class Game {
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
    std::optional<std::vector<PathStep>> findPath(UnitId unit, Hex target) const;
    // Movement points needed for this unit to enter `to` from adjacent `from`;
    // nullopt when it cannot enter.
    std::optional<Fixed> moveCost(const Unit& unit, Hex from, Hex to) const;
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
    std::vector<ProductionItem> buildableItems(CityId city) const;
    std::vector<CityId> citiesNeedingProduction(PlayerId player) const;
    // Net gold per turn: city gold minus building and unit maintenance.
    Fixed goldPerTurn(PlayerId player) const;

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
    void assignCitizens(City& city);
    bool completeItem(City& city, ProductionItem item);  // false when it cannot complete now
    bool growBorders(City& city);  // false when no plot was available
    std::optional<Hex> unitSpawnPlot(const City& city, TypeIndex unitType) const;
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
