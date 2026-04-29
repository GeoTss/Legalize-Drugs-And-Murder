#ifndef ARCHETYPE_HPP
#define ARCHETYPE_HPP
#pragma once

#include <array>
#include <vector>
#include <ostream>

#include "ComponentFamily.hpp"
#include "Table.hpp"
#include "Entity.hpp"

using ArchSignature_t = std::bitset<MAX_COMPONENTS>;
using ArchetypeId = std::uint32_t;

struct ArchetypeEdge {
    ArchetypeId add = 0;
    ArchetypeId remove = 0;
};

// Just for component representaion
struct Archetype {

    std::array<ArchetypeEdge, MAX_COMPONENTS> edges;
    ArchSignature_t typeSet;
    
    Table dataTable;

    std::vector<EntityId> entities;

    ArchetypeId id;
    uint16_t trueComponentCount;

    Archetype() = default;
    Archetype(ArchetypeId _id) : id{_id}, trueComponentCount{0} {}

    friend std::ostream &operator<<(std::ostream &os, const Archetype &arch) {
        os << "Archetype ID: " << arch.id << '\n';
        os << "Entity count: " << arch.entities.size() << '\n';
        os << arch.dataTable;
        return os;
    }
};

#endif