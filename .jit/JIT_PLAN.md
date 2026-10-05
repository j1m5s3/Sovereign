# Plan: the strategy AI and difficulty (step 6, "everything else")

Status: active, 2026-10-05. Previous: `jit_history/2026-10-05-civ-systems-2.md`. Chosen by Claude under James's standing consent: the climate soak showed the AI far behind Civ VI's pace (4 techs and 2 cities at turn 50, 31-37 techs at turn 300 on a Small map, Standard speed), so the first AI layer from leader doc §10 comes next. James can redirect at any point.

Specs: 10-ai-ui-implementation (AI architecture: the data shape, Recommended clone architecture); 00-overview (Difficulty levels, AI starting units); leader-character-brainstorm §10 (the four AI layers; Difficulty [decided]: skill first, Civ-style bonuses only at the top two levels). Code: `core/src/ai.cpp`, `core/include/sovereign/ai.h`, `core/tools/sovsim.cpp`.

## Milestones (one PR each)

1. **Pace benchmark and the economic planner:**
   - A benchmark: `sovsim --bench` plays several seeds and prints per-checkpoint averages (turns 50/100/150/200/300: cities, population, techs, civics, science, culture, production, gold banked, era) so every change is measured. A test pins a floor so pace cannot silently regress.
   - Check the core's yield rules the pace rests on (growth, science per citizen, boosts, housing) against the specs before tuning the AI around them.
   - Economic planner per 10-ai Recommended architecture: settling by StandardSettlePlot with the minimum site value and decay, settlers early while good sites remain (Rapid Expansion), builders on the best unimproved worked tiles and boost triggers, citizen management with a food floor, research and civics scored by unlocks x yield bias x boost progress (chasing boosts), production by yield value per cost with needs (housing, amenities, threat), gold spent by the DefaultSavings split (units, slush fund, plots) instead of hoarded.
2. **Grand strategy:** the 16 strategies with their conditions (victory strategies, Early Exploration, Rapid Expansion, Naval, Wonder Obsessed, Dark Age, one per era) re-evaluated every few turns; active strategies shift research, civic, production, diplomacy and military weights; leaders' agendas add their own weights.
3. **Military layer:** threat and opportunity influence maps; operations (attack city, attack walled city with siege, camp clearing, city defence, escorted settlers, naval superiority) launched at their strength ratios and minimum odds; tactical moves that focus fire, retreat to heal and avoid ending exposed; upgrades.
4. **Difficulty:** Settler..Deity in the setup. Per leader doc §10, levels scale skill first (how far the planner looks ahead, boost chasing from Warlord, Rapid Expansion off at Warlord and below, operation quality, how much player modelling it will use later); the Civ yield, combat, XP, free-boost and starting-unit bonuses apply only at Immortal and Deity. Human bonuses at Settler and Chieftain and the barbarian distance and throttle follow 00-overview. Unreal launch option and HUD.

Player modelling (leader doc §10 layer 2) is the plan after this one.

## Decisions (Claude's recommendations; James gave standing consent)

- Measure first: every economic change lands with its benchmark numbers in the PR. Targets are Sovereign's own (no published per-turn Civ VI curves): roughly double today's turn-100 techs and finish the tech tree before turn 400 at Prince on Standard speed, without AI cheats.
