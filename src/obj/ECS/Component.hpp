#ifndef COMPONENT_HPP
#define COMPONENT_HPP
#pragma once

#include <stdint.h>

#include "unique_counter.hpp"
#include "Entity.hpp"

using ComponentId = uint16_t;

template <typename T> struct ComponentID {
    static constexpr ComponentId _id = unique_id<T>();
};

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


struct MainPlayerTag{};
struct IdleStateTag{};
struct RunningStateTag{};
struct AttackingStateTag{};

struct DamageEnemiesTag {};
struct DamageCharacterTag {};

struct HunterLightAttackEventTag {};

#endif