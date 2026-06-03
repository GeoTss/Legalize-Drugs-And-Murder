#ifndef ENTITY_HPP
#define ENTITY_HPP
#pragma once

#include <cstdint>

using EntityId = uint32_t;

constexpr uint32_t getEntityIndex(EntityId id) {
    return id & 0xFFFFF;
}

constexpr uint32_t getEntityGeneration(EntityId id) {
    return (id >> 20) & 0xFFF;
}

constexpr EntityId createEntityId(uint32_t index, uint32_t generation) {
    return (generation << 20) | index;
}

#endif