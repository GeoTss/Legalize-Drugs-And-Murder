#ifndef ANIMATION_SYSTEM_HPP
#define ANIMATION_SYSTEM_HPP
#pragma once

#include <iostream>
#include <raylib.h>
#include <stdint.h>
#include <unordered_map>
#include <vector>

#include "./ECS/Manager.hpp"
#include "./ECS/View.hpp"
#include "EventDispatcher.hpp"
#include "SpriteManager.hpp"

constexpr uint32_t HashState(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

struct AnimationStateComponent {

    AnimationProfile *profile = nullptr;

    uint32_t currentState = 0;
    uint32_t currentFrame = 0;
    float stateTimer = 0.f;
};

struct AnimationEventComponent {
    EntityId sourceEntity;
};

struct AnimationSystem {
    static void
    update(Manager &manager, SpriteManager &spriteManager, EventDispatcher &dispatcher, float dt) {
        auto view = manager.view<TransformComponent, AnimationStateComponent>();

        for (auto entity : view) {
            auto &transform = view.get<TransformComponent>(entity);
            auto &anim = view.get<AnimationStateComponent>(entity);

            if (anim.profile == nullptr ||
                !anim.profile->stateAnimations.contains(anim.currentState))
                continue;

            const AnimationTrack &track = anim.profile->stateAnimations[anim.currentState];

            anim.stateTimer += dt;

            if (anim.stateTimer >= track.frameDuration) {
                anim.stateTimer -= track.frameDuration;

                anim.currentFrame += 1;

                if (anim.currentFrame >= track.frames.size()) {
                    if (track.loop)
                        anim.currentFrame = 0;
                    else
                        anim.currentFrame = track.frames.size() - 1;
                }
            }

            auto eventIter = track.frameEvents.find(anim.currentFrame);

            if (eventIter != track.frameEvents.end()) {
                for (auto eventId : eventIter->second) {

                    auto eventEntityId = manager.addEntity();

                    AnimationEventComponent eventComp = {.sourceEntity = entity};
                    manager.addComponent<AnimationEventComponent>(eventEntityId, &eventComp);

                    dispatcher.dispatchConstruction(manager, eventEntityId, eventId);
                }
            }

            uint32_t spriteIndex = track.frames[anim.currentFrame];

            int columns = track.texture.width / track.frameWidth;

            float srcX = (spriteIndex % columns) * track.frameWidth;
            float srcY = (spriteIndex / columns) * track.frameHeight;

            Rectangle sourceRec = {srcX, srcY, (float)track.frameWidth, (float)track.frameHeight};

            if (transform.facingDirection == -1) {
                sourceRec.width *= -1;
            }

            Rectangle destRec = {
                transform.pos.x, 
                transform.pos.y,
                (float)track.frameWidth,
                (float)track.frameHeight
            };

            Vector2 origin = { 
                (float)track.frameWidth / 2.0f, 
                (float)track.frameHeight / 2.0f 
            };

            DrawTexturePro(track.texture, sourceRec, destRec, origin, 0.0f, WHITE);
        }
    }
};

#endif