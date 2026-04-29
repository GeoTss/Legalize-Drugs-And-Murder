#include "obj/GameSystems.hpp"
#include "obj/ECS/Manager.hpp"
#include "obj/ECS/View.hpp"
#include "obj/ECS/Component.hpp"
#include "obj/GameDefines.hpp"

// Make sure to include your command buffer!
#include "obj/ECS/CommandBuffer.hpp" 

namespace GameSystems {

void UpdateInput(Manager &manager) {
    manager.runSystem<PlayerInput>([](EntityId entity, PlayerInput &input) {
        input = {0};
        if (IsKeyDown(KEY_W)) input.pressed_W = 1;
        if (IsKeyDown(KEY_S)) input.pressed_S = 1;
        if (IsKeyDown(KEY_A)) input.pressed_A = 1;
        if (IsKeyDown(KEY_D)) input.pressed_D = 1;
        if (IsKeyDown(KEY_E)) input.pressed_E = 1;
    });
}

void UpdatePlayerLogic(Manager &manager, float dt) {
    DeferredCommandBuffer cmd(manager);

    // 3. Attack state completion
    manager.runSystem<StateComponent, AttackingStateTag, AnimationCompleteTag>(
        [&manager, &cmd](EntityId entity, StateComponent &state) {
            state.stateID = (uint8_t)hunterStates::IDLE;
            
            cmd.removeComponent<AttackingStateTag>(entity);
            cmd.addComponent<IdleStateTag>(entity);
            cmd.removeComponent<AnimationCompleteTag>(entity);

            auto anim = manager.getComponent<AnimationStateComponent>(entity);
            if (anim) { anim->currentFrame = 0; anim->stateTimer = 0.0f; }
        });

    // 4. Damage state completion
    manager.runSystem<StateComponent, DamagedStateTag, AnimationCompleteTag>(
        [&manager, &cmd](EntityId entity, StateComponent &state) {
            state.stateID = (uint8_t)enemyStates::IDLE;

            cmd.removeComponent<DamagedStateTag>(entity);
            cmd.addComponent<IdleStateTag>(entity);
            cmd.removeComponent<AnimationCompleteTag>(entity);

            auto anim = manager.getComponent<AnimationStateComponent>(entity);
            if (anim) { anim->currentFrame = 0; anim->stateTimer = 0.0f; }
        });

    // 1. Input state transitions
    manager.runSystem<PlayerInput, StateComponent>(
        [&manager, &cmd](EntityId entity, PlayerInput &input, StateComponent &state) {
            uint8_t targetState = (uint8_t)hunterStates::IDLE;

            if (input.pressed_W || input.pressed_S || input.pressed_A || input.pressed_D) {
                targetState = (uint8_t)hunterStates::RUNNING;
            }
            if (input.pressed_E) {
                targetState = (uint8_t)hunterStates::ATTACKING;
            }

            if (state.stateID != targetState) {
                // Remove old tags via Command Buffer
                switch (state.stateID) {
                    case (uint8_t)hunterStates::IDLE: cmd.removeComponent<IdleStateTag>(entity); break;
                    case (uint8_t)hunterStates::RUNNING: cmd.removeComponent<RunningStateTag>(entity); break;
                    case (uint8_t)hunterStates::ATTACKING: cmd.removeComponent<AttackingStateTag>(entity); break;
                }

                // Add new tags via Command Buffer
                switch (targetState) {
                    case (uint8_t)hunterStates::IDLE: cmd.addComponent<IdleStateTag>(entity); break;
                    case (uint8_t)hunterStates::RUNNING: cmd.addComponent<RunningStateTag>(entity); break;
                    case (uint8_t)hunterStates::ATTACKING: cmd.addComponent<AttackingStateTag>(entity); break;
                }

                // Modifying data in-place is safe!
                state.stateID = targetState;
                auto anim = manager.getComponent<AnimationStateComponent>(entity);
                if (anim) {
                    anim->currentFrame = 0;
                    anim->stateTimer = 0.0f;
                }
            }
        });

    // 2. Player Movement (Pure data mutation, no command buffer needed)
    manager.runSystem<PlayerInput, TransformComponent, StatsComponent, RunningStateTag>(
        [dt](EntityId entity, PlayerInput &input, TransformComponent &transform, StatsComponent &stats) {
            if (input.pressed_W) { transform.pos.y -= stats.speed * dt; }
            if (input.pressed_S) { transform.pos.y += stats.speed * dt; }
            if (input.pressed_A) { transform.pos.x -= stats.speed * dt; transform.facingDirection = -1; }
            if (input.pressed_D) { transform.pos.x += stats.speed * dt; transform.facingDirection = 1; }
        });

    // Apply all queued structural changes
    cmd.execute();
}

void UpdateEnemyLogic(Manager &manager, float dt, Vector2 playerPos) {
    DeferredCommandBuffer cmd(manager);

    manager.runSystem<TransformComponent, StateComponent, StatsComponent, EnemyTag>(
        [playerPos, dt, &manager, &cmd](EntityId entity, TransformComponent &transform, StateComponent &state, StatsComponent &stats) {
            if (manager.has_component<DamagedStateTag>(entity) || manager.has_component<AttackingStateTag>(entity)) {
                return;
            }

            float aggroRadius = 400.0f;
            float attackRange = 50.0f;
            float dx = playerPos.x - transform.pos.x;
            float dy = playerPos.y - 40.f - transform.pos.y;
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
                    case (uint8_t)enemyStates::IDLE: cmd.removeComponent<IdleStateTag>(entity); break;
                    case (uint8_t)enemyStates::RUNNING: cmd.removeComponent<RunningStateTag>(entity); break;
                    case (uint8_t)enemyStates::ATTACKING: cmd.removeComponent<AttackingStateTag>(entity); break;
                }

                switch (targetState) {
                    case (uint8_t)enemyStates::IDLE: cmd.addComponent<IdleStateTag>(entity); break;
                    case (uint8_t)enemyStates::RUNNING: cmd.addComponent<RunningStateTag>(entity); break;
                    case (uint8_t)enemyStates::ATTACKING: cmd.addComponent<AttackingStateTag>(entity); break;
                }

                state.stateID = targetState;
                auto anim = manager.getComponent<AnimationStateComponent>(entity);
                if (anim != nullptr) { anim->currentFrame = 0; anim->stateTimer = 0.0f; }
            }
        });

    cmd.execute();
}

template <typename... EventTags, typename Func>
void updateEvents(Manager &manager, const Func &&callback) {
    auto view = manager.view<AnimationEventComponent, EventTags...>();
    for (auto entity : view) { callback(manager, entity); }
}

void UpdateCombatAndHitboxes(Manager &manager, float dt, std::chrono::steady_clock::time_point nowTime) {
    DeferredCommandBuffer cmd(manager);

    // 1. Hitbox Spawning logic
    updateEvents<SpawnHitboxEvent>(manager, [&cmd, nowTime](Manager &m, const EntityId eventEntity) {
        auto eventInfo = m.getComponent<AnimationEventComponent>(eventEntity);
        auto hitboxInfo = m.getComponent<SpawnHitboxEvent>(eventEntity);
        EntityId entity = eventInfo->sourceEntity;

        auto transformComp = m.getComponent<TransformComponent>(entity);
        auto statsComp = m.getComponent<StatsComponent>(entity);
        if (!transformComp || !statsComp) return;

        float dirMultiplier = (transformComp->facingDirection == -1) ? -1.0f : 1.0f;
        float attackCenterX = transformComp->pos.x + (hitboxInfo->offsetX * dirMultiplier);
        float attackCenterY = transformComp->pos.y + hitboxInfo->offsetY;

        float finalWidth = hitboxInfo->width * statsComp->hitboxScale;
        float finalHeight = hitboxInfo->height * statsComp->hitboxScale;
        float spawnX = attackCenterX - (finalWidth / 2.0f);
        float spawnY = attackCenterY - (finalHeight / 2.0f);

        // Safely generate a new ID. The components won't be mapped until cmd.execute()
        auto hitboxEntity = m.addEntity(); 

        HitboxComponent hitboxComp = {
            .srcEntity = entity, .x = spawnX, .y = spawnY,
            .width = finalWidth, .height = finalHeight,
            .offsetX = hitboxInfo->offsetX, .offsetY = hitboxInfo->offsetY,
            .damage = statsComp->attackPower, .attached = hitboxInfo->attached
        };

        LifespanComponent lifespanComp = {.startPoint = nowTime, .duration = hitboxInfo->duration};

        // Defer all structural additions
        if (m.has_component<MainPlayerTag>(entity)) {
            cmd.addComponent<DamageEnemiesTag>(hitboxEntity);
        } else {
            cmd.addComponent<DamageCharacterTag>(hitboxEntity);
        }
        
        cmd.addComponent<LifespanComponent>(hitboxEntity, lifespanComp);
        cmd.addComponent<HitboxComponent>(hitboxEntity, hitboxComp);
    });

    // 2. Hitbox Collision system
    manager.runSystem<HitboxComponent, DamageEnemiesTag>(
        [&manager, &cmd](EntityId hitboxEntity, HitboxComponent &hitbox) {
            auto enemyView = manager.view<EnemyTag>();

            for (auto enemy : enemyView) {
                auto enemyTransform = manager.getComponent<TransformComponent>(enemy);
                if (enemyTransform == nullptr) continue;

                float actualBodyWidth = 40.0f;
                float actualBodyHeight = 40.0f;

                float enemyLeft = enemyTransform->pos.x - (actualBodyWidth / 2.0f);
                float enemyRight = enemyTransform->pos.x + (actualBodyWidth / 2.0f);
                float enemyTop = enemyTransform->pos.y - (actualBodyHeight / 2.0f);
                float enemyBottom = enemyTransform->pos.y + (actualBodyHeight / 2.0f);

                float hitboxRight = hitbox.x + hitbox.width;
                float hitboxBottom = hitbox.y + hitbox.height;

                bool overlapX = hitbox.x < enemyRight && hitboxRight > enemyLeft;
                bool overlapY = hitbox.y < enemyBottom && hitboxBottom > enemyTop;

                if (overlapX && overlapY && !manager.has_component<DamagedStateTag>(enemy)) {
                    // Queue structural state changes via Command Buffer
                    cmd.removeComponent<IdleStateTag>(enemy);
                    cmd.removeComponent<RunningStateTag>(enemy);
                    cmd.removeComponent<AttackingStateTag>(enemy);
                    cmd.addComponent<DamagedStateTag>(enemy);

                    auto state = manager.getComponent<StateComponent>(enemy);
                    if (state) state->stateID = (uint8_t)enemyStates::DAMAGED;

                    auto enemyHealth = manager.getComponent<HealthComponent>(enemy);
                    if (enemyHealth != nullptr) {
                        enemyHealth->health -= hitbox.damage;
                    }
                }
            }
        });

    cmd.execute();
}

void Render(Manager &manager, Camera2D &camera, Texture2D water, Texture2D tileset) {
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < MAP_WIDTH; ++x) {
            DrawTexture(water, x * 64, y * 64, WHITE);
        }
    }

    manager.runSystem<TileComponent>([&tileset](EntityId entity, TileComponent &tile) {
        DrawTextureRec(tileset, tile.sourceRect, tile.worldPos, WHITE);
    });

    manager.runSystem<TransformComponent>([](EntityId entity, TransformComponent &transform) {
        float bodyWidth = 40.0f;
        float bodyHeight = 40.0f;
        float topLeftX = transform.pos.x - (bodyWidth / 2.0f);
        float topLeftY = transform.pos.y - (bodyHeight / 2.0f);
        DrawRectangleLines(topLeftX, topLeftY, bodyWidth, bodyHeight, GREEN);
        DrawLine(transform.pos.x - 5, transform.pos.y, transform.pos.x + 5, transform.pos.y, BLUE);
        DrawLine(transform.pos.x, transform.pos.y - 5, transform.pos.x, transform.pos.y + 5, BLUE);
    });

    manager.runSystem<HitboxComponent>([](EntityId entity, HitboxComponent &hitbox) {
        DrawRectangleLines(hitbox.x, hitbox.y, hitbox.width, hitbox.height, RED);
    });
}

void Cleanup(Manager &manager, float dt) {
    DeferredCommandBuffer cmd(manager);

    manager.runSystem<HealthComponent>(
        [&cmd](EntityId entity, HealthComponent &healthComp) {
            if (healthComp.health <= 0.f) cmd.destroyEntity(entity);
        });

    manager.runSystem<LifespanComponent>(
        [&cmd, dt](EntityId entity, LifespanComponent &lifespan) {
            lifespan.duration -= dt;
            if (lifespan.duration <= 0.f) cmd.destroyEntity(entity);
        });

    manager.runSystem<AnimationEventComponent>(
        [&cmd](EntityId eventEntity, AnimationEventComponent &eventComp) {
            cmd.destroyEntity(eventEntity);
        });

    cmd.execute();
}

} // namespace GameSystems