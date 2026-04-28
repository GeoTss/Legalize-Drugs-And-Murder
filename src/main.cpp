#include <iostream>
#include <cassert>
#include <vector>

#include "obj/ECS/Manager.hpp"
#include "obj/HitboxAttachmentSystem.hpp"
#include "obj/PerlinNoise.hpp"
#include "obj/SpriteManager.hpp"

#include <iostream>
#include <memory>
#include <raylib.h>
#include <raymath.h>

enum struct hunterStates : uint8_t { IDLE, RUNNING, DAMAGED, ATTACKING };

enum struct enemyStates : uint8_t { IDLE, RUNNING, DAMAGED, ATTACKING };

enum struct eventIDs { LIGHT_ATTACK, LIGHTNING };

void initializeCharacterAnimations(Manager &manager,
                                   SpriteManager &spriteManager,
                                   EventDispatcher &eventDispatcher) {
    spriteManager.createProfile("main");

    AnimationTrack idleTrack{ASSET_PATH "/Blind-Huntress/1 - Idle.png", 240, 128, 0, 0, {}, 0.1f, true};

    spriteManager.addAnimationTrack("main", std::move(idleTrack), (uint32_t)hunterStates::IDLE);

    AnimationTrack runningTrack(ASSET_PATH "/Blind-Huntress/2 - Run.png", 240, 128, 0, 0, {}, 0.1f, true);

    spriteManager.addAnimationTrack(
        "main", std::move(runningTrack), (uint32_t)hunterStates::RUNNING);

    std::unordered_map<uint32_t, std::vector<uint64_t>> lightAttackEvents;
    lightAttackEvents[0] = {};

    SpawnHitboxEvent lightAttackHitbox = {.width = 90,
                                          .height = 20,
                                          .offsetX = 45,
                                          .offsetY = 0,
                                          .duration = 0.200f,
                                          .attached = true};

    auto lightAttackEvent =
        eventDispatcher.registerPayloadEvent<SpawnHitboxEvent>(lightAttackHitbox);

    lightAttackEvents[0].push_back(lightAttackEvent);

    AnimationTrack lightAttackTrack{
        ASSET_PATH "/Blind-Huntress/10 - attack 1.png", 240, 128, 0, 0, lightAttackEvents, 0.1f, false};

    spriteManager.addAnimationTrack(
        "main", std::move(lightAttackTrack), (uint32_t)hunterStates::ATTACKING);
}

void initializeEnemyAnimations(Manager &manager,
                               SpriteManager &spriteManager,
                               EventDispatcher &eventDispatcher) {

    spriteManager.createProfile("enemy");

    AnimationTrack idleTrack{ASSET_PATH "/stormhead/idle.png", 119, 124, 7, 40, {}, 0.1f, true};
    spriteManager.addAnimationTrack("enemy", std::move(idleTrack), (uint32_t)enemyStates::IDLE);

    AnimationTrack runningTrack{ASSET_PATH "/stormhead/run.png", 119, 124, 7, 40, {}, 0.1f, true};
    spriteManager.addAnimationTrack("enemy", std::move(idleTrack), (uint32_t)enemyStates::RUNNING);

    AnimationTrack damagedTrack{ASSET_PATH "/stormhead/damaged.png", 119, 124, 7, 40, {}, 0.1f, false};
    spriteManager.addAnimationTrack(
        "enemy", std::move(damagedTrack), (uint32_t)enemyStates::DAMAGED);

    std::unordered_map<uint32_t, std::vector<uint64_t>> enemyAttackEvents;

    // Frame 7: The first lightning strike hits the ground
    enemyAttackEvents[7] = {};

    // We make the hitbox tall and thin like a lightning bolt,
    // pushed to the side of the enemy, and detached so it stays on the ground!
    SpawnHitboxEvent lightningHitbox = {.width = 40,
                                        .height = 60,
                                        .offsetX = 40,
                                        .offsetY = 0,
                                        .duration = 0.200f,
                                        .attached = false};

    auto lightningEvent = eventDispatcher.registerPayloadEvent<SpawnHitboxEvent>(lightningHitbox);

    enemyAttackEvents[7].push_back(lightningEvent);

    // Frame 11: The secondary lightning strike (Optional, but matches the sprite sheet!)
    enemyAttackEvents[11] = {};

    SpawnHitboxEvent secondaryLightningHitbox = {.width = 40,
                                                 .height = 60,
                                                 .offsetX = -40,
                                                 .offsetY = 0,
                                                 .duration = 0.200f,
                                                 .attached = false};

    auto secondaryLightningEvent =
        eventDispatcher.registerPayloadEvent<SpawnHitboxEvent>(secondaryLightningHitbox);

    enemyAttackEvents[11].push_back(secondaryLightningEvent);

    // Register the track (Set looping to false so they don't machine-gun cast lightning)
    AnimationTrack attackTrack{
        ASSET_PATH "/stormhead/attack.png", 119, 124, 7, 40, enemyAttackEvents, 0.1f, false};

    spriteManager.addAnimationTrack(
        "enemy", std::move(attackTrack), (uint32_t)enemyStates::ATTACKING);
}

void testSwapAndPop(Manager& m) {
    std::cout << "[TEST] Swap and Pop (Component Removal)... ";
    
    EntityId e1 = m.addEntity();
    EntityId e2 = m.addEntity();
    EntityId e3 = m.addEntity();

    Position p1{1.0f, 1.0f};
    Position p2{2.0f, 2.0f};
    Position p3{3.0f, 3.0f};

    // All entities land in the same Archetype Table
    m.addComponent<Position>(e1, &p1);
    m.addComponent<Position>(e2, &p2);
    m.addComponent<Position>(e3, &p3);

    // ==========================================
    // PHASE 1: GENERATE BASE GRASS (LAYER 1)
    // ==========================================
    float noiseScale = 0.1f;
    for (int y = 0; y < mapHeight; ++y) {
        for (int x = 0; x < mapWidth; ++x) {
            // Force edges to be water so we get an island, not a cut-off continent
            if (x < 2 || x > mapWidth - 3 || y < 2 || y > mapHeight - 3) {
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
        for (int y = 1; y < mapHeight - 1; ++y) {
            for (int x = 1; x < mapWidth - 1; ++x) {
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
    for (int y = 2; y < mapHeight - 2; ++y) {
        for (int x = 2; x < mapWidth - 2; ++x) {
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
        for (int y = 1; y < mapHeight - 1; ++y) {
            for (int x = 1; x < mapWidth - 1; ++x) {
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
        if (x < 0 || x >= mapWidth || y < 0 || y >= mapHeight)
            return false;
        if (mapData[y][x] == 3)
            return true; // Treat stairs as land
        return mapData[y][x] >= currentLayer;
    };

    // A clean struct to hold our spritesheet coordinates
    struct TileCoord { int col; int row; };

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
        for (int y = 0; y < mapHeight; ++y) {
            for (int x = 0; x < mapWidth; ++x) {

                int tileID = mapData[y][x];

                if (tileID == 0) continue;
                if (layer == 2 && tileID == 1) continue;

                int sheetCol = 1;
                int sheetRow = 1;

                if (tileID == 3) {
                    if (layer == 2) continue;
                    sheetCol = 0;
                    sheetRow = 5;
                } else {
                    bool hasNorth = isLand(x, y - 1, layer);
                    bool hasSouth = isLand(x, y + 1, layer);
                    bool hasEast  = isLand(x + 1, y, layer);
                    bool hasWest  = isLand(x - 1, y, layer);

                    // True if we are drawing the elevated layer (Layer 2)
                    bool isLayered = (layer == 2);

                    // Create the 5-bit mask!
                    int bitmask = hasNorth | (hasSouth << 1) | (hasEast << 2) | (hasWest << 3) | (isLayered << 4);

                    // Fetch the coordinates directly from our Table!
                    sheetCol = bitmaskToTile[bitmask].col;
                    sheetRow = bitmaskToTile[bitmask].row;
                }

                auto tileEntity = manager.addEntity();
                Rectangle sourceRect = {
                    (float)sheetCol * tileSize, (float)sheetRow * tileSize, tileSize, tileSize};

                float visualYOffset = (layer == 2) ? -16.0f : 0.0f;
                Vector2 position = {(float)x * tileSize, ((float)y * tileSize) + visualYOffset};
                
                TileComponent tileComp = {.sourceRect = sourceRect, .worldPos = position};

                manager.addComponent<TileComponent>(tileEntity, &tileComp);
                manager.addComponent<SolidWallTag>(tileEntity);
            }
        }
    }
}

int main() {
    std::cout << "Your asset path: " << ASSET_PATH << "\n";

    Manager manager;

    InitWindow(800, 600, "My ECS Game");

    SpriteManager spriteManager{};
    EventDispatcher eventDispatcher{};

    initializeCharacterAnimations(manager, spriteManager, eventDispatcher);
    initializeEnemyAnimations(manager, spriteManager, eventDispatcher);

    auto hunter = manager.addEntity();

    manager.addComponent<MainPlayerTag>(hunter);

    TransformComponent transformComp = {{500, 300}};
    HealthComponent healthComp = {.health = 100.f};
    StatsComponent stats = {.attackPower = 10.f, .hitboxScale = 1.f, .speed = 100.f};
    StateComponent stateComp = {.stateID = (uint8_t)hunterStates::IDLE};

    AnimationStateComponent animationComponent = {0};
    animationComponent.profile = spriteManager.getProfile("main");

    manager.addComponents<TransformComponent,
                          HealthComponent,
                          StatsComponent,
                          StateComponent,
                          AnimationStateComponent>(
        hunter, &transformComp, &healthComp, &stats, &stateComp, &animationComponent);

    manager.addComponent<PlayerInput>(hunter);
    manager.addComponent<IdleStateTag>(hunter);

    auto enemy = manager.addEntity();

    manager.addComponent<EnemyTag>(enemy);

    TransformComponent enemyTransformComp = {{700, 100}};
    HealthComponent enemyHealthComp = {.health = 100.f};
    StatsComponent enemyStats = {.attackPower = 10.f, .hitboxScale = 1.f, .speed = 35.f};
    StateComponent enemyStateComp = {.stateID = (uint8_t)enemyStates::IDLE};

    AnimationStateComponent enemyAnimationComponent = {0};
    enemyAnimationComponent.profile = spriteManager.getProfile("enemy");

    manager.addComponents<TransformComponent,
                          HealthComponent,
                          StatsComponent,
                          StateComponent,
                          AnimationStateComponent>(enemy,
                                                   &enemyTransformComp,
                                                   &enemyHealthComp,
                                                   &enemyStats,
                                                   &enemyStateComp,
                                                   &enemyAnimationComponent);
    manager.addComponent<IdleStateTag>(enemy);

    std::cout << "Initialized entities.\n";
    std::cout << "Character id: " << hunter << "\n";
    std::cout << "Enemy id: " << enemy << "\n";

    SetTargetFPS(120);

    Texture2D tilesetTexture =
        LoadTexture(ASSET_PATH "/Tiny Swords (Free Pack)/Terrain/Tileset/Tilemap_color5.png");
    Texture2D waterTexture = LoadTexture(
        ASSET_PATH "/Tiny Swords (Free Pack)/Terrain/Tileset/Water Background color.png");
    Texture2D shadowTexture =
        LoadTexture(ASSET_PATH "/Tiny Swords (Free Pack)/Terrain/Tileset/Shadow.png");

    loadMap(manager, 1572);

    Camera2D camera = {0};
    camera.offset = {800.0f / 2.0f, 600.0f / 2.0f};
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        auto nowTime = std::chrono::steady_clock::now();

        auto hunterTransform = manager.getComponent<TransformComponent>(hunter);
        if (hunterTransform != nullptr) {

            float mapPixelWidth = mapWidth * 64.0f;
            float mapPixelHeight = mapHeight * 64.0f;

            float screenWidth = 800.0f;
            float screenHeight = 600.0f;

            float halfScreenW = (screenWidth / 2.0f) / camera.zoom;
            float halfScreenH = (screenHeight / 2.0f) / camera.zoom;

            Vector2 desiredTarget = hunterTransform->pos;

            desiredTarget.x = std::clamp(desiredTarget.x, halfScreenW, mapPixelWidth - halfScreenW);
            desiredTarget.y =
                std::clamp(desiredTarget.y, halfScreenH, mapPixelHeight - halfScreenH);

            camera.target = desiredTarget;
        }

        manager.runSystem<PlayerInput>([](EntityId entity, PlayerInput &input) {
            input = {0};

            if (IsKeyDown(KEY_W)) {
                input.pressed_W = 1;
            }
            if (IsKeyDown(KEY_S)) {
                input.pressed_S = 1;
            }
            if (IsKeyDown(KEY_A)) {
                input.pressed_A = 1;
            }
            if (IsKeyDown(KEY_D)) {
                input.pressed_D = 1;
            }
            if (IsKeyDown(KEY_E)) {
                input.pressed_E = 1;
            }
        });
        // std::cout << "Ran input system.\n";

        manager.runSystem<PlayerInput, StateComponent>(
            [&manager](EntityId entity, PlayerInput &input, StateComponent &state) {
                uint8_t targetState = (uint8_t)hunterStates::IDLE;

                if (input.pressed_W || input.pressed_S || input.pressed_A || input.pressed_D) {
                    targetState = (uint8_t)hunterStates::RUNNING;
                }

                if (input.pressed_E) {
                    targetState = (uint8_t)hunterStates::ATTACKING;
                }

                if (state.stateID != targetState) {

                    switch (state.stateID) {
                    case (uint8_t)hunterStates::IDLE:
                        manager.removeComponent<IdleStateTag>(entity);
                        break;
                    case (uint8_t)hunterStates::RUNNING:
                        manager.removeComponent<RunningStateTag>(entity);
                        break;
                    case (uint8_t)hunterStates::ATTACKING:
                        manager.removeComponent<AttackingStateTag>(entity);
                        break;
                    }

                    switch (targetState) {
                    case (uint8_t)hunterStates::IDLE:
                        manager.addComponent<IdleStateTag>(entity);
                        break;
                    case (uint8_t)hunterStates::RUNNING:
                        manager.addComponent<RunningStateTag>(entity);
                        break;
                    case (uint8_t)hunterStates::ATTACKING:
                        manager.addComponent<AttackingStateTag>(entity);
                        break;
                    }

                    state.stateID = targetState;

                    auto anim = manager.getComponent<AnimationStateComponent>(entity);
                    if (anim) {
                        anim->currentFrame = 0;
                        anim->stateTimer = 0.0f;
                    }
                }
            });
        // std::cout << "Ran state system\n";

        manager.runSystem<MainPlayerTag, IdleStateTag>([]() {
            // Maybe do stuff
        });

        manager.runSystem<PlayerInput, TransformComponent, StatsComponent, RunningStateTag>(
            [dt](EntityId entity,
                 PlayerInput &input,
                 TransformComponent &transform,
                 StatsComponent &stats) {
                if (input.pressed_W) {
                    transform.pos.y -= stats.speed * dt;
                }
                if (input.pressed_S) {
                    transform.pos.y += stats.speed * dt;
                }
                if (input.pressed_A) {
                    transform.pos.x -= stats.speed * dt;
                    transform.facingDirection = -1;
                }
                if (input.pressed_D) {
                    transform.pos.x += stats.speed * dt;
                    transform.facingDirection = 1;
                }
            });

        manager.runSystem<MainPlayerTag, AttackingStateTag>([]() {
            // Maybe do stuff
        });

        hunterTransform = manager.getComponent<TransformComponent>(hunter);
        if (hunterTransform != nullptr) {
            Vector2 hunterPos = hunterTransform->pos;

            manager.runSystem<TransformComponent, StateComponent, StatsComponent, EnemyTag>(
                [hunterPos, dt, &manager](EntityId entity,
                                          TransformComponent &transform,
                                          StateComponent &state,
                                          StatsComponent &stats) {
                    if (manager.has_component<DamagedStateTag>(entity) ||
                        manager.has_component<AttackingStateTag>(entity)) {
                        return;
                    }

                    float aggroRadius = 400.0f;
                    float attackRange = 50.0f;

                    float dx = hunterPos.x - transform.pos.x;
                    float dy = hunterPos.y - 40.f - transform.pos.y;
                    float distance = std::sqrt(dx * dx + dy * dy);

                    uint8_t targetState = (uint8_t)enemyStates::IDLE;

                    if (distance < aggroRadius && distance > attackRange) {
                        targetState = (uint8_t)enemyStates::RUNNING;

                        float dirX = dx / distance;
                        float dirY = dy / distance;

                        transform.pos.x += dirX * stats.speed * dt;
                        transform.pos.y += dirY * stats.speed * dt;

                        transform.facingDirection = (dirX < 0) ? -1 : 1;

                    } else if (distance <= attackRange) {
                        targetState = (uint8_t)enemyStates::ATTACKING;
                    }

                    if (state.stateID != targetState) {

                        switch (state.stateID) {
                        case (uint8_t)enemyStates::IDLE:
                            manager.removeComponent<IdleStateTag>(entity);
                            break;
                        case (uint8_t)enemyStates::RUNNING:
                            manager.removeComponent<RunningStateTag>(entity);
                            break;
                        case (uint8_t)enemyStates::ATTACKING:
                            manager.removeComponent<AttackingStateTag>(entity);
                            break;
                        }

                        switch (targetState) {
                        case (uint8_t)enemyStates::IDLE:
                            manager.addComponent<IdleStateTag>(entity);
                            break;
                        case (uint8_t)enemyStates::RUNNING:
                            manager.addComponent<RunningStateTag>(entity);
                            break;
                        case (uint8_t)enemyStates::ATTACKING:
                            manager.addComponent<AttackingStateTag>(entity);
                            break;
                        }

                        state.stateID = targetState;

                        auto anim = manager.getComponent<AnimationStateComponent>(entity);
                        if (anim != nullptr) {
                            anim->currentFrame = 0;
                            anim->stateTimer = 0.0f;
                        }
                    }
                });
        }

        // HitboxAttachmentSystem::update(manager);

        BeginDrawing();

        ClearBackground({20, 160, 210, 255});

        BeginMode2D(camera);
        
        for (int y = 0; y < mapHeight; ++y) {
            for (int x = 0; x < mapWidth; ++x) {
                DrawTexture(waterTexture, x * 64, y * 64, WHITE);
            }
        }

        manager.runSystem<TileComponent>([&tilesetTexture](EntityId entity, TileComponent &tile) {
            DrawTextureRec(tilesetTexture, tile.sourceRect, tile.worldPos, WHITE);
        });

        AnimationSystem::update(manager, spriteManager, eventDispatcher, dt);

        updateEvents<SpawnHitboxEvent>(
            manager, [hunter, nowTime](Manager &manager, const EntityId eventEntity) {
                auto eventInfo = manager.getComponent<AnimationEventComponent>(eventEntity);
                auto hitboxInfo = manager.getComponent<SpawnHitboxEvent>(eventEntity);

                EntityId entity = eventInfo->sourceEntity;

                auto transformComp = manager.getComponent<TransformComponent>(entity);
                auto statsComp = manager.getComponent<StatsComponent>(entity);

                float dirMultiplier = (transformComp->facingDirection == -1) ? -1.0f : 1.0f;

                float attackCenterX = transformComp->pos.x + (hitboxInfo->offsetX * dirMultiplier);
                float attackCenterY = transformComp->pos.y + hitboxInfo->offsetY;

                float finalWidth = hitboxInfo->width * statsComp->hitboxScale;
                float finalHeight = hitboxInfo->height * statsComp->hitboxScale;

                float spawnX = attackCenterX - (finalWidth / 2.0f);
                float spawnY = attackCenterY - (finalHeight / 2.0f);

                auto hitboxEntity = manager.addEntity();
                HitboxComponent hitboxComp = {.srcEntity = entity,
                                              .x = spawnX,
                                              .y = spawnY,
                                              .width = finalWidth,
                                              .height = finalHeight,
                                              .offsetX = hitboxInfo->offsetX,
                                              .offsetY = hitboxInfo->offsetY,
                                              .damage = statsComp->attackPower,
                                              .attached = hitboxInfo->attached};

                LifespanComponent lifespanComp = {.startPoint = nowTime,
                                                  .duration = hitboxInfo->duration};
                if (entity == hunter) {
                    manager.addComponents<LifespanComponent, HitboxComponent, DamageEnemiesTag>(
                        hitboxEntity);
                } else {
                    manager.addComponents<LifespanComponent, HitboxComponent, DamageCharacterTag>(
                        hitboxEntity);
                }
                manager.setComponent<LifespanComponent>(hitboxEntity, lifespanComp);
                manager.setComponent<HitboxComponent>(hitboxEntity, hitboxComp);
            });
        
        manager.runSystem<TransformComponent>([](EntityId entity, TransformComponent &transform) {
            
            // Shrink the box so it tightly wraps the visual pixels, not the 128x128 empty space!
            float bodyWidth = 40.0f;
            float bodyHeight = 40.0f;

            // Convert the character's CENTER pos to a TOP-LEFT coordinate to draw the rectangle
            float topLeftX = transform.pos.x - (bodyWidth / 2.0f);
            float topLeftY = transform.pos.y - (bodyHeight / 2.0f);

            DrawRectangleLines(topLeftX, topLeftY, bodyWidth, bodyHeight, GREEN);
            
            // Draw a tiny crosshair exactly on their pos to prove it is the true center
            DrawLine(transform.pos.x - 5, transform.pos.y, transform.pos.x + 5, transform.pos.y, BLUE);
            DrawLine(transform.pos.x, transform.pos.y - 5, transform.pos.x, transform.pos.y + 5, BLUE);
        });

        manager.runSystem<HitboxComponent>([](EntityId entity, HitboxComponent &hitbox) {
            DrawRectangleLines(hitbox.x, hitbox.y, hitbox.width, hitbox.height, RED);
        });

        EndMode2D();

        DrawText(TextFormat("%d fps", GetFPS()), 10, 10, 25, WHITE);

        EndDrawing();

        manager.runSystem<HitboxComponent, DamageEnemiesTag>([&manager](EntityId hitboxEntity,
                                                                        HitboxComponent &hitbox) {
            auto enemyView = manager.view<EnemyTag>();

            for (auto enemy : enemyView) {
                auto enemyTransform = manager.getComponent<TransformComponent>(enemy);
                if (enemyTransform == nullptr)
                continue;
                
                float actualBodyWidth = 40.0f;  
                float actualBodyHeight = 40.0f; 
                
                // 1. Convert the centered position directly to a Top/Bottom/Left/Right Box
                // Since pos IS the center, we just step outwards by half the size in each direction.
                float enemyLeft   = enemyTransform->pos.x - (actualBodyWidth / 2.0f);
                float enemyRight  = enemyTransform->pos.x + (actualBodyWidth / 2.0f);
                float enemyTop    = enemyTransform->pos.y - (actualBodyHeight / 2.0f);
                float enemyBottom = enemyTransform->pos.y + (actualBodyHeight / 2.0f);

                // 2. Calculate the Hitbox's far edges
                // (Hitboxes are already top-left coordinates, so we just add the full width/height)
                float hitboxRight = hitbox.x + hitbox.width;
                float hitboxBottom = hitbox.y + hitbox.height;

                // 3. The precise AABB formula
                bool overlapX = hitbox.x < enemyRight && hitboxRight > enemyLeft;
                bool overlapY = hitbox.y < enemyBottom && hitboxBottom > enemyTop;

                if (overlapX && overlapY && !manager.has_component<DamagedStateTag>(enemy)) {

                    manager.removeComponent<IdleStateTag>(enemy);
                    manager.removeComponent<RunningStateTag>(enemy);
                    manager.removeComponent<AttackingStateTag>(enemy);

                    auto state = manager.getComponent<StateComponent>(enemy);
                    state->stateID = (uint8_t)enemyStates::DAMAGED;
                    manager.addComponent<DamagedStateTag>(enemy);

                    auto enemyHealth = manager.getComponent<HealthComponent>(enemy);
                    if (enemyHealth != nullptr) {
                        enemyHealth->health -= hitbox.damage;
                        std::cout << "Direct hit! Enemy health: " << enemyHealth->health << "\n";
                    }
                }
            }
        });

        manager.runSystem<StateComponent, AttackingStateTag, AnimationCompleteTag>(
            [&manager](EntityId entity, StateComponent &state) {
                state.stateID = (uint8_t)hunterStates::IDLE;
                manager.removeComponent<AttackingStateTag>(entity);
                manager.addComponent<IdleStateTag>(entity);

                manager.removeComponent<AnimationCompleteTag>(entity);

                auto anim = manager.getComponent<AnimationStateComponent>(entity);
                if (anim) {
                    anim->currentFrame = 0;
                    anim->stateTimer = 0.0f;
                }
            });

        manager.runSystem<StateComponent, DamagedStateTag, AnimationCompleteTag>(
            [&manager](EntityId entity, StateComponent &state) {
                state.stateID = (uint8_t)enemyStates::IDLE;

                manager.removeComponent<DamagedStateTag>(entity);
                manager.addComponent<IdleStateTag>(entity);

                manager.removeComponent<AnimationCompleteTag>(entity);

                auto anim = manager.getComponent<AnimationStateComponent>(entity);
                if (anim) {
                    anim->currentFrame = 0;
                    anim->stateTimer = 0.0f;
                }
            });

        std::vector<EntityId> entitiesToDestroy;

        manager.runSystem<HealthComponent>(
            [&entitiesToDestroy](EntityId entity, HealthComponent &healthComp) {
                if (healthComp.health <= 0.f)
                    entitiesToDestroy.push_back(entity);
            });

        manager.runSystem<LifespanComponent>(
            [&entitiesToDestroy, dt](EntityId timedEntity, LifespanComponent &lifespan) {
                lifespan.duration -= dt;

                if (lifespan.duration <= 0)
                    entitiesToDestroy.push_back(timedEntity);
            });

        manager.runSystem<AnimationEventComponent>(
            [&entitiesToDestroy](EntityId eventEntity, AnimationEventComponent &eventComp) {
                entitiesToDestroy.push_back(eventEntity);
            });

        for (auto entity : entitiesToDestroy) {
            manager.destroyEntity(entity);
        }
    }

    CloseWindow();
    return 0;
}