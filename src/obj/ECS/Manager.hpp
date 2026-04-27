#ifndef MANAGER_HPP
#define MANAGER_HPP
#pragma once

#include <algorithm>
#include <concepts>
#include <cstring>
#include <functional>
#include <tuple>
#include <type_traits>
#include <unordered_map>

#include "Archetype.hpp"
#include "Component.hpp"
#include "Entity.hpp"
#include "Table.hpp"
#include "PagedColumn.hpp" // MUST BE INCLUDED

template <typename... Ts> struct TypeList {};
template <typename L1, typename L2> struct ConcatLists;
template <typename... Ts, typename... Ys> struct ConcatLists<TypeList<Ts...>, TypeList<Ys...>> {
    using type = TypeList<Ts..., Ys...>;
};

template <typename... Ts> struct FilterEmpty;
template <> struct FilterEmpty<> {
    using type = TypeList<>;
};

template <typename T, typename... Rest> struct FilterEmpty<T, Rest...> {
    using type =
        typename ConcatLists<std::conditional_t<std::is_empty_v<T>, TypeList<>, TypeList<T>>,
                             typename FilterEmpty<Rest...>::type>::type;
};

struct Record {
    Archetype *archetype = nullptr;
    size_t row = -1;
    uint32_t generation : 12;
};

template <typename... Components> struct View;

struct Manager {

    std::array<size_t, MAX_COMPONENTS> component_size;
    std::vector<Record> entity_index;
    std::vector<EntityId> freeEnttIds;

    std::unordered_map<ArchSignature_t, Archetype> archetype_index;
    std::unordered_map<ArchetypeId, Archetype *> archetypeIdIndex;

    std::vector<std::pair<ArchSignature_t, std::vector<Archetype*>*>> active_queries;

    std::unordered_map<TableSignature_t, Table> table_index;
    std::unordered_map<TableID, Table *> tableIdIndex;

    std::array<ComponentTraits, MAX_COMPONENTS> component_traits;

    EntityId entityIdCount = 0;
    ArchetypeId archetypeIdCount = 0;
    TableID tableIdCount = 0;

    EntityId addEntity() {
        uint32_t index;
        uint32_t generation;

        if (!freeEnttIds.empty()) {
            index = freeEnttIds.back();
            freeEnttIds.pop_back();
            generation = entity_index[index].generation;
        } else {
            index = entityIdCount++;
            generation = 0;
            if (index >= entity_index.size()) {
                entity_index.resize(index + 1000);
            }
        }

        EntityId newId = createEntityId(index, generation);
        entity_index[index].archetype = nullptr;
        entity_index[index].row = 0;

        return newId;
    }

    bool isEntityValid(EntityId entity) {
        uint32_t index = getEntityIndex(entity);
        if (index >= entity_index.size())
            return false;
        return entity_index[index].generation == getEntityGeneration(entity);
    }

    void destroyEntity(EntityId entity) {
        if (!isEntityValid(entity))
            return;

        uint32_t index = getEntityIndex(entity);
        Record &record = entity_index[index];

        if (record.archetype != nullptr) {
            Archetype *arch = record.archetype;
            Table *table = arch->dataTable;

            auto entIt = std::find(arch->entities.begin(), arch->entities.end(), entity);
            if (entIt != arch->entities.end()) {
                *entIt = arch->entities.back();
                arch->entities.pop_back();
            }

            if (table != nullptr) {
                size_t rowToDelete = record.row;
                size_t lastRow = table->tableEntities.size() - 1;

                if (rowToDelete != lastRow) {
                    EntityId entityToMove = table->tableEntities[lastRow];
                    uint32_t moveToIdx = getEntityIndex(entityToMove);

                    table->tableEntities[rowToDelete] = entityToMove;
                    entity_index[moveToIdx].row = rowToDelete;

                    for (ComponentId cid : table->componentIds) {
                        const auto &traits = component_traits[cid];
                        size_t col = table->column_mapping[cid];

                        void *hole_dest = table->components[col].get(rowToDelete);
                        void *last_src = table->components[col].get(lastRow);

                        traits.destroy(hole_dest);
                        traits.move_construct(hole_dest, last_src);
                        traits.destroy(last_src);
                    }
                } else {
                    for (ComponentId cid : table->componentIds) {
                        const auto &traits = component_traits[cid];
                        size_t col = table->column_mapping[cid];

                        void *target = table->components[col].get(rowToDelete);
                        traits.destroy(target);
                    }
                }

                table->tableEntities.pop_back();

                // Shrink the PagedColumns
                for (ComponentId cid : table->componentIds) {
                    size_t col = table->column_mapping[cid];
                    table->components[col].pop_back();
                }
            }
        }

        record.archetype = nullptr;
        record.row = 0;

        if (record.generation < 4095) {
            record.generation++;
            freeEnttIds.push_back(index);
        }
    }

    template <typename T> T *getComponent(EntityId entity) {
        if constexpr (std::is_empty_v<T>)
            return nullptr;
        static const ComponentId component = ComponentID<T>::_id;

        if (!isEntityValid(entity))
            return nullptr;

        uint32_t index = getEntityIndex(entity);
        Record &record = entity_index[index];

        if (!record.archetype || !record.archetype->dataTable)
            return nullptr;

        Table *table = record.archetype->dataTable;
        if (!table->signature.test(component))
            return nullptr;

        size_t col = table->column_mapping[component];
        return static_cast<T *>(table->components[col].get(record.row));
    }

    template <typename T> void setComponent(EntityId entity, T &component) {
        T *toSetComp = getComponent<T>(entity);
        if (toSetComp != nullptr)
            *toSetComp = component; // Uses standard assignment
    }

    template <typename T>
    void addComponent(EntityId entity, const T *initialValue = nullptr)
        requires std::is_trivially_copyable_v<T>
    {
        static const ComponentId component = ComponentID<T>::_id;
        static const size_t componentSize = std::is_empty_v<T> ? 0 : sizeof(T);

        if (has_component(entity, component))
            return;

        component_size[component] = componentSize;
        component_traits[component] = make_component_traits<T>();

        if (!isEntityValid(entity))
            return;

        uint32_t index = getEntityIndex(entity);
        Record &record = entity_index[index];

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

        if (currentArchtype)
            removeEntityFromArchetype(currentArchtype, entity);
        nextArchtype->entities.push_back(entity);

        Table *oldTable = currentArchtype ? currentArchtype->dataTable : nullptr;
        Table *newTable = nextArchtype->dataTable;

        if (oldTable == newTable) {
            record.archetype = nextArchtype;
            return;
        }

        size_t newRow = newTable->tableEntities.size();
        newTable->tableEntities.push_back(entity);

        // Safely push_back on PagedColumn (allocates pages internally if needed)
        for (ComponentId cid : newTable->componentIds) {
            size_t col = newTable->column_mapping[cid];
            newTable->components[col].push_back();
        }

        if (oldTable != nullptr) {
            size_t oldRow = record.row;

            for (ComponentId cid : oldTable->componentIds) {
                if (!newTable->signature.test(cid))
                    continue;

                const auto &traits = component_traits[cid];
                size_t oldCol = oldTable->column_mapping[cid];
                size_t newCol = newTable->column_mapping[cid];

                void *dest = newTable->components[newCol].get(newRow);
                void *src = oldTable->components[oldCol].get(oldRow);

                traits.move_construct(dest, src);
            }

            size_t lastRow = oldTable->tableEntities.size() - 1;
            EntityId lastEntity = oldTable->tableEntities.back();

            if (oldRow != lastRow) {
                for (ComponentId cid : oldTable->componentIds) {
                    const auto &traits = component_traits[cid];
                    size_t col = oldTable->column_mapping[cid];

                    void *hole_dest = oldTable->components[col].get(oldRow);
                    void *last_src = oldTable->components[col].get(lastRow);

                    traits.destroy(hole_dest);
                    traits.move_construct(hole_dest, last_src);
                    traits.destroy(last_src);
                }

                uint32_t lastEntityIdx = getEntityIndex(lastEntity);
                entity_index[lastEntityIdx].row = oldRow;
                oldTable->tableEntities[oldRow] = lastEntity;
            } else {
                for (ComponentId cid : oldTable->componentIds) {
                    const auto &traits = component_traits[cid];
                    size_t col = oldTable->column_mapping[cid];

                    void *moved_src = oldTable->components[col].get(oldRow);
                    traits.destroy(moved_src);
                }
            }

            for (ComponentId cid : oldTable->componentIds) {
                size_t col = oldTable->column_mapping[cid];
                oldTable->components[col].pop_back();
            }
            oldTable->tableEntities.pop_back();
        }

        if (initialValue != nullptr && !std::is_empty_v<T>) {
            size_t newCol = newTable->column_mapping[component];
            void *dest = newTable->components[newCol].get(newRow);
            std::construct_at(static_cast<T *>(dest), *initialValue);
        } else if (!std::is_empty_v<T>) {
            size_t newCol = newTable->column_mapping[component];
            void *dest = newTable->components[newCol].get(newRow);
            std::construct_at(static_cast<T *>(dest));
        }

        record.archetype = nextArchtype;
        record.row = newRow;
    }

    template <typename... Ts> void addComponents(EntityId entity, const Ts *...initialValues) {
        static_assert(sizeof...(Ts) > 0, "Must add at least one component.");

        ((component_traits[ComponentID<Ts>::_id] = make_component_traits<Ts>()), ...);

        if (!isEntityValid(entity))
            return;

        uint32_t index = getEntityIndex(entity);
        Record &record = entity_index[index];

        Archetype *currentArchtype = record.archetype;

        ArchSignature_t targetSignature;
        if (currentArchtype != nullptr) {
            targetSignature = currentArchtype->typeSet;
        }
        (targetSignature.set(ComponentID<Ts>::_id), ...);

        Archetype *nextArchtype = getOrCreateArchetype(targetSignature);

        if (currentArchtype == nextArchtype) {
            (
                [&]() {
                    if (initialValues != nullptr && !std::is_empty_v<Ts>) {
                        ComponentId cid = ComponentID<Ts>::_id;
                        size_t col = nextArchtype->dataTable->column_mapping[cid];
                        void *dest = nextArchtype->dataTable->components[col].get(record.row);

                        component_traits[cid].destroy(dest);
                        std::construct_at(static_cast<Ts *>(dest), *initialValues);
                    }
                }(),
                ...);
            return;
        }

        if (currentArchtype != nullptr) {
            removeEntityFromArchetype(currentArchtype, entity);
        }
        nextArchtype->entities.push_back(entity);

        Table *oldTable = currentArchtype ? currentArchtype->dataTable : nullptr;
        Table *newTable = nextArchtype->dataTable;

        if (oldTable == newTable) {
            record.archetype = nextArchtype;
            return;
        }

        size_t newRow = newTable->tableEntities.size();
        newTable->tableEntities.push_back(entity);

        for (ComponentId cid : newTable->componentIds) {
            size_t col = newTable->column_mapping[cid];
            newTable->components[col].push_back();
        }

        if (oldTable != nullptr) {
            size_t oldRow = record.row;

            for (ComponentId cid : oldTable->componentIds) {
                if (!newTable->signature.test(cid))
                    continue;

                const auto &traits = component_traits[cid];
                size_t oldCol = oldTable->column_mapping[cid];
                size_t newCol = newTable->column_mapping[cid];

                void *dest = newTable->components[newCol].get(newRow);
                void *src = oldTable->components[oldCol].get(oldRow);

                traits.move_construct(dest, src);
            }

            size_t lastRow = oldTable->tableEntities.size() - 1;
            EntityId lastEntity = oldTable->tableEntities.back();

            if (oldRow != lastRow) {
                for (ComponentId cid : oldTable->componentIds) {
                    const auto &traits = component_traits[cid];
                    size_t col = oldTable->column_mapping[cid];

                    void *hole_dest = oldTable->components[col].get(oldRow);
                    void *last_src = oldTable->components[col].get(lastRow);

                    traits.destroy(hole_dest);
                    traits.move_construct(hole_dest, last_src);
                    traits.destroy(last_src);
                }
                uint32_t lastEntityIdx = getEntityIndex(lastEntity);
                entity_index[lastEntityIdx].row = oldRow;
                oldTable->tableEntities[oldRow] = lastEntity;
            } else {
                for (ComponentId cid : oldTable->componentIds) {
                    const auto &traits = component_traits[cid];
                    size_t col = oldTable->column_mapping[cid];
                    void *target = oldTable->components[col].get(oldRow);
                    traits.destroy(target);
                }
            }

            for (ComponentId cid : oldTable->componentIds) {
                size_t col = oldTable->column_mapping[cid];
                oldTable->components[col].pop_back();
            }
            oldTable->tableEntities.pop_back();
        }

        (
            [&]() {
                if (!std::is_empty_v<Ts>) {
                    ComponentId cid = ComponentID<Ts>::_id;
                    size_t newCol = newTable->column_mapping[cid];
                    void *dest = newTable->components[newCol].get(newRow);

                    if (initialValues != nullptr) {
                        std::construct_at(static_cast<Ts *>(dest), *initialValues);
                    } else {
                        std::construct_at(static_cast<Ts *>(dest));
                    }
                }
            }(),
            ...);

        record.archetype = nextArchtype;
        record.row = newRow;
    }

    template <typename... Components> void addComponents(EntityId entity) {
        static_assert(sizeof...(Components) > 0, "Must add at least one component.");
        addComponents<Components...>(entity, static_cast<const Components *>(nullptr)...);
    }

    template <typename T> void removeComponent(EntityId entity) {
        static const ComponentId component = ComponentID<T>::_id;

        if (!has_component(entity, component))
            return;
        if (!isEntityValid(entity))
            return;

        uint32_t index = getEntityIndex(entity);
        Record &record = entity_index[index];

        Archetype *currentArchtype = record.archetype;
        if (currentArchtype == nullptr)
            return;

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
        if (nextArchtype)
            nextArchtype->entities.push_back(entity);

        Table *oldTable = currentArchtype->dataTable;
        Table *newTable = nextArchtype ? nextArchtype->dataTable : nullptr;

        if (oldTable == newTable) {
            record.archetype = nextArchtype;
            return;
        }

        size_t newRow = 0;
        if (newTable != nullptr) {
            newRow = newTable->tableEntities.size();
            newTable->tableEntities.push_back(entity);

            for (ComponentId cid : newTable->componentIds) {
                size_t col = newTable->column_mapping[cid];
                newTable->components[col].push_back();
            }
        }

        size_t oldRow = record.row;

        if (newTable != nullptr) {
            for (ComponentId cid : oldTable->componentIds) {
                if (cid == component || !newTable->signature.test(cid))
                    continue;

                const auto &traits = component_traits[cid];
                size_t oldCol = oldTable->column_mapping[cid];
                size_t newCol = newTable->column_mapping[cid];

                void *dest = newTable->components[newCol].get(newRow);
                void *src = oldTable->components[oldCol].get(oldRow);

                traits.move_construct(dest, src);
            }
        }

        size_t lastRow = oldTable->tableEntities.size() - 1;
        EntityId lastEntity = oldTable->tableEntities.back();

        if (oldRow != lastRow) {
            for (ComponentId cid : oldTable->componentIds) {
                const auto &traits = component_traits[cid];
                size_t col = oldTable->column_mapping[cid];

                void *hole_dest = oldTable->components[col].get(oldRow);
                void *last_src = oldTable->components[col].get(lastRow);

                traits.destroy(hole_dest);
                traits.move_construct(hole_dest, last_src);
                traits.destroy(last_src);
            }
            uint32_t lastEntityIdx = getEntityIndex(lastEntity);
            entity_index[lastEntityIdx].row = oldRow;

            oldTable->tableEntities[oldRow] = lastEntity;
        } else {
            for (ComponentId cid : oldTable->componentIds) {
                const auto &traits = component_traits[cid];
                size_t col = oldTable->column_mapping[cid];
                void *target = oldTable->components[col].get(oldRow);
                traits.destroy(target);
            }
        }

        for (ComponentId cid : oldTable->componentIds) {
            size_t col = oldTable->column_mapping[cid];
            oldTable->components[col].pop_back();
        }
        oldTable->tableEntities.pop_back();

        record.archetype = nextArchtype;
        record.row = newRow;
    }

    template <typename T> bool has_component(EntityId entity) {
        static size_t component = ComponentID<T>::_id;
        if (!isEntityValid(entity)) return false;

        uint32_t index = getEntityIndex(entity);
        Archetype *archetype = entity_index[index].archetype;
        if (archetype == nullptr) return false;
        
        return archetype->typeSet.test(component);
    }

    bool has_component(EntityId entity, ComponentId component) {
        if (!isEntityValid(entity)) return false;

        uint32_t index = getEntityIndex(entity);
        Archetype *archetype = entity_index[index].archetype;
        if (archetype == nullptr) return false;
        
        return archetype->typeSet.test(component);
    }

    template <typename... Components> const std::vector<Archetype *> &queryArchtypes() {
        static const ArchSignature_t querySig = []() {
            ArchSignature_t sig;
            (sig.set(ComponentID<Components>::_id), ...);
            return sig;
        }();

        static std::vector<Archetype*> queryRes;
        static bool initialized = false;

        if (!initialized) {
            for (auto &[archId, arch] : archetypeIdIndex) {
                if ((arch->typeSet & querySig) == querySig) {
                    queryRes.push_back(arch);
                }
            }
            active_queries.push_back({querySig, &queryRes});
            initialized = true;
        }
        return queryRes;
    }

    template <typename... Components, typename Func> inline void runSystem(Func &&func) {
        if constexpr (sizeof...(Components) == 0) return;
        using FilteredList = typename FilterEmpty<Components...>::type;
        runSystemImpl<Components...>(std::forward<Func>(func), FilteredList{});
    }

    template <typename... Ts> View<Ts...> view();

  private:
    template <typename... Components, typename Func, typename... Filtered>
    void runSystemImpl(Func &&systemFunction, TypeList<Filtered...>)
        requires std::is_invocable_v<Func, EntityId, Filtered &...>
    {
        const auto& queriedArchetypes = queryArchtypes<Components...>();

        for (auto arch : queriedArchetypes) {
            if (arch->entities.empty()) continue;
            Table *table = arch->dataTable;

            for (EntityId e : arch->entities) {
                uint32_t index = getEntityIndex(e);
                size_t physRow = entity_index[index].row;

                // Expand components dynamically without relying on contiguous .data()
                systemFunction(
                    e,
                    *reinterpret_cast<Filtered *>(
                        table->components[table->column_mapping[ComponentID<Filtered>::_id]].get(physRow)
                    )...
                );
            }
        }
    }

    template <typename... Components, typename Func, typename... Filtered>
    void runSystemImpl(Func &&systemFunction, TypeList<Filtered...>)
        requires std::is_invocable_v<Func, Filtered &...>
    {
        const auto& queriedArchetypes = queryArchtypes<Components...>();

        for (auto arch : queriedArchetypes) {
            if (arch->entities.empty()) continue;
            Table *table = arch->dataTable;

            for (EntityId e : arch->entities) {
                uint32_t index = getEntityIndex(e);
                size_t physRow = entity_index[index].row;

                systemFunction(
                    *reinterpret_cast<Filtered *>(
                        table->components[table->column_mapping[ComponentID<Filtered>::_id]].get(physRow)
                    )...
                );
            }
        }
    }

    void removeEntityFromArchetype(Archetype *arch, EntityId entity) {
        auto it = std::find(arch->entities.begin(), arch->entities.end(), entity);
        if (it != arch->entities.end()) {
            *it = arch->entities.back();
            arch->entities.pop_back();
        }
    }

    Table *getOrCreateTable(const TableSignature_t &signature) {
        auto it = table_index.find(signature);
        if (it != table_index.end())
            return &it->second;

        TableID newId = tableIdCount++;
        auto [insertedIt, success] = table_index.try_emplace(signature, Table{newId});
        Table &newTable = insertedIt->second;

        newTable.signature = signature;
        tableIdIndex[newId] = &newTable;

        size_t col = 0;
        for (size_t i = 0; i < MAX_COMPONENTS; ++i) {
            if (signature.test(i)) {
                newTable.componentIds.push_back(i);
                newTable.column_mapping[i] = col++;
                
                // CRUCIAL: Initialize the PagedColumn with the exact byte size
                newTable.components.emplace_back(component_size[i]);
            }
        }
        return &newTable;
    }

    Archetype *getOrCreateArchetype(const ArchSignature_t &signature) {
        auto archIt = archetype_index.find(signature);
        if (archIt != archetype_index.end())
            return &archIt->second;

        ArchetypeId newId = archetypeIdCount++;
        auto [insertedIt, success] = archetype_index.try_emplace(signature, Archetype{newId});
        Archetype &newArchtype = insertedIt->second;

        newArchtype.typeSet = signature;
        archetypeIdIndex[newId] = &newArchtype;

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

        for (auto& [querySignature, queryCachePtr] : active_queries) {
            if ((signature & querySignature) == querySignature) {
                queryCachePtr->push_back(&newArchtype);
            }
        }

        return &newArchtype;
    }
};

#endif