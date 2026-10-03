# 09 Civilizations, Leaders, Ages, Victory, Climate

## Civilization and leader structure
Each **Civilization** has: name, city name list, start bias, **civ ability** (unique), **unique unit** (1, sometimes 2), **unique infrastructure** (district, building, or improvement), color palette.
Each **Leader** belongs to one civ (some civs have several leaders, "personas"), has: **leader ability**, sometimes a leader-specific unique unit, **historical agenda**, AI personality weights (grand strategy preferences), voice/3D model.

Data schema:
```
Civilization { id, name, adjectives, cities[], start_bias[{terrain|feature|resource|river|coast, tier}],
               ability_modifiers[], unique_units[], unique_infrastructure[] }
Leader { id, civ, name, ability_modifiers[], unique_units[], agenda, ai_weights{science, culture, religion,
         military, expansion, wonders, diplomacy, ...}, favored_civics/techs, favored_victory }
```

### Examples (base game roster plus expansions; ~50 civs, ~77 leaders total)
| Civ | Leader | Leader ability (summary) | Civ ability (summary) | Uniques |
|---|---|---|---|---|
| America | Teddy Roosevelt | Roosevelt Corollary: +5 CS on home continent; +1 Appeal in cities with a National Park | Founding Fathers: all Diplomatic policy slots in the current government become Wildcard slots | P-51 Mustang, Film Studio, Rough Rider (Teddy) |
| Arabia | Saladin | Worship building cheaper, +10% science/culture/faith in cities with it | The Last Prophet: guaranteed last Great Prophet | Mamluk, Madrasa |
| Aztec | Montezuma | Luxuries give +1 amenity to extra city; +1 CS per luxury type | Legend of the Five Suns: spend builder charges to complete 20% of districts | Eagle Warrior, Tlachtli |
| Brazil | Pedro II | Recruiting a GP refunds 20% of its cost | Amazon: rainforest gives +1 adjacency to districts | Minas Geraes, Street Carnival |
| China | Qin Shi Huang | First Emperor: Builders +1 charge; spend a charge for 15% of Ancient/Classical wonders | Dynastic Cycle: Eurekas/Inspirations give 50% | Crouching Tiger, Great Wall |
| Egypt | Cleopatra | Mediterranean's Bride: +4 Gold on Egypt's international routes; foreign routes to Egypt give their owner +2 Food and Egypt +2 Gold | Iteru: +15% district/wonder production next to river; floodplains don't block | Maryannu Chariot Archer, Sphinx |
| England | Victoria | Pax Britannica: free melee unit in each city founded on a foreign continent; Redcoat (leader unit) | Workshop of the World: iron/coal mines +2 resource/turn, +100% strategic stockpile, Military Engineers +2 charges, powered buildings +4 yields | Sea Dog, Royal Navy Dockyard, Redcoat (Victoria) |
| France | Catherine de Medici | Flying Squadron: +1 level diplomatic visibility with every met civ; free Spy with Castles | Grand Tour: +20% Medieval–Industrial wonder production; double tourism from wonders | Garde Impériale, Château |
| Germany | Frederick Barbarossa | +7 CS vs city-states; +1 military slot | Free Imperial Cities: +1 district limit | U-Boat, Hansa |
| Greece | Pericles / Gorgo | +5% culture per suzerainty / culture on kills | Plato's Republic: +1 wildcard slot | Hoplite, Acropolis |
| India | Gandhi | Satyagraha: +5 faith per met civ that founded a religion and is at peace; enemies get double war weariness | Dharma: get follower beliefs of all religions present | Varu, Stepwell |
| Japan | Hojo Tokimune | Divine Wind: land units +5 CS on coast-adjacent tiles, naval units +5 CS in shallow water; Holy Sites/Theaters/Encampments built in half the time | Meiji Restoration: +1 adjacency per district | Samurai, Electronics Factory |
| Kongo | Mvemba a Nzinga | Cannot found religion; gets all religions' follower beliefs; apostles from Mbanza | Nkisi: relics/artifacts/sculptures yield extra food, prod, gold | Ngao Mbeba, Mbanza |
| Norway | Harald Hardrada | Thunderbolt of the North: naval melee units can coastal raid; +50% production toward naval melee units | Knarr: units can enter Ocean after Shipbuilding; no movement cost to embark/disembark | Berserker, Stave Church, Viking Longship (Harald) |
| Rome | Trajan | Free Monument in every city | All Roads Lead to Rome: free road to capital; trading posts | Legion, Bath |
| Russia | Peter | Trade routes give +S/C based on tech gaps | Mother Russia: +5 extra tiles on founding; tundra yields | Cossack, Lavra |
| Scythia | Tomyris | +5 CS vs wounded; heal on kill | People of the Steppe: double light cavalry | Saka Horse Archer, Kurgan |
| Spain | Philip II | +4 CS vs other religions | Treasure Fleet: fleets, intercontinental routes | Conquistador, Mission |
| Sumeria | Gilgamesh | Adventures of Enkidu: share pillage rewards and combat XP with allies fighting nearby | Epic Quest: clearing a barbarian outpost also grants a tribal village reward | War-Cart, Ziggurat |
| [R&F] Mongolia | Genghis Khan | Mongol Horde: cavalry-class units +3 CS and may capture defeated enemy cavalry | Örtoo: trade routes instantly create a Trading Post; +1 diplomatic visibility where you have one; +3 CS per visibility level advantage | Keshig, Ordu |
| [R&F] Netherlands | Wilhelmina | Radio Oranje: domestic routes give +1 Loyalty/turn to the origin; international routes give +1 Culture | Grote Rivieren: river +2 adjacency to Campus/Theater/Industrial Zone; Harbors culture-bomb; +50% Dam/Flood Barrier production | De Zeven Provinciën, Polder |
| [R&F] Korea | Seondeok | Hwarang: cities with an established governor +3% Culture and Science per governor promotion | Three Kingdoms: Mines +1 Science and Farms +1 Food when adjacent to a Seowon | Hwacha, Seowon |
| [R&F] Cree | Poundmaker | Favorable Terms: trade routes +1 Food to the origin and +1 Gold per Camp/Pasture at the destination; shared visibility with trade partners | Nîhithaw: free Trader and +1 route capacity with Pottery; military units claim unowned tiles within 3 of a Cree city | Okihtcitaw, Mekewap |
| [R&F] Georgia | Tamar | Glory of the World, Kingdom and Faith: envoys to city-states of your religion count double; +100% Faith 10 turns after a Protectorate War | Strength in Unity: Golden Age dedications also grant their normal-age bonus | Khevsur, Tsikhe |
| [R&F] Scotland | Robert the Bruce | Bannockburn: War of Liberation from Defensive Tactics; +100% Production and +2 Movement for 10 turns after declaring it | Scottish Enlightenment: Happy cities +5% Science/Production and +1 Great Scientist/Engineer points (doubled when Ecstatic) | Highlander, Golf Course |
| [R&F] Zulu | Shaka | Amabutho: Corps with Mercenaries, Armies with Nationalism; Corps/Armies +5 CS | Isibongo: garrisoned units add Loyalty; capturing a city upgrades the unit to Corps/Army | Impi, Ikanda |
| [R&F] Mapuche | Lautaro | Swift Hawk: defeating a unit inside an enemy city's borders costs that city 20 Loyalty | Toqui: +10 CS vs civs in a Golden Age; governor cities get bonus yields and faster unit XP | Malón Raider, Chemamull |
| [R&F] India (alt.) | Chandragupta | Arthashastra: War of Territorial Expansion from Military Training; +2 Movement and +5 CS for 10 turns after declaring it | Dharma (as India) | Varu, Stepwell |
| [GS] Canada | Wilfrid Laurier | The Last Best West: Farms on Tundra; 50% cheaper tile purchase on Tundra/Snow; extra strategic resources on Tundra/Snow | Four Faces of Peace: no Surprise Wars by or against Canada; Diplomatic Favor from Tourism and doubled from emergencies/competitions | Mountie, Ice Hockey Rink |
| [GS] Hungary | Matthias Corvinus | Raven King: levied city-state units +2 Movement, +5 CS and cheap upgrades; +2 Envoys per levy | Pearl of the Danube: +50% production of districts/buildings across a river from the City Center | Black Army (Matthias), Huszár, Thermal Bath |
| [GS] Inca | Pachacuti | Qhapaq Ñan: domestic routes +1 Food per Mountain in the origin; Qhapaq Ñan mountain tunnels | Mit'a: citizens can work Mountain tiles (Production) | Warak'aq, Terrace Farm |
| [GS] Mali | Mansa Musa | Sahel Merchants: international routes +1 Gold per flat Desert tile in the origin; extra route capacity in Golden Ages | Songs of the Jeli: City Center +1 Faith/+1 Food per adjacent Desert; Mines −1 Production +4 Gold; −30% Production for units/buildings; buy Commercial Hub buildings with Faith | Mandekalu Cavalry, Suguba |
| [GS] Māori | Kupe | Kupe's Voyage: starts at sea; first city gets a free Builder and extra population; Science/Culture per turn before founding | Mana: starts with Sailing and Shipbuilding; embarked units +2 Movement; unimproved Woods/Rainforest +1 Production; cannot harvest | Toa, Marae |
| [GS] Ottoman | Suleiman | Grand Vizier: unique governor Ibrahim; Janissary leader unit | Great Turkish Bombard: +50% siege unit production, siege +5 CS vs districts; conquered cities keep population and gain Amenity/Loyalty | Barbary Corsair, Grand Bazaar, Janissary (Suleiman) |
| [GS] Phoenicia | Dido | Founder of Carthage: can move the Capital to a city with a Cothon via a project | Mediterranean Colonies: Writing eureka; coastal cities founded on the capital's continent are always fully loyal | Bireme, Cothon |
| [GS] Sweden | Kristina | Minerva of the North: buildings with 3+ Great Work slots and wonders with slots auto-theme | Nobel Prize: +50 Diplomatic Favor per Great Person; Great Engineer/Scientist points from Factories/Universities; Nobel competitions | Carolean, Open-Air Museum, Queen's Bibliotheque (Kristina) |
| [GS] England / France (alt.) | Eleanor of Aquitaine | Court of Love: foreign cities within 9 tiles lose 1 Loyalty per Great Work in her nearby cities; cities that flip join her | civ ability of the chosen civ | civ uniques |
| [DLC] Poland | Jadwiga | Lithuanian Union: culture bombs convert the city to your religion; Relics give extra Faith/Culture/Gold | Golden Liberty: Encampments/Forts culture-bomb; one Military slot becomes Wildcard | Winged Hussar, Sukiennice |
| [DLC] Australia | John Curtin | Citadel of Civilization: +100% Production for 10 turns after being declared on or liberating a city | Land Down Under: +3 Housing in coastal cities; districts gain adjacency from Charming/Breathtaking appeal; Pastures culture-bomb | Digger, Outback Station |
| [DLC] Persia | Cyrus | Fall of Babylon: +2 Movement for 10 turns after a Surprise War; reduced Surprise War penalties | Satrapies: +1 trade route capacity with Political Philosophy; domestic routes +2 Gold +1 Culture | Immortal, Pairidaeza |
| [DLC] Macedon | Alexander | To the World's End: no war weariness; all units heal when capturing a city with a wonder | Hellenistic Fusion: Eurekas/Inspirations from conquered cities' districts | Hypaspist, Hetairoi (Alexander), Basilikoi Paides |
| [DLC] Nubia | Amanitore | Kandake of Meroë: +20% district production (+40% next to a Nubian Pyramid) | Ta-Seti: +50% ranged unit production and XP; extra yields on Mines over strategic resources | Pítati Archer, Nubian Pyramid |
| [DLC] Khmer | Jayavarman VII | Monasteries of the King: Holy Sites +2 Food, +1 Housing next to rivers; Holy Sites culture-bomb | Grand Barays: Aqueducts +1 Amenity and Faith per population; Farms next to Aqueducts +2 Food | Domrey, Prasat |
| [DLC] Indonesia | Gitarja | Exalted Goddess of the Three Worlds: buy naval units with Faith; coastal City Centers +2 Faith | Great Nusantara: coast/lake adjacency for districts; Entertainment Complexes next to coast give extra Amenity | Jong, Kampung |
| [DLC] Vietnam | Bà Triệu | Drive Out the Aggressors: +5 CS and +1 Movement when fighting/starting in features in your territory | Nine Dragon River Delta: specialty districts only on features; buildings on Rainforest/Marsh/Woods give +1 Science/Production/Culture | Voi Chiến, Thành |
| [DLC] Gaul | Ambiorix | King of the Eburones: +2 CS per adjacent combat unit; Culture when training melee/ranged/anti-cavalry units | Hallstatt Culture: Mines culture-bomb and give Culture; specialty districts cannot be adjacent to the City Center | Gaesatae, Oppidum |
| [DLC] Byzantium | Basil II | Porphyrogénnētos: heavy/light cavalry deal full damage to cities; Tagma leader unit | Taxis: +3 combat and religious strength per Holy City converted; kills spread your religion | Dromon, Hippodrome, Tagma (Basil) |
| [DLC] Babylon | Hammurabi | Ninu Ilu Sirum: first of each specialty district gives its cheapest building | Enuma Anu Enlil: Eurekas grant the full tech; −50% Science per turn | Sabum Kibittum, Palgum |
| [DLC] Ethiopia | Menelik II | Council of Ministers: hill cities convert 15% of Faith into Science and Culture; +4 CS on Hills | Aksumite Legacy: international routes give Faith per resource at origin; improved resources +1 Faith; Archaeologists/Museums buyable with Faith | Oromo Cavalry, Rock-Hewn Church |
| [DLC] Gran Colombia | Simón Bolívar | Campaña Admirable: a Comandante General is earned with each new era | Ejército Patriota: all units +1 Movement; promoting does not end the turn | Llanero, Hacienda |
| [DLC] Maya | Lady Six Sky | Ix Mutal Ajaw: non-capital cities within 6 tiles of the capital +10% yields; farther cities −15% | Mayab: no Housing from fresh water; Farms +1 Housing and +1 Gold next to Observatories; Amenity from luxuries next to City Center | Hul'che, Observatory |
| [DLC] Portugal | João III | Porta do Cerco: +1 trade route capacity; open borders with all city-states | Casa da Índia: international routes only to coastal cities but +50% yields; Traders +50% water range | Nau, Navigation School |

Leader ability text is a guide; all effects are modifiers in data. A clone should ship ~8–12 well-differentiated civs for MVP.

## Era score and Ages [R&F]
- **Historic Moments** add era score and are logged on a **Timeline** (with illustrations). Examples (score): first to found a religion (+3 (tunable)), circumnavigation (+3), first wonder built (+3; any wonder +1–4 by "first in world"), first to unlock a civic/tech of a new era (+1), meet a new civ (+1), meeting each new city-state (+1), clear barbarian camp (+1/+2), kill a unit with a GG nearby (+1), build a specialty district first time per type (+1), city founded on new continent (+2), unit reaching its final promotion level (+1), natural wonder discovered (+1/+2 first), Great Person recruited (+1), become suzerain first time (+1), trade route to new civ (+1), city reaches 10/15/20/25 pop, etc. Data table with "first in world" vs "first for player" variants and era decay.
- **Thresholds** per player, per era: `dark_threshold` and `golden_threshold` (golden = dark + gap). Base thresholds rise with each era and slightly with number of cities: `dark = base_dark[era] + cities_mod`, `golden = dark + 12` (tunable).
- When the global era changes, each player's age for the new era is set:
  - Score < dark threshold → **Dark Age**: −5 Loyalty per turn in every city (tunable), can slot one Dark Age policy (powerful trade-offs: e.g., Isolationism, Robber Barons, Inquisition, Twilight Valor, Letters of Marque, Monasticism, Collectivism, Elite Forces, Disinformation Campaign), easier golden next era.
  - Between → **Normal Age**.
  - ≥ golden threshold → **Golden Age**: +5 Loyalty per turn in every city (tunable), choose a **Dedication** with stronger golden bonus.
  - Golden after Dark → **Heroic Age**: choose 3 dedications.
- **Dedications** (commemorations) chosen at each new era (1 normally, 3 for heroic): each gives era score bonus for certain actions during the era (normal/dark) and a separate bonus when in a Golden age. Examples: Monumentality (+1 era score per specialty district; golden: −30% builder/settler gold/faith cost, +2 movement builders), Free Inquiry (era score on Eurekas; golden: Commercial hub/harbor adjacency also gives science), Exodus of the Evangelists, Pen Brush and Voice, Heartbeat of Steam, Hic Sunt Dracones, To Arms!, Wish You Were Here, Reform the Coinage, Sky and Stars, Automaton Warfare, Bodyguard of Lies.
- **Global era transitions** also re-evaluate grievances, emergencies, World Congress cadence, barbarian tech.

## Victory conditions (each can be toggled at setup)
1. **Science**: complete space projects in Spaceports:
   - [GS] Launch Earth Satellite → Launch Moon Landing → Launch Mars Colony → **Exoplanet Expedition** project creates an expedition that must travel **50 light-years** (base speed 1 ly/turn); Terrestrial Laser Station and Lagrange Laser Station projects (repeatable, high Production cost, tunable) each add +1 ly/turn (tunable); enemies can capture/destroy the Spaceport. Win when expedition arrives.
   - Base: Earth Satellite, Moon Landing, 3 Mars Colony modules (Reactor, Habitation, Hydroponics) → win.
2. **Culture**: tourism condition (see 07).
3. **Domination**: own every other major civ's **original capital** (and keep your own). City-states don't count. Eliminated civs' capitals count when held.
4. **Religious**: your religion is majority in every major civ (see 06).
5. **Diplomatic** [GS]: 20 diplomatic victory points (see 08).
6. **Score**: highest score when the turn limit is reached (Standard: turn 500).
- Players are **eliminated** when they lose all cities (surviving units, including Settlers, do not keep them alive). Last player standing also wins (domination edge case).

## Climate and Disasters [GS]
- **Global CO2 / Climate Change**: each player emits CO2 from: burning Coal and Oil (Uranium emits no CO2 but carries nuclear-accident risk) in power plants, unit maintenance of fossil-fuel units, consumption of strategic resources; deforestation adds. Global total crosses thresholds → **Climate Phases 1–7**.
- Effects per phase: increased frequency/intensity of storms, droughts, floods; polar ice melts; **sea level rise** submerges Coastal Lowlands (tiles flagged 1m, 2m, 3m elevation band at map gen) starting from phase ~3; submerged tiles are permanently lost (cities can protect with **Flood Barrier** project, very costly). Phase 7 extreme.
- Mitigation: renewable power (Solar, Wind, Geothermal, Hydro), Carbon Recapture project, World Congress treaties, Global Warming Mitigation civic.
- **Natural disasters** (random per turn, frequency set by "disaster intensity" option and climate phase):
  - **Volcanic eruption** (active volcanoes): damage adjacent units/improvements/population, then **volcanic soil** (+1F, +1P, sometimes +1S (tunable)) on affected tiles.
  - **River flood** (floodplains): +1F (+1P) permanent to affected tiles; pillages improvements; damages units/pop. Dams prevent.
  - **Drought**: −food to tiles in an area for some turns.
  - **Storms** (moving entities for several turns): Blizzard (tundra/snow), Dust storm/Haboob (desert), Tornado (plains/grass), Hurricane/Cyclone (coast/ocean, Gale). Damage units, pillage, sometimes fertile soil.
  - **Forest fires** (later patch; woods/rainforest burns, then fertility).
  - **Nuclear accident** at aging Nuclear Power Plants (fallout).
- Disasters trigger World Congress aid-request competitions (rewards: Diplomatic Victory Points, era score, favor).

## Power [GS]
- Industrial-era+ buildings require Power (Factory 1, Stock Exchange 3, Research Lab 3, Stadium 3, Broadcast Center 3, Shopping Mall 3, Food Market 3, Seaport 3, Airport 3 (tunable)). Power produced by Coal (4 per coal), Oil (4 per oil), Nuclear (16 per uranium) power plants (regional 6 tiles), Hydroelectric Dam (6), Solar/Wind/Geothermal (2–4 each, local), city-state bonuses. Unpowered buildings give base effects only; powered give full (+% bonuses).
- Player chooses per power plant which resource to burn (Coal Power Plant burns coal; Oil power plant oil; Nuclear uranium).
