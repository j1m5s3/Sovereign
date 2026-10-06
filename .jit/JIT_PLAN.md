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
2. **Policy changes for Gold** (`POLICY_COST_*`); **Inquisitors, Launch Inquisition and Gurus** (Remove Heresy, heal charges); **Trading Posts** (route range from the last post, +Gold per foreign post); **Future Tech and Future Civic** (repeatable).
3. **Deals and city-states:** ceding cities and trading Diplomatic Favor in deals; Suzerain War (city-states join their suzerain's wars); Make Demand.
4. **Larger, as time allows:**
   - World Congress resolutions (12 more);
   - historic moments (about 140 still unawarded);
   - disasters pillaging districts and buildings; moving storms, spreading fires, meteor showers; deforestation's effect on warming; the Flood Barrier's scaled cost;
   - the 9 governor promotions still without effect.

## Not planned

- Occupied cities' Amenity penalty: the spec gives no amount.
- Era score in the score: the core keeps no lifetime era score.

- AI random (hidden) agendas: Sovereign's leaders each have one hand-written agenda (specs/sovereign/leaders-and-art-style.md).
- Liberation envoys by era: no data for them.
- Trade route length in round trips: the flat length stands as an approximation.
