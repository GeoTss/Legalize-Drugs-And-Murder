#include <iostream>
#include <raylib.h>
#include <raymath.h>

#include "obj/AnimationSystem.hpp"
#include "obj/ECS/Manager.hpp"
#include "obj/EventDispatcher.hpp"
#include "obj/SpriteManager.hpp"

#include "obj/EntitySetup.hpp"
#include "obj/GameDefines.hpp"
#include "obj/GameSystems.hpp"
#include "obj/MapGenerator.hpp"

int main() {
    InitWindow(800, 600, "My ECS Game");
    SetTargetFPS(120);

    Manager manager;
    SpriteManager spriteManager;
    EventDispatcher eventDispatcher;

    // Initialization
    initializeCharacterAnimations(spriteManager, eventDispatcher);
    initializeEnemyAnimations(spriteManager, eventDispatcher);

    EntityId hunter = spawnPlayer(manager, spriteManager);
    EntityId enemy = spawnEnemy(manager, spriteManager);

    loadMap(manager, 1572);

    // Load Assets
    Texture2D tilesetTexture =
        LoadTexture(ASSET_PATH "/Tiny Swords (Free Pack)/Terrain/Tileset/Tilemap_color5.png");
    Texture2D waterTexture = LoadTexture(
        ASSET_PATH "/Tiny Swords (Free Pack)/Terrain/Tileset/Water Background color.png");

    Camera2D camera = {0};
    camera.offset = {800.0f / 2.0f, 600.0f / 2.0f};
    camera.zoom = 1.0f;

    // --- GAME LOOP ---
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        auto nowTime = std::chrono::steady_clock::now();

        // Update Camera
        auto hunterTransform = manager.getComponent<TransformComponent>(hunter);
        if (hunterTransform) {
            float halfScreenW = (800.0f / 2.0f) / camera.zoom;
            float halfScreenH = (600.0f / 2.0f) / camera.zoom;
            camera.target.x = std::clamp(
                hunterTransform->pos.x, halfScreenW, (MAP_WIDTH * TILE_SIZE) - halfScreenW);
            camera.target.y = std::clamp(
                hunterTransform->pos.y, halfScreenH, (MAP_HEIGHT * TILE_SIZE) - halfScreenH);
        }

        // --- ECS PIPELINE ---
        GameSystems::UpdateInput(manager);
        GameSystems::UpdatePlayerLogic(manager, dt);

        if (hunterTransform) {
            GameSystems::UpdateEnemyLogic(manager, dt, hunterTransform->pos);
        }
        BeginDrawing();
        ClearBackground({20, 160, 210, 255});
        BeginMode2D(camera);
        GameSystems::Render(manager, camera, waterTexture, tilesetTexture);
        AnimationSystem::update(manager, spriteManager, eventDispatcher, dt);

        GameSystems::UpdateCombatAndHitboxes(manager, dt, nowTime);
        EndMode2D();
        DrawText(TextFormat("%d fps", GetFPS()), 10, 10, 25, WHITE);
        EndDrawing();
        GameSystems::Cleanup(manager, dt);
    }

    CloseWindow();
    return 0;
}