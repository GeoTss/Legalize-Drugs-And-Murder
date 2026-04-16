#ifndef ARCHETYPE_HPP
#define ARCHETYPE_HPP
#pragma once

#include "Component.hpp"
#include "Entity.hpp"
#include <cstddef>
#include <ostream>
#include <unordered_map>
#include <vector>
#include <bitset>

using ArchetypeId = std::uint16_t;

constexpr size_t MAX_COMPONENTS = 64;
using ArchSignature_t = std::bitset<MAX_COMPONENTS>;

struct Archetype;

struct Column {
    std::vector<std::byte> data;
    size_t elementSize;
};

struct ArchetypeEdge {
    Archetype *add = nullptr;
    Archetype *remove = nullptr;

    std::unordered_map<ComponentId, Archetype *> replace;
};

struct Archetype {

    std::unordered_map<ComponentId, ArchetypeEdge> edges;
    ArchSignature_t typeSet;
    std::vector<ComponentId> componentIds;
    std::vector<std::vector<std::byte>> components;
    std::vector<EntityId> entities;

    ArchetypeId id;
    uint16_t trueComponentCount;

    Archetype() = default;
    Archetype(ArchetypeId _id) : id{_id} {}

    friend std::ostream &operator<<(std::ostream &os, const Archetype &arch) {
        os << "Entity count: " << arch.entities.size() << '\n';

        os << "Component count: " << arch.typeSet.size() << '\n';
        os << "Component Ids: [ ";
        for (const auto cid : arch.componentIds) {
            os << cid << ' ';
        }
        os << "]\n";

        size_t totalArchCompSize = 0;
        os << "Component row sizes: [ ";
        for (const auto &componentRow : arch.components) {
            os << componentRow.size() << ' ';
            totalArchCompSize += componentRow.size();
        }
        os << "]\n";

        os << "Total size of components: " << totalArchCompSize << " bytes\n";

        return os;
    }
};

#endif