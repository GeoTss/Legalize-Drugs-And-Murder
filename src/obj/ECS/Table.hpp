#ifndef TABLE_HPP
#define TABLE_HPP
#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <ostream>

#include "ComponentFamily.hpp"
#include "PagedColumn.hpp"

constexpr size_t MAX_COMPONENTS = 64;

// Just for component data
struct Table {
    
    std::vector<ComponentId> componentIds;
    std::vector<PagedColumn> components;
    std::array<size_t, MAX_COMPONENTS> column_mapping{};

    Table() {
        column_mapping.fill(static_cast<size_t>(-1));
    }

    friend std::ostream &operator<<(std::ostream &os, const Table &table) {
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