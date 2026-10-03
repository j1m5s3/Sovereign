# 07 Economy, Trade, Great People, Great Works, Tourism

## Gold
- Empire treasury. Income: tiles, Commercial Hubs/Harbors and buildings, trade routes, Palace, city-states, policies, specialists, plunder, selling/trading with other civs, deals (gold per turn).
- Expenses (per turn): building and district maintenance (most buildings 1–4G; districts 1G except City Center, unique ones reduced), unit maintenance (by era; ancient/classical 1G, modern 4–6G (tunable); free units from government/policies), deals paid per turn. Governors have no maintenance.
- Treasury < 0 with negative income: each turn one unit is disbanded (the one with the highest gold maintenance) and all cities suffer −1 Amenity while the treasury is negative (tunable).
- Gold purchase: units/buildings/tiles; Great People via Patronage; unit upgrades; levying city-state militaries; diplomatic favor via deals [GS]; policy swaps outside a civic completion. Envoys cannot be bought with gold.

## Trade routes
- **Capacity**: +1 from Foreign Trade civic, +1 per city with a Commercial Hub, +1 per city with a Harbor if that city has no Commercial Hub (max +1 per city from districts); wonders (Colossus +1, Great Zimbabwe +1), governments (Merchant Republic +2), Great Merchant Marco Polo +1, some civ abilities, policies.
- **Trader** unit (unlocked by the Foreign Trade civic; cost 40 P): moves to a city it can start from, then chooses a destination. Range: 15 tiles over land, 30 over water (tunable); routes may cross water once the trader can embark (Celestial Navigation) or the origin has a Harbor. Routes follow the path the trader takes, building **roads** along land segments.
- Duration: route lasts N turns (scaled ~20–30 by era/speed); Trader repeatedly goes back and forth; at end it is free to pick a new route (can renew same).
- **Yields** (origin unless stated):
  - **Domestic route**: origin gets food and production: base +1 Food +1 Production, plus extra Food/Production per specialty district in the **destination** city (data table keyed by destination district; e.g., +1 Food per Harbor, +1 Production per Industrial Zone (tunable)). Destination gets nothing.
  - **International route** (to other civ): origin gains gold (+3 base, +1 per specialty district in destination, +2 extra for a Commercial Hub, +1 extra for a Harbor, +1 per luxury resource in destination (tunable)), and the origin gets science/culture/faith from destination districts via policies/abilities; destination gets +1 Gold (tunable). Gives the origin's owner +25% tourism toward the destination civ and religion pressure both ways.
  - **City-state route**: like international; quests may require; +gold; suzerain bonuses.
- Trade routes add **tourism +25%** between civs and allow sharing of religious pressure.
- **Trading Posts**: when a route terminates in a foreign city, your trading post is established there (visible); routes passing through cities with your trading posts get +range and +1 Gold.
- Plunder: enemy military units on the route's path plunder: trader destroyed, plunderer gets gold.

## Diplomatic deals (trade screen)
Tradeable items: gold (lump sum), gold per turn (30 turns), luxury and strategic resources (per turn), Great Works, cities (peace deals), open borders, embassy/delegation, joint war, alliances [R&F], diplomatic favor [GS]. Each item valued by AI using utility (see AI file).

## Great People
Classes: Great Prophet, Great Scientist, Great Engineer, Great Merchant, Great Writer, Great Artist, Great Musician, Great General, Great Admiral. ([GS] Comandantes in late era are General variants.)
- Each class has an ordered roster of historical individuals per era (e.g., Scientists: Hypatia, Euclid, Aryabhata, ..., Albert Einstein). At any time, each class exposes **one** "available" individual (the earliest unclaimed one in the current era or earlier; era gating by world era).
- **GPP**: per class, per player, accumulated each turn from districts (+1), buildings (+1 each), wonders, policies (+2 Inspiration etc.), projects, specialists.
- **Recruit**: when a player's GPP in a class ≥ cost of the current individual, that player recruits them (points deducted). Cost increases per individual recruited in that class globally (each subsequent person costs more).
- **Patronage**: buy the current individual with gold or faith at cost proportional to the missing points (gold = 3× missing points, faith = 2× missing points (tunable)); others' progress unaffected.
- **Pass**: decline the current individual at no cost; points are kept, the individual stays available to everyone (including this player later), and this player is not prompted again for that individual.
- Usage: each individual has: charges, an "Activate" ability valid on a target tile/district type, or Great Works creation, or passive aura (Generals/Admirals) + retire ability. Examples:
  - Great Scientist: Euclid (boost Mathematics + 1 Medieval tech), Isaac Newton (+2S to universities), Albert Einstein (triggers Eurekas for two random late-era techs (tunable)), Hypatia (+1S to all Libraries; builds a Library in the Campus).
  - Great Engineer: Imhotep (Wonder production +x), Leonardo da Vinci (+... ), James Watt (+industrial)... Typically "add 175+ production to a wonder" or build canal/dams.
  - Great Merchant: Marco Polo (+1 trade route), Colaeus (+1 faith), Zhang Qian, Adam Smith (Monopolies), John Rockefeller (oil), artificial luxuries (Toys, Perfume, Cosmetics, Jeans) creators.
  - Great Prophet: found religion (see 06).
  - Great Writer/Artist/Musician: create Great Works in slot (Writer 2 books, Artist 3 paintings/sculptures, Musician 2 compositions). Great Musicians do not perform concerts; concert tourism bursts are the Rock Band's role.
  - Great General/Admiral: aura +5 CS +1 MP within 2 tiles; retire bonus.

## Great Works and Culture
- Types: Writing (books), Art (Portrait, Landscape, Religious, Sculpture), Music, Relics, Artifacts.
- Slots (Libraries have none): Amphitheater 2 writing, Art Museum 3 art, Archaeological Museum 3 artifacts, Broadcast Center 1 music, Temple 1 relic, Cathedral 1 religious art, Palace 1 any, wonders with slots (Great Library 2 writing, Hermitage 4 art, Sydney Opera House 3 music).
- Yields per work: Writing +2C, Art +3C, Music +4C, Relic +4Fa, Artifact +3C. Great Works produce tourism from the start (no tech needed): tourism per work equals its culture value (Writing 2, Art 3, Music 4, Artifact 3); Relics give 8 Tourism.
- **Theming bonuses**: museums filled with 3 works fitting the rule (e.g., same era, different civ, same type) double yields (theming = +100% culture/tourism for that building's works).
- **Archaeology**: after Natural History, Antiquity Sites and Shipwrecks [R&F] appear on tiles where battles happened/ancient ruins; Archaeologist (civilian) excavates → Artifact (into museum slot) or create a **Landmark** improvement (tourism). Excavating in foreign territory requires open borders.
- **National Parks**: Naturalist (Conservation civic; faith-purchased) creates on 4-tile diamond shape with Charming+ appeal, owned, no districts/improvements; yields tourism = sum of appeal.
- **Seaside Resorts, Ski Resorts**: tourism = appeal.
- **Rock Bands** [GS]: faith-purchased; perform concert in foreign city → tourism burst; promotions; can disband on poor roll.

## Tourism and Culture Victory
- **Domestic tourists** (per player) = lifetime culture generated / 100.
- **Visiting tourists** to you from civ X accumulate: each turn your tourism toward X (modified) is added to a running total; every 150 tourism points = 1 visiting tourist from X (tunable): `visiting_from_X = floor(accumulated_tourism_toward_X / 150)`. Your total visiting tourists = sum over all other civs.
- Tourism modifiers toward X: +25% open borders with X, +25% trade route to X, policies (Online Communities +50% toward civs you have a route to), wonders and civ abilities.
- **Culture victory**: you win when your total visiting tourists (summed over all other civs) exceed the domestic tourists of **every** other major civ, i.e. `visiting_total > max(domestic of each other civ)`.
- Tourism sources: Great Works, wonders (+2 each base (tunable)), relics, artifacts, religious tourism, improvements (after Flight: tiles yield tourism = culture), national parks, resorts, rock bands, city-state bonuses, Cristo Redentor, policies.

## Monopolies & Corporations (optional game mode)
After Economics, Industries are created on improved luxury tiles (by Great Merchants or a gold-cost action (tunable)); Industries upgrade to Corporations, which create Products (Great Works) for tourism. Keep optional.

## Score
Running score: civics (+3 each (tunable)), techs (+2 each (tunable)), wonders, great people, era score, cities/population, religion followers, future techs. Score victory at turn limit uses this.
