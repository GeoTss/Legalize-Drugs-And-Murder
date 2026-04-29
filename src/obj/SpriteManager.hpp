#ifndef SPRITE_MANAGER_HPP
#define SPRITE_MANAGER_HPP
#pragma once

#include <memory>
#include <numeric>
#include <raylib.h>
#include <unordered_map>
#include <vector>

#include "ECS/Entity.hpp"
#include "ECS/Component.hpp"

struct SpriteManager {
    std::unordered_map<std::string, std::unique_ptr<AnimationProfile>> loadedProfiles;

    ~SpriteManager() {
        for (auto &[name, profile] : loadedProfiles) {
            for (auto &[stateId, track] : profile->stateAnimations) {
                if (track.texture.id != 0) {
                    UnloadTexture(track.texture);
                }
            }
        }
    }

    AnimationProfile *createProfile(const std::string &assetName) {
        auto profilePtr = std::make_unique<AnimationProfile>();
        loadedProfiles[assetName] = std::move(profilePtr);

        return loadedProfiles[assetName].get();
    }

    AnimationProfile *getProfile(const std::string &assetName) {
        auto profileIt = loadedProfiles.find(assetName);
        if (profileIt != loadedProfiles.end()) {
            return profileIt->second.get();
        }
        return nullptr;
    }

    void addAnimationTrack(const std::string assetName,
                           const AnimationTrack &track,
                           const uint32_t stateId) {
        AnimationProfile *profile = getProfile(assetName);

        if (profile == nullptr)
            return;

        profile->stateAnimations[stateId] = track;
    }

    void addAnimationTrack(const std::string assetName,
                           const AnimationTrack &&track,
                           const uint32_t stateId) {
        AnimationProfile *profile = getProfile(assetName);

        if (profile == nullptr)
            return;

        profile->stateAnimations[stateId] = track;
    }
};

#endif