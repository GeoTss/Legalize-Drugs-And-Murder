#include "obj/AnimationSystem.hpp"
#include "obj/ECS/Component.hpp"
#include "obj/ECS/Manager.hpp"
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

    std::unordered_map<uint32_t, std::vector<uint64_t>> lightAttackEvents;
    lightAttackEvents[0] = {};

    auto lightAttackEvent = eventDispatcher.registerEvent<HunterLightAttackEventTag>();
    lightAttackEvents[0].push_back(lightAttackEvent);
    lightAttackEvents[0].push_back((uint64_t)eventIDs::LIGHT_ATTACK);

    AnimationTrack lightAttackTrack{
        ASSET_PATH "/Blind-Huntress/10 - attack 1.png", 240, 128, lightAttackEvents, 0.1f, true};

    spriteManager.addAnimationTrack(
        "main", std::move(lightAttackTrack), (uint32_t)hunterStates::ATTACKING);
}

template<typename... EventTags, typename Func>
void updateEvents(Manager& manager, const Func&& callback){
    auto view = manager.view<AnimationEventComponent, EventTags...>();

    for(const auto entity: view){
        callback(manager, entity);
    }
}

int main() {

    std::cout << "Your asset path: " << ASSET_PATH << "\n";

    Manager manager;

    auto hunter = manager.addEntity();

    std::cout << "Position component ID: " << ComponentID<TransformComponent>::_id << "\n";
    std::cout << "AnimationStateComponent ID: " << ComponentID<AnimationStateComponent>::_id
              << "\n";
    std::cout << "Health component ID: " << ComponentID<HealthComponent>::_id << "\n";
    std::cout << "NothingEffectTag component ID: " << ComponentID<NothingEffectTag>::_id << "\n";
    std::cout << "PoisonEffectTag component ID: " << ComponentID<PoisonEffectTag>::_id << "\n\n";

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

    InitWindow(800, 600, "My ECS Game");

    SpriteManager spriteManager{};
    EventDispatcher eventDispatcher{};

    initializeAnimations(manager, spriteManager, eventDispatcher);

    AnimationStateComponent animationComponent = {0};
    animationComponent.currentState = (uint32_t)hunterStates::ATTACKING;
    animationComponent.profile = spriteManager.getProfile("main");

    manager.addComponent<AnimationStateComponent>(hunter, &animationComponent);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        auto nowTime = std::chrono::steady_clock::now();

        BeginDrawing();
        ClearBackground(BLACK);

        AnimationSystem::update(manager, spriteManager, eventDispatcher, dt, nowTime);

        EndDrawing();
        
        updateEvents<HunterLightAttackEventTag>(manager, [](Manager& manager, const EntityId eventEntity){
            auto eventInfo = manager.getComponent<AnimationEventComponent>(eventEntity);
            EntityId entity = eventInfo->sourceEntity;

            auto transformComp = manager.getComponent<TransformComponent>(entity);
            
            // DrawRectangleLines(transformComp->pos.x, transformComp->pos.y, 90, 20, YELLOW);

            auto hitboxEntity = manager.addEntity();
            HitboxComponent hitboxComp = {
                .srcEntity = eventEntity,
                .x = transformComp->pos.x,
                .y = transformComp->pos.y,
                .width = 90,
                .height = 20
            };

            manager.addComponents<HitboxComponent, DamageEnemiesTag>(hitboxEntity);

            manager.setComponent<HitboxComponent>(hitboxEntity, hitboxComp);
        });


        std::vector<EntityId> entitiesToDestroy;

        auto hitboxView = manager.view<HitboxComponent>();
        for(auto hitbox: hitboxView){
            entitiesToDestroy.push_back(hitbox);
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