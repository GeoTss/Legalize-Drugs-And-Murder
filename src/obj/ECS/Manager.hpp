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
#include "ComponentFamily.hpp"
#include "Entity.hpp"
#include "PagedColumn.hpp"
#include "Table.hpp"
#include "ThreadPool.hpp"

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

enum class Exec { Seq, Par };

namespace execution {
struct par_t {};
inline constexpr par_t par{};
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
    std::vector<Archetype*> archetypeIdIndex;
    
    std::unordered_map<ArchSignature_t, std::vector<Archetype *>> query_cache;

    EntityId entityIdCount = 0;
    ArchetypeId archetypeIdCount = 1;

    ThreadPool threadPool;

    Manager(){
        archetypeIdIndex.push_back(nullptr);
    }

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
            Table &table = arch->dataTable;

            size_t rowToDelete = record.row;
            size_t lastRow = arch->entities.size() - 1;
            EntityId entityToMove = arch->entities.back();

            if (rowToDelete != lastRow) {
                arch->entities[rowToDelete] = entityToMove;
                entity_index[getEntityIndex(entityToMove)].row = rowToDelete;

                for (ComponentId cid : table.componentIds) {
                    size_t col = table.column_mapping[cid];
                    void *dest = table.components[col].get(rowToDelete);
                    void *src = table.components[col].get(lastRow);
                    std::memcpy(dest, src, component_size[cid]);
                }
            }

            arch->entities.pop_back();
            for (ComponentId cid : table.componentIds) {
                size_t col = table.column_mapping[cid];
                table.components[col].pop_back();
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

        if (!record.archetype || !record.archetype->typeSet.test(component))
            return nullptr;

        Table &table = record.archetype->dataTable;
        size_t col = table.column_mapping[component];
        return static_cast<T *>(table.components[col].get(record.row));
    }

    template <typename T> void setComponent(EntityId entity, T &component) {
        T *toSetComp = getComponent<T>(entity);
        if (toSetComp != nullptr)
            *toSetComp = component;
    }

    template <typename T>
    void addComponent(EntityId entity, const T *initialValue = nullptr)
        requires std::is_trivially_copyable_v<T>
    {
        static const ComponentId component = ComponentID<T>::_id;

        assert(component < MAX_COMPONENTS && "Component ID exceeds MAX_COMPONENTS!");

        static const size_t componentSize = std::is_empty_v<T> ? 0 : sizeof(T);

        if (has_component(entity, component))
            return;

        component_size[component] = componentSize;

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
            if (edge.add != 0) {
                nextArchtype = archetypeIdIndex[edge.add];
            } else {
                ArchSignature_t newSignature = currentArchtype->typeSet;
                newSignature.set(component);
                nextArchtype = getOrCreateArchetype(newSignature);
                edge.add = nextArchtype->id;
                nextArchtype->edges[component].remove = currentArchtype->id;
            }
        }

        if (currentArchtype == nextArchtype) {
            return;
        }

        size_t newRow = nextArchtype->entities.size();
        nextArchtype->entities.push_back(entity);

        Table &newTable = nextArchtype->dataTable;
        for (ComponentId cid : newTable.componentIds) {
            size_t col = newTable.column_mapping[cid];
            newTable.components[col].push_back();
        }

        if (currentArchtype != nullptr) {
            size_t oldRow = record.row;
            Table &oldTable = currentArchtype->dataTable;

            for (ComponentId cid : oldTable.componentIds) {
                if (!nextArchtype->typeSet.test(cid))
                    continue;

                size_t oldCol = oldTable.column_mapping[cid];
                size_t newCol = newTable.column_mapping[cid];

                void *dest = newTable.components[newCol].get(newRow);
                void *src = oldTable.components[oldCol].get(oldRow);

                std::memcpy(dest, src, component_size[cid]);
            }

            size_t lastRow = currentArchtype->entities.size() - 1;
            EntityId lastEntity = currentArchtype->entities.back();

            if (oldRow != lastRow) {
                currentArchtype->entities[oldRow] = lastEntity;
                entity_index[getEntityIndex(lastEntity)].row = oldRow;

                for (ComponentId cid : oldTable.componentIds) {
                    size_t col = oldTable.column_mapping[cid];
                    void *dest = oldTable.components[col].get(oldRow);
                    void *src = oldTable.components[col].get(lastRow);
                    std::memcpy(dest, src, component_size[cid]);
                }
            }

            currentArchtype->entities.pop_back();
            for (ComponentId cid : oldTable.componentIds) {
                size_t col = oldTable.column_mapping[cid];
                oldTable.components[col].pop_back();
            }
        }

        if constexpr (!std::is_empty_v<T>) {
            ComponentId cid = ComponentID<T>::_id;
            size_t newCol = newTable.column_mapping[cid];
            void *dest = newTable.components[newCol].get(newRow);

            if (initialValue != nullptr) {
                std::memcpy(dest, initialValue, sizeof(T));
            } else {
                std::memset(dest, 0, sizeof(T));
            }
        }

        record.archetype = nextArchtype;
        record.row = newRow;
    }

    template <typename... Ts> void addComponents(EntityId entity, const Ts *...initialValues) {
        static_assert(sizeof...(Ts) > 0, "Must add at least one component.");

        ((component_size[ComponentID<Ts>::_id] = std::is_empty_v<Ts> ? 0 : sizeof(Ts)), ...);

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
                    if constexpr (!std::is_empty_v<Ts>) {
                        if (initialValues != nullptr) {
                            ComponentId cid = ComponentID<Ts>::_id;
                            size_t col = nextArchtype->dataTable.column_mapping[cid];
                            void *dest = nextArchtype->dataTable.components[col].get(record.row);
                            std::memcpy(dest, initialValues, sizeof(Ts));
                        }
                    }
                }(),
                ...);
            return;
        }

        size_t newRow = nextArchtype->entities.size();
        nextArchtype->entities.push_back(entity);

        Table &newTable = nextArchtype->dataTable;
        for (ComponentId cid : newTable.componentIds) {
            size_t col = newTable.column_mapping[cid];
            newTable.components[col].push_back();
        }

        if (currentArchtype != nullptr) {
            size_t oldRow = record.row;
            Table &oldTable = currentArchtype->dataTable;

            for (ComponentId cid : oldTable.componentIds) {
                if (!nextArchtype->typeSet.test(cid))
                    continue;

                size_t oldCol = oldTable.column_mapping[cid];
                size_t newCol = newTable.column_mapping[cid];

                void *dest = newTable.components[newCol].get(newRow);
                void *src = oldTable.components[oldCol].get(oldRow);

                std::memcpy(dest, src, component_size[cid]);
            }

            size_t lastRow = currentArchtype->entities.size() - 1;
            EntityId lastEntity = currentArchtype->entities.back();

            if (oldRow != lastRow) {
                currentArchtype->entities[oldRow] = lastEntity;
                entity_index[getEntityIndex(lastEntity)].row = oldRow;

                for (ComponentId cid : oldTable.componentIds) {
                    size_t col = oldTable.column_mapping[cid];
                    void *dest = oldTable.components[col].get(oldRow);
                    void *src = oldTable.components[col].get(lastRow);
                    std::memcpy(dest, src, component_size[cid]);
                }
            }

            currentArchtype->entities.pop_back();
            for (ComponentId cid : oldTable.componentIds) {
                size_t col = oldTable.column_mapping[cid];
                oldTable.components[col].pop_back();
            }
        }

        (
            [&]() {
                if constexpr (!std::is_empty_v<Ts>) {
                    ComponentId cid = ComponentID<Ts>::_id;
                    size_t newCol = newTable.column_mapping[cid];
                    void *dest = newTable.components[newCol].get(newRow);

                    if (initialValues != nullptr) {
                        std::memcpy(dest, initialValues, sizeof(Ts));
                    } else {
                        std::memset(dest, 0, sizeof(Ts));
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

        if (edge.remove != 0) {
            nextArchtype = archetypeIdIndex[edge.remove];
        } else {
            ArchSignature_t newSignature = currentArchtype->typeSet;
            newSignature.reset(component);

            if (newSignature.any()) {
                nextArchtype = getOrCreateArchetype(newSignature);
                edge.remove = nextArchtype->id;
                nextArchtype->edges[component].add = currentArchtype->id;
            }
        }

        if (currentArchtype == nextArchtype)
            return;

        size_t oldRow = record.row;
        Table &oldTable = currentArchtype->dataTable;
        size_t newRow = 0;

        if (nextArchtype != nullptr) {
            newRow = nextArchtype->entities.size();
            nextArchtype->entities.push_back(entity);

            Table &newTable = nextArchtype->dataTable;
            for (ComponentId cid : newTable.componentIds) {
                size_t col = newTable.column_mapping[cid];
                newTable.components[col].push_back();
            }

            for (ComponentId cid : oldTable.componentIds) {
                if (!nextArchtype->typeSet.test(cid))
                    continue;

                size_t oldCol = oldTable.column_mapping[cid];
                size_t newCol = newTable.column_mapping[cid];

                void *dest = newTable.components[newCol].get(newRow);
                void *src = oldTable.components[oldCol].get(oldRow);

                std::memcpy(dest, src, component_size[cid]);
            }
        }

        size_t lastRow = currentArchtype->entities.size() - 1;
        EntityId lastEntity = currentArchtype->entities.back();

        if (oldRow != lastRow) {
            currentArchtype->entities[oldRow] = lastEntity;
            entity_index[getEntityIndex(lastEntity)].row = oldRow;

            for (ComponentId cid : oldTable.componentIds) {
                size_t col = oldTable.column_mapping[cid];
                void *dest = oldTable.components[col].get(oldRow);
                void *src = oldTable.components[col].get(lastRow);
                std::memcpy(dest, src, component_size[cid]);
            }
        }

        currentArchtype->entities.pop_back();
        for (ComponentId cid : oldTable.componentIds) {
            size_t col = oldTable.column_mapping[cid];
            oldTable.components[col].pop_back();
        }

        record.archetype = nextArchtype;
        record.row = newRow;
    }

    template <typename T> bool has_component(EntityId entity) {
        static size_t component = ComponentID<T>::_id;
        if (!isEntityValid(entity))
            return false;

        uint32_t index = getEntityIndex(entity);
        Archetype *archetype = entity_index[index].archetype;
        if (archetype == nullptr)
            return false;

        return archetype->typeSet.test(component);
    }

    bool has_component(EntityId entity, ComponentId component) {
        if (!isEntityValid(entity))
            return false;

        uint32_t index = getEntityIndex(entity);
        Archetype *archetype = entity_index[index].archetype;
        if (archetype == nullptr)
            return false;

        return archetype->typeSet.test(component);
    }

    template <typename... Components> const std::vector<Archetype *> &queryArchtypes() {
        ArchSignature_t targetSig;
        (targetSig.set(ComponentID<Components>::_id), ...);

        auto [it, inserted] = query_cache.try_emplace(targetSig);

        if (inserted) {
            for (auto &[sig, arch] : archetype_index) {
                if ((sig & targetSig) == targetSig) {
                    it->second.push_back(&arch);
                }
            }
        }

        return it->second;
    }

    template <typename... Components, typename Func> inline void runSystem(Func &&func) {
        if constexpr (sizeof...(Components) == 0)
            return;
        using FilteredList = typename FilterEmpty<Components...>::type;
        runSystemImpl<Exec::Seq, Components...>(std::forward<Func>(func), FilteredList{});
    }

    template <typename... Components, typename Func>
    inline void runSystem(execution::par_t, Func &&func) {
        if constexpr (sizeof...(Components) == 0)
            return;
        using FilteredList = typename FilterEmpty<Components...>::type;
        runSystemImpl<Exec::Par, Components...>(std::forward<Func>(func), FilteredList{});
    }

    template <typename... Ts> View<Ts...> view();

  private:
    template <Exec Policy, typename... Components, typename Func, typename... Filtered>
    void runSystemImpl(Func &&systemFunction, TypeList<Filtered...>)
        requires std::is_invocable_v<Func, EntityId, Filtered &...>
    {
        const auto &queriedArchetypes = queryArchtypes<Components...>();

        for (auto arch : queriedArchetypes) {
            Table &table = arch->dataTable;

            if (arch->entities.empty())
                continue;

            size_t total_entities = arch->entities.size();
            size_t total_pages = (total_entities + PagedColumn::ENTITIES_PER_PAGE - 1) /
                                 PagedColumn::ENTITIES_PER_PAGE;

            auto columns = std::make_tuple(
                &table.components[table.column_mapping[ComponentID<Filtered>::_id]]...);

            if constexpr (Policy == Exec::Par) {
                size_t num_threads = threadPool.getThreadCount();
                size_t pages_per_batch = (total_pages + num_threads - 1) / num_threads;

                for (size_t t = 0; t < num_threads; ++t) {
                    size_t start_page = t * pages_per_batch;
                    if (start_page >= total_pages)
                        break;
                    size_t end_page = std::min(start_page + pages_per_batch, total_pages);

                    threadPool.enqueue(
                        [start_page, end_page, total_entities, arch, columns, &systemFunction]() {
                            for (size_t p = start_page; p < end_page; ++p) {
                                size_t count =
                                    total_entities - (p * PagedColumn::ENTITIES_PER_PAGE);
                                if (count > PagedColumn::ENTITIES_PER_PAGE)
                                    count = PagedColumn::ENTITIES_PER_PAGE;

                                const EntityId *entity_array = arch->entities.data() +
                                                               (p * PagedColumn::ENTITIES_PER_PAGE);

                                std::apply(
                                    [&](auto *...col) {
                                        auto execute_hot_loop = [&](auto *...raw_arrays) {
                                            for (size_t i = 0; i < count; ++i) {
                                                systemFunction(entity_array[i], raw_arrays[i]...);
                                            }
                                        };
                                        execute_hot_loop(
                                            static_cast<Filtered *>(col->get_page_data(p))...);
                                    },
                                    columns);
                            }
                        });
                }

                threadPool.wait_for_all();

            } else {
                for (size_t p = 0; p < total_pages; ++p) {
                    size_t count = total_entities - (p * PagedColumn::ENTITIES_PER_PAGE);
                    if (count > PagedColumn::ENTITIES_PER_PAGE)
                        count = PagedColumn::ENTITIES_PER_PAGE;

                    const EntityId *entity_array =
                        arch->entities.data() + (p * PagedColumn::ENTITIES_PER_PAGE);

                    std::apply(
                        [&](auto *...col) {
                            auto execute_hot_loop = [&](auto *...raw_arrays) {
                                for (size_t i = 0; i < count; ++i) {
                                    systemFunction(entity_array[i], raw_arrays[i]...);
                                }
                            };
                            execute_hot_loop(static_cast<Filtered *>(col->get_page_data(p))...);
                        },
                        columns);
                }
            }
        }
    }

    template <Exec Policy, typename... Components, typename Func, typename... Filtered>
    void runSystemImpl(Func &&systemFunction, TypeList<Filtered...>)
        requires std::is_invocable_v<Func, Filtered &...>
    {
        const auto &queriedArchetypes = queryArchtypes<Components...>();

        for (auto arch : queriedArchetypes) {
            Table &table = arch->dataTable;

            if (arch->entities.empty())
                continue;

            size_t total_entities = arch->entities.size();
            size_t total_pages = (total_entities + PagedColumn::ENTITIES_PER_PAGE - 1) /
                                 PagedColumn::ENTITIES_PER_PAGE;

            auto columns = std::make_tuple(
                &table.components[table.column_mapping[ComponentID<Filtered>::_id]]...);

            if constexpr (Policy == Exec::Par) {
                size_t num_threads = threadPool.getThreadCount();
                size_t pages_per_batch = (total_pages + num_threads - 1) / num_threads;

                for (size_t t = 0; t < num_threads; ++t) {
                    size_t start_page = t * pages_per_batch;
                    if (start_page >= total_pages)
                        break;

                    size_t end_page = std::min(start_page + pages_per_batch, total_pages);

                    threadPool.enqueue(
                        [start_page, end_page, total_entities, columns, &systemFunction]() {
                            for (size_t p = start_page; p < end_page; ++p) {
                                size_t count =
                                    total_entities - (p * PagedColumn::ENTITIES_PER_PAGE);
                                if (count > PagedColumn::ENTITIES_PER_PAGE)
                                    count = PagedColumn::ENTITIES_PER_PAGE;

                                std::apply(
                                    [&](auto *...col) {
                                        auto execute_hot_loop = [&](auto *...raw_arrays) {
                                            for (size_t i = 0; i < count; ++i) {
                                                systemFunction(raw_arrays[i]...);
                                            }
                                        };
                                        execute_hot_loop(
                                            static_cast<Filtered *>(col->get_page_data(p))...);
                                    },
                                    columns);
                            }
                        });
                }

                threadPool.wait_for_all();

            } else {
                for (size_t p = 0; p < total_pages; ++p) {
                    size_t count = total_entities - (p * PagedColumn::ENTITIES_PER_PAGE);
                    if (count > PagedColumn::ENTITIES_PER_PAGE)
                        count = PagedColumn::ENTITIES_PER_PAGE;

                    std::apply(
                        [&](auto *...col) {
                            auto execute_hot_loop = [&](auto *...raw_arrays) {
                                for (size_t i = 0; i < count; ++i) {
                                    systemFunction(raw_arrays[i]...);
                                }
                            };
                            execute_hot_loop(static_cast<Filtered *>(col->get_page_data(p))...);
                        },
                        columns);
                }
            }
        }
    }

    Archetype *getOrCreateArchetype(const ArchSignature_t &signature) {
        auto archIt = archetype_index.find(signature);
        if (archIt != archetype_index.end())
            return &archIt->second;

        ArchetypeId newId = archetypeIdCount++;
        auto [insertedIt, success] = archetype_index.try_emplace(signature, Archetype{newId});
        Archetype &newArchtype = insertedIt->second;

        newArchtype.typeSet = signature;
        archetypeIdIndex.push_back(&newArchtype);

        newArchtype.trueComponentCount = signature.count();

        Table& newTable = newArchtype.dataTable;
        size_t col = 0;
        
        for (size_t i = 0; i < MAX_COMPONENTS; ++i) {
            if (signature.test(i) && component_size[i] > 0) {
                newTable.componentIds.push_back(i);
                newTable.column_mapping[i] = col++;
                newTable.components.emplace_back(component_size[i]);
            }
        }

        for (auto &[querySignature, matchedCache] : query_cache) {
            if ((signature & querySignature) == querySignature) {
                matchedCache.push_back(&newArchtype);
            }
        }

        return &newArchtype;
    }
};

#endif