#ifndef MANAGER_HPP
#define MANAGER_HPP
#pragma once

#include <algorithm>
#include <concepts>
#include <functional>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <cstring>

#include "Archetype.hpp"
#include "Table.hpp"
#include "Component.hpp"
#include "Entity.hpp"

// --- Meta-programming utilities (Unchanged) ---
template <typename... Ts> struct TypeList {};
template <typename L1, typename L2> struct ConcatLists;
template <typename... Ts, typename... Ys> struct ConcatLists<TypeList<Ts...>, TypeList<Ys...>> {
    using type = TypeList<Ts..., Ys...>;
};

template <typename... Ts> struct FilterEmpty;
template <> struct FilterEmpty<> { using type = TypeList<>; };

template <typename T, typename... Rest> struct FilterEmpty<T, Rest...> {
    using type = typename ConcatLists<
        std::conditional_t<std::is_empty_v<T>, TypeList<>, TypeList<T>>,
        typename FilterEmpty<Rest...>::type
    >::type;
};

// The global record now tracks the logical group (Archetype) 
// and the physical memory row inside the Table!
struct Record {
    Archetype *archetype;
    size_t row; // Physically represents the Table Row
};

template <typename... Components> struct View;

struct Manager {

    std::unordered_map<ComponentId, size_t> component_size;
    std::unordered_map<EntityId, Record> entity_index;
    
    // Logical groupings (Tags + Data)
    std::unordered_map<ArchSignature_t, Archetype> archetype_index;
    std::unordered_map<ArchetypeId, Archetype *> archetypeIdIndex;
    
    // Physical groupings (Data only)
    std::unordered_map<TableSignature_t, Table> table_index;
    std::unordered_map<TableID, Table *> tableIdIndex;
    
    // Maps ComponentId -> TableID -> Array Column Index
    std::unordered_map<ComponentId, std::unordered_map<TableID, size_t>> table_column_index;

    EntityId entityIdCount = 0;
    ArchetypeId archetypeIdCount = 0;
    TableID tableIdCount = 0;

    EntityId addEntity() {
        EntityId newId = entityIdCount++;
        Record record = {.archetype = nullptr, .row = 0};
        entity_index[newId] = record;
        return newId;
    }

    void destroyEntity(EntityId entity) {
        auto it = entity_index.find(entity);
        if (it == entity_index.end()) return;

        Record record = it->second;

        if (record.archetype != nullptr) {
            Archetype *arch = record.archetype;
            Table *table = arch->dataTable;

            // 1. Remove from logical Archetype (Swap and pop on entities list)
            auto entIt = std::find(arch->entities.begin(), arch->entities.end(), entity);
            if (entIt != arch->entities.end()) {
                *entIt = arch->entities.back();
                arch->entities.pop_back();
            }

            // 2. Remove from physical Table (Swap and pop on byte arrays)
            if (table != nullptr) {
                size_t rowToDelete = record.row;
                size_t lastRow = table->tableEntities.size() - 1;

                if (rowToDelete != lastRow) {
                    EntityId entityToMove = table->tableEntities[lastRow];
                    
                    table->tableEntities[rowToDelete] = entityToMove;
                    entity_index[entityToMove].row = rowToDelete;

                    for (ComponentId cid : table->componentIds) {
                        size_t compSize = component_size[cid];
                        size_t col = table_column_index[cid][table->id];

                        std::memcpy(&table->components[col][rowToDelete * compSize],
                                    &table->components[col][lastRow * compSize],
                                    compSize);
                    }
                }

                table->tableEntities.pop_back();

                for (ComponentId cid : table->componentIds) {
                    size_t compSize = component_size[cid];
                    size_t col = table_column_index[cid][table->id];
                    table->components[col].resize(lastRow * compSize);
                }
            }
        }

        entity_index.erase(it);
    }

    template <typename T> T *getComponent(EntityId entity) {
        if constexpr (std::is_empty_v<T>) return nullptr;
        static const ComponentId component = ComponentID<T>::_id;

        Record &record = entity_index[entity];
        if (!record.archetype || !record.archetype->dataTable) return nullptr;

        Table *table = record.archetype->dataTable;
        if (!table->signature.test(component)) return nullptr;

        size_t col = table_column_index[component][table->id];
        return (T *)&table->components[col][record.row * sizeof(T)];
    }

    template <typename T> T &getComponentSure(EntityId entity) {
        static const ComponentId component = ComponentID<T>::_id;
        Record &record = entity_index[entity];
        Table *table = record.archetype->dataTable;
        size_t col = table_column_index[component][table->id];
        return (T &)table->components[col][record.row * sizeof(T)];
    }

    Archetype *getArchetype(EntityId entity) {
        return entity_index[entity].archetype;
    }

    template <typename T> void setComponent(EntityId entity, T &component) {
        T *toSetComp = getComponent<T>(entity);
        if (toSetComp != nullptr) std::memcpy(toSetComp, (void *)&component, sizeof(T));
    }

    template <typename T>
    void addComponent(EntityId entity, const T *initialValue = nullptr)
        requires std::is_trivially_copyable_v<T>
    {
        static const ComponentId component = ComponentID<T>::_id;
        static const size_t componentSize = std::is_empty_v<T> ? 0 : sizeof(T);

        if (has_component(entity, component)) return;
        component_size[component] = componentSize;

        Record &record = entity_index[entity];
        Archetype *currentArchtype = record.archetype;
        Archetype *nextArchtype = nullptr;

        if (currentArchtype == nullptr) {
            ArchSignature_t rootArch;
            rootArch.set(component);
            nextArchtype = getOrCreateArchetype(rootArch);
        } else {
            ArchetypeEdge &edge = currentArchtype->edges[component];
            if (edge.add != nullptr) {
                nextArchtype = edge.add;
            } else {
                ArchSignature_t newSignature = currentArchtype->typeSet;
                newSignature.set(component);
                nextArchtype = getOrCreateArchetype(newSignature);
                edge.add = nextArchtype;
                nextArchtype->edges[component].remove = currentArchtype;
            }
        }

        // 1. Logical Group Move
        if (currentArchtype) removeEntityFromArchetype(currentArchtype, entity);
        nextArchtype->entities.push_back(entity);

        Table *oldTable = currentArchtype ? currentArchtype->dataTable : nullptr;
        Table *newTable = nextArchtype->dataTable;

        // --- THE HOLY GRAIL: ZERO COST TAG SWITCH ---
        if (oldTable == newTable) {
            record.archetype = nextArchtype;
            return; // No memory moved!
        }

        // 2. Physical Data Move
        size_t newRow = newTable->tableEntities.size();
        newTable->tableEntities.push_back(entity);

        for (ComponentId cid : newTable->componentIds) {
            size_t compSize = component_size[cid];
            size_t col = table_column_index[cid][newTable->id];
            newTable->components[col].resize((newRow + 1) * compSize);
        }

        if (oldTable != nullptr) {
            size_t oldRow = record.row;

            for (ComponentId cid : oldTable->componentIds) {
                if (!newTable->signature.test(cid)) continue;
                size_t compSize = component_size[cid];
                size_t oldCol = table_column_index[cid][oldTable->id];
                size_t newCol = table_column_index[cid][newTable->id];

                std::memcpy(&newTable->components[newCol][newRow * compSize],
                            &oldTable->components[oldCol][oldRow * compSize],
                            compSize);
            }

            size_t lastRow = oldTable->tableEntities.size() - 1;
            EntityId lastEntity = oldTable->tableEntities.back();

            if (oldRow != lastRow) {
                for (ComponentId cid : oldTable->componentIds) {
                    size_t compSize = component_size[cid];
                    size_t col = table_column_index[cid][oldTable->id];
                    std::memcpy(&oldTable->components[col][oldRow * compSize],
                                &oldTable->components[col][lastRow * compSize],
                                compSize);
                }
                entity_index[lastEntity].row = oldRow;
                oldTable->tableEntities[oldRow] = lastEntity;
            }

            for (ComponentId cid : oldTable->componentIds) {
                size_t compSize = component_size[cid];
                size_t col = table_column_index[cid][oldTable->id];
                oldTable->components[col].resize(lastRow * compSize);
            }
            oldTable->tableEntities.pop_back();
        }

        if (initialValue != nullptr && !std::is_empty_v<T>) {
            size_t newCol = table_column_index[component][newTable->id];
            std::memcpy(&newTable->components[newCol][newRow * sizeof(T)],
                        (void *)initialValue, sizeof(T));
        }

        record.archetype = nextArchtype;
        record.row = newRow;
    }

    template <typename... Ts> 
    void addComponents(EntityId entity, const Ts *...initialValues) {
        static_assert(sizeof...(Ts) > 0, "Must add at least one component.");

        // 1. Register component sizes
        ((component_size[ComponentID<Ts>::_id] = std::is_empty_v<Ts> ? 0 : sizeof(Ts)), ...);

        Record &record = entity_index[entity];
        Archetype *currentArchtype = record.archetype;

        // 2. Calculate the FINAL destination signature instantly using a fold expression
        ArchSignature_t targetSignature;
        if (currentArchtype != nullptr) {
            targetSignature = currentArchtype->typeSet;
        }
        (targetSignature.set(ComponentID<Ts>::_id), ...);

        Archetype *nextArchtype = getOrCreateArchetype(targetSignature);

        // 3. Edge Case: The entity already has all these components!
        if (currentArchtype == nextArchtype) {
            // Just update the values if pointers were provided
            (
                [&]() {
                    if (initialValues != nullptr && !std::is_empty_v<Ts>) {
                        size_t col = table_column_index[ComponentID<Ts>::_id][nextArchtype->dataTable->id];
                        std::memcpy(&nextArchtype->dataTable->components[col][record.row * sizeof(Ts)],
                                    (void *)initialValues,
                                    sizeof(Ts));
                    }
                }(),
                ...);
            return;
        }

        // 4. Logical Group Move (Fast)
        if (currentArchtype != nullptr) {
            removeEntityFromArchetype(currentArchtype, entity);
        }
        nextArchtype->entities.push_back(entity);

        Table *oldTable = currentArchtype ? currentArchtype->dataTable : nullptr;
        Table *newTable = nextArchtype->dataTable;

        // --- ZERO COST TAG SWITCH ---
        // If we only added tags (0-byte components), the table doesn't change!
        if (oldTable == newTable) {
            record.archetype = nextArchtype;
            return; // We skip the physical move entirely!
        }

        // 5. Physical Data Move (We only do this ONCE for all new components)
        size_t newRow = newTable->tableEntities.size();
        newTable->tableEntities.push_back(entity);

        for (ComponentId cid : newTable->componentIds) {
            size_t compSize = component_size[cid];
            size_t col = table_column_index[cid][newTable->id];
            newTable->components[col].resize((newRow + 1) * compSize);
        }

        // Copy existing intersection data from the old table
        if (oldTable != nullptr) {
            size_t oldRow = record.row;

            for (ComponentId cid : oldTable->componentIds) {
                if (!newTable->signature.test(cid)) continue;
                size_t compSize = component_size[cid];
                size_t oldCol = table_column_index[cid][oldTable->id];
                size_t newCol = table_column_index[cid][newTable->id];

                std::memcpy(&newTable->components[newCol][newRow * compSize],
                            &oldTable->components[oldCol][oldRow * compSize],
                            compSize);
            }

            // Swap and Pop in the old table
            size_t lastRow = oldTable->tableEntities.size() - 1;
            EntityId lastEntity = oldTable->tableEntities.back();

            if (oldRow != lastRow) {
                for (ComponentId cid : oldTable->componentIds) {
                    size_t compSize = component_size[cid];
                    size_t col = table_column_index[cid][oldTable->id];
                    std::memcpy(&oldTable->components[col][oldRow * compSize],
                                &oldTable->components[col][lastRow * compSize],
                                compSize);
                }
                entity_index[lastEntity].row = oldRow;
                oldTable->tableEntities[oldRow] = lastEntity;
            }

            for (ComponentId cid : oldTable->componentIds) {
                size_t compSize = component_size[cid];
                size_t col = table_column_index[cid][oldTable->id];
                oldTable->components[col].resize(lastRow * compSize);
            }
            oldTable->tableEntities.pop_back();
        }

        // 6. Initialize the brand new components
        (
            [&]() {
                if (initialValues != nullptr && !std::is_empty_v<Ts>) {
                    ComponentId cid = ComponentID<Ts>::_id;
                    size_t newCol = table_column_index[cid][newTable->id];
                    std::memcpy(&newTable->components[newCol][newRow * sizeof(Ts)],
                                (void *)initialValues,
                                sizeof(Ts));
                }
            }(),
            ...);

        record.archetype = nextArchtype;
        record.row = newRow;
    }

    template <typename... Components> 
    void addComponents(EntityId entity) {
        static_assert(sizeof...(Components) > 0, "Must add at least one component.");
        addComponents<Components...>(entity, static_cast<const Components *>(nullptr)...);
    }

    template <typename T> void removeComponent(EntityId entity) {
        static const ComponentId component = ComponentID<T>::_id;

        if (!has_component(entity, component)) return;

        Record &record = entity_index[entity];
        Archetype *currentArchtype = record.archetype;
        if (currentArchtype == nullptr) return;

        Archetype *nextArchtype = nullptr;
        ArchetypeEdge &edge = currentArchtype->edges[component];

        if (edge.remove != nullptr) {
            nextArchtype = edge.remove;
        } else {
            ArchSignature_t newSignature = currentArchtype->typeSet;
            newSignature.reset(component);
            if (newSignature.any()) {
                nextArchtype = getOrCreateArchetype(newSignature);
                edge.remove = nextArchtype;
                nextArchtype->edges[component].add = currentArchtype;
            }
        }

        removeEntityFromArchetype(currentArchtype, entity);
        if (nextArchtype) nextArchtype->entities.push_back(entity);

        Table *oldTable = currentArchtype->dataTable;
        Table *newTable = nextArchtype ? nextArchtype->dataTable : nullptr;

        // --- ZERO COST TAG SWITCH ---
        if (oldTable == newTable) {
            record.archetype = nextArchtype;
            return; 
        }

        size_t newRow = 0;
        if (newTable != nullptr) {
            newRow = newTable->tableEntities.size();
            newTable->tableEntities.push_back(entity);

            for (ComponentId cid : newTable->componentIds) {
                size_t compSize = component_size[cid];
                size_t col = table_column_index[cid][newTable->id];
                newTable->components[col].resize((newRow + 1) * compSize);
            }
        }

        size_t oldRow = record.row;

        if (newTable != nullptr) {
            for (ComponentId cid : oldTable->componentIds) {
                if (cid == component || !newTable->signature.test(cid)) continue;
                size_t compSize = component_size[cid];
                size_t oldCol = table_column_index[cid][oldTable->id];
                size_t newCol = table_column_index[cid][newTable->id];

                std::memcpy(&newTable->components[newCol][newRow * compSize],
                            &oldTable->components[oldCol][oldRow * compSize],
                            compSize);
            }
        }

        size_t lastRow = oldTable->tableEntities.size() - 1;
        EntityId lastEntity = oldTable->tableEntities.back();

        if (oldRow != lastRow) {
            for (ComponentId cid : oldTable->componentIds) {
                size_t compSize = component_size[cid];
                size_t col = table_column_index[cid][oldTable->id];
                std::memcpy(&oldTable->components[col][oldRow * compSize],
                            &oldTable->components[col][lastRow * compSize],
                            compSize);
            }
            entity_index[lastEntity].row = oldRow;
            oldTable->tableEntities[oldRow] = lastEntity;
        }

        for (ComponentId cid : oldTable->componentIds) {
            size_t compSize = component_size[cid];
            size_t col = table_column_index[cid][oldTable->id];
            oldTable->components[col].resize(lastRow * compSize);
        }
        oldTable->tableEntities.pop_back();

        record.archetype = nextArchtype;
        record.row = newRow;
    }

    template <typename _SrcComp, typename _DstComp>
    void replaceComponent(EntityId entity, const _DstComp *initialValue = nullptr) {
        static_assert(ComponentID<_SrcComp>::_id != ComponentID<_DstComp>::_id);

        Record &record = entity_index[entity];
        Archetype *currentArchetype = record.archetype;
        static const ComponentId srcId = ComponentID<_SrcComp>::_id;
        static const ComponentId dstId = ComponentID<_DstComp>::_id;

        if (currentArchetype == nullptr || !has_component(entity, srcId) || has_component(entity, dstId)) return;

        component_size[dstId] = std::is_empty_v<_DstComp> ? 0 : sizeof(_DstComp);

        Archetype *withDstArch = nullptr;
        auto &edge = currentArchetype->edges[srcId];

        if (edge.replace.contains(dstId)) {
            withDstArch = edge.replace[dstId];
        } else {
            ArchSignature_t withDstSignature = currentArchetype->typeSet;
            withDstSignature.reset(srcId);
            withDstSignature.set(dstId);
            withDstArch = getOrCreateArchetype(withDstSignature);
            edge.replace[dstId] = withDstArch;
        }

        removeEntityFromArchetype(currentArchetype, entity);
        withDstArch->entities.push_back(entity);

        Table *oldTable = currentArchetype->dataTable;
        Table *newTable = withDstArch->dataTable;

        // --- ZERO COST TAG SWITCH ---
        if (oldTable == newTable) {
            record.archetype = withDstArch;
            return; 
        }

        size_t newRow = newTable->tableEntities.size();
        newTable->tableEntities.push_back(entity);
        
        for (ComponentId cid : newTable->componentIds) {
            size_t compSize = component_size[cid];
            size_t col = table_column_index[cid][newTable->id];
            newTable->components[col].resize((newRow + 1) * compSize);
        }

        size_t oldRow = record.row;

        for (ComponentId cid : oldTable->componentIds) {
            if (cid == srcId || !newTable->signature.test(cid)) continue;
            size_t compSize = component_size[cid];
            size_t oldCol = table_column_index[cid][oldTable->id];
            size_t newCol = table_column_index[cid][newTable->id];

            std::memcpy(&newTable->components[newCol][newRow * compSize],
                        &oldTable->components[oldCol][oldRow * compSize],
                        compSize);
        }

        if (initialValue != nullptr && !std::is_empty_v<_DstComp>) {
            size_t newCol = table_column_index[dstId][newTable->id];
            std::memcpy(&newTable->components[newCol][newRow * sizeof(_DstComp)],
                        initialValue, sizeof(_DstComp));
        }

        size_t lastRow = oldTable->tableEntities.size() - 1;
        EntityId lastEntity = oldTable->tableEntities.back();

        if (oldRow != lastRow) {
            for (ComponentId cid : oldTable->componentIds) {
                size_t compSize = component_size[cid];
                size_t col = table_column_index[cid][oldTable->id];
                std::memcpy(&oldTable->components[col][oldRow * compSize],
                            &oldTable->components[col][lastRow * compSize],
                            compSize);
            }
            entity_index[lastEntity].row = oldRow;
            oldTable->tableEntities[oldRow] = lastEntity;
        }

        for (ComponentId cid : oldTable->componentIds) {
            size_t compSize = component_size[cid];
            size_t col = table_column_index[cid][oldTable->id];
            oldTable->components[col].resize(lastRow * compSize);
        }
        oldTable->tableEntities.pop_back();

        record.row = newRow;
        record.archetype = withDstArch;
    }

    bool has_component(EntityId entity, ComponentId component) {
        Archetype *archetype = entity_index[entity].archetype;
        if (archetype == nullptr) return false;
        return archetype->typeSet.test(component);
    }

    template <typename... Components> std::vector<Archetype *> queryArchtypes() {
        ArchSignature_t querySig;
        (querySig.set(ComponentID<Components>::_id), ...);
        std::vector<Archetype *> queryRes;

        for (auto &[archId, arch] : archetypeIdIndex) {
            if ((arch->typeSet & querySig) == querySig) {
                queryRes.push_back(arch);
            }
        }
        return queryRes;
    }

    template <typename... Components, typename Func> void runSystem(Func &&systemFunction) {
        if constexpr (sizeof...(Components) == 0) return;

        using FilteredList = typename FilterEmpty<Components...>::type;
        auto queriedArchetypes = queryArchtypes<Components...>();

        auto executeLoops = [&]<typename... Filtered>(TypeList<Filtered...>) {
            for (auto arch : queriedArchetypes) {
                if (arch->entities.empty()) continue;
                Table *table = arch->dataTable;

                // Cache the base pointers of the arrays inside the Table
                const std::tuple<Filtered *...> basePointers = {
                    reinterpret_cast<Filtered *>(
                        table->components[table_column_index[ComponentID<Filtered>::_id][table->id]].data()
                    )...
                };

                // Iterate over the logical entities, fetching their exact row in the shared Table
                for (EntityId e : arch->entities) {
                    size_t physRow = entity_index[e].row;
                    std::apply([&](auto *...compArray) {
                        systemFunction(compArray[physRow]...);
                    }, basePointers);
                }
            }
        };

        executeLoops(FilteredList{});
    }

    template<typename... Ts>
    View<Ts...> view();

  private:
    void removeEntityFromArchetype(Archetype* arch, EntityId entity) {
        auto it = std::find(arch->entities.begin(), arch->entities.end(), entity);
        if (it != arch->entities.end()) {
            *it = arch->entities.back();
            arch->entities.pop_back();
        }
    }

    Table *getOrCreateTable(const TableSignature_t &signature) {
        auto it = table_index.find(signature);
        if (it != table_index.end()) return &it->second;

        TableID newId = tableIdCount++;
        auto [insertedIt, success] = table_index.try_emplace(signature, Table{newId});
        Table &newTable = insertedIt->second;
        
        newTable.signature = signature;
        tableIdIndex[newId] = &newTable;

        size_t col = 0;
        for (size_t i = 0; i < MAX_COMPONENTS; ++i) {
            if (signature.test(i)) {
                newTable.componentIds.push_back(i);
                table_column_index[i][newId] = col++;
            }
        }
        
        newTable.components.resize(newTable.componentIds.size());
        return &newTable;
    }

    Archetype *getOrCreateArchetype(const ArchSignature_t &signature) {
        auto archIt = archetype_index.find(signature);
        if (archIt != archetype_index.end()) return &archIt->second;

        ArchetypeId newId = archetypeIdCount++;
        auto [insertedIt, success] = archetype_index.try_emplace(signature, Archetype{newId});
        Archetype &newArchtype = insertedIt->second;

        newArchtype.typeSet = signature;
        archetypeIdIndex[newId] = &newArchtype;

        // Generate the Table signature by filtering out tags (0-byte components)
        TableSignature_t tableSig;
        size_t trueSize = 0;
        for (size_t i = 0; i < MAX_COMPONENTS; ++i) {
            if (signature.test(i)) {
                if (component_size[i] > 0) {
                    tableSig.set(i);
                    trueSize++;
                }
            }
        }

        newArchtype.trueComponentCount = trueSize;
        newArchtype.dataTable = getOrCreateTable(tableSig);

        return &newArchtype;
    }
};

#endif