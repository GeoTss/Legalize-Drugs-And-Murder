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
};

struct NothingEffectTag {};
struct PoisonEffectTag {};

struct AnimationEventComponent {
    EntityId sourceEntity;
};

struct LifespanComponent{
    std::chrono::steady_clock::time_point startPoint;
    std::chrono::milliseconds duration;
};

struct HitboxComponent {
    EntityId srcEntity;
    float x;
    float y;
    float width;
    float height;
    float damage;
};

struct SpawnHitboxEvent {
    float width;
    float height;
    std::chrono::milliseconds duration;
};

struct DamageEnemiesTag {};
struct DamageCharacterTag {};

struct HunterLightAttackEventTag {};

#endif