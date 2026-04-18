#ifndef TABLE_HPP
#define TABLE_HPP
#pragma once

#include <cstdint>
#include <bitset>
#include <vector>
#include <ostream>

#include "Component.hpp"
#include "Entity.hpp"

using TableID = std::uint16_t;

constexpr size_t MAX_COMPONENTS = 64;
using TableSignature_t = std::bitset<MAX_COMPONENTS>;

// Just for component data
struct Table {
    
    TableSignature_t signature;
    std::vector<ComponentId> componentIds;
    std::vector<std::vector<std::byte>> components;

    std::vector<std::uint64_t> tableEntities;

    TableID id;

    Table() = default;
    Table(TableID _id) : id{_id} {}

    friend std::ostream &operator<<(std::ostream &os, const Table &table) {

        os << "Component count: " << table.signature.size() << '\n';
        os << "Component Ids: [ ";
        for (const auto cid : table.componentIds) {
            os << cid << ' ';
        }
        os << "]\n";

        size_t totalTableCompSize = 0;
        os << "Component row sizes: [ ";
        for (const auto &componentRow : table.components) {
            os << componentRow.size() << ' ';
            totalTableCompSize += componentRow.size();
        }
        os << "]\n";

        os << "Total size of components: " << totalTableCompSize << " bytes\n";

        return os;
    }
};

#endif