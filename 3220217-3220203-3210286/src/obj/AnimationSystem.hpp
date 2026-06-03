#ifndef ANIMATION_SYSTEM_HPP
#define ANIMATION_SYSTEM_HPP
#pragma once

#include <iostream>
#include <raylib.h>
#include <stdint.h>
#include <unordered_map>
#include <vector>

#include "./ECS/CommandBuffer.hpp"
#include "./ECS/Component.hpp"
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

using namespace std::chrono_literals;

struct AnimationSystem {
    static void
    update(Manager &manager, SpriteManager &spriteManager, EventDispatcher &dispatcher, const float dt) {

        DeferredCommandBuffer cmd(manager);

        manager.runSystem<TransformComponent, AnimationStateComponent, StateComponent>(
            [&manager, &cmd, &dispatcher, dt](EntityId entity,
                                              TransformComponent &transform,
                                              AnimationStateComponent &anim,
                                              StateComponent &stateComponent) {
                uint8_t currentState = stateComponent.stateID;

                if (anim.profile == nullptr ||
                    !anim.profile->stateAnimations.contains(currentState))
                    return;

                const AnimationTrack &track = anim.profile->stateAnimations[currentState];

                anim.stateTimer += dt;

                if (anim.stateTimer >= track.frameDuration) {
                    anim.stateTimer = 0;
                    anim.currentFrame += 1;
                }

                if (anim.currentFrame >= track.frames.size()) {
                    if (track.loop)
                        anim.currentFrame = 0;
                    else {
                        anim.currentFrame = track.frames.size() - 1;
                        cmd.addComponent<AnimationCompleteTag>(entity);
                    }
                }

                if (anim.lastProcessedFrame != anim.currentFrame) {
                    auto eventIter = track.frameEvents.find(anim.currentFrame);
                    if (eventIter != track.frameEvents.end() && !eventIter->second.empty()) {
                        for (auto eventId : eventIter->second) {

                            auto eventEntityId = manager.addEntity();
                            dispatcher.dispatchConstruction(cmd, eventEntityId, entity, eventId);
                        }
                    }
                    anim.lastProcessedFrame = anim.currentFrame;
                }
            });

        cmd.execute();
    }

    static void render(Manager &manager) {
        manager.runSystem<TransformComponent, AnimationStateComponent, StateComponent>(
            [](TransformComponent &transform,
               AnimationStateComponent &anim,
               StateComponent &stateComp) {
                uint8_t currentState = stateComp.stateID;

                if (anim.profile == nullptr ||
                    !anim.profile->stateAnimations.contains(currentState))
                    return;

                const AnimationTrack &track = anim.profile->stateAnimations[currentState];

                uint32_t spriteIndex = track.frames[anim.currentFrame];
                int columns = track.texture.width / track.frameWidth;

                float srcX = (spriteIndex % columns) * track.frameWidth;
                float srcY = (spriteIndex / columns) * track.frameHeight;

                Rectangle sourceRec = {
                    srcX, srcY, (float)track.frameWidth, (float)track.frameHeight};

                if (transform.facingDirection == -1) {
                    sourceRec.width *= -1;
                }

                Rectangle destRec = {transform.pos.x,
                                     transform.pos.y,
                                     (float)track.frameWidth,
                                     (float)track.frameHeight};

                Vector2 origin = {((float)track.frameWidth / 2.0f) + track.trueCenterOffsetX,
                                  ((float)track.frameHeight / 2.0f) + track.trueCenterOffsetY};

                DrawTexturePro(track.texture, sourceRec, destRec, origin, 0.0f, WHITE);
            });
    }
};

#endif