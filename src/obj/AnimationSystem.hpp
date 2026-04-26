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
    uint32_t currentFrame = 0;
    float stateTimer = 0.f;
    int lastProcessedFrame = -1;
};

using namespace std::chrono_literals;

struct AnimationSystem {
    // 1. ADDED Camera3D TO THE ARGUMENTS
    static void
    update(Manager &manager, SpriteManager &spriteManager, EventDispatcher &dispatcher, float dt) {
        manager.runSystem<TransformComponent, AnimationStateComponent, StateComponent>([&manager, &spriteManager, &dispatcher, dt](EntityId entity, TransformComponent& transform, AnimationStateComponent& anim, StateComponent& stateComponent){
            
            uint8_t currentState = stateComponent.stateID;

            if (anim.profile == nullptr ||
                !anim.profile->stateAnimations.contains(currentState))
                return;

            const AnimationTrack &track = anim.profile->stateAnimations[currentState];

            anim.stateTimer += dt;

            if (anim.stateTimer >= track.frameDuration) {
                anim.stateTimer = 0;

                anim.currentFrame += 1;

                if (anim.currentFrame >= track.frames.size()) {
                    if (track.loop)
                        anim.currentFrame = 0;
                    else{
                        anim.currentFrame = track.frames.size() - 1;
                        manager.addComponent<AnimationCompleteTag>(entity);
                    }
                }
            }

            if (anim.lastProcessedFrame != anim.currentFrame) {
                auto eventIter = track.frameEvents.find(anim.currentFrame);
                if (eventIter != track.frameEvents.end() && !eventIter->second.empty()) {

                    for (auto eventId : eventIter->second) {

                        auto eventEntityId = manager.addEntity();
                        dispatcher.dispatchConstruction(manager, eventEntityId, entity, eventId);
                    }
                }
                anim.lastProcessedFrame = anim.currentFrame;
            }

            uint32_t spriteIndex = track.frames[anim.currentFrame];

            int columns = track.texture.width / track.frameWidth;

            float srcX = (spriteIndex % columns) * track.frameWidth;
            float srcY = (spriteIndex / columns) * track.frameHeight;

            Rectangle sourceRec = {srcX, srcY, (float)track.frameWidth, (float)track.frameHeight};

            if (transform.facingDirection == -1) {
                sourceRec.width *= -1;
            }

            Rectangle destRec = {transform.pos.x,
                                 transform.pos.y,
                                 (float)track.frameWidth,
                                 (float)track.frameHeight};

            Vector2 origin = {(float)track.frameWidth / 2.0f, (float)track.frameHeight / 2.0f};

            DrawTexturePro(track.texture, sourceRec, destRec, origin, 0.0f, WHITE);
        });
    }
};

#endif