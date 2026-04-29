#include "obj/EntitySetup.hpp"
#include "obj/ECS/Component.hpp"
#include "obj/GameDefines.hpp"

void initializeCharacterAnimations(SpriteManager &spriteManager, EventDispatcher &eventDispatcher) {
    spriteManager.createProfile("main");

    AnimationTrack idleTrack{
        ASSET_PATH "/Blind-Huntress/1 - Idle.png", 240, 128, 0, 0, {}, 0.1f, true};

    spriteManager.addAnimationTrack("main", std::move(idleTrack), (uint32_t)hunterStates::IDLE);

    AnimationTrack runningTrack(
        ASSET_PATH "/Blind-Huntress/2 - Run.png", 240, 128, 0, 0, {}, 0.1f, true);

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

    AnimationTrack lightAttackTrack{ASSET_PATH "/Blind-Huntress/10 - attack 1.png",
                                    240,
                                    128,
                                    0,
                                    0,
                                    lightAttackEvents,
                                    0.1f,
                                    false};

    spriteManager.addAnimationTrack(
        "main", std::move(lightAttackTrack), (uint32_t)hunterStates::ATTACKING);
}

void initializeEnemyAnimations(SpriteManager &spriteManager, EventDispatcher &eventDispatcher) {

    spriteManager.createProfile("enemy");

    AnimationTrack idleTrack{ASSET_PATH "/stormhead/idle.png", 119, 124, 7, 40, {}, 0.1f, true};
    spriteManager.addAnimationTrack("enemy", std::move(idleTrack), (uint32_t)enemyStates::IDLE);

    AnimationTrack runningTrack{ASSET_PATH "/stormhead/run.png", 119, 124, 7, 40, {}, 0.1f, true};
    spriteManager.addAnimationTrack("enemy", std::move(idleTrack), (uint32_t)enemyStates::RUNNING);

    AnimationTrack damagedTrack{
        ASSET_PATH "/stormhead/damaged.png", 119, 124, 7, 40, {}, 0.1f, false};
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

EntityId spawnPlayer(Manager &manager, SpriteManager &spriteManager) {
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

    return hunter;
}

EntityId spawnEnemy(Manager &manager, SpriteManager &spriteManager) {
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
    return enemy;
}