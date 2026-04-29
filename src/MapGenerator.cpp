#include "obj/MapGenerator.hpp"
#include "obj/PerlinNoise.hpp"
#include "obj/ECS/Component.hpp"
#include "obj/GameDefines.hpp"

void loadMap(Manager &manager, size_t seed) {

    std::vector<std::vector<int>> mapData(MAP_HEIGHT, std::vector<int>(MAP_WIDTH, 0));
    PerlinNoise perlin(seed);

    // ==========================================
    // PHASE 1: GENERATE BASE GRASS (LAYER 1)
    // ==========================================
    float noiseScale = 0.1f;
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < MAP_WIDTH; ++x) {
            // Force edges to be water so we get an island, not a cut-off continent
            if (x < 2 || x > MAP_WIDTH - 3 || y < 2 || y > MAP_HEIGHT - 3) {
                mapData[y][x] = 0;
                continue;
            }
            float n = (perlin.noise((float)x * noiseScale, (float)y * noiseScale) + 1.0f) / 2.0f;
            mapData[y][x] = (n > 0.45f) ? 1 : 0;
        }
    }

    // Cellular Automata Smoothing (3 Passes for Grass)
    for (int pass = 0; pass < 3; ++pass) {
        std::vector<std::vector<int>> nextMap = mapData;
        for (int y = 1; y < MAP_HEIGHT - 1; ++y) {
            for (int x = 1; x < MAP_WIDTH - 1; ++x) {
                int neighbors = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0)
                            continue;
                        if (mapData[y + dy][x + dx] >= 1)
                            neighbors++;
                    }
                }
                // If it's land and has enough neighbors, stay land. If water has 5+ neighbors, fill
                // it with land!
                if (mapData[y][x] >= 1)
                    nextMap[y][x] = (neighbors >= 4) ? 1 : 0;
                else
                    nextMap[y][x] = (neighbors >= 5) ? 1 : 0;
            }
        }
        mapData = nextMap;
    }

    // ==========================================
    // PHASE 2: GENERATE HILLS (LAYER 2)
    // ==========================================
    float hillNoiseScale = 0.15f; // Slightly higher frequency for smaller hill blobs
    for (int y = 2; y < MAP_HEIGHT - 2; ++y) {
        for (int x = 2; x < MAP_WIDTH - 2; ++x) {
            // We ONLY place hills on existing grass
            if (mapData[y][x] == 1) {
                // Add an offset (e.g., +100) so the hill noise doesn't perfectly match the grass
                // noise
                float n = (perlin.noise((float)(x + 100) * hillNoiseScale,
                                        (float)(y + 100) * hillNoiseScale) +
                           1.0f) /
                          2.0f;
                if (n > 0.55f) {
                    mapData[y][x] = 2;
                }
            }
        }
    }

    // Cellular Automata Smoothing (2 Passes for Hills)
    for (int pass = 0; pass < 2; ++pass) {
        std::vector<std::vector<int>> nextMap = mapData;
        for (int y = 1; y < MAP_HEIGHT - 1; ++y) {
            for (int x = 1; x < MAP_WIDTH - 1; ++x) {
                if (mapData[y][x] == 0)
                    continue; // Ignore water

                int hillNeighbors = 0;
                bool touchesWater = false;

                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0)
                            continue;
                        if (mapData[y + dy][x + dx] == 2)
                            hillNeighbors++;
                        if (mapData[y + dy][x + dx] == 0)
                            touchesWater = true;
                    }
                }

                // If a hill touches water, it causes a graphical glitch. Force it back to grass!
                if (touchesWater) {
                    nextMap[y][x] = 1;
                    continue;
                }

                if (mapData[y][x] == 2)
                    nextMap[y][x] = (hillNeighbors >= 3) ? 2 : 1;
                else if (mapData[y][x] == 1)
                    nextMap[y][x] = (hillNeighbors >= 5) ? 2 : 1;
            }
        }
        mapData = nextMap;
    }

    // ==========================================
    // PHASE 3: RENDERING & AUTOTILING
    // ==========================================
    auto isLand = [&](int x, int y, int currentLayer) -> bool {
        if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT)
            return false;
        if (mapData[y][x] == 3)
            return true; // Treat stairs as land
        return mapData[y][x] >= currentLayer;
    };

    // A clean struct to hold our spritesheet coordinates
    struct TileCoord {
        int col;
        int row;
    };

    // This array acts as our Map. The index (0-15) matches the exact bitmask value!
    // Bitmask format: North(1) | South(2) | East(4) | West(8)
    // This array acts as our Map. The index (0-31) matches the exact bitmask value!
    // Bitmask format: North(1) | South(2) | East(4) | West(8) | isLayered(16)
    const TileCoord bitmaskToTile[32] = {
        // --- BASE LAYER (isLayered = 0) ---
        {1, 1}, //  0: None
        {3, 2}, //  1: North
        {3, 0}, //  2: South
        {3, 1}, //  3: North, South
        {0, 3}, //  4: East
        {0, 2}, //  5: North, East (Bottom-Left Corner)
        {0, 0}, //  6: South, East (Top-Left Corner)
        {0, 1}, //  7: North, South, East (Left Edge)
        {2, 3}, //  8: West
        {2, 2}, //  9: North, West (Bottom-Right Corner)
        {2, 0}, // 10: South, West (Top-Right Corner)
        {2, 1}, // 11: North, South, West (Right Edge)
        {1, 3}, // 12: East, West
        {1, 2}, // 13: North, East, West (Bottom Edge)
        {1, 0}, // 14: South, East, West (Top Edge)
        {1, 1}, // 15: North, South, East, West (Center)

        // --- ELEVATED LAYER (isLayered = 1) [Base cols + 6] ---
        {6, 1}, // 16: None
        {8, 2}, // 17: North
        {8, 0}, // 18: South
        {5, 1}, // 19: North, South
        {5, 3}, // 20: East
        {5, 2}, // 21: North, East (Bottom-Left Corner)
        {5, 0}, // 22: South, East (Top-Left Corner)
        {5, 1}, // 23: North, South, East (Left Edge)
        {7, 3}, // 24: West
        {7, 2}, // 25: North, West (Bottom-Right Corner)
        {7, 0}, // 26: South, West (Top-Right Corner)
        {7, 1}, // 27: North, South, West (Right Edge)
        {6, 3}, // 28: East, West
        {6, 2}, // 29: North, East, West (Bottom Edge)
        {6, 0}, // 30: South, East, West (Top Edge)
        {6, 1}  // 31: North, South, East, West (Center)
    };

    for (int layer = 1; layer <= 2; ++layer) {
        for (int y = 0; y < MAP_HEIGHT; ++y) {
            for (int x = 0; x < MAP_WIDTH; ++x) {

                int tileID = mapData[y][x];

                if (tileID == 0)
                    continue;
                if (layer == 2 && tileID == 1)
                    continue;

                int sheetCol = 1;
                int sheetRow = 1;

                if (tileID == 3) {
                    if (layer == 2)
                        continue;
                    sheetCol = 0;
                    sheetRow = 5;
                } else {
                    bool hasNorth = isLand(x, y - 1, layer);
                    bool hasSouth = isLand(x, y + 1, layer);
                    bool hasEast = isLand(x + 1, y, layer);
                    bool hasWest = isLand(x - 1, y, layer);

                    // True if we are drawing the elevated layer (Layer 2)
                    bool isLayered = (layer == 2);

                    // Create the 5-bit mask!
                    int bitmask = hasNorth | (hasSouth << 1) | (hasEast << 2) | (hasWest << 3) |
                                  (isLayered << 4);

                    // Fetch the coordinates directly from our Table!
                    sheetCol = bitmaskToTile[bitmask].col;
                    sheetRow = bitmaskToTile[bitmask].row;
                }

                auto tileEntity = manager.addEntity();
                Rectangle sourceRect = {
                    (float)sheetCol * TILE_SIZE, (float)sheetRow * TILE_SIZE, TILE_SIZE, TILE_SIZE};

                float visualYOffset = (layer == 2) ? -16.0f : 0.0f;
                Vector2 position = {(float)x * TILE_SIZE, ((float)y * TILE_SIZE) + visualYOffset};

                TileComponent tileComp = {.sourceRect = sourceRect, .worldPos = position};

                manager.addComponent<TileComponent>(tileEntity, &tileComp);
                manager.addComponent<SolidWallTag>(tileEntity);
            }
        }
    }
}
