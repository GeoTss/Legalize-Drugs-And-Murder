#ifndef ARCHETYPE_HPP
#define ARCHETYPE_HPP
#pragma once

#include "ComponentFamily.hpp"
#include "Entity.hpp"
#include "Table.hpp"

#include <cstddef>
#include <ostream>
#include <unordered_map>
#include <vector>
#include <bitset>

using ArchetypeId = std::uint16_t;

using ArchSignature_t = std::bitset<MAX_COMPONENTS>;

struct Archetype;

struct ArchetypeEdge {
    Archetype *add = nullptr;
    Archetype *remove = nullptr;
};

// For components + tags representation
struct Archetype {

    std::array<ArchetypeEdge, MAX_COMPONENTS> edges;
    ArchSignature_t typeSet;
    
    Table* dataTable;

    std::vector<EntityId> entities;

    ArchetypeId id;
    uint16_t trueComponentCount;

    Archetype() = default;
    Archetype(ArchetypeId _id) : id{_id} {}

    friend std::ostream &operator<<(std::ostream &os, const Archetype &arch) {
        os << "Entity count: " << arch.entities.size() << '\n';

        return os;
    }
};

#endif