#ifndef EVENT_DISPATCHER_HPP
#define EVENT_DISPATCHER_HPP
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>

#include "ECS/Manager.hpp"
#include "ECS/Component.hpp"
#include "ECS/CommandBuffer.hpp" // <-- Include the Command Buffer

struct EventDispatcher {
    std::unordered_map<uint64_t, std::function<void(DeferredCommandBuffer &, const EntityId, const EntityId)>> eventMap;

    uint64_t nextEventId = 1;

    template <typename... Tags> uint64_t registerEvent() {
        uint64_t id = nextEventId++;

        eventMap[id] = [](DeferredCommandBuffer &cmd, const EntityId eventEntity, const EntityId srcEntity) {
            AnimationEventComponent eventComp = { .sourceEntity = srcEntity};
            cmd.addComponent<AnimationEventComponent>(eventEntity, eventComp);
            
            if constexpr (sizeof...(Tags) > 0) {
                (cmd.addComponent<Tags>(eventEntity), ...); 
            }
        };

        return id;
    }

    template <typename T> 
    uint64_t registerPayloadEvent(T payloadData) {
        uint64_t id = nextEventId++;
        
        eventMap[id] = [payloadData](DeferredCommandBuffer &cmd, const EntityId eventEntity, const EntityId srcEntity) {
            AnimationEventComponent eventComp = { .sourceEntity = srcEntity };
            cmd.addComponent<AnimationEventComponent>(eventEntity, eventComp);
            
            cmd.addComponent<T>(eventEntity, payloadData);
        };

        return id;
    }

    void dispatchConstruction(DeferredCommandBuffer &cmd, const EntityId eventEntity, const EntityId srcEntity, uint64_t hash) {
        auto it = eventMap.find(hash);
        if (it != eventMap.end())
            it->second(cmd, eventEntity, srcEntity);
    }
};

#endif