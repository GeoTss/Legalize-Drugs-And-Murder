#ifndef COMPONENT_HPP
#define COMPONENT_HPP
#pragma once

#include <stdint.h>
#include <raylib.h>
#include <numeric>

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

struct AnimationTrack {
    std::string textureFilepath;
    Texture2D texture;

    uint32_t frameWidth;
    uint32_t frameHeight;
    float trueCenterOffsetX;
    float trueCenterOffsetY;

    std::unordered_map<uint32_t, std::vector<uint64_t>> frameEvents;

    int totalFrames = 0;
    std::vector<uint32_t> frames;
    float frameDuration = 0.1f;
    bool loop = true;

    AnimationTrack() = default;

    AnimationTrack(const std::string _textureFilepath,
                   const uint32_t _width,
                   const uint32_t _height,
                   const float centerOffX,
                   const float centerOffY,
                   const std::unordered_map<uint32_t, std::vector<uint64_t>> _frameEvents,
                   const float _frameDuration,
                   const bool _inLoop) {

        frameWidth = _width;
        frameHeight = _height;
        trueCenterOffsetX = centerOffX;
        trueCenterOffsetY = centerOffY;
        frameEvents = _frameEvents;
        frameDuration = _frameDuration;
        loop = _inLoop;

        texture = LoadTexture(_textureFilepath.c_str());

        textureFilepath = _textureFilepath;

        int columns = texture.width / frameWidth;
        int rows = texture.height / frameHeight;

        totalFrames = columns * rows;

        frames.resize(totalFrames);
        std::iota(frames.begin(), frames.end(), 0);
    }
};

struct AnimationProfile {
    std::unordered_map<uint32_t, AnimationTrack> stateAnimations;
};

struct AnimationStateComponent {
    AnimationProfile *profile = nullptr;
    uint32_t currentFrame = 0;
    float stateTimer = 0.f;
    int lastProcessedFrame = -1;
};

struct AnimationCompleteTag{};

struct TileComponent {
    Rectangle sourceRect;
    Vector2 worldPos;
};

struct SolidWallTag {};

#endif