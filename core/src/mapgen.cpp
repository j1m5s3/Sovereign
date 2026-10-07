#include "sovereign/mapgen.h"

#include <algorithm>

namespace sov {

namespace {

constexpr int kNoiseMax = 1000;

int smooth(int t) {  // smoothstep on [0, 1000]
    return static_cast<int>(static_cast<int64_t>(t) * t * (3000 - 2 * t) / 1000000);
}

// Value noise in [0, 1000] per plot. Cells wrap east-west when the map does.
std::vector<int> noiseField(const HexGrid& g, Rng& rng, int cell) {
    const int w = g.width(), h = g.height();
    const int cx = std::max(2, w / cell);
    const int cy = std::max(2, h / cell);
    const int gw = g.wrapX() ? cx : cx + 1;
    const int gh = cy + 1;
    std::vector<int> grid(static_cast<size_t>(gw * gh));
    for (int& v : grid) v = static_cast<int>(rng.below(kNoiseMax + 1));
    auto at = [&](int x, int y) {
        if (g.wrapX()) x %= cx;
        else x = std::min(x, gw - 1);
        y = std::min(y, gh - 1);
        return grid[static_cast<size_t>(y * gw + x)];
    };
    std::vector<int> out(static_cast<size_t>(g.size()));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // Half-hex units so odd rows sit between even-row samples.
            int64_t fx = static_cast<int64_t>(x * 2 + (y & 1)) * cx * 1000 / (2 * w);
            int64_t fy = static_cast<int64_t>(y) * cy * 1000 / std::max(1, h - 1);
            int gx = static_cast<int>(fx / 1000), tx = smooth(static_cast<int>(fx % 1000));
            int gy = static_cast<int>(fy / 1000), ty = smooth(static_cast<int>(fy % 1000));
            int v00 = at(gx, gy), v10 = at(gx + 1, gy), v01 = at(gx, gy + 1), v11 = at(gx + 1, gy + 1);
            int top = v00 + (v10 - v00) * tx / 1000;
            int bottom = v01 + (v11 - v01) * tx / 1000;
            out[static_cast<size_t>(y * w + x)] = top + (bottom - top) * ty / 1000;
        }
    }
    return out;
}

std::vector<int> fractal(const HexGrid& g, Rng& rng, std::initializer_list<std::pair<int, int>> octaves) {
    std::vector<int> sum(static_cast<size_t>(g.size()), 0);
    int total = 0;
    for (auto [cell, weight] : octaves) {
        std::vector<int> n = noiseField(g, rng, cell);
        for (size_t i = 0; i < sum.size(); ++i) sum[i] += n[i] * weight;
        total += weight;
    }
    for (int& v : sum) v /= total;
    return sum;
}

// Value at the given percentile (0-100) of the samples.
int percentile(std::vector<int> values, int pct) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    size_t i = static_cast<size_t>(static_cast<int64_t>(values.size() - 1) * pct / 100);
    return values[i];
}

bool listHas(const std::vector<TypeIndex>& v, TypeIndex t) {
    return std::find(v.begin(), v.end(), t) != v.end();
}

bool featureAllowed(const Rules& rules, TypeIndex feature, TypeIndex terrain) {
    return feature != kNone && listHas(rules.features[static_cast<size_t>(feature)].validTerrains, terrain);
}

Yields plotYields(const GameState& s, const Rules& r, const Plot& p) {
    Yields y = r.terrains[static_cast<size_t>(p.terrain)].yields;
    if (p.feature != kNone) {
        const Yields& f = r.features[static_cast<size_t>(p.feature)].yields;
        for (size_t i = 0; i < kNumYields; ++i) y[i] += f[i];
    }
    if (p.resource != kNone) {
        const Yields& f = r.resources[static_cast<size_t>(p.resource)].yields;
        for (size_t i = 0; i < kNumYields; ++i) y[i] += f[i];
    }
    (void)s;
    return y;
}

}  // namespace

bool hasRiver(const GameState& state, Hex h, Dir d) {
    switch (d) {
        case Dir::E: return state.plot(h).riverEdges & kRiverE;
        case Dir::SE: return state.plot(h).riverEdges & kRiverSE;
        case Dir::SW: return state.plot(h).riverEdges & kRiverSW;
        default: {
            auto n = state.grid.neighbor(h, d);
            return n && hasRiver(state, *n, opposite(d));
        }
    }
}

void setRiver(GameState& state, Hex h, Dir d) {
    switch (d) {
        case Dir::E: state.plot(h).riverEdges |= kRiverE; break;
        case Dir::SE: state.plot(h).riverEdges |= kRiverSE; break;
        case Dir::SW: state.plot(h).riverEdges |= kRiverSW; break;
        default: {
            auto n = state.grid.neighbor(h, d);
            if (n) setRiver(state, *n, opposite(d));
        }
    }
}

bool isRiverAdjacent(const GameState& state, Hex h) {
    for (int d = 0; d < kNumDirs; ++d) {
        if (hasRiver(state, h, static_cast<Dir>(d))) return true;
    }
    return false;
}

bool isLake(const GameState& state, const Rules& rules, Hex h, const std::vector<uint8_t>* lakes) {
    if (lakes) return (*lakes)[static_cast<size_t>(state.grid.index(h))] != 0;
    const auto water = [&](Hex x) { return rules.terrains[static_cast<size_t>(state.plot(x).terrain)].water; };
    if (!water(h)) return false;
    static const std::string kLimit = "LAKE_MAX_AREA_SIZE";
    const size_t limit = static_cast<size_t>(std::max(0, rules.globalInt(kLimit)));
    std::vector<Hex> body;
    body.reserve(std::max<size_t>(limit, 1));  // it never holds more than the limit, so it is allocated once
    body.push_back(h);
    for (size_t i = 0; i < body.size(); ++i) {
        for (int d = 0; d < kNumDirs; ++d) {
            const auto n = state.grid.neighbor(body[i], static_cast<Dir>(d));
            if (!n || !water(*n) || std::find(body.begin(), body.end(), *n) != body.end()) continue;
            if (body.size() >= limit) return false;  // one plot too many: the sea or an inland sea
            body.push_back(*n);
        }
    }
    return true;
}

bool isLakeAdjacent(const GameState& state, const Rules& rules, Hex h, const std::vector<uint8_t>* lakes) {
    for (int d = 0; d < kNumDirs; ++d) {
        const auto n = state.grid.neighbor(h, static_cast<Dir>(d));
        if (n && isLake(state, rules, *n, lakes)) return true;
    }
    return false;
}

std::vector<uint8_t> lakeMap(const GameState& state, const Rules& rules) {
    // isLake keeps a plot's body of water while it is no larger than the limit, and so finds a lake exactly when the
    // body has at most that many plots, or is the plot alone. Here a body is gathered until it proves too big, by its
    // size or by reaching water already found too big (the same body), and the rest of such a body is found too big
    // as soon as it is reached; a body gathered whole within the limit is a lake.
    constexpr uint8_t kLand = 0, kOpen = 1, kBody = 2, kBig = 3, kLake = 4;
    const size_t limit = static_cast<size_t>(std::max(1, rules.globalInt("LAKE_MAX_AREA_SIZE")));
    const size_t plots = state.plots.size();
    std::vector<uint8_t> mark(plots);
    for (size_t i = 0; i < plots; ++i) mark[i] = rules.terrains[static_cast<size_t>(state.plots[i].terrain)].water ? kOpen : kLand;
    std::vector<int32_t> body;
    for (size_t i = 0; i < plots; ++i) {
        if (mark[i] != kOpen) continue;
        mark[i] = kBody;
        body.assign(1, static_cast<int32_t>(i));
        bool big = false;
        for (size_t k = 0; k < body.size() && !big; ++k) {
            const Hex h = state.grid.at(body[k]);
            for (int d = 0; d < kNumDirs && !big; ++d) {
                const auto n = state.grid.neighbor(h, static_cast<Dir>(d));
                if (!n) continue;
                uint8_t& m = mark[static_cast<size_t>(state.grid.index(*n))];
                if (m == kBig || (m == kOpen && body.size() >= limit)) {
                    big = true;
                } else if (m == kOpen) {
                    m = kBody;
                    body.push_back(state.grid.index(*n));
                }
            }
        }
        for (const int32_t j : body) mark[static_cast<size_t>(j)] = big ? kBig : kLake;
    }
    for (uint8_t& m : mark) m = static_cast<uint8_t>(m == kLake ? 1 : 0);
    return mark;
}

bool hasFreshWater(const GameState& state, const Rules& rules, Hex h, const std::vector<uint8_t>* lakes) {
    if (isRiverAdjacent(state, h)) return true;
    for (const Hex& n : state.grid.within(h, 1)) {
        const Plot& p = state.plot(n);
        if (p.feature != kNone && rules.features[static_cast<size_t>(p.feature)].freshWater) return true;
    }
    return isLakeAdjacent(state, rules, h, lakes);
}

bool isLandPassable(const GameState& state, const Rules& rules, Hex h) {
    return isLandPassable(rules, state.plot(h));
}

// Natural wonders per map size: Duel 2, Tiny 3, Small 4, Standard 5, Large 6, Huge 7 (01). Each goes on
// a plot of a valid terrain with no feature, resource or owner, at least 4 plots from any start, plus
// as many neighbouring valid plots as its footprint needs (Sovereign: any shape of neighbours).
void placeNaturalWonders(GameState& state, const Rules& rules) {
    static const std::pair<const char*, int> kCounts[] = {{"MAPSIZE_DUEL", 2}, {"MAPSIZE_TINY", 3}, {"MAPSIZE_SMALL", 4},
                                                          {"MAPSIZE_STANDARD", 5}, {"MAPSIZE_LARGE", 6}, {"MAPSIZE_HUGE", 7}};
    int want = 2;
    for (const auto& [id, n] : kCounts) want = state.setup.mapSize == id ? n : want;
    std::vector<TypeIndex> wonders;
    for (size_t f = 0; f < rules.features.size(); ++f) {
        if (rules.features[f].naturalWonder) wonders.push_back(static_cast<TypeIndex>(f));
    }
    Rng& rng = state.rng.get(RngStream::MapGen);
    const auto fits = [&](const FeatureType& f, Hex h) {
        const Plot& p = state.plot(h);
        if (p.feature != kNone || p.resource != kNone || p.owner != kNoPlayer) return false;
        return std::find(f.validTerrains.begin(), f.validTerrains.end(), p.terrain) != f.validTerrains.end();
    };
    int placed = 0;
    for (int tries = 0; tries < 400 && placed < want && !wonders.empty(); ++tries) {
        const size_t k = rng.below(static_cast<uint32_t>(wonders.size()));
        const FeatureType& f = rules.features[static_cast<size_t>(wonders[k])];
        const Hex h = state.grid.at(static_cast<int>(rng.below(static_cast<uint32_t>(state.grid.size()))));
        if (!fits(f, h)) continue;
        bool far = true;
        for (const Player& p : state.players) far = far && state.grid.distance(p.startPos, h) >= 4;
        if (!far) continue;
        std::vector<Hex> cluster{h};
        for (const Hex& n : state.grid.within(h, 1)) {
            if (static_cast<int>(cluster.size()) >= f.tiles) break;
            if (n != h && fits(f, n)) cluster.push_back(n);
        }
        if (static_cast<int>(cluster.size()) < f.tiles) continue;
        for (const Hex& c : cluster) state.plot(c).feature = wonders[k];
        wonders.erase(wonders.begin() + static_cast<std::ptrdiff_t>(k));  // each wonder once
        ++placed;
    }
}

// Sovereign reading of the map script's density (unverified in the specs): one village per 25 land
// plots, at least 4 plots from any start and 4 from each other, on unowned passable land with no
// resource of note left unchecked (resources are kept).
void placeVillages(GameState& state, const Rules& rules) {
    Rng& rng = state.rng.get(RngStream::MapGen);
    std::vector<Hex> land;
    for (int i = 0; i < state.grid.size(); ++i) {
        const Hex h = state.grid.at(i);
        if (isLandPassable(state, rules, h) && state.plot(h).owner == kNoPlayer) land.push_back(h);
    }
    const size_t target = land.size() / 25;
    std::vector<Hex> placed;
    for (size_t tries = 0; tries < land.size() * 2 && placed.size() < target && !land.empty(); ++tries) {
        const Hex h = land[rng.below(static_cast<uint32_t>(land.size()))];
        bool ok = true;
        for (const Player& p : state.players) ok = ok && state.grid.distance(p.startPos, h) >= 4;
        for (const Hex& o : placed) ok = ok && state.grid.distance(o, h) >= 4;
        if (!ok) continue;
        state.plot(h).village = true;
        placed.push_back(h);
    }
}

void generateMap(GameState& state, const Rules& rules) {
    const HexGrid& g = state.grid;
    const int w = g.width(), h = g.height();
    Rng& rng = state.rng.get(RngStream::MapGen);
    state.plots.assign(static_cast<size_t>(g.size()), Plot{});

    // 1. Height: fractal noise, pushed down near the poles (and the east/west
    //    edges on non-wrapping maps) so land sits away from the map border.
    std::vector<int> height = fractal(g, rng, {{std::max(4, w / 5), 5}, {std::max(3, w / 10), 3}, {3, 1}});
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int& v = height[static_cast<size_t>(y * w + x)];
            int edge = std::min(y, h - 1 - y);
            if (!g.wrapX()) edge = std::min(edge, std::min(x, w - 1 - x));
            int band = std::max(2, h / 8);
            if (edge < band) v -= (band - edge) * 400 / band;
            if (y == 0 || y == h - 1) v = -kNoiseMax;  // polar rows are always water
        }
    }
    const int seaLevel = percentile(height, 100 - rules.globalInt("MAPGEN_LAND_PERCENT"));
    std::vector<int> landHeights;
    for (int v : height) if (v > seaLevel) landHeights.push_back(v);
    const int mountainLevel = percentile(landHeights, 100 - rules.globalInt("MAPGEN_MOUNTAIN_PERCENT"));
    const int hillLevel = percentile(landHeights, 100 - rules.globalInt("MAPGEN_MOUNTAIN_PERCENT") -
                                                      rules.globalInt("MAPGEN_HILLS_PERCENT"));
    std::vector<int> rough = noiseField(g, rng, 3);
    std::vector<int> rain = fractal(g, rng, {{std::max(4, w / 6), 3}, {4, 1}});
    std::vector<int> tempNoise = noiseField(g, rng, std::max(4, w / 8));

    auto latitude = [&](int y) {  // 0 at the equator, 100 at the poles
        return std::abs(2 * y - (h - 1)) * 100 / std::max(1, h - 1);
    };
    auto temperature = [&](int y, size_t i) { return 100 - latitude(y) + (tempNoise[i] - 500) / 25; };

    // 2. Terrain: relief from height, climate from latitude and rainfall.
    const TypeIndex coast = rules.terrainFor("COAST", Relief::Flat);
    const TypeIndex ocean = rules.terrainFor("OCEAN", Relief::Flat);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t i = static_cast<size_t>(y * w + x);
            Plot& p = state.plots[i];
            if (height[i] <= seaLevel) {
                p.terrain = ocean;
                continue;
            }
            int temp = temperature(y, i);
            const char* base = "GRASSLAND";
            if (temp < 12) base = "SNOW";
            else if (temp < 28) base = "TUNDRA";
            else if (rain[i] < 330 && temp > 55) base = "DESERT";
            else if (rain[i] < 520) base = "PLAINS";
            Relief relief = Relief::Flat;
            if (height[i] > mountainLevel) relief = Relief::Mountain;
            else if (height[i] > hillLevel || rough[i] > 880) relief = Relief::Hills;
            TypeIndex t = rules.terrainFor(base, relief);
            p.terrain = t != kNone ? t : rules.terrainFor("GRASSLAND", Relief::Flat);
        }
    }
    auto isWater = [&](Hex hx) { return rules.terrains[static_cast<size_t>(state.plot(hx).terrain)].water; };
    for (int i = 0; i < g.size(); ++i) {
        Hex hx = g.at(i);
        if (!isWater(hx)) continue;
        for (int d = 0; d < kNumDirs; ++d) {
            auto n = g.neighbor(hx, static_cast<Dir>(d));
            if (n && !isWater(*n)) {
                state.plot(hx).terrain = coast;
                break;
            }
        }
    }

    // 3. Rivers: from high land downhill to water, running along plot edges
    //    on the left bank of the downhill path.
    std::vector<int> sources;
    for (int i = 0; i < g.size(); ++i) {
        const TerrainType& t = rules.terrains[static_cast<size_t>(state.plots[static_cast<size_t>(i)].terrain)];
        if (!t.water && t.relief == Relief::Hills) sources.push_back(i);
    }
    std::sort(sources.begin(), sources.end(), [&](int a, int b) {
        return height[static_cast<size_t>(a)] != height[static_cast<size_t>(b)]
                   ? height[static_cast<size_t>(a)] > height[static_cast<size_t>(b)]
                   : a < b;
    });
    int riverCount = static_cast<int>(landHeights.size()) / std::max(1, rules.globalInt("MAPGEN_LAND_PER_RIVER"));
    std::vector<Hex> usedSources;
    for (int si = 0; si < static_cast<int>(sources.size()) && riverCount > 0; ++si) {
        Hex cur = g.at(sources[static_cast<size_t>(si)]);
        bool tooClose = false;
        for (const Hex& u : usedSources) tooClose = tooClose || g.distance(u, cur) < 4;
        if (tooClose || isRiverAdjacent(state, cur)) continue;
        usedSources.push_back(cur);
        --riverCount;
        std::optional<Hex> prevBank;
        for (int step = 0; step < 24; ++step) {
            std::optional<Hex> next;
            int best = height[static_cast<size_t>(g.index(cur))];
            for (int d = 0; d < kNumDirs; ++d) {
                auto n = g.neighbor(cur, static_cast<Dir>(d));
                if (!n) continue;
                int hv = height[static_cast<size_t>(g.index(*n))];
                if (hv < best) {
                    best = hv;
                    next = n;
                }
            }
            if (!next) break;
            Dir d = *g.directionTo(cur, *next);
            auto bank = g.neighbor(cur, static_cast<Dir>((static_cast<int>(d) + 5) % 6));
            if (!bank) break;
            // Wrap the river around `cur` from the previous bank to this one.
            if (prevBank) {
                auto from = g.directionTo(cur, *prevBank);
                auto to = g.directionTo(cur, *bank);
                if (from && to) {
                    int a = static_cast<int>(*from), b = static_cast<int>(*to);
                    int cw = (b - a + 6) % 6, step6 = cw <= 3 ? 1 : 5;
                    for (int k = a; k != b; k = (k + step6) % 6) {
                        auto nb = g.neighbor(cur, static_cast<Dir>(k));
                        if (nb && !(isWater(cur) && isWater(*nb))) setRiver(state, cur, static_cast<Dir>(k));
                    }
                }
            }
            if (!(isWater(cur) && isWater(*bank))) setRiver(state, cur, *g.directionTo(cur, *bank));
            if (!(isWater(*next) && isWater(*bank))) setRiver(state, *next, *g.directionTo(*next, *bank));
            if (isWater(*next)) break;
            prevBank = bank;
            cur = *next;
        }
    }

    // 3b. Lakes (01: lakes in basins). One flat inland plot in LAKE_PLOT_RANDOM, off the rivers and away from any
    //     water, becomes a lake, and each such plot beside it joins the lake one time in four, then five, six...
    //     (Sovereign reading of Civ VI's map scripts, which grow some lakes past one plot). A lake never touches
    //     other water, so each stays a body of at most seven plots.
    const int lakeOdds = rules.globalInt("LAKE_PLOT_RANDOM");
    auto inland = [&](Hex hx) {
        const TerrainType& t = rules.terrains[static_cast<size_t>(state.plot(hx).terrain)];
        if (t.water || t.relief != Relief::Flat || isRiverAdjacent(state, hx)) return false;
        for (int d = 0; d < kNumDirs; ++d) {
            const auto n = g.neighbor(hx, static_cast<Dir>(d));
            if (!n || isWater(*n)) return false;
        }
        return true;
    };
    for (int i = 0; i < g.size() && lakeOdds > 0; ++i) {
        const Hex hx = g.at(i);
        if (!inland(hx) || rng.below(static_cast<uint32_t>(lakeOdds)) != 0) continue;
        std::vector<Hex> lake{hx};
        for (int d = 0; d < kNumDirs; ++d) {
            const auto n = g.neighbor(hx, static_cast<Dir>(d));
            if (n && inland(*n) && rng.below(static_cast<uint32_t>(3 + lake.size())) == 0) lake.push_back(*n);
        }
        for (const Hex& l : lake) state.plot(l).terrain = coast;
    }

    // 4. Features by climate.
    auto featureId = [&](const char* id) { return rules.feature(id); };
    const TypeIndex woods = featureId("FEATURE_FOREST"), jungle = featureId("FEATURE_JUNGLE"),
                    marsh = featureId("FEATURE_MARSH"), oasis = featureId("FEATURE_OASIS"),
                    ice = featureId("FEATURE_ICE"), reef = featureId("FEATURE_REEF"),
                    floodDesert = featureId("FEATURE_FLOODPLAINS"),
                    floodGrass = featureId("FEATURE_FLOODPLAINS_GRASSLAND"),
                    floodPlains = featureId("FEATURE_FLOODPLAINS_PLAINS");
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t i = static_cast<size_t>(y * w + x);
            Hex hx{x, y};
            Plot& p = state.plots[i];
            const TerrainType& t = rules.terrains[static_cast<size_t>(p.terrain)];
            int temp = temperature(y, i);
            auto tryPlace = [&](TypeIndex f, uint32_t pct) {
                if (p.feature == kNone && featureAllowed(rules, f, p.terrain) && rng.chance(pct)) p.feature = f;
            };
            if (t.water) {
                if (latitude(y) > 86) tryPlace(ice, 85);
                else if (temp > 70) tryPlace(reef, 8);
                continue;
            }
            if (t.relief == Relief::Mountain) continue;
            bool river = isRiverAdjacent(state, hx);
            if (river && t.relief == Relief::Flat) {
                tryPlace(floodDesert, 100);
                tryPlace(floodGrass, 35);
                tryPlace(floodPlains, 35);
            }
            if (t.base == "DESERT" && t.relief == Relief::Flat && !river) {
                bool near = false;
                for (const Hex& n : g.within(hx, 2)) near = near || (state.plot(n).feature == oasis);
                if (!near) tryPlace(oasis, 4);
            }
            if (temp > 68 && rain[i] > 520) tryPlace(jungle, 60);
            if (rain[i] > 430) tryPlace(woods, 45);
            if (rain[i] > 620 && t.relief == Relief::Flat) tryPlace(marsh, 18);
        }
    }

    // 5. Resources, weighted by each resource's frequency on its valid plots.
    for (int i = 0; i < g.size(); ++i) {
        Plot& p = state.plots[static_cast<size_t>(i)];
        const TerrainType& t = rules.terrains[static_cast<size_t>(p.terrain)];
        if (t.impassable) continue;
        int total = 0;
        std::vector<std::pair<TypeIndex, int>> options;
        for (size_t r = 0; r < rules.resources.size(); ++r) {
            const ResourceType& res = rules.resources[r];
            int weight = t.water ? res.seaFrequency : res.frequency;
            if (weight <= 0) continue;
            // On a featured plot the feature decides; otherwise the terrain.
            bool ok = p.feature != kNone ? listHas(res.validFeatures, p.feature) : listHas(res.validTerrains, p.terrain);
            if (!ok) continue;
            options.emplace_back(static_cast<TypeIndex>(r), weight);
            total += weight;
        }
        if (options.empty()) continue;
        if (rng.below(1000) >= static_cast<uint32_t>(std::min(total * rules.globalInt("MAPGEN_RESOURCE_DENSITY"), 400)))
            continue;
        int pick = static_cast<int>(rng.below(static_cast<uint32_t>(total)));
        for (auto [r, weight] : options) {
            if (pick < weight) {
                p.resource = r;
                p.resourceAmount = 1;
                break;
            }
            pick -= weight;
        }
    }

    // 6. Continent ids by flood fill over land.
    int16_t nextId = 0;
    std::vector<int> stack;
    for (int i = 0; i < g.size(); ++i) {
        Plot& p0 = state.plots[static_cast<size_t>(i)];
        if (p0.continent >= 0 || rules.terrains[static_cast<size_t>(p0.terrain)].water) continue;
        p0.continent = nextId;
        stack.push_back(i);
        while (!stack.empty()) {
            Hex cur = g.at(stack.back());
            stack.pop_back();
            for (int d = 0; d < kNumDirs; ++d) {
                auto n = g.neighbor(cur, static_cast<Dir>(d));
                if (!n) continue;
                Plot& np = state.plot(*n);
                if (np.continent >= 0 || rules.terrains[static_cast<size_t>(np.terrain)].water) continue;
                np.continent = nextId;
                stack.push_back(g.index(*n));
            }
        }
        ++nextId;
    }
}

bool chooseStartPositions(GameState& state, const Rules& rules, std::string* error) {
    const HexGrid& g = state.grid;
    struct Cand { int index; int score; };
    std::vector<Cand> cands;
    for (int i = 0; i < g.size(); ++i) {
        Hex hx = g.at(i);
        if (!isLandPassable(state, rules, hx)) continue;
        const TerrainType& t = rules.terrains[static_cast<size_t>(state.plot(hx).terrain)];
        if (t.base == "SNOW") continue;
        int edge = std::min(hx.y, g.height() - 1 - hx.y);
        if (edge < 3) continue;
        int freeNeighbors = 0;
        bool coastal = false;
        for (int d = 0; d < kNumDirs; ++d) {
            auto n = g.neighbor(hx, static_cast<Dir>(d));
            if (!n) continue;
            if (isLandPassable(state, rules, *n)) ++freeNeighbors;
            const TerrainType& nt = rules.terrains[static_cast<size_t>(state.plot(*n).terrain)];
            coastal = coastal || nt.shallowWater;
        }
        if (freeNeighbors == 0) continue;
        int score = 0;
        for (const Hex& n : g.within(hx, 2)) {
            const Plot& p = state.plot(n);
            if (rules.terrains[static_cast<size_t>(p.terrain)].impassable) continue;
            Yields y = plotYields(state, rules, p);
            score += static_cast<int>((y[0] * 3 + y[1] * 2 + y[2]).toInt());
        }
        for (const Hex& n : g.within(hx, 3)) score += isLandPassable(state, rules, n) ? 2 : 0;
        if (hasFreshWater(state, rules, hx)) score += 15;
        if (coastal) score += 6;
        cands.push_back({i, score});
    }
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
        return a.score != b.score ? a.score > b.score : a.index < b.index;
    });
    const size_t need = state.players.size();
    const int minAllowed = rules.globalInt(HotGlobal::CityMinRange) + 1;
    for (int minDist = rules.globalInt("START_DISTANCE_MAJOR_CIVILIZATION"); minDist >= minAllowed; --minDist) {
        std::vector<Hex> chosen;
        for (const Cand& c : cands) {
            Hex hx = g.at(c.index);
            bool ok = true;
            for (const Hex& o : chosen) ok = ok && g.distance(o, hx) >= minDist;
            if (ok) chosen.push_back(hx);
            if (chosen.size() == need) break;
        }
        if (chosen.size() == need) {
            for (size_t i = 0; i < need; ++i) state.players[i].startPos = chosen[i];
            return true;
        }
    }
    if (error) *error = "map has no room for every player's start";
    return false;
}

}  // namespace sov
