#ifndef EVENT_DISPATCHER_HPP
#define EVENT_DISPATCHER_HPP
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>

#include "ECS/Manager.hpp"

constexpr uint64_t fnv1a_64(const char *str, size_t len) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint64_t>(str[i]);
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

template <typename T> constexpr uint64_t typeHash() {
#if defined(__clang__) || defined(__GNUC__)
    return fnv1a_64(__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__) - 1);
#elif defined(_MSC_VER)
    return fnv1a_64(__FUNCSIG__, sizeof(__FUNCSIG__) - 1);
#else
#error "Unsupported compiler for compile-time type hashing."
#endif
}

template <typename... Tags> constexpr uint64_t hashTags() {
    if constexpr (sizeof...(Tags) == 0) {
        return 0;
    } else {

        uint64_t hashes[] = {typeHash<Tags>()...};
        constexpr size_t count = sizeof...(Tags);

        for (size_t i = 0; i < count; ++i) {
            for (size_t j = 0; j < count - i - 1; ++j) {
                if (hashes[j] > hashes[j + 1]) {
                    uint64_t temp = hashes[j];
                    hashes[j] = hashes[j + 1];
                    hashes[j + 1] = temp;
                }
            }
        }

        uint64_t combined = 0xcbf29ce484222325ULL;
        for (size_t i = 0; i < count; ++i) {
            combined ^= hashes[i];
            combined *= 0x100000001b3ULL;
        }
        return combined;
    }
}

struct EventDispatcher {
    std::unordered_map<uint64_t, std::function<void(Manager &, EntityId, const std::chrono::steady_clock::time_point&, const std::chrono::milliseconds&)>> eventMap;

    template <typename... Tags> uint64_t registerEvent() {
        constexpr uint64_t hash = hashTags<Tags...>();

        eventMap[hash] = [](Manager &manager, EntityId entity, const std::chrono::steady_clock::time_point& startPoint, const std::chrono::milliseconds& duration) {
            AnimationEventComponent eventComp = {
                .sourceEntity = entity, .startPoint = startPoint, .duration = duration};
            
            manager.addComponent<AnimationEventComponent>(entity, &eventComp);
            manager.addComponents<Tags...>(entity);
        };

        return hash;
    }

    void dispatchConstruction(Manager &manager, EntityId entity, uint64_t hash, const std::chrono::steady_clock::time_point& startPoint, const std::chrono::milliseconds& duration) {
        auto it = eventMap.find(hash);
        if (it != eventMap.end())
            it->second(manager, entity, startPoint, duration);
    }
};

#endif