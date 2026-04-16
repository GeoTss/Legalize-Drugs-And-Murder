#include "obj/AnimationSystem.hpp"
#include "obj/ECS/Component.hpp"
#include "obj/ECS/Manager.hpp"
#include "obj/HitboxAttachmentSystem.hpp"
#include "obj/SpriteManager.hpp"

#include <iostream>
#include <memory>
#include <raylib.h>
#include <raymath.h>

enum struct hunterStates { IDLE, RUNNING, ATTACKING };

enum struct eventIDs { LIGHT_ATTACK, LIGHTNING };

void initializeAnimations(Manager &manager,
                          SpriteManager &spriteManager,
                          EventDispatcher &eventDispatcher) {
    spriteManager.createProfile("main");

    AnimationTrack idleTrack{ASSET_PATH "/Blind-Huntress/1 - Idle.png", 240, 128, {}, 0.1f, true};

    spriteManager.addAnimationTrack("main", std::move(idleTrack), (uint32_t)hunterStates::IDLE);

    AnimationTrack runningTrack(ASSET_PATH "/Blind-Huntress/2 - Run.png", 240, 128, {}, 0.1f, true);

    spriteManager.addAnimationTrack(
        "main", std::move(runningTrack), (uint32_t)hunterStates::RUNNING);

    std::unordered_map<uint32_t, std::vector<uint64_t>> lightAttackEvents;
    lightAttackEvents[0] = {};

    SpawnHitboxEvent lightAttackHitbox = {.width = 90,
                                          .height = 20,
                                          .offsetX = 45,
                                          .offsetY = 0,
                                          .duration = 0.300f,
                                          .attached = true};

    auto lightAttackEvent =
        eventDispatcher.registerPayloadEvent<SpawnHitboxEvent>(lightAttackHitbox);

    lightAttackEvents[0].push_back(lightAttackEvent);

    AnimationTrack lightAttackTrack{
        ASSET_PATH "/Blind-Huntress/10 - attack 1.png", 240, 128, lightAttackEvents, 0.1f, true};

    spriteManager.addAnimationTrack(
        "main", std::move(lightAttackTrack), (uint32_t)hunterStates::ATTACKING);
}

template <typename... EventTags, typename Func>
void updateEvents(Manager &manager, const Func &&callback) {
    auto view = manager.view<AnimationEventComponent, EventTags...>();

    for (auto entity : view) {
        callback(manager, entity);
    }
}

int main() {

    std::cout << "Your asset path: " << ASSET_PATH << "\n";

    std::cout << "Position component ID: " << ComponentID<TransformComponent>::_id << "\n";
    std::cout << "AnimationStateComponent ID: " << ComponentID<AnimationStateComponent>::_id
              << "\n";
    std::cout << "Health component ID: " << ComponentID<HealthComponent>::_id << "\n";
    std::cout << "NothingEffectTag component ID: " << ComponentID<NothingEffectTag>::_id << "\n";
    std::cout << "PoisonEffectTag component ID: " << ComponentID<PoisonEffectTag>::_id << "\n\n";

    Manager manager;

    auto hunter = manager.addEntity();

    manager.addComponent<MainPlayerTag>(hunter);

    manager.addComponent<TransformComponent>(hunter);
    TransformComponent transformComp = {{100, 100}};
    manager.setComponent<TransformComponent>(hunter, transformComp);

    HealthComponent healthComp = {.health = 100.f};
    manager.addComponent<HealthComponent>(hunter, &healthComp);

    manager.addComponent<NothingEffectTag>(hunter);

    std::cout << *manager.getArchetype(hunter) << "\n";

    HealthComponent *hunterHealth = manager.getComponent<HealthComponent>(hunter);
    std::cout << "Hunter's health before poison: " << hunterHealth->health << "\n";

    manager.replaceComponent<NothingEffectTag, PoisonEffectTag>(hunter);

    manager.runSystem<HealthComponent, PoisonEffectTag>([](HealthComponent &_health) {
        _health.health -= 10.f;
    });

    hunterHealth = manager.getComponent<HealthComponent>(hunter);
    std::cout << "Hunter's health after poison: " << hunterHealth->health << "\n";

    std::cout << *manager.getArchetype(hunter) << "\n";

    manager.removeComponent<PoisonEffectTag>(hunter);

    std::cout << *manager.getArchetype(hunter) << "\n";

    manager.runSystem<HealthComponent, PoisonEffectTag>([](HealthComponent &_health) {
        _health.health -= 10.f;
    });

    hunterHealth = manager.getComponent<HealthComponent>(hunter);
    std::cout << "Hunter's health after removing poison effect: " << hunterHealth->health << "\n";

    StatsComponent stats = {.attackPower = 10.f, .hitboxScale = 1.f, .speed = 100.f};
    manager.addComponent<StatsComponent>(hunter, &stats);

    InitWindow(800, 600, "My ECS Game");

    SpriteManager spriteManager{};
    EventDispatcher eventDispatcher{};

    initializeAnimations(manager, spriteManager, eventDispatcher);

    AnimationStateComponent animationComponent = {0};
    animationComponent.profile = spriteManager.getProfile("main");

    manager.addComponent<AnimationStateComponent>(hunter, &animationComponent);
    StateComponent stateComp = {.stateID = (uint8_t)hunterStates::IDLE};
    manager.addComponent<StateComponent>(hunter, &stateComp);
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        auto nowTime = std::chrono::steady_clock::now();

        manager.runSystem<MainPlayerTag, TransformComponent, StatsComponent, StateComponent>(
            [dt](TransformComponent &transform, StatsComponent &stats, StateComponent &state) {
                state.stateID = (uint8_t)hunterStates::IDLE;

                if (IsKeyDown(KEY_W)) {
                    transform.pos.y -= stats.speed * dt;
                    state.stateID = (uint8_t)hunterStates::RUNNING;
                }
                if (IsKeyDown(KEY_S)) {
                    transform.pos.y += stats.speed * dt;
                    state.stateID = (uint8_t)hunterStates::RUNNING;
                }
                if (IsKeyDown(KEY_A)) {
                    transform.pos.x -= stats.speed * dt;
                    transform.facingDirection = -1;
                    state.stateID = (uint8_t)hunterStates::RUNNING;
                }
                if (IsKeyDown(KEY_D)) {
                    transform.pos.x += stats.speed * dt;
                    transform.facingDirection = 1;
                    state.stateID = (uint8_t)hunterStates::RUNNING;
                }
                if (IsKeyDown(KEY_E)) {
                    state.stateID = (uint8_t)hunterStates::ATTACKING;
                }
            });

        HitboxAttachmentSystem::update(manager);

        BeginDrawing();
        ClearBackground(BLACK);

        AnimationSystem::update(manager, spriteManager, eventDispatcher, dt);

        updateEvents<SpawnHitboxEvent>(
            manager, [&nowTime](Manager &manager, const EntityId eventEntity) {
                auto eventInfo = manager.getComponent<AnimationEventComponent>(eventEntity);
                auto hitboxInfo = manager.getComponent<SpawnHitboxEvent>(eventEntity);

                EntityId entity = eventInfo->sourceEntity;

                auto transformComp = manager.getComponent<TransformComponent>(entity);
                auto statsComp = manager.getComponent<StatsComponent>(entity);

                float finalWidth = hitboxInfo->width * statsComp->hitboxScale;
                float finalHeight = hitboxInfo->height * statsComp->hitboxScale;

                float dirMultiplier = (transformComp->facingDirection == -1) ? -1.0f : 1.0f;

                float startingOffsetX = hitboxInfo->offsetX * dirMultiplier;

                float spawnX = (transformComp->pos.x + startingOffsetX) - (finalWidth / 2.0f);
                float spawnY = (transformComp->pos.y + hitboxInfo->offsetY);

                auto hitboxEntity = manager.addEntity();
                HitboxComponent hitboxComp = {
                    .srcEntity = entity,
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

                manager.addComponents<LifespanComponent, HitboxComponent, DamageEnemiesTag>(
                    hitboxEntity);
                manager.setComponent<LifespanComponent>(hitboxEntity, lifespanComp);
                manager.setComponent<HitboxComponent>(hitboxEntity, hitboxComp);
            });

        manager.runSystem<HitboxComponent>([](HitboxComponent &hitbox) {
            DrawRectangleLines(hitbox.x, hitbox.y, hitbox.width, hitbox.height, YELLOW);
        });

        EndDrawing();

        std::vector<EntityId> entitiesToDestroy;

        auto lifeView = manager.view<LifespanComponent>();

        for (auto timedEntity : lifeView) {
            auto &lifespanComp = lifeView.get<LifespanComponent>(timedEntity);

            lifespanComp.duration -= dt;

            if (lifespanComp.duration <= 0)
                entitiesToDestroy.push_back(timedEntity);
        }

        auto eventView = manager.view<AnimationEventComponent>();
        for (auto event : eventView) {
            entitiesToDestroy.push_back(event);
        }

        for (auto entity : entitiesToDestroy) {
            manager.destroyEntity(entity);
        }
    }

    CloseWindow();

    return 0;
}