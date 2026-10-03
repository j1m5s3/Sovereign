# 06 Religion

## Faith
Faith is a yield accumulated at empire level. Sources: Holy Site and its buildings, pantheon/religion beliefs, natural wonders, policies, city-states (Religious), Stonehenge etc. Spent on: pantheon founding, religious units, buildings via belief (worship building), Great People patronage, Theocracy military purchases, certain civ abilities, Rock Bands, Naturalists, builders/settlers with Monumentality [R&F], Soothsayers [mode].

## Pantheon
- Available once a player accumulates **25 Faith** (scaled by speed). A blocking choice: pick one pantheon belief not already taken by another player.
- Pantheon belief examples (each is a modifier): Dance of the Aurora (Holy Site +1 Faith per adjacent Tundra), Desert Folklore (+1 per Desert), Divine Spark (+1 GPP from Holy Site/Campus/Theater), Earth Goddess (+1 Faith on Breathtaking-Appeal tiles), Fertility Rites (free Builder, +10% growth), God of Craftsmen (+1P +1Fa on strategic improvements), God of Healing (+30 heal near Holy Site), God of the Forge (+25% ancient/classical military prod), God of the Open Sky (+1 Culture per pasture), God of the Sea (+1P fishing boats), God of War (faith from kills near Holy Site), Goddess of Festivals (+1F plantations), Goddess of Fire [GS] (+2 Faith geothermal/volcanic soil), Goddess of the Harvest (faith on chop/harvest), Initiation Rites (+50 faith from clearing barbarian camps), Lady of the Reeds and Marshes (+2P marsh, oasis, floodplains), Monument to the Gods (+15% ancient/classical wonder prod), Oral Tradition (+1C plantations), Religious Idols (+2Fa on luxury/bonus mines), Religious Settlements (border growth +15%), River Goddess (+2 amenity/housing to Holy Site on river), Sacred Path (Holy Site +1 Faith adjacent Rainforest), Stone Circles (+2 faith quarries), City Patron Goddess (+25% district production in cities without a specialty district).
- Pantheon effects apply only to cities you own. Before you found a religion they apply to all your cities that do not follow another religion; once you found a religion the pantheon becomes part of it and applies to your cities whose majority religion is yours (or that have no majority religion). A city that converts to a foreign religion loses your pantheon and does not gain the foreign one.

## Founding a religion
- Max religions in game = `floor(majors / 2) + 1` (e.g., 8 civs → 5; equivalent to the per-map-size table Duel 2 … Huge 7). Once full, Great Prophets stop being available.
- Requires a **Great Prophet** (GPP from Holy Sites, Stonehenge) activated on a completed Holy Site (or Stonehenge tile). That city becomes the **Holy City**.
- Choose name/icon from the list (Buddhism, Catholicism, Confucianism, Eastern Orthodoxy, Hinduism, Islam, Judaism, Protestantism, Shinto, Sikhism, Taoism, Zoroastrianism; or custom).
- On founding, choose **1 Founder belief** and **1 Follower belief**. The religion is then **enhanced** by Apostles: each use of an Apostle's **Evangelize Belief** (consumes the Apostle) adds one belief from a category not yet filled (Worship, Enhancer). Final maximum: pantheon + 1 Founder + 1 Follower + 1 Worship + 1 Enhancer. Beliefs are unique per world.

### Belief categories (examples)
- **Founder** (empire-wide, scales with followers worldwide): Tithe (+1 gold per 4 followers), Church Property (+2 gold per city following), Lay Ministry (+1 culture/faith per Theater/Holy Site in cities following), Papal Primacy (+envoy on conversion), Pilgrimage (+2 faith per city following), Stewardship (+1 science and gold per Campus/Commercial Hub), World Church (+1 culture per 4 followers), Cross-Cultural Dialogue (+1 science per 4 followers), Religious Unity (religion spreads to friendly city-states twice as fast).
- **Follower** (applies to each following city): Choral Music, Divine Inspiration (+4 faith from wonders), Feed the World (+food/housing from shrines/temples), Jesuit Education (buy Campus/Theater buildings with faith), Reliquaries (relics tripled), Religious Community (+international trade route yields), Warrior Monks (buy Warrior Monk unit), Work Ethic (Holy Site adjacency also to production), Zen Meditation (+1 amenity in cities with 2+ districts).
- **Worship** (unlocks a building in Holy Site bought with Faith; all give +3 Faith unless stated): Cathedral (Art slot), Gurdwara (food/housing), Meeting House (production), Mosque (+1 spread charges for missionaries), Pagoda (housing/faith), Synagogue (+5 faith), Wat (science), Stupa (amenity), Dar-e Mehr (faith growing by era).
- **Enhancer**: Crusade (+10 CS near foreign cities following your religion), Defender of the Faith (+10 CS in your lands following), Holy Order (−30% cost missionaries/apostles), Itinerant Preachers (+30% spread range), Missionary Zeal (religious units ignore terrain/feature movement costs), Monastic Isolation (Holy City does not lose pressure when your religious units lose theological combat), Scripture (+25% pressure).

## Spread mechanics
- Each city has a follower count per religion = its population split by **pressure points** accumulated per religion. The **majority religion** is the religion followed by **more than half** of the city's citizens; if no religion exceeds half, the city has no majority religion.
- **Passive pressure**: every city with a majority religion exerts pressure each turn on cities within **10 tiles** (1 pressure per such city; the Holy City exerts 4 (tunable); boosted by beliefs and Itinerant Preachers range +30%). Trade routes passing religion from origin to destination and vice versa add pressure.
- Pressure converts to followers: city follower distribution ∝ accumulated pressure; recalculated each turn.
- **Religious units** (purchased with faith in cities with Holy Site buildings, only of the city's majority religion):
  - Missionary (Shrine): 3 spread charges, M4. Spread Religion: adds a large pressure burst (200 (tunable)) to the target city and a smaller burst (50 (tunable)) to cities within 9 tiles.
  - Apostle (Temple): 3 spread charges, theological combat (religious strength 110), on first purchase/promotion picks a random promotion (Chaplain, Debater, Heathen Conversion, Indulgence Vendor, Martyr (relic on death), Orator, Pilgrim, Proselytizer (removes other religions 75% on spread), Translator). Evangelize Belief (adds a belief), Launch Inquisition.
  - Inquisitor (after Launch Inquisition): Remove Heresy (removes 75% of other religions' pressure in your city; 3 charges), religious strength 75.
  - Guru: heals adjacent religious units.
  - Great Prophet: founds a religion only; it cannot take part in theological combat.
- Religious units can enter any territory (no open borders needed) except that of a civ at war with them; a civ may ask you to stop converting its cities (a diplomatic promise).

## Theological combat
- Only Apostles and Inquisitors can initiate theological combat (Gurus support/heal; Warrior Monks are military units). They attack adjacent religious units of a different religion; no war is required. Formula identical to military damage with religious strength. Loser destroyed; winner's religion gets pressure burst (250) in cities within 10 tiles, loser's religion loses pressure there.
- Military units can kill enemy religious units only when at war ("condemn heretic").

## Holy cities and relics
- Holy City adds bonus pressure; if captured by another religion's civ, pressure shifts. Holy city can't be razed.
- Relics: Great Works placed in Temples/Cathedral/etc. +4 Faith and +8 Tourism (relic tourism needs no tech). From: Martyr Apostles, goody huts, some leader/civ abilities.

## Religious victory
A player wins when their religion is the **majority religion in every major civilization** still in the game (each civ counts as converted when more than half its cities follow the religion). Must have founded a religion.

## Integration points
- Religion lens UI shows pressure and majority.
- Diplomacy: AI with "religious" agendas dislike spreading.
- Policy cards and governments affect faith purchase costs.
- Tourism: religious tourism (Holy cities) via Cristo Redentor.
