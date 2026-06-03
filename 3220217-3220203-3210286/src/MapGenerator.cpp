#include "obj/MapGenerator.hpp"
#include "obj/ECS/Component.hpp"
#include "obj/GameDefines.hpp"
#include "obj/PerlinNoise.hpp"

void loadMap(Manager &manager, size_t seed) {

    std::vector<std::vector<int>> mapData(MAP_HEIGHT, std::vector<int>(MAP_WIDTH, 0));
    PerlinNoise perlin(seed);

    float noiseScale = 0.1f;
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < MAP_WIDTH; ++x) {
            if (x < 2 || x > MAP_WIDTH - 3 || y < 2 || y > MAP_HEIGHT - 3) {
                mapData[y][x] = 0;
                continue;
            }
            float n = (perlin.noise((float)x * noiseScale, (float)y * noiseScale) + 1.0f) / 2.0f;
            mapData[y][x] = (n > 0.45f) ? 1 : 0;
        }
    }

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

                if (mapData[y][x] >= 1)
                    nextMap[y][x] = (neighbors >= 4) ? 1 : 0;
                else
                    nextMap[y][x] = (neighbors >= 5) ? 1 : 0;
            }
        }
        mapData = nextMap;
    }

    float hillNoiseScale = 0.15f;
    for (int y = 2; y < MAP_HEIGHT - 2; ++y) {
        for (int x = 2; x < MAP_WIDTH - 2; ++x) {
            if (mapData[y][x] == 1) {
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

    for (int pass = 0; pass < 2; ++pass) {
        std::vector<std::vector<int>> nextMap = mapData;
        for (int y = 1; y < MAP_HEIGHT - 1; ++y) {
            for (int x = 1; x < MAP_WIDTH - 1; ++x) {
                if (mapData[y][x] == 0)
                    continue;

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

    auto isLand = [&](int x, int y, int currentLayer) -> bool {
        if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT)
            return false;
        if (mapData[y][x] == 3)
            return true;
        return mapData[y][x] >= currentLayer;
    };

    struct TileCoord {
        int col;
        int row;
    };

    const TileCoord bitmaskToTile[32] = {
        {1, 1},
        {3, 2},
        {3, 0},
        {3, 1},
        {0, 3},
        {0, 2},
        {0, 0},
        {0, 1},
        {2, 3},
        {2, 2},
        {2, 0},
        {2, 1},
        {1, 3},
        {1, 2},
        {1, 0},
        {1, 1},

        {6, 1},
        {8, 2},
        {8, 0},
        {5, 1},
        {5, 3},
        {5, 2},
        {5, 0},
        {5, 1},
        {7, 3},
        {7, 2},
        {7, 0},
        {7, 1},
        {6, 3},
        {6, 2},
        {6, 0},
        {6, 1}
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

                    bool isLayered = (layer == 2);

                    int bitmask = hasNorth | (hasSouth << 1) | (hasEast << 2) | (hasWest << 3) |
                                  (isLayered << 4);

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
