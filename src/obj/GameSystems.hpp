#pragma once
#include "ECS/Manager.hpp"
#include "SpriteManager.hpp"
#include "EventDispatcher.hpp"
#include <raylib.h>
#include <chrono>

namespace GameSystems {
    void UpdateInput(Manager& manager);
    void UpdatePlayerLogic(Manager& manager, float dt);
    void UpdateEnemyLogic(Manager& manager, float dt, Vector2 playerPos);
    void UpdateCombatAndHitboxes(Manager& manager, float dt, std::chrono::steady_clock::time_point nowTime);
    void Render(Manager& manager, Camera2D& camera, Texture2D water, Texture2D tileset);
    void Cleanup(Manager& manager, float dt);
}