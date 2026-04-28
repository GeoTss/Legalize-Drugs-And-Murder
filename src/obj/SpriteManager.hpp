#ifndef SPRITE_MANAGER_HPP
#define SPRITE_MANAGER_HPP
#pragma once

#include <memory>
#include <numeric>
#include <raylib.h>
#include <unordered_map>
#include <vector>

#include "ECS/Entity.hpp"

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