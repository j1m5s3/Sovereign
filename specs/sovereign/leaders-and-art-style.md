# Sovereign: art style and leader roster

Status: art style and roster rules decided; the roster below is a proposal (2026-10-04). Companion to [leader-character-brainstorm.md](leader-character-brainstorm.md) and [world-scale-and-generation.md](world-scale-and-generation.md). All numbers are starting points for playtesting.

## Decisions

- **[decided, James 2026-10-04] Art style: stylized realism.** Realistic proportions, hand-painted and simplified textures (references: Dishonored, Arcane, Humankind's units). Not Civ VI's caricature look, which clashes with assassinations, live battles and street-level walking; not photorealism, which ages fast and multiplies the cost of the ~30 building kits. At the furthest zoom the map keeps a Civ-style parchment layer for fog of war and the strategic view.
- **[decided, James 2026-10-04] Historical leaders only.** No custom or generated leaders and no character creator: they risk balance problems and the creator would cost more than it adds.
- **[decided, James 2026-10-04] Launch roster: 12 civs**, two per architectural style, so every building kit is used.

## Art toolchain (recommended)

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

1. **Leader ability:** an empire-wide bonus in Civ terms (yields, production, combat strength, loyalty). Lost if the leader dies and a successor takes over (see open question 1).
2. **Promotion leaning:** the leader starts with the first promotion of one `SOVEREIGN` branch (Warlord, Statesman or Builder-King, see leader doc section 3). The player can still finish any one branch.
3. **AI agenda:** what the AI version of this leader likes and dislikes, feeding diplomacy and the nemesis memory (player-retention doc).

Abilities are Sovereign's own designs. Real historical people are free to use; Civ VI's portrayals and ability texts are not, so models, voices and wording are ours.

Excluded on principle: 20th-century dictators and religious founders.

## Proposed launch roster

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
- Ability: Builders +1 charge; +50% Production toward walls; roads cost no Builder charge.
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

## Open questions

1. **Heirs without a character generator.** Succession puts new people on the throne (heir, governor, Great General/Admiral, level 4+ unit), and each needs a model. Proposal: each civ ships a short historical dynasty (starting leader plus 2 successors, e.g. Augustus → Tiberius → Claudius) as hand-made heirs, each with a small personal trait instead of a full ability. Great Generals and Admirals reuse their Great Person models. Governors and units become leader using the matching unit or governor model.
2. **Civ abilities and uniques** for the 12 civs are still to design.
3. **Roster alternates** if one of the above is swapped out: Alfred the Great (England), Alexander the Great (Greece/Macedon), Genghis Khan (Mongolia, needs a steppe style), Shaka (Zulu), Ewuare (Benin).
