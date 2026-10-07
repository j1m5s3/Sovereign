# Sovereign: art style and leader roster

Status: art style, roster and leader abilities decided by James (2026-10-04); ability numbers to be tuned in playtesting. Companion to [leader-character-brainstorm.md](leader-character-brainstorm.md) and [world-scale-and-generation.md](world-scale-and-generation.md). All numbers are starting points for playtesting.

## Decisions

- **[decided, James 2026-10-04] Art style: stylized realism.** Realistic proportions, hand-painted and simplified textures (references: Dishonored, Arcane, Humankind's units). Not Civ VI's caricature look, which clashes with assassinations, live battles and street-level walking; not photorealism, which ages fast and multiplies the cost of the ~30 building kits. At the furthest zoom the map keeps a Civ-style parchment layer for fog of war and the strategic view.
- **[decided, James 2026-10-04] Historical leaders only.** No custom or generated leaders and no character creator: they risk balance problems and the creator would cost more than it adds.
- **[decided, James 2026-10-04] Launch roster: 12 civs**, two per architectural style, so every building kit is used.
- **[decided, James 2026-10-04] Roster and leader abilities as listed below.** Leaders who died recently or are living are out (likeness rights, assassination mechanics, politics); 19th/20th-century figures dead 50+ years remain possible for later civs, with no dictators.

## Art toolchain (recommended)

**Pipeline built 2026-10-05:** `tools/art/` (Blender scripts → FBX → scripted Unreal import), first slice: Nature, temperate Classical City Center and Figures kits; see `tools/art/README.md`.

**Who makes the art **[decided, James 2026-10-04]**:** Claude makes the art (models, kit pieces, textures) through scripted tools such as Blender's Python API, modelling from reference images gathered online. References are used only to study shapes, proportions and period detail; no downloaded image, texture or model ships in the game unless its license allows it.


| Tool | Use |
|---|---|
| Blender (free) | Modeling, kit pieces, hero models, rigging, animation. Geometry Nodes for kit-piece variants. |
| Substance 3D Painter (subscription) | Hand-painted, simplified textures and shared trim sheets. ArmorPaint is a free fallback. |
| Unreal Engine 5 | Runtime world generator (code + PCG), streaming, rendering. Free until $1M revenue, then 5% royalty. |
| Optional later | Rokoko suit or Move.ai video mocap for battle animation; SpeedTree for vegetation. |

Skip Houdini (its procedural tools run in the editor, not in the shipped game, and our generator must run during play) and MetaHuman (photoreal, clashes with the style).

**Scale:** Blender only makes pieces; the engine assembles the world from game state at runtime, so nobody authors the ~84 × 54 km world. The art workload is ~30 kits of a few hundred pieces each, plus hero models. The style helps performance: shared texture sheets and simpler materials mean less memory per hex.

## How a leader is built

Civ comes first: each civ keeps its own civ ability and unique units/buildings (to design separately). The leader adds:

1. **Leader ability:** an empire-wide bonus in Civ terms (yields, production, combat strength, loyalty). Belongs to the throne: every successor keeps it and adds a small personal trait (**[decided, James 2026-10-04, gap review]**).
2. **Promotion leaning:** the leader starts with the first promotion of one `SOVEREIGN` branch (Warlord, Statesman or Builder-King, see leader doc section 3). The player can still finish any one branch.
3. **AI agenda:** what the AI version of this leader likes and dislikes, feeding diplomacy and the nemesis memory (player-retention doc).

Abilities are Sovereign's own designs. Real historical people are free to use; Civ VI's portrayals and ability texts are not, so models, voices and wording are ours.

Excluded on principle: 20th-century dictators and religious founders.

## Launch roster [decided, James 2026-10-04]

| Style | Civ | Leader | Reign | Leaning | Plays toward |
|---|---|---|---|---|---|
| European | England | Elizabeth I | 1558–1603 | Statesman | Naval, trade |
| European | France (Franks) | Charlemagne | 768–814 | Warlord | Religion, science, cavalry |
| Mediterranean | Rome | Augustus | 27 BC–AD 14 | Builder-King | Wide building, peace |
| Mediterranean | Greece (Sparta) | Leonidas I | c. 489–480 BC | Warlord | Defense, culture from war |
| Middle Eastern | Persia | Cyrus the Great | c. 559–530 BC | Statesman | Expansion, loyalty, diplomacy |
| Middle Eastern | Arabia (Ayyubids) | Saladin | 1174–1193 | Warlord | Religion, holy war |
| Asian | China | Qin Shi Huang | 221–210 BC | Builder-King | Infrastructure, walls |
| Asian | Japan | Tokugawa Ieyasu | 1600–1616 (de facto) | Statesman | Peaceful science and culture |
| African | Egypt | Ramesses II | 1279–1213 BC | Warlord | Wonders, chariots |
| African | Mali | Mansa Musa | 1312–1337 | Statesman | Gold, faith, trade |
| American | Aztec | Moctezuma I | 1440–1469 | Warlord | War feeding faith |
| American | Inca | Pachacuti | 1438–1471 | Builder-King | Mountains, roads, growth |

Leanings: 5 Warlord, 4 Statesman, 3 Builder-King.

### Leader details

**Elizabeth I (England), "Sea Dogs"**
- Ability: +25% Production toward naval units; +50% Great Admiral points; international trade routes to another continent +2 Gold.
- Agenda, *Queen of the Seas:* likes civs that keep their fleets away from her coasts; dislikes civs with a navy larger than hers nearby.

**Charlemagne (France), "Carolingian Renaissance"**
- Ability: Holy Site buildings also give +1 Science and +1 Culture; heavy cavalry +3 Combat Strength while within 2 tiles of the leader.
- Agenda, *Defender of the Faith:* likes civs following his religion; dislikes civs converting his cities.

**Augustus (Rome), "City of Marble"**
- Ability: +20% Production toward City Center buildings and wonders; +1 Amenity in every city with an established governor.
- Agenda, *Pax Romana:* likes civs at peace with him; dislikes warmongers near his borders.

**Leonidas I (Greece), "Hold the Pass"**
- Ability: melee and anti-cavalry units +5 Combat Strength when defending on hills or in a district; +1 Culture per Encampment building.
- Agenda, *Spartan Pride:* respects civs with strong armies; scorns civs with weak ones.

**Cyrus the Great (Persia), "King of Kings"**
- Ability: captured cities keep their buildings and get +5 Loyalty per turn; +1 Amenity in your cities that follow a different religion from yours; roads +1 Movement inside your territory (Royal Road).
- Agenda, *Tolerant Conqueror:* likes civs that spare cities; hates civs that raze them.

**Saladin (Arabia), "Chivalry of the Sultan"**
- Ability: units +4 Combat Strength within 2 tiles of a city following your religion; Holy Site buildings +1 Science.
- Agenda, *Magnanimous:* likes civs that release captured leaders and honour peace deals; hates civs that send assassins.

**Qin Shi Huang (China), "Standardization"**
- Ability: Builders +1 charge; +50% Production toward walls; roads cost no Builder charge (Builders lay no roads in Civ VI; his lay a road on their tile, which takes their turn but no charge).
- Agenda, *First Emperor:* dislikes civs that build wonders before him.

**Tokugawa Ieyasu (Japan), "Edo Peace"**
- Ability: while at peace with every major civ, +10% Science and Culture in all cities; Encampment buildings +1 Amenity in their city.
- Agenda, *Closed Country:* dislikes civs that send missionaries or traders into his territory.

**Ramesses II (Egypt), "Builder of Monuments"**
- Ability: wonders +2 Culture; +15% Production toward wonders; light cavalry and chariots +5 Combat Strength on flat desert and floodplains.
- Agenda, *Eternal Name:* dislikes civs with more wonders than he has.

**Mansa Musa (Mali), "Golden Pilgrimage"**
- Ability: international trade routes +2 Gold and +1 Faith; Commercial Hub buildings can be bought with Faith.
- Agenda, *Patron of Trade:* likes civs that send trade routes to him; dislikes civs that plunder traders.

**Moctezuma I (Aztec), "Flower Wars"**
- Ability: killing an enemy unit grants Faith equal to 50% of its Combat Strength; +1 Amenity in the capital per 3 enemy units killed this era (max +3).
- Agenda, *Honourable War:* respects civs that declare war formally; hates surprise wars.

**Pachacuti (Inca), "Earthshaker"**
- Ability: roads on hills and next to mountains +1 Movement; +1 Housing in cities next to a mountain; Farms on hills +1 Food.
- Agenda, *Sapa Inca:* dislikes civs that settle or build in mountain ranges near him.

## Civ abilities, uniques and dynasties

Status: proposed by Claude from each civ's historical specialty, at James's request (2026-10-04); numbers are starting points for playtesting. Each civ gets one civ ability, one unique unit and one unique building or improvement. Civ abilities are written to sit beside the leader ability, not repeat it. Names are historical; wording and numbers are Sovereign's own.

| Civ | Specialty | Civ ability | Unique unit (replaces) | Unique building or improvement (replaces) |
|---|---|---|---|---|
| England | Industry, sea trade | **Mills and Mines:** Iron and Coal mines +1 Production; Industrial Zones +1 Production adjacency from a Harbor | **Longbowman** (Crossbowman): +5 Ranged Strength when attacking from hills or woods, 10% cheaper | **Mill Town** (Factory): +1 Housing, +1 Production to adjacent mines |
| France (Franks) | Religion, scholarship, cavalry | **Cathedral Builders:** +15% Production toward Medieval and Renaissance wonders; each wonder +1 Amenity in its city | **Scara** (Knight): +1 Movement; heals 10 HP when it kills a unit | **Royal Abbey** (Temple): +1 Science and +1 Culture |
| Rome | Expansion, roads, legions | **Colonia:** new cities start with +1 Population and a free Monument | **Legionary** (Swordsman): +4 Combat Strength; one Builder charge for roads and forts | **Forum** (Market): +1 Amenity and +1 Gold per trade route from this city |
| Greece (Sparta) | City-states, culture, phalanx | **Polis:** +2 Culture per city-state you are suzerain of; Theater Squares +1 adjacency from an Encampment | **Hoplite** (Spearman): +10 Combat Strength next to another Hoplite | **Odeon** (Amphitheater): +1 Great Work of Writing slot, +1 Envoy when built |
| Persia | Satrapies, governance, roads | **Satrapies:** cities with an established governor +2 Loyalty and +2 Gold; +1 Governor title at Political Philosophy | **Immortal** (Swordsman): can make a ranged attack (strength 25) as well as melee | **Paradise Garden** (improvement): +2 Culture, +1 Amenity to the city; +Appeal to adjacent tiles |
| Arabia (Ayyubids) | Desert trade, scholarship | **Caravan Cities:** trade routes through desert +2 Gold; Campuses +1 adjacency from a Commercial Hub | **Mamluk** (Knight): heals at the end of every turn, even after moving or attacking | **Madrasa** (University): +2 Faith, +1 Great Scientist point |
| China | Bureaucracy, examinations | **Imperial Examinations:** each Governor title gives +1 Science and +1 Culture in the capital | **Repeating Crossbow** (Crossbowman): can attack after moving | **Beacon Tower** (improvement, own border tiles): +1 Culture, +2 vision, +4 Combat Strength to units defending on it |
| Japan | Crafts, compact districts | **Craft Guilds:** Industrial Zones and Theater Squares +1 adjacency from each other | **Samurai** (Man-At-Arms): no Combat Strength loss when damaged | **Castle Town** (Medieval Walls): +2 Housing, +1 Culture |
| Egypt | Rivers, floodplains | **Gift of the Nile:** districts and wonders can be built on floodplains with no penalty; river tiles +1 Food | **War Chariot** (Heavy Chariot): +1 Movement on flat land; no movement penalty for attacking | **Nilometer** (improvement, river tiles): +1 Food to adjacent Farms; flood damage to adjacent tiles halved |
| Mali | Desert gold, Saharan trade | **Saharan Riches:** Mines on desert and desert hills +2 Gold; Commercial Hubs +1 adjacency per 2 adjacent desert tiles | **Mandinka Lancer** (Knight): +1 Movement in desert, +5 Combat Strength vs. units in desert | **Sahel Mosque** (Temple): +2 Gold, +1 Housing |
| Aztec | Lake farming, tribute | **Chinampas:** Farms next to a lake or river +1 Food and +0.5 Housing | **Jaguar Warrior** (Warrior): +4 Combat Strength in woods and rainforest; captures defeated units as Builders | **Calmecac** (Library): +1 Faith; +25% XP for units trained in the city |
| Inca | Mountains, terraces, relay roads | **Mit'a Labor:** +20% Production toward districts in cities next to a mountain; mountain tiles can be worked for +2 Production | **Chasqui** (Scout): +1 Movement, +1 extra Movement on roads | **Qullqa** (Granary): +2 Housing, +1 Food per adjacent mountain (max +2) |

Movement on a kind of ground (the War Chariot on flat land, the Mandinka Lancer in desert, the Chasqui, Cyrus's Royal Road and Pachacuti's Earthshaker on roads) counts where the unit's turn starts, as Civ VI's own terrain movement bonuses do; the War Chariot keeps the Heavy Chariot's +1 on open ground.

A unique building stands in for the one it replaces wherever a rule names that building: building and wonder prerequisites (the Calmecac opens the Great Library), Apostles and worship buildings (the Sahel Mosque is a Temple), city-state envoy bonuses, Eureka and Inspiration counts, Religious Community, and the Lighthouse's trade route, which a city with Rome's Forum does not get, as it would not with a Market.

**Dynasties (heirs).** Two hand-made successors per launch leader, in historical order; after them, successors come from the pool. Each heir brings a small personal trait (to design with the art pass).

| Civ | Starting leader | Heir 1 | Heir 2 |
|---|---|---|---|
| England | Elizabeth I | James I | Charles I |
| France | Charlemagne | Louis the Pious | Charles the Bald |
| Rome | Augustus | Tiberius | Claudius |
| Greece | Leonidas I | Pleistarchus | Pleistoanax |
| Persia | Cyrus the Great | Cambyses II | Darius I |
| Arabia | Saladin | al-Adil I | al-Kamil |
| China | Qin Shi Huang | Qin Er Shi | Ziying |
| Japan | Tokugawa Ieyasu | Tokugawa Hidetada | Tokugawa Iemitsu |
| Egypt | Ramesses II | Merneptah | Seti II |
| Mali | Mansa Musa | Maghan I | Suleyman |
| Aztec | Moctezuma I | Axayacatl | Ahuitzotl |
| Inca | Pachacuti | Topa Inca Yupanqui | Huayna Capac |

**Heir traits** (designed by Claude under James's standing consent, 2026-10-05; small by intent, built in `data/rules/leader.json`). A trait applies while its heir rules; a regent, a crowned unit or a pool successor brings none.

| Civ | Heir 1 trait | Heir 2 trait |
|---|---|---|
| England | James I, *King James Bible:* +2 Faith in the capital | Charles I, *Divine Right:* +2 Loyalty per turn in every city |
| France | Louis the Pious, *The Pious:* Holy Site buildings +1 Faith | Charles the Bald, *Patron of Learning:* Campus buildings +1 Science |
| Rome | Tiberius, *Full Treasury:* +3 Gold in the capital | Claudius, *Conquest of Britain:* +15% Production toward naval units |
| Greece | Pleistarchus, *Raised in the Agoge:* +15% combat XP | Pleistoanax, *Thirty Years' Peace:* +2 Culture in the capital |
| Persia | Cambyses II, *Conqueror of Egypt:* melee units +2 Combat Strength | Darius I, *Royal Treasury:* +2 Gold in the capital |
| Arabia | al-Adil I, *The Consolidator:* +2 Loyalty per turn in every city | al-Kamil, *Patron of Scholars:* +2 Science in the capital |
| China | Qin Er Shi, *Heir to the Wall:* +25% Production toward walls | Ziying, *Keeper of the Seal:* +2 Culture in the capital |
| Japan | Tokugawa Hidetada, *Shogunate Law:* +2 Loyalty per turn in every city | Tokugawa Iemitsu, *Sakoku:* +5% Culture while at peace with every major civ |
| Egypt | Merneptah, *Victory Stele:* wonders +1 Culture | Seti II, *The Builder's Son:* +10% Production toward wonders |
| Mali | Maghan I, *Keeper of the Hajj:* international trade routes +1 Faith | Suleyman, *Golden Court:* +3 Gold in the capital |
| Aztec | Axayacatl, *The Sun Stone:* +2 Faith in the capital | Ahuitzotl, *Lord of the Waters:* melee units +2 Combat Strength |
| Inca | Topa Inca Yupanqui, *Conqueror of the Andes:* +2 Production in the capital | Huayna Capac, *Road Builder:* +10% Production toward land units |

## Open questions

1. **Heirs without a character generator** **[decided, James 2026-10-04, gap review]**. Succession puts new people on the throne (heir, governor, Great General/Admiral, level 4+ unit), and each needs a model. Decided: each civ ships a short historical dynasty (starting leader plus 2 successors, e.g. Augustus → Tiberius → Claudius) as hand-made heirs, each with a small personal trait instead of a full ability. Great Generals and Admirals reuse their Great Person models. Governors and units become leader using the matching unit or governor model. Once the dynasty runs out, successors come from the pool only. Each civ's dynasty still needs choosing.
2. ~~**Civ abilities and uniques** for the 12 civs are still to design.~~ Drafted above (James 2026-10-04: Claude designs them from each civ's specialty). Pattern **[decided, James 2026-10-04, gap review]**: as in Civ VI, one civ ability, one unique unit and one unique building, district or improvement per civ.
3. **Roster alternates** if one of the above is swapped out: Alfred the Great (England), Alexander the Great (Greece/Macedon), Genghis Khan (Mongolia, needs a steppe style), Shaka (Zulu), Ewuare (Benin).
