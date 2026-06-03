#include "obj/GameSystems.hpp"
#include "obj/AnimationSystem.hpp"
#include "obj/ECS/CommandBuffer.hpp"
#include "obj/ECS/Component.hpp"
#include "obj/ECS/Manager.hpp"
#include "obj/ECS/View.hpp"
#include "obj/GameDefines.hpp"
#include "obj/EntitySetup.hpp"
#include <raymath.h>

namespace GameSystems {

void updateCamera(Camera2D &camera, const TransformComponent *transform) noexcept {
    float halfScreenW = (800.0f / 2.0f) / camera.zoom;
    float halfScreenH = (600.0f / 2.0f) / camera.zoom;
    camera.target.x =
        std::clamp(transform->pos.x, halfScreenW, (MAP_WIDTH * TILE_SIZE) - halfScreenW);
    camera.target.y =
        std::clamp(transform->pos.y, halfScreenH, (MAP_HEIGHT * TILE_SIZE) - halfScreenH);
}

void UpdateInput(Manager &manager) {
    manager.runSystem<PlayerInput, DifficultyComponent>([](EntityId entity, PlayerInput &input, DifficultyComponent &diff) {
        input = {0};
        if (IsKeyDown(KEY_W))
            input.pressed_W = 1;
        if (IsKeyDown(KEY_S))
            input.pressed_S = 1;
        if (IsKeyDown(KEY_A))
            input.pressed_A = 1;
        if (IsKeyDown(KEY_D))
            input.pressed_D = 1;
        if (IsKeyDown(KEY_E))
            input.pressed_E = 1;
        if (IsKeyDown(KEY_R))
            input.pressed_R = 1;
        if (IsKeyDown(KEY_LEFT_SHIFT))
            input.pressed_Shift = 1;
        
        if (IsKeyPressed(KEY_G)) {
            input.pressed_G = 1;
            diff.debugMode = !diff.debugMode;
        }
    });
}

void UpdateAttributeModifiers(Manager &manager) {
    manager.runSystem<StatBuffer>([](StatBuffer &buffer) {
        buffer.attackMult = 1.f;
        buffer.speedMult = 1.f;
        buffer.scaleMult = 1.f;
        buffer.lifesteal = 0.0f;
        buffer.defense = 0.0f;
    });

    manager.runSystem<StatModifier>([&manager](StatModifier &mod) {
        if (mod.active) {
            if (auto *targetBuffer = manager.getComponent<StatBuffer>(mod.target)) {
                if (mod.field == &StatBuffer::lifesteal || mod.field == &StatBuffer::defense) {
                    targetBuffer->*mod.field += mod.multiplier;
                } else {
                    targetBuffer->*mod.field *= mod.multiplier;
                }
            }
        }
    });
}

void UpdatePlayerLogic(Manager &manager, DeferredCommandBuffer &cmd, float dt) {
    manager.runSystem<StateComponent, AttackingStateTag, AnimationCompleteTag>(
        [&manager, &cmd](EntityId entity, StateComponent &state) {
            state.stateID = (uint8_t)hunterStates::IDLE;

            cmd.removeComponent<AttackingStateTag>(entity);
            cmd.addComponent<IdleStateTag>(entity);
            cmd.removeComponent<AnimationCompleteTag>(entity);

            auto anim = manager.getComponent<AnimationStateComponent>(entity);
            if (anim) {
                anim->currentFrame = 0;
                anim->stateTimer = 0.0f;
            }
        });

    manager.runSystem<StateComponent, HeavyAttackingStateTag, AnimationCompleteTag>(
        [&manager, &cmd](EntityId entity, StateComponent &state) {
            state.stateID = (uint8_t)hunterStates::IDLE;

            cmd.removeComponent<HeavyAttackingStateTag>(entity);
            cmd.addComponent<IdleStateTag>(entity);
            cmd.removeComponent<AnimationCompleteTag>(entity);

            auto anim = manager.getComponent<AnimationStateComponent>(entity);
            if (anim) {
                anim->currentFrame = 0;
                anim->stateTimer = 0.0f;
            }
        });

    manager.runSystem<StateComponent, DamagedStateTag, AnimationCompleteTag>(
        [&manager, &cmd](EntityId entity, StateComponent &state) {
            state.stateID = (uint8_t)hunterStates::IDLE;

            cmd.removeComponent<DamagedStateTag>(entity);
            cmd.addComponent<IdleStateTag>(entity);
            cmd.removeComponent<AnimationCompleteTag>(entity);

            auto anim = manager.getComponent<AnimationStateComponent>(entity);
            if (anim) {
                anim->currentFrame = 0;
                anim->stateTimer = 0.0f;
            }
        });

    manager.runSystem<StateComponent, DashingStateTag, AnimationCompleteTag>(
        [&manager, &cmd](EntityId entity, StateComponent &state) {
            state.stateID = (uint8_t)hunterStates::IDLE;

            cmd.removeComponent<DashingStateTag>(entity);
            cmd.addComponent<IdleStateTag>(entity);
            cmd.removeComponent<AnimationCompleteTag>(entity);

            auto anim = manager.getComponent<AnimationStateComponent>(entity);
            if (anim) {
                anim->currentFrame = 0;
                anim->stateTimer = 0.0f;
            }
        });

    manager.runSystem<PlayerInput, StateComponent, TransformComponent, DashComponent>(
        [&manager, &cmd](EntityId entity, PlayerInput &input, StateComponent &state, TransformComponent &transform, DashComponent &dash) {
            if (state.stateID == (uint8_t)hunterStates::ATTACKING || 
                state.stateID == (uint8_t)hunterStates::HEAVY_ATTACKING ||
                state.stateID == (uint8_t)hunterStates::DAMAGED ||
                state.stateID == (uint8_t)hunterStates::DASHING) {
                return;
            }

            uint8_t targetState = (uint8_t)hunterStates::IDLE;

            bool moving = input.pressed_W || input.pressed_S || input.pressed_A || input.pressed_D;
            if (moving) {
                targetState = (uint8_t)hunterStates::RUNNING;
            }
            if (input.pressed_E) {
                targetState = (uint8_t)hunterStates::ATTACKING;
            }
            if (input.pressed_R) {
                targetState = (uint8_t)hunterStates::HEAVY_ATTACKING;
            }
            if (input.pressed_Shift && dash.cooldown <= 0.0f) {
                targetState = (uint8_t)hunterStates::DASHING;
                
                Vector2 dir = {0, 0};
                if (input.pressed_W) dir.y -= 1;
                if (input.pressed_S) dir.y += 1;
                if (input.pressed_A) dir.x -= 1;
                if (input.pressed_D) dir.x += 1;
                
                if (dir.x == 0 && dir.y == 0) {
                    dir.x = (transform.facingDirection == -1) ? -1.0f : 1.0f;
                } else {
                    dir = Vector2Normalize(dir);
                }
                
                dash.direction = dir;
                dash.cooldown = 1.0f;
                dash.timer = 0.3f;
            }

            if (state.stateID != targetState) {
                switch (state.stateID) {
                case (uint8_t)hunterStates::IDLE:
                    cmd.removeComponent<IdleStateTag>(entity);
                    break;
                case (uint8_t)hunterStates::RUNNING:
                    cmd.removeComponent<RunningStateTag>(entity);
                    break;
                case (uint8_t)hunterStates::ATTACKING:
                    cmd.removeComponent<AttackingStateTag>(entity);
                    break;
                case (uint8_t)hunterStates::HEAVY_ATTACKING:
                    cmd.removeComponent<HeavyAttackingStateTag>(entity);
                    break;
                }

                switch (targetState) {
                case (uint8_t)hunterStates::IDLE:
                    cmd.addComponent<IdleStateTag>(entity);
                    break;
                case (uint8_t)hunterStates::RUNNING:
                    cmd.addComponent<RunningStateTag>(entity);
                    break;
                case (uint8_t)hunterStates::ATTACKING:
                    cmd.addComponent<AttackingStateTag>(entity);
                    break;
                case (uint8_t)hunterStates::HEAVY_ATTACKING:
                    cmd.addComponent<HeavyAttackingStateTag>(entity);
                    break;
                case (uint8_t)hunterStates::DASHING:
                    cmd.addComponent<DashingStateTag>(entity);
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

    manager.runSystem<TransformComponent, DashComponent, DashingStateTag>(
        [&manager, dt](EntityId entity, TransformComponent &transform, DashComponent &dash) {
            float dashSpeed = 600.0f;
            transform.pos.x += dash.direction.x * dashSpeed * dt;
            transform.pos.y += dash.direction.y * dashSpeed * dt;
            
            if (dash.direction.x != 0) {
                transform.facingDirection = (dash.direction.x < 0) ? -1 : 1;
            }

            auto part = manager.addEntity();
            float offsetRange = 10.0f;
            float offX = ((float)(rand() % 100) / 100.0f - 0.5f) * offsetRange;
            float offY = ((float)(rand() % 100) / 100.0f - 0.5f) * offsetRange;
            
            DashParticleComponent p = {
                .pos = { transform.pos.x + offX, transform.pos.y + offY },
                .color = { 200, 200, 255, 200 },
                .life = 0.4f,
                .maxLife = 0.4f,
                .size = (float)(rand() % 3 + 1)
            };
            manager.addComponent<DashParticleComponent>(part, &p);
        });

    manager.runSystem<PlayerInput, TransformComponent, StatsComponent, StatBuffer, RunningStateTag>(
        [dt](EntityId entity,
             PlayerInput &input,
             TransformComponent &transform,
             StatsComponent &base,
             StatBuffer &buffer) {
            float finalSpeed = base.speed * buffer.speedMult;
            if (input.pressed_W) {
                transform.pos.y -= finalSpeed * dt;
            }
            if (input.pressed_S) {
                transform.pos.y += finalSpeed * dt;
            }
            if (input.pressed_A) {
                transform.pos.x -= finalSpeed * dt;
                transform.facingDirection = -1;
            }
            if (input.pressed_D) {
                transform.pos.x += finalSpeed * dt;
                transform.facingDirection = 1;
            }
        });
}

void UpdateProjectiles(Manager &manager, DeferredCommandBuffer &cmd, float dt) {
    manager.runSystem<ProjectileComponent, HitboxComponent>(
        [&cmd, dt](EntityId entity, ProjectileComponent &proj, HitboxComponent &hitbox) {
            float dist = Vector2Length(proj.velocity) * dt;
            proj.traveled += dist;

            hitbox.x += proj.velocity.x * dt;
            hitbox.y += proj.velocity.y * dt;

            if (proj.traveled >= proj.maxDistance) {
                cmd.destroyEntity(entity);
            }
        });
}

void UpdateSpawning(Manager &manager, SpriteManager &spriteManager, float dt, Vector2 playerPos) {
    manager.runSystem<DifficultyComponent>([&](EntityId entity, DifficultyComponent &diff) {
        diff.totalTime += dt;
        diff.enemySpawnTimer += dt;
        diff.attributeSpawnTimer += dt;

        diff.enemySpawnInterval = std::max(0.5f, 4.0f - (diff.totalTime / 60.0f));
        
        float scalingFactor = 1.0f + (diff.totalTime / 180.0f);

        if (diff.enemySpawnTimer >= diff.enemySpawnInterval) {
            diff.enemySpawnTimer = 0.0f;

            float angle = (float)(rand() % 360) * DEG2RAD;
            float distance = 500.0f;
            float spawnX = playerPos.x + cosf(angle) * distance;
            float spawnY = playerPos.y + sinf(angle) * distance;

            spawnX = std::clamp(spawnX, TILE_SIZE * 2, (MAP_WIDTH - 3) * TILE_SIZE);
            spawnY = std::clamp(spawnY, TILE_SIZE * 2, (MAP_HEIGHT - 3) * TILE_SIZE);

            EntityId enemy = spawnEnemy(manager, spriteManager, spawnX, spawnY);
            
            if (auto health = manager.getComponent<HealthComponent>(enemy)) {
                health->maxHealth *= scalingFactor;
                health->health = health->maxHealth;
            }
            if (auto stats = manager.getComponent<StatsComponent>(enemy)) {
                stats->attackPower *= scalingFactor;
                stats->speed = std::min(150.0f, stats->speed * (1.0f + diff.totalTime / 300.0f));
            }
        }

        if (diff.attributeSpawnTimer >= diff.attributeSpawnInterval) {
            diff.attributeSpawnTimer = 0.0f;
            
            float angle = (float)(rand() % 360) * DEG2RAD;
            float distance = (float)(rand() % 300 + 100);
            float spawnX = std::clamp(playerPos.x + cosf(angle) * distance, TILE_SIZE * 2, (MAP_WIDTH - 3) * TILE_SIZE);
            float spawnY = std::clamp(playerPos.y + sinf(angle) * distance, TILE_SIZE * 2, (MAP_HEIGHT - 3) * TILE_SIZE);

            spawnRandomAttribute(manager, spawnX, spawnY);
        }
    });
}

void UpdateElectricPath(Manager &manager, DeferredCommandBuffer &cmd, float dt) {
    manager.runSystem<ElectricTrailComponent, TransformComponent>(
        [&manager, &cmd, dt](EntityId trailEntity, ElectricTrailComponent &trail, TransformComponent &transform) {
            trail.life -= dt;
            trail.damageTimer += dt;

            if (trail.life <= 0.0f) {
                cmd.destroyEntity(trailEntity);
                return;
            }

            if (trail.damageTimer >= 0.2f) {
                trail.damageTimer = 0.0f;

                auto enemyView = manager.view<EnemyTag, TransformComponent, HealthComponent>();
                for (auto enemy : enemyView) {
                    auto enemyTrans = manager.getComponent<TransformComponent>(enemy);
                    
                    float dist = Vector2Distance(transform.pos, enemyTrans->pos);
                    if (dist < 50.0f) {
                        auto health = manager.getComponent<HealthComponent>(enemy);
                        health->health -= trail.damage;

                        auto textEntity = manager.addEntity();
                        FloatingTextComponent textComp = {
                            .pos = { enemyTrans->pos.x, enemyTrans->pos.y - 40.0f },
                            .color = BLUE,
                            .timer = 0.0f,
                            .duration = 0.5f
                        };
                        snprintf(textComp.text, sizeof(textComp.text), "-%.1f", trail.damage);
                        cmd.addComponent<FloatingTextComponent>(textEntity, textComp);
                    }
                }
            }
        });
}

void UpdateEnemyLogic(Manager &manager, DeferredCommandBuffer &cmd, float dt, Vector2 playerPos) {
    manager.runSystem<TransformComponent, StateComponent, StatsComponent, EnemyTag>(
        [playerPos, dt, &manager, &cmd](EntityId entity,
                                        TransformComponent &transform,
                                        StateComponent &state,
                                        StatsComponent &stats) {
            if (manager.has_component<DamagedStateTag>(entity) ||
                manager.has_component<AttackingStateTag>(entity)) {
                return;
            }

            const float aggroRadius = 400.0f;
            const float attackRange = 50.0f;
            const float dx = playerPos.x - transform.pos.x;
            const float dy = playerPos.y - 40.f - transform.pos.y;
            const float distance = std::sqrt(dx * dx + dy * dy);

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
                    cmd.removeComponent<IdleStateTag>(entity);
                    break;
                case (uint8_t)enemyStates::RUNNING:
                    cmd.removeComponent<RunningStateTag>(entity);
                    break;
                case (uint8_t)enemyStates::ATTACKING:
                    cmd.removeComponent<AttackingStateTag>(entity);
                    break;
                }

                switch (targetState) {
                case (uint8_t)enemyStates::IDLE:
                    cmd.addComponent<IdleStateTag>(entity);
                    break;
                case (uint8_t)enemyStates::RUNNING:
                    cmd.addComponent<RunningStateTag>(entity);
                    break;
                case (uint8_t)enemyStates::ATTACKING:
                    cmd.addComponent<AttackingStateTag>(entity);
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
}

template <typename... EventTags, typename Func>
void updateEvents(Manager &manager, const Func &&callback) {
    auto view = manager.view<AnimationEventComponent, EventTags...>();
    for (auto entity : view) {
        callback(manager, entity);
    }
}

void UpdateCombatAndHitboxes(Manager &manager,
                             DeferredCommandBuffer &cmd,
                             float dt,
                             std::chrono::steady_clock::time_point nowTime) {
    updateEvents<SpawnHitboxEvent>(
        manager, [&cmd, nowTime](Manager &m, const EntityId eventEntity) {
            auto eventInfo = m.getComponent<AnimationEventComponent>(eventEntity);
            auto hitboxInfo = m.getComponent<SpawnHitboxEvent>(eventEntity);
            EntityId entity = eventInfo->sourceEntity;

            auto transformComp = m.getComponent<TransformComponent>(entity);
            auto statsComp = m.getComponent<StatsComponent>(entity);
            if (!transformComp || !statsComp)
                return;

            float dirMultiplier = (transformComp->facingDirection == -1) ? -1.0f : 1.0f;
            float attackCenterX = transformComp->pos.x + (hitboxInfo->offsetX * dirMultiplier);
            float attackCenterY = transformComp->pos.y + hitboxInfo->offsetY;

            float finalWidth = hitboxInfo->width * statsComp->hitboxScale;
            float finalHeight = hitboxInfo->height * statsComp->hitboxScale;
            float spawnX = attackCenterX - (finalWidth / 2.0f);
            float spawnY = attackCenterY - (finalHeight / 2.0f);

            float finalDamage = statsComp->attackPower * hitboxInfo->damageMultiplier;
            StatBuffer *statBuffer = m.getComponent<StatBuffer>(entity);

            if (statBuffer != nullptr) {
                finalDamage *= statBuffer->attackMult;
            }

            auto hitboxEntity = m.addEntity();

            HitboxComponent hitboxComp = {.srcEntity = entity,
                                          .x = spawnX,
                                          .y = spawnY,
                                          .width = finalWidth,
                                          .height = finalHeight,
                                          .offsetX = hitboxInfo->offsetX,
                                          .offsetY = hitboxInfo->offsetY,
                                          .damage = finalDamage,
                                          .attached = hitboxInfo->attached,
                                          .hitEntities = {0},
                                          .hitCount = 0};

            LifespanComponent lifespanComp = {.startPoint = nowTime,
                                              .duration = hitboxInfo->duration};

            if (m.has_component<MainPlayerTag>(entity)) {
                cmd.addComponent<DamageEnemiesTag>(hitboxEntity);
            } else {
                cmd.addComponent<DamageCharacterTag>(hitboxEntity);
            }

            cmd.addComponent<LifespanComponent>(hitboxEntity, lifespanComp);
            cmd.addComponent<HitboxComponent>(hitboxEntity, hitboxComp);

            if (auto fireballAbility = m.getComponent<FireballAbilityComponent>(entity)) {
                float fireballSpeed = 400.0f;
                int count = fireballAbility->count;

                for (int i = 0; i < count; ++i) {
                    auto fireball = m.addEntity();
                    float verticalSpread = (float)(i - (count - 1) / 2.0f) * 15.0f;

                    Vector2 velocity = {
                        (transformComp->facingDirection == -1 ? -1.0f : 1.0f) * fireballSpeed, 0};

                    ProjectileComponent proj = {
                        .velocity = velocity, .maxDistance = 500.0f, .traveled = 0.0f};

                    HitboxComponent fireHitbox = {.srcEntity = entity,
                                                  .x = spawnX,
                                                  .y = spawnY + verticalSpread,
                                                  .width = 20.0f,
                                                  .height = 20.0f,
                                                  .offsetX = 0,
                                                  .offsetY = verticalSpread,
                                                  .damage = finalDamage * 0.5f,
                                                  .attached = false,
                                                  .hitEntities = {0},
                                                  .hitCount = 0};

                    cmd.addComponent<ProjectileComponent>(fireball, proj);
                    cmd.addComponent<HitboxComponent>(fireball, fireHitbox);
                    cmd.addComponent<DamageEnemiesTag>(fireball);
                }
            }

            if (m.has_component<HeavyAttackingStateTag>(entity)) {
                if (auto elec = m.getComponent<ElectricAbilityComponent>(entity)) {
                    for (int i = 1; i <= 5; ++i) {
                        auto trailEntity = m.addEntity();
                        float distOffset = i * 60.0f;
                        Vector2 trailPos = {transformComp->pos.x +
                                                ((transformComp->facingDirection == -1 ? -1.0f
                                                                                       : 1.0f) *
                                                 distOffset),
                                            transformComp->pos.y};

                        ElectricTrailComponent trail = {
                            .life = 2.0f,
                            .maxLife = 2.0f,
                            .damage = statsComp->attackPower * 0.2f * elec->count,
                            .damageTimer = 0.0f};
                        TransformComponent trailTrans = {.pos = trailPos};

                        cmd.addComponent<ElectricTrailComponent>(trailEntity, trail);
                        cmd.addComponent<TransformComponent>(trailEntity, trailTrans);
                    }
                }
            }
        });

    manager.runSystem<HitboxComponent, DamageEnemiesTag>(
        [&manager, &cmd](EntityId hitboxEntity, HitboxComponent &hitbox) {
            auto enemyView = manager.view<EnemyTag>();

            for (auto enemy : enemyView) {
                bool alreadyHit = false;
                for (uint8_t i = 0; i < hitbox.hitCount; ++i) {
                    if (hitbox.hitEntities[i] == enemy) {
                        alreadyHit = true;
                        break;
                    }
                }
                if (alreadyHit)
                    continue;

                auto enemyTransform = manager.getComponent<TransformComponent>(enemy);
                if (!enemyTransform)
                    continue;

                const float actualBodyWidth = 40.0f;
                const float actualBodyHeight = 40.0f;

                float enemyLeft = enemyTransform->pos.x - (actualBodyWidth / 2.0f);
                float enemyRight = enemyTransform->pos.x + (actualBodyWidth / 2.0f);
                float enemyTop = enemyTransform->pos.y - (actualBodyHeight / 2.0f);
                float enemyBottom = enemyTransform->pos.y + (actualBodyHeight / 2.0f);

                float hitboxRight = hitbox.x + hitbox.width;
                float hitboxBottom = hitbox.y + hitbox.height;

                bool overlapX = hitbox.x < enemyRight && hitboxRight > enemyLeft;
                bool overlapY = hitbox.y < enemyBottom && hitboxBottom > enemyTop;

                if (overlapX && overlapY && !manager.has_component<DamagedStateTag>(enemy)) {
                    if (hitbox.hitCount < 16) {
                        hitbox.hitEntities[hitbox.hitCount++] = enemy;
                    }

                    cmd.removeComponent<IdleStateTag>(enemy);
                    cmd.removeComponent<RunningStateTag>(enemy);
                    cmd.removeComponent<AttackingStateTag>(enemy);
                    cmd.addComponent<DamagedStateTag>(enemy);

                    auto state = manager.getComponent<StateComponent>(enemy);
                    if (state)
                        state->stateID = (uint8_t)enemyStates::DAMAGED;

                    auto enemyHealth = manager.getComponent<HealthComponent>(enemy);
                    if (enemyHealth) {
                        enemyHealth->health -= hitbox.damage;

                        if (auto srcStatBuffer =
                                manager.getComponent<StatBuffer>(hitbox.srcEntity)) {
                            if (srcStatBuffer->lifesteal > 0.0f) {
                                if (auto srcHealth =
                                        manager.getComponent<HealthComponent>(hitbox.srcEntity)) {
                                    srcHealth->health =
                                        std::min(srcHealth->health +
                                                     (hitbox.damage * srcStatBuffer->lifesteal),
                                                 srcHealth->maxHealth);
                                }
                            }
                        }

                        auto textEntity = manager.addEntity();
                        FloatingTextComponent textComp = {
                            .pos = {enemyTransform->pos.x, enemyTransform->pos.y - 40.0f},
                            .color = RED,
                            .timer = 0.0f,
                            .duration = 1.0f};
                        snprintf(textComp.text, sizeof(textComp.text), "-%.0f", hitbox.damage);
                        cmd.addComponent<FloatingTextComponent>(textEntity, textComp);
                    }
                }
            }
        });

    manager.runSystem<HitboxComponent, DamageCharacterTag>(
        [&manager, &cmd](EntityId hitboxEntity, HitboxComponent &hitbox) {
            auto playerView = manager.view<MainPlayerTag, HealthComponent>();

            for (auto player : playerView) {
                auto playerHealth = manager.getComponent<HealthComponent>(player);
                if (!playerHealth) continue;

                if (playerHealth->invincibilityTimer > 0.0f || manager.has_component<DamagedStateTag>(player)) continue;

                bool alreadyHitByThis = false;
                for (uint8_t i = 0; i < hitbox.hitCount; ++i) {
                    if (hitbox.hitEntities[i] == player) {
                        alreadyHitByThis = true;
                        break;
                    }
                }
                if (alreadyHitByThis)
                    continue;

                auto playerTransform = manager.getComponent<TransformComponent>(player);
                if (!playerTransform)
                    continue;

                const float playerWidth = 40.0f;
                const float playerHeight = 40.0f;

                float pLeft = playerTransform->pos.x - (playerWidth / 2.0f);
                float pRight = playerTransform->pos.x + (playerWidth / 2.0f);
                float pTop = playerTransform->pos.y - (playerHeight / 2.0f);
                float pBottom = playerTransform->pos.y + (playerHeight / 2.0f);

                float hitboxRight = hitbox.x + hitbox.width;
                float hitboxBottom = hitbox.y + hitbox.height;

                bool overlapX = hitbox.x < pRight && hitboxRight > pLeft;
                bool overlapY = hitbox.y < pBottom && hitboxBottom > pTop;

                if (overlapX && overlapY) {
                    if (hitbox.hitCount < 16) {
                        hitbox.hitEntities[hitbox.hitCount++] = player;
                    }

                    if (playerHealth) {
                        float finalDamage = hitbox.damage;
                        if (auto playerBuffer = manager.getComponent<StatBuffer>(player)) {
                            finalDamage = std::max(0.0f, finalDamage - playerBuffer->defense);
                        }

                        playerHealth->health -= finalDamage;
                        playerHealth->invincibilityTimer = 0.5f;

                        manager.addComponent<DamagedStateTag>(player);
                        
                        cmd.removeComponent<IdleStateTag>(player);
                        cmd.removeComponent<RunningStateTag>(player);
                        cmd.removeComponent<AttackingStateTag>(player);
                        cmd.addComponent<DamagedStateTag>(player);

                        auto state = manager.getComponent<StateComponent>(player);
                        if (state)
                            state->stateID = (uint8_t)hunterStates::DAMAGED;

                        auto textEntity = manager.addEntity();
                        FloatingTextComponent textComp = {
                            .pos = {playerTransform->pos.x, playerTransform->pos.y - 40.0f},
                            .color = RED,
                            .timer = 0.0f,
                            .duration = 1.0f};
                        snprintf(textComp.text, sizeof(textComp.text), "-%.1f", finalDamage);
                        cmd.addComponent<FloatingTextComponent>(textEntity, textComp);
                    }
                }
            }
        });
}

void UpdateAttributeCollisions(Manager &manager, DeferredCommandBuffer &cmd, EntityId characterId) {
    auto playerTransform = manager.getComponent<TransformComponent>(characterId);
    if (!playerTransform)
        return;

    const float playerWidth = 40.0f;
    const float playerHeight = 40.0f;
    float pLeft = playerTransform->pos.x - (playerWidth / 2.0f);
    float pRight = playerTransform->pos.x + (playerWidth / 2.0f);
    float pTop = playerTransform->pos.y - (playerHeight / 2.0f);
    float pBottom = playerTransform->pos.y + (playerHeight / 2.0f);

    manager.runSystem<TransformComponent, AttributeTag>(
        [&manager, &cmd, characterId, pLeft, pRight, pTop, pBottom](
            EntityId entity, TransformComponent &modTransform) {
            
            float modWidth = 20.0f;
            float modHeight = 20.0f;

            if (auto png = manager.getComponent<PngComponent>(entity)) {
                modWidth = (float)png->texture.width * png->scale;
                modHeight = (float)png->texture.height * png->scale;
            }

            float mLeft = modTransform.pos.x;
            float mRight = modTransform.pos.x + modWidth;
            float mTop = modTransform.pos.y;
            float mBottom = modTransform.pos.y + modHeight;

            bool overlapX = pLeft < mRight && pRight > mLeft;
            bool overlapY = pTop < mBottom && pBottom > mTop;

            if (overlapX && overlapY) {
                bool isConsumed = false;

                if (auto modifier = manager.getComponent<StatModifier>(entity)) {
                    modifier->active = true;
                    modifier->target = characterId;
                    isConsumed = true;

                    std::string label = "BUFF";
                    Color textCol = YELLOW;

                    if (modifier->field == &StatBuffer::speedMult) label = "SPEED+";
                    if (modifier->field == &StatBuffer::attackMult) label = "ATTACK+";
                    if (modifier->field == &StatBuffer::lifesteal) { label = "VAMPIRE"; textCol = PURPLE; }
                    if (modifier->field == &StatBuffer::defense) { label = "DEFENSE+"; textCol = BLUE; }

                    if (modifier->type == StatType::TIMED) {
                        label += " (TIMED)";
                        float randomFraction =
                            static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                        float randomTime = 30.0f + (randomFraction * 30.0f);

                        LifespanComponent lifespan;
                        lifespan.duration = randomTime;
                        cmd.addComponent<LifespanComponent>(entity, lifespan);
                    }

                    auto textEntity = manager.addEntity();
                    FloatingTextComponent textComp = {
                        .pos = { modTransform.pos.x, modTransform.pos.y - 20.0f },
                        .color = textCol,
                        .timer = 0.0f,
                        .duration = 1.5f
                    };
                    strncpy(textComp.text, label.c_str(), sizeof(textComp.text) - 1);
                    textComp.text[sizeof(textComp.text) - 1] = '\0';
                    cmd.addComponent<FloatingTextComponent>(textEntity, textComp);
                }

                if (auto healthMod = manager.getComponent<HealthModifier>(entity)) {
                    if (auto health = manager.getComponent<HealthComponent>(characterId)) {
                        health->health =
                            std::min(health->health + healthMod->healAmount, health->maxHealth);
                    }

                    auto textEntity = manager.addEntity();
                    FloatingTextComponent textComp = {
                        .pos = { modTransform.pos.x, modTransform.pos.y - 20.0f },
                        .color = GREEN,
                        .timer = 0.0f,
                        .duration = 1.5f
                    };
                    strncpy(textComp.text, "HEAL", sizeof(textComp.text) - 1);
                    textComp.text[sizeof(textComp.text) - 1] = '\0';
                    cmd.addComponent<FloatingTextComponent>(textEntity, textComp);

                    cmd.destroyEntity(entity);
                    return;
                }

                if (manager.has_component<FireballItemTag>(entity)) {
                    if (auto fireballAbility = manager.getComponent<FireballAbilityComponent>(characterId)) {
                        fireballAbility->count++;
                    } else {
                        FireballAbilityComponent newAbility = { .count = 1 };
                        cmd.addComponent<FireballAbilityComponent>(characterId, newAbility);
                    }

                    auto textEntity = manager.addEntity();
                    FloatingTextComponent textComp = {
                        .pos = { modTransform.pos.x, modTransform.pos.y - 20.0f },
                        .color = ORANGE,
                        .timer = 0.0f,
                        .duration = 1.5f
                    };
                    strncpy(textComp.text, "FIREBALL UP!!", sizeof(textComp.text) - 1);
                    textComp.text[sizeof(textComp.text) - 1] = '\0';
                    cmd.addComponent<FloatingTextComponent>(textEntity, textComp);

                    cmd.destroyEntity(entity);
                    return;
                }

                if (manager.has_component<ElectricItemTag>(entity)) {
                    if (auto elecAbility = manager.getComponent<ElectricAbilityComponent>(characterId)) {
                        elecAbility->count++;
                    } else {
                        ElectricAbilityComponent newAbility = { .count = 1 };
                        cmd.addComponent<ElectricAbilityComponent>(characterId, newAbility);
                    }

                    auto textEntity = manager.addEntity();
                    FloatingTextComponent textComp = {
                        .pos = { modTransform.pos.x, modTransform.pos.y - 20.0f },
                        .color = BLUE,
                        .timer = 0.0f,
                        .duration = 1.5f
                    };
                    strncpy(textComp.text, "ELECTRIC UP!!", sizeof(textComp.text) - 1);
                    textComp.text[sizeof(textComp.text) - 1] = '\0';
                    cmd.addComponent<FloatingTextComponent>(textEntity, textComp);

                    cmd.destroyEntity(entity);
                    return;
                }

                if (isConsumed) {
                    cmd.removeComponent<AttributeTag>(entity);
                    cmd.removeComponent<TransformComponent>(entity);
                    cmd.removeComponent<PngComponent>(entity);
                }
            }
        });
}

void Render(Manager &manager, Camera2D &camera, Texture2D water, Texture2D tileset) {
    BeginDrawing();
    ClearBackground({20, 160, 210, 255});
    BeginMode2D(camera);

    for (int y = 0; y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < MAP_WIDTH; ++x) {
            DrawTexture(water, x * 64, y * 64, WHITE);
        }
    }

    manager.runSystem<TileComponent>([&tileset](EntityId entity, TileComponent &tile) {
        DrawTextureRec(tileset, tile.sourceRect, tile.worldPos, WHITE);
    });

    bool debugMode = false;
    manager.runSystem<DifficultyComponent>([&debugMode](DifficultyComponent &diff) {
        debugMode = diff.debugMode;
    });

    if (debugMode) {
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

    manager.runSystem<ProjectileComponent, HitboxComponent>([](EntityId entity, ProjectileComponent &proj, HitboxComponent &hitbox) {
        DrawCircleV({hitbox.x + hitbox.width/2, hitbox.y + hitbox.height/2}, 8, RED);
        DrawCircleGradient((int)(hitbox.x + hitbox.width/2), (int)(hitbox.y + hitbox.height/2), 15, {255, 100, 0, 150}, {255, 0, 0, 0});
    });

    manager.runSystem<ElectricTrailComponent, TransformComponent>([](EntityId entity, ElectricTrailComponent &trail, TransformComponent &transform) {
        float flicker = (float)(rand() % 100) / 100.0f;
        Color col = { 100, 200, 255, (unsigned char)(150 + flicker * 105) };
        DrawCircleV(transform.pos, 15, col);
        DrawCircleV(transform.pos, 25, { col.r, col.g, col.b, (unsigned char)(col.a / 3) });
        
        if (rand() % 5 == 0) {
            float offX = (float)(rand() % 40 - 20);
            float offY = (float)(rand() % 40 - 20);
            DrawLineV(transform.pos, {transform.pos.x + offX, transform.pos.y + offY}, BLUE);
        }
    });

    manager.runSystem<PngComponent, TransformComponent>(
        [](PngComponent &pngComp, TransformComponent &transformComp) {
            DrawTextureEx(pngComp.texture, transformComp.pos, 0.0f, pngComp.scale, WHITE);
        });

    manager.runSystem<FloatingTextComponent>([](EntityId entity, FloatingTextComponent &text) {
        float alpha = 1.0f - (text.timer / text.duration);
        Color color = text.color;
        color.a = (unsigned char)(alpha * 255);
        DrawText(text.text, (int)text.pos.x, (int)text.pos.y, 20, color);
    });

    manager.runSystem<DashParticleComponent>([](EntityId entity, DashParticleComponent &p) {
        float alpha = p.life / p.maxLife;
        Color col = p.color;
        col.a = (unsigned char)(alpha * 200);
        DrawCircleV(p.pos, p.size, col);
        DrawCircleV(p.pos, p.size * 1.5f, {col.r, col.g, col.b, (unsigned char)(col.a / 2)});
    });

    AnimationSystem::render(manager);

    EndMode2D();

    // Render UI Health Bar
    manager.runSystem<MainPlayerTag, HealthComponent>([&manager](EntityId player, HealthComponent& health) {
        float barWidth = 200.0f;
        float barHeight = 25.0f;
        float margin = 20.0f;
        
        Vector2 pos = { (float)GetScreenWidth() - barWidth - margin, margin };
        
        DrawRectangleV(pos, { barWidth, barHeight }, BLACK);
        
        float healthPercentage = std::clamp(health.health / health.maxHealth, 0.0f, 1.0f);
        DrawRectangleV(pos, { barWidth * healthPercentage, barHeight }, RED);
        
        DrawRectangleLinesEx({ pos.x, pos.y, barWidth, barHeight }, 2, WHITE);
        
        DrawText(TextFormat("%.0f/%.0f", health.health, health.maxHealth), (int)pos.x + 5, (int)pos.y + 5, 15, WHITE);
        DrawText("HEALTH", (int)pos.x, (int)pos.y - 15, 12, WHITE);

        if (auto stats = manager.getComponent<StatsComponent>(player)) {
            if (auto buffer = manager.getComponent<StatBuffer>(player)) {
                float curDmg = stats->attackPower * buffer->attackMult;
                float curSpd = stats->speed * buffer->speedMult;
                float curDef = buffer->defense;

                DrawText(TextFormat("DMG: %.0f", curDmg), (int)pos.x, (int)pos.y + 35, 15, WHITE);
                DrawText(TextFormat("SPD: %.0f", curSpd), (int)pos.x + 70, (int)pos.y + 35, 15, WHITE);
                DrawText(TextFormat("DEF: %.0f", curDef), (int)pos.x + 140, (int)pos.y + 35, 15, WHITE);
            }
        }
    });

    DrawText(TextFormat("%d fps", GetFPS()), 10, 10, 25, WHITE);

    manager.runSystem<DifficultyComponent>([](DifficultyComponent& diff) {
        int minutes = (int)diff.totalTime / 60;
        int seconds = (int)diff.totalTime % 60;
        DrawText(TextFormat("SURVIVED: %02d:%02d", minutes, seconds), 10, 40, 20, WHITE);
    });

    manager.runSystem<MainPlayerTag, HealthComponent>([&](EntityId player, HealthComponent& health) {
        if (health.health < 25.0f) {
            float intensity = 1.0f - (health.health / 25.0f);
            float pulse = (sinf((float)GetTime() * 5.0f) * 0.5f) + 0.5f;
            unsigned char alpha = (unsigned char)(intensity * pulse * 100.0f);
            
            Color glowColor = { 255, 0, 0, alpha };
            int thickness = 60;

            DrawRectangleGradientEx({0, 0, (float)GetScreenWidth(), (float)thickness}, glowColor, glowColor, {255,0,0,0}, {255,0,0,0});
            DrawRectangleGradientEx({0, (float)GetScreenHeight() - thickness, (float)GetScreenWidth(), (float)thickness}, {255,0,0,0}, {255,0,0,0}, glowColor, glowColor);
            DrawRectangleGradientEx({0, 0, (float)thickness, (float)GetScreenHeight()}, glowColor, {255,0,0,0}, glowColor, {255,0,0,0});
            DrawRectangleGradientEx({(float)GetScreenWidth() - thickness, 0, (float)thickness, (float)GetScreenHeight()}, {255,0,0,0}, glowColor, {255,0,0,0}, glowColor);
        }
    });

    EndDrawing();
}

void Cleanup(Manager &manager, DeferredCommandBuffer &cmd, float dt) {
    manager.runSystem<HealthComponent>([&cmd, dt](EntityId entity, HealthComponent &healthComp) {
        if (healthComp.invincibilityTimer > 0.0f) {
            healthComp.invincibilityTimer -= dt;
        }

        if (healthComp.health <= 0.f)
            cmd.destroyEntity(entity);
    });

    manager.runSystem<LifespanComponent>([&cmd, dt](EntityId entity, LifespanComponent &lifespan) {
        lifespan.duration -= dt;
        if (lifespan.duration <= 0.f)
            cmd.destroyEntity(entity);
    });

    manager.runSystem<AnimationEventComponent>(
        [&cmd](EntityId eventEntity, AnimationEventComponent &eventComp) {
            cmd.destroyEntity(eventEntity);
        });

    manager.runSystem<FloatingTextComponent>([&cmd, dt](EntityId entity, FloatingTextComponent &text) {
        text.timer += dt;
        text.pos.y -= 30.0f * dt;
        if (text.timer >= text.duration) {
            cmd.destroyEntity(entity);
        }
    });

    manager.runSystem<DashComponent>([dt](EntityId entity, DashComponent &dash) {
        if (dash.cooldown > 0.0f) {
            dash.cooldown -= dt;
        }
    });

    manager.runSystem<DashParticleComponent>([&cmd, dt](EntityId entity, DashParticleComponent &p) {
        p.life -= dt;
        if (p.life <= 0.0f) {
            cmd.destroyEntity(entity);
        }
    });
}
} // namespace GameSystems