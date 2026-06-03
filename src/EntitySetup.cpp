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
                                          .duration = 0.0001f,
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

    std::unordered_map<uint32_t, std::vector<uint64_t>> heavyAttackEvents;
    heavyAttackEvents[2] = {};

    SpawnHitboxEvent heavyAttackHitbox = {.width = 90,
                                          .height = 80,
                                          .offsetX = 10,
                                          .offsetY = 0,
                                          .duration = 0.0001f,
                                          .attached = true,
                                          .damageMultiplier = 2.0f};

    auto heavyAttackEvent =
        eventDispatcher.registerPayloadEvent<SpawnHitboxEvent>(heavyAttackHitbox);

    heavyAttackEvents[2].push_back(heavyAttackEvent);

    AnimationTrack heavyAttackTrack{ASSET_PATH "/Blind-Huntress/11 - attack 3.png",
                                    240,
                                    128,
                                    0,
                                    0,
                                    heavyAttackEvents,
                                    0.125f,
                                    false};

    spriteManager.addAnimationTrack(
        "main", std::move(heavyAttackTrack), (uint32_t)hunterStates::HEAVY_ATTACKING);

    AnimationTrack damagedTrack{
        ASSET_PATH "/Blind-Huntress/14 - hit.png", 240, 128, 0, 0, {}, 0.1f, false};
    spriteManager.addAnimationTrack(
        "main", std::move(damagedTrack), (uint32_t)hunterStates::DAMAGED);

    AnimationTrack dashTrack{
        ASSET_PATH "/Blind-Huntress/6 - dash.png", 240, 128, 0, 0, {}, 0.05f, false};
    spriteManager.addAnimationTrack(
        "main", std::move(dashTrack), (uint32_t)hunterStates::DASHING);
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

    enemyAttackEvents[7] = {};

    SpawnHitboxEvent lightningHitbox = {.width = 40,
                                        .height = 60,
                                        .offsetX = 40,
                                        .offsetY = 0,
                                        .duration = 0.200f,
                                        .attached = false};

    auto lightningEvent = eventDispatcher.registerPayloadEvent<SpawnHitboxEvent>(lightningHitbox);

    enemyAttackEvents[7].push_back(lightningEvent);

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

    AnimationTrack attackTrack{
        ASSET_PATH "/stormhead/attack.png", 119, 124, 7, 40, enemyAttackEvents, 0.1f, false};

    spriteManager.addAnimationTrack(
        "enemy", std::move(attackTrack), (uint32_t)enemyStates::ATTACKING);
}

EntityId spawnAttribute(Manager& manager, PngComponent pngComp, TransformComponent transform, StatModifier mod){
    auto modifier = manager.addEntity();

    manager.addComponents<PngComponent, TransformComponent, StatModifier>(modifier, &pngComp, &transform, &mod);
    manager.addComponent<AttributeTag>(modifier);

    return modifier;
}

void spawnRandomAttribute(Manager& manager, float x, float y) {
    int type = rand() % 7;
    bool isTimed = (rand() % 2) == 0; 

    std::string filepath;
    StatModifier statMod = {};
    HealthModifier healthMod = {};
    bool hasStatMod = false;
    bool hasHealthMod = false;
    bool isFireball = false;
    bool isElectric = false;

    float potencyMult = isTimed ? 2.0f : 1.0f;

    if (type == 0) {
        int beerId = (rand() % 4) + 1;
        filepath = ASSET_PATH "/Booze/Beer " + std::to_string(beerId) + ".png";
        
        statMod.field = &StatBuffer::speedMult;
        statMod.type = isTimed ? StatType::TIMED : StatType::PERMANENT;
        float increase = ((rand() % 50) + 10) / 100.0f; 
        statMod.multiplier = 1.0f + (increase * potencyMult); 
        statMod.active = false;
        hasStatMod = true;
    } else if (type == 1) {
        int drinkId = (rand() % 3) + 1;
        filepath = ASSET_PATH "/Booze/Drink " + std::to_string(drinkId) + ".png";
        
        float baseHeal = 20.0f + (rand() % 30);
        healthMod.healAmount = baseHeal * potencyMult; 
        hasHealthMod = true;
    } else if (type == 2) {
        int bottleId = (rand() % 15) + 1;
        filepath = ASSET_PATH "/Booze/Liquor Bottle " + std::to_string(bottleId) + ".png";
        
        statMod.field = &StatBuffer::attackMult;
        statMod.type = isTimed ? StatType::TIMED : StatType::PERMANENT;
        float increase = ((rand() % 50) + 10) / 100.0f;
        statMod.multiplier = 1.0f + (increase * potencyMult); 
        statMod.active = false;
        hasStatMod = true;
    } else if (type == 3) {
        filepath = ASSET_PATH "/Booze/Liquor Bottle 13.png"; 
        
        statMod.field = &StatBuffer::lifesteal;
        statMod.type = isTimed ? StatType::TIMED : StatType::PERMANENT;
        float baseLifesteal = ((rand() % 10) + 5) / 100.0f;
        statMod.multiplier = baseLifesteal * potencyMult; 
        statMod.active = false;
        hasStatMod = true;
    } else if (type == 4) {
        filepath = ASSET_PATH "/Booze/Glass 3.png"; 
        
        statMod.field = &StatBuffer::defense;
        statMod.type = isTimed ? StatType::TIMED : StatType::PERMANENT;
        float baseDef = (float)((rand() % 4) + 2);
        statMod.multiplier = baseDef * potencyMult; 
        statMod.active = false;
        hasStatMod = true;
    } else if (type == 5) {
        filepath = ASSET_PATH "/Booze/Blender 1.png"; 
        isFireball = true;
    } else {
        filepath = ASSET_PATH "/Booze/Glass 1.png";
        isElectric = true;
    }

    PngComponent pngComp(filepath, 1.5f); 
    TransformComponent transform = {{x, y}};

    auto entity = manager.addEntity();
    manager.addComponents<PngComponent, TransformComponent>(entity, &pngComp, &transform);
    manager.addComponent<AttributeTag>(entity);

    if (hasStatMod) {
        manager.addComponent<StatModifier>(entity, &statMod);
    }
    if (hasHealthMod) {
        manager.addComponent<HealthModifier>(entity, &healthMod);
    }
    if (isFireball) {
        manager.addComponent<FireballItemTag>(entity);
    }
    if (isElectric) {
        manager.addComponent<ElectricItemTag>(entity);
    }
}

EntityId spawnPlayer(Manager &manager, SpriteManager &spriteManager) {
    auto hunter = manager.addEntity();
    manager.addComponent<MainPlayerTag>(hunter);

    TransformComponent transformComp = {{500, 300}};
    HealthComponent healthComp = {.health = 100.f, .maxHealth = 100.f};
    StatsComponent stats = {.attackPower = 10.f, .hitboxScale = 1.f, .speed = 100.f};

    StatBuffer statBuffer;

    StateComponent stateComp = {.stateID = (uint8_t)hunterStates::IDLE};

    AnimationStateComponent animationComponent = {0};
    animationComponent.profile = spriteManager.getProfile("main");

    DifficultyComponent difficulty;
    DashComponent dash;

    manager.addComponents<TransformComponent,
                          HealthComponent,
                          StatsComponent,
                          StateComponent,
                          StatBuffer,
                          AnimationStateComponent,
                          DifficultyComponent,
                          DashComponent>(
        hunter,
        &transformComp,
        &healthComp,
        &stats,
        &stateComp,
        &statBuffer,
        &animationComponent,
        &difficulty,
        &dash);

    manager.addComponent<PlayerInput>(hunter);
    manager.addComponent<IdleStateTag>(hunter);

    return hunter;
}

EntityId spawnEnemy(Manager &manager, SpriteManager &spriteManager, float x, float y) {
    auto enemy = manager.addEntity();
    manager.addComponent<EnemyTag>(enemy);

    TransformComponent enemyTransformComp = {{x, y}};
    HealthComponent enemyHealthComp = {.health = 50.f, .maxHealth = 50.f};
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