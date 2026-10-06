# Plan: the spec audit, part 2 (step 6, "everything else")

Status: active, 2026-10-06. Previous: `jit_history/2026-10-06-spec-audit-gaps.md` (part 1: specs 01, 02, 03, 05). Two more read-only audits compared specs/civ6 04, 06, 07, 08 and 09 against the core and confirmed 28 gaps by grep. Chosen by Claude under James's standing consent.

## Milestones (one PR each)

1. **Done: small rules** (occupied cities' amenities and era score in the score skipped, see below):
   - Tech and civic costs ±20% around the world era [GS] (`TECH_COST_PERCENT_CHANGE_*`).
   - Religious tourism: 50% toward civs of another majority religion, and 50% after The Enlightenment (`TOURISM_DIFFERENT_RELIGION_REDUCTION`).
   - Trade routes to city-states give a bonus by the city-state's type (`MINOR_CIV_*_SEND_TRADE_ROUTE_BONUS`).
   - Condemn Heretic removes the victim religion's pressure nearby (`RELIGION_SPREAD_UNIT_CAPTURE`).
   - Grievances for war on a city-state where the victim's rival has envoys or is suzerain (`GRIEVANCES_*_CITY_STATE_DOW`).
   - The suzerain gets the city-state's luxuries and strategics.
   - Occupied cities lose Amenities until peace.
   - Monarchy: +50% envoys, and Favor per city with Renaissance Walls.
   - The score's missing line items: great people, religion, wonders, era score.
   - Moved art is locked for 10 turns (`GREATWORK_ART_LOCK_TIME`).
   - Legacy policy cards can be slotted after their government.
2. **Done:** **Policy changes for Gold** (`POLICY_COST_*`); **Inquisitors, Launch Inquisition and Gurus** (Remove Heresy, heal charges); **Trading Posts** (route range from the last post, +Gold per foreign post); **Future Tech and Future Civic** (repeatable).
3. **Done: Deals and city-states:** ceding cities and trading Diplomatic Favor in deals; Suzerain War (city-states join their suzerain's wars); Make Demand.
4. **Larger, as time allows** (done so far: the governor promotions and the disaster items marked below):
   - World Congress resolutions (12 more);
   - historic moments (about 140 still unawarded);
   - **done:** disasters pillaging districts and buildings (a meltdown destroys them); storms moving on for 3 turns; forest fires spreading; deforestation scaling CO2 (-20% to +50%);
   - **done:** governor promotions: Air Defense Initiative, Foreign Investor, Grand Inquisitor, Laying On Of Hands, Patron Saint, Black Marketeer, Vertical Integration (Messenger was already in place);
   - left: meteor showers and Aquaculture/Parks and Recreation (the generated rules lack the meteor event, the Fishery and the City Park); the Flood Barrier's scaled cost (costs have no per-city path yet).

## Not planned

- Occupied cities' Amenity penalty: the spec gives no amount.
- Era score in the score: the core keeps no lifetime era score.

- AI random (hidden) agendas: Sovereign's leaders each have one hand-written agenda (specs/sovereign/leaders-and-art-style.md).
- Liberation envoys by era: no data for them.
- Trade route length in round trips: the flat length stands as an approximation.

## Decisions (Claude's, under James's standing consent)

- Policy changes for Gold: the spec leaves the formula unverified. The core charges POLICY_COST_BASE + (POLICY_COST_INCREASE_TO_BE_EXPONENTED x civics done)^1.5, rounded down to POLICY_COST_VISIBLE_DIVISOR; the `BuyPolicyChanges` command opens the turn's changes.
- Launch Inquisition is open to the founder of the Apostle's religion, once. Inquisitors are bought in cities following the player's own religion.
- Trade range refuels in the player's own cities and in cities holding its Trading Post. Routes may go to city-states (they could not before).
- Make Demand is a deal in which only the other side gives. An AI yields to a civ at least twice as strong when the demand costs it at most 200 Gold of worth, plus 100 for each further multiple of strength. It resents the demand (−10 opinion). The AI does not make demands itself yet.
- Ceded cities: peace deals only; never the capital or the giver's last city. A ceded city arrives at 50 loyalty. An AI losing badly values peace at 400. The AI does not offer cities itself.
- Suzerain War: the city-states of either side's suzerainty join a war between majors, and make peace with them.
- Grand Inquisitor: +10 religious strength in Moksha's city's territory (the core does not record where a unit was bought). Black Marketeer: units trained there need 80% fewer strategic resources, rounded up. Deforestation: the level comes from the world's share of woods lost since climate tracking began, not from a running average.
