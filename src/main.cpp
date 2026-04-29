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
#ifdef NDEBUG
    std::cout << "In Release mode.\n";
#else
    std::cout << "In Debug mode.\n";
#endif

    InitWindow(800, 600, "My ECS Game");
    SetTargetFPS(120);

    Manager manager;
    SpriteManager spriteManager;
    EventDispatcher eventDispatcher;

    // Initialization
    initializeCharacterAnimations(spriteManager, eventDispatcher);
    initializeEnemyAnimations(spriteManager, eventDispatcher);

    EntityId hunter = spawnPlayer(manager, spriteManager);
    for (int i = 0; i < 10; ++i) {
        EntityId enemy = spawnEnemy(manager, spriteManager, rand() % 700 + 500, rand() % 100 + 10);
    }

    loadMap(manager, 1572);

    
    Texture2D tilesetTexture =
        LoadTexture(ASSET_PATH "/Tiny Swords (Free Pack)/Terrain/Tileset/Tilemap_color5.png");
    Texture2D waterTexture = LoadTexture(
        ASSET_PATH "/Tiny Swords (Free Pack)/Terrain/Tileset/Water Background color.png");

    Camera2D camera = {0};
    camera.offset = {800.0f / 2.0f, 600.0f / 2.0f};
    camera.zoom = 1.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        auto nowTime = std::chrono::steady_clock::now();

        auto hunterTransform = manager.getComponent<TransformComponent>(hunter);
        if (hunterTransform) {
            GameSystems::updateCamera(camera, hunterTransform);
        }

        GameSystems::UpdateInput(manager);
        GameSystems::UpdatePlayerLogic(manager, dt);

        if (hunterTransform) {
            GameSystems::UpdateEnemyLogic(manager, dt, hunterTransform->pos);
        }
        AnimationSystem::update(manager, spriteManager, eventDispatcher, dt);
        GameSystems::UpdateCombatAndHitboxes(manager, dt, nowTime);

        GameSystems::Render(manager, camera, waterTexture, tilesetTexture);

        GameSystems::Cleanup(manager, dt);
    }

    CloseWindow();
    return 0;
}