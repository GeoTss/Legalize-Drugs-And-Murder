#pragma once
#include "ECS/Manager.hpp"
#include "ECS/CommandBuffer.hpp"
#include "SpriteManager.hpp"
#include "EventDispatcher.hpp"
#include <raylib.h>
#include <chrono>

namespace GameSystems {
    void updateCamera(Camera2D& camera, const TransformComponent* transform) noexcept;
    void UpdateInput(Manager& manager);
    void UpdatePlayerLogic(Manager& manager, DeferredCommandBuffer& cmd, float dt);
    void UpdateEnemyLogic(Manager& manager, DeferredCommandBuffer& cmd, float dt, Vector2 playerPos);
    void UpdateCombatAndHitboxes(Manager& manager, DeferredCommandBuffer& cmd, float dt, std::chrono::steady_clock::time_point nowTime);
    void Render(Manager& manager, Camera2D& camera, Texture2D water, Texture2D tileset);
    void Cleanup(Manager& manager, DeferredCommandBuffer& cmd, float dt);
}