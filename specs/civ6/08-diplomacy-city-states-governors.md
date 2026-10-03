# 08 Diplomacy, City-States, Governors, Espionage, World Congress

## Meeting and relationship states
- Players meet when units/cities see each other. Before meeting: no interaction.
- Relationship states: Neutral, Friendly, Unfriendly, Declared Friend, Denounced, Allied, At War. Plus an **opinion score** (sum of modifiers with reasons and decay) that drives AI behavior; display as emoji state.
- **Access level** (how much you see about another civ: agendas, gossip, cities): None → Limited (met) → Open (delegation/embassy) → Secret (alliance/declared friend + trade routes) → Top Secret (spies with listening post / alliance level). Gossip messages about their activity are revealed according to access.

## Diplomatic actions (data each with prereq, cost, duration, AI acceptance)
| Action | Prereq | Effect |
|---|---|---|
| Send Delegation | met | 25G; +opinion; small access gain; refused if AI dislikes you |
| Resident Embassy | Writing [R&F] | access +1 level (Diplomatic Visibility); no tourism effect |
| Open Borders | Early Empire | units can enter their territory; 30 turns; +tourism 25% |
| Declaration of Friendship | met + delegation; AI must be Friendly (no civic needed) | 30 turns; can't declare war on each other without penalty; no denounce |
| Denounce | met | 5-turn wait to formal war; opinion penalties; lasts until revoked/time |
| Alliance [R&F] | Civil Service; requires friendship | Types: Research, Military, Economic, Cultural, Religious. Alliance points accumulate → levels 1–3 with escalating type bonuses; shared visibility; shared wars (call to arms) |
| Joint War | friend/ally | coordinated war declaration |
| Trade Deal | met | see 07 |
| Make Demand | met | AI may yield items if weaker |
| Declare War | see war types | — |
| Make Peace | ≥10 turns at war | peace treaty with items (cities, gold, resources) |
| Ask Promise | met | AI/you request: stop settling near, stop converting, stop spying, move troops away, stop excavating; breaking promises causes grievances |

## War
- **War types** (R&F/GS): Surprise War (declared without 5 turns of denouncement: huge warmonger/grievance penalty), Formal War (after denouncement ≥5 turns), and **Casus Belli** (reduced penalties, conditions): Holy War (they converted your cities), Liberation War (they captured a city of your friend/ally), Reconquest War (they hold your former city), Protectorate War (they attacked a city-state you are suzerain of), Colonial War (they are ≥1 era behind you), Territorial Expansion War (unlocked by Mobilization; target owns at least 2 cities bordering yours (tunable)), Ideological War (different governments tier 3), Golden Age War (during your golden age dedication).
- **Warmonger/Grievances** [GS replaces warmonger]: Grievances against a civ accrue from war declarations, capturing/razing cities, nukes, broken promises (including converting cities after promising not to), espionage caught, and decay per turn. Grievances held by a civ: affect AI opinion, war weariness (fighting a civ you have grievances against reduces your war weariness), justify casus belli, and cost Diplomatic Favor: each turn a civ loses favor in proportion to the grievances others hold against it.
- **War weariness**: accumulates from combat (per combat in your/their territory, more in foreign), unit losses, nukes; decays at peace. Effect: amenities penalty in all cities (−1 per X weariness). Reduced by Grievances and policies (e.g., Propaganda).
- Peace is forced on AI after 10 turns minimum; peace deals include ceding cities. City-states at war persist.
- Occupied cities: −amenities until peace grants them.

## Agendas (AI personality)
- Each leader has a **historical agenda** (visible as soon as you meet the leader): conditions it likes/dislikes in others (e.g., Trajan "Optimus Princeps" likes large empires; Montezuma "Tlatoani" likes civs that have the same luxuries he has, dislikes those with luxuries he lacks; Gandhi "Peacekeeper" likes peaceful civs and hates warmongers; Cleopatra "Queen of the Nile" likes strong militaries).
- One or more **hidden agendas** chosen randomly at start from a pool (e.g., Airpower, Backstabber, Cultured, Darwinist, Devout, Enviromentalist, Exploitationist, Industrialist, Nuke Happy, Pillager, Standing Army, Wonder Obsessed, Ironclad, Money Grubber, Paranoid, Sycophant, Tourist...). Revealed through access/gossip.
- Agendas produce opinion modifiers each turn (+/−), shaping AI deal acceptance and war decisions.

## City-States
- Minor civilizations (one city, cannot expand). Each has a type and a unique suzerain bonus. Types and yields per envoy tier:
  - Scientific (+Science), Cultural (+Culture), Religious (+Faith), Trade (+Gold), Industrial (+Production), Militaristic (+Production toward units).
- **Envoys**: players earn envoys from civics (e.g., Political Philosophy +1, etc.), Influence Points (Charismatic Leader +; accumulates each turn; threshold rising → +1 envoy), quests, policies, Diplomatic Quarter, Amani. Send envoy to a met city-state (permanent).
- **Envoy tier bonuses** (to the player):
  - 1 envoy: +2 of type yield in capital (Industrial: +2P in capital; Trade +4G).
  - 3 envoys: +2 in every city with the matching district (Campus/Theater/Holy Site/Commercial Hub/Industrial Zone/Encampment) or building.
  - 6 envoys: +2 more in those cities.
  - **Suzerain**: player with most envoys (≥3, strictly more than any other) gets the unique bonus (e.g., Geneva: +15% science when not at war; Kabul: double XP; Hattusa: +1 of each revealed strategic resource you have no source of; Antananarivo +2% culture per GP; Jerusalem...), the city-state's luxury/strategic resources, and can Levy Military (pay gold to control its units for 30 turns). City-state joins suzerain's wars; can't be attacked by suzerain's allies without penalty.
- **Quests**: city-states periodically issue quests (train unit X, build wonder/district, send trade route, clear barbarian camp, convert to religion, recruit great person, get boost) → reward +1 envoy.
- City-state conquest: Protectorate War CB for suzerain; capturing it removes it. City-states can be bullied/levied.
- City-state AI: defends itself, has units, spawns based on era.
- **Unique improvements** granted via suzerain (e.g., Colossal Head (La Venta), Moai (Rapa Nui), Alcázar (Granada), Kampung (Bandar Brunei), Nazca Line (Nazca), Mahavihara (Anuradhapura), Batey (Yaxchilan)). Keep as data.

## Governors [R&F]
- **Governor titles** earned from specific civics (State Workforce, Early Empire, Defensive Tactics, Recorded History, Mercenaries, Medieval Faires, Guilds, Civil Engineering, Nationalism, Mass Media, Mobilization, Globalization, Social Media — 13 total (tunable list)) and some civ abilities. A title either appoints a new governor or promotes an existing one.
- Governors (base ability + promotion tree of 5–6):
  - **Victor (Castellan)**: city defense, +strength; Embrasure (ranged strike twice), Garrison Commander (loyalty), Air Defense, Arms Race Proponent (nuke build speed).
  - **Amani (Diplomat)**: can be placed in city-states (+2 envoys), +loyalty pressure, Puppeteer, Foreign Investor.
  - **Magnus (Steward)**: Groundbreaker (+50% harvest yields), Surplus Logistics, Provision (settlers don't consume pop), Industrialist, Black Marketeer, Vertical Integration.
  - **Liang (Surveyor)**: Guildmaster (+1 builder charge), Zoning Commissioner, Aquaculture (fish farm), Reinforced Materials (disaster immunity), Water Works, Parks and Recreation (City Park).
  - **Pingala (Educator)**: Librarian (+15% science and culture), Connoisseur, Researcher, Grants (+100% GPP), Space Initiative, Curator.
  - **Reyna (Financier)**: Land Acquisition (tile growth, +3G from foreign trade routes), Harbormaster, Forestry Management, Contractor (buy districts with gold), Taxman, Renewable Subsidizer [GS].
  - **Moksha (Cardinal)**: Bishop (+2 religious pressure; +), Grand Inquisitor, Laying on of Hands, Citadel of God (faith purchase), Patron Saint, Divine Architect.
  - [GS, Ottoman pack]: Ibrahim (Grand Vizier), an 8th governor available to every player once the Ottomans are in the game data.
- Assigning to a city: governor takes ~5 turns to establish (abilities inactive until established); moving resets. Established governor: +8 loyalty/turn; Amani in city-states counts as 2 envoys.
- Spies can neutralize governors (inactive for some turns).

## Espionage
- Spies unlocked by Diplomatic Service; capacity +1 per related civics (Diplomatic Service +1, Nationalism +1, Cold War +1 (tunable)) and the Diplomatic Quarter district [GS] (+1).
- Spies travel to a city (takes turns), then perform missions in a district:
  - Gain Sources (one-shot intel; access level up), Listening Post (any district; passively reveals gossip from that city), Counterspy (defend district in your city), Siphon Funds (Commercial Hub, steal gold), Steal Tech Boost (Campus), Sabotage Production (Industrial Zone), Great Work Heist (Theater Square), Recruit Partisans (Neighborhood; spawns rebels), Neutralize Governor, Disrupt Rocketry (Spaceport), Foment Unrest (City Center; lowers loyalty), Fabricate Scandal (lower envoys of target at city-states), Breach Dam [GS].
- Success: probability table by spy level (Recruit, Agent, Special Agent, Senior Agent, Master Spy) and mission difficulty, defended by counterspies/Chancery. Outcomes: success (escape), success caught (captured), failure escape, failure killed/captured. Captured spies can be traded back.
- Spies gain promotions (Ace Driver, Cat Burglar, Demolitions, Disguise, Guerrilla Leader, Linguist, Quartermaster, Rocket Scientist, Satchel Charges, Seduction, Smear Campaign, Surveillance, Technologist).

## Diplomatic Favor and World Congress [GS]
- **Diplomatic Favor**: currency earned per turn from government (+1/+3 by tier), alliances (+1 each), suzerainties (+1 each), policy cards, Diplomatic Quarter buildings, Statue of Liberty, completing emergencies/competitions; minus per grievances held against you. Also gained/spent in trades.
- **World Congress**: first convenes once the first civ reaches the Medieval era; then every 30 turns (scaled by speed). Each session votes on **2 resolutions** drawn from a pool, each with options A/B and a target (player, district, resource, unit class, policy type, etc.). Examples: Mercenary Companies (unit prod ±), Urban Development Treaty, Diplomatic Victory point allotment, Luxury Licensing, Heritage Organization, Border Control, Military Advisory, Arms Control, Public Works, Deforestation Treaty, Global Energy Treaty, Patronage. Each player gets 1 free vote and can buy more votes with favor (cost escalating: 10, 30, 60, 100, 150 ...).
- **Special sessions** for emergencies and aid requests (after disasters): contribute to win diplomatic victory points.
- **Scored Competitions**: World's Fair, World Games, Climate Accords, Aid Request; winners get points.
- **Diplomatic Victory**: first to **20 Diplomatic Victory Points** (sources: the Diplomatic Victory resolution (+2 to the winner of the vote), emergencies/competitions/aid requests (+1 to +2), Mahabodhi Temple +2, Statue of Liberty +4, Potala Palace +1, late-game tech/civic milestones (e.g., Seasteads +1, tunable)). Points can also be lost via resolutions.

## Emergencies [R&F]
Triggered when a civ: captures a city-state or a capital (Military Emergency), converts a Holy City (Religious Emergency), uses a nuclear weapon (Nuclear Emergency), or becomes a runaway threat (e.g., GS Climate). Eligible civs choose to join; the target civ receives the opposite goal. Duration fixed; success gives rewards (era score, envoys, favor), failure gives the target rewards.
