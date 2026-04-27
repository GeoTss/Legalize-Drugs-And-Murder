#ifndef COMPONENT_HPP
#define COMPONENT_HPP
#pragma once

#include <stdint.h>

#include "unique_counter.hpp"
#include "ComponentFamily.hpp"
#include "ComponentTraits.hpp"
#include "Entity.hpp"

struct TransformComponent {
    Vector2 pos;
    Vector2 velocity;
    int8_t facingDirection;
};

struct HealthComponent {
    float health;
};

struct StatsComponent{
    float attackPower;
    float hitboxScale = 1.0f;
    float speed = 10.0f;
};

struct NothingEffectTag {};
struct PoisonEffectTag {};

struct AnimationEventComponent {
    EntityId sourceEntity;
};

struct LifespanComponent{
    std::chrono::steady_clock::time_point startPoint;
    float duration;
};

struct HitboxComponent {
    EntityId srcEntity;
    float x;
    float y;
    float width;
    float height;
    float offsetX;
    float offsetY;
    
    float damage;

    bool attached;
};

struct SpawnHitboxEvent {
    float width;
    float height;
    float offsetX;
    float offsetY;
    float duration;

    bool attached = false;
};

struct StateComponent{
    uint8_t stateID;
};

struct PlayerInput{
    bool pressed_W: 1;
    bool pressed_S: 1;
    bool pressed_A: 1;
    bool pressed_D: 1;
    bool pressed_E: 1;
};

struct MainPlayerTag{};
struct EnemyTag{};

struct IdleStateTag{};
struct RunningStateTag{};
struct AttackingStateTag{};
struct DamagedStateTag{};

struct DamageEnemiesTag {};
struct DamageCharacterTag {};

struct AnimationCompleteTag{};

struct TileComponent {
    Rectangle sourceRect;
    Vector2 worldPos;
};

struct SolidWallTag {};

#endif