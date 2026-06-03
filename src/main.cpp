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
    // InitAudioDevice();
    SetTargetFPS(120);

    // Music music = LoadMusicStream(ASSET_PATH "/music/Unshaken  The Music of Red Dead Redemption 2 OST.mp3");
    // PlayMusicStream(music);

    Manager manager;
    SpriteManager spriteManager;
    EventDispatcher eventDispatcher;

    initializeCharacterAnimations(spriteManager, eventDispatcher);
    initializeEnemyAnimations(spriteManager, eventDispatcher);

    EntityId hunter = spawnPlayer(manager, spriteManager);

    loadMap(manager, 1572);

    Texture2D tilesetTexture =
        LoadTexture(ASSET_PATH "/Tiny Swords (Free Pack)/Terrain/Tileset/Tilemap_color5.png");
    Texture2D waterTexture = LoadTexture(
        ASSET_PATH "/Tiny Swords (Free Pack)/Terrain/Tileset/Water Background color.png");

    Camera2D camera = {0};
    camera.offset = {800.0f / 2.0f, 600.0f / 2.0f};
    camera.zoom = 1.0f;

    DeferredCommandBuffer cmd(manager);

    while (!WindowShouldClose()) {
        // UpdateMusicStream(music);
        float dt = GetFrameTime();
        auto nowTime = std::chrono::steady_clock::now();

        auto hunterTransform = manager.getComponent<TransformComponent>(hunter);
        if (hunterTransform) {
            GameSystems::updateCamera(camera, hunterTransform);
            GameSystems::UpdateSpawning(manager, spriteManager, dt, hunterTransform->pos);
        }

        GameSystems::UpdateInput(manager);
        GameSystems::UpdateAttributeCollisions(manager, cmd, hunter);
        GameSystems::UpdateAttributeModifiers(manager);
        GameSystems::UpdatePlayerLogic(manager, cmd, dt);
        GameSystems::UpdateProjectiles(manager, cmd, dt);
        GameSystems::UpdateElectricPath(manager, cmd, dt);

        if (hunterTransform) {
            GameSystems::UpdateEnemyLogic(manager, cmd, dt, hunterTransform->pos);
        }
        AnimationSystem::update(manager, spriteManager, eventDispatcher, dt);
        GameSystems::UpdateCombatAndHitboxes(manager, cmd, dt, nowTime);

        cmd.execute();

        GameSystems::Render(manager, camera, waterTexture, tilesetTexture);

        GameSystems::Cleanup(manager, cmd, dt);
    }

    // UnloadMusicStream(music);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}