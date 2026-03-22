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
#include "Component.hpp"
#include "Entity.hpp"

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
    Archetype *archetype;
    size_t row;
};

struct ArchtypeRecord {
    size_t column;

    ArchtypeRecord() = default;
    ArchtypeRecord(size_t _col) : column{_col} {}
};

using ArchetypeMap = std::unordered_map<ArchetypeId, ArchtypeRecord>;

template <typename... Components> consteval auto getSortedArray() {
    std::array<ComponentId, sizeof...(Components)> arr = {ComponentID<Components>::_id...};
    std::sort(arr.begin(), arr.end());
    return arr;
}

template <typename... Components> struct View;

struct Manager {

    std::unordered_map<ComponentId, size_t> component_size;
    std::unordered_map<EntityId, Record> entity_index;
    std::unordered_map<ArchSignature_t, Archetype, ArchSignatureHash> archetype_index;
    std::unordered_map<ArchetypeId, Archetype *> archetypeIdIndex;
    std::unordered_map<ComponentId, ArchetypeMap> component_index;

    EntityId entityIdCount = 0;
    ArchetypeId archetypeIdCount = 0;

    EntityId addEntity() {
        EntityId newId = entityIdCount++;
        Record record = {.archetype = nullptr, .row = 0};
        entity_index[newId] = record;
        return newId;
    }

    template <typename T> T *getComponent(EntityId entity) {
        if constexpr (std::is_empty_v<T>)
            return nullptr;

        static const ComponentId component = ComponentID<T>::_id;

        Record &record = entity_index[entity];
        Archetype *archetype = record.archetype;

        ArchetypeMap &archtypes = component_index[component];
        if (!archtypes.contains(archetype->id)) {
            return nullptr;
        }

        ArchtypeRecord &a_record = archtypes[archetype->id];
        return (T *)&archetype->components[a_record.column][record.row * sizeof(T)];
    }

    Archetype *getArchetype(EntityId entity) {
        Record &record = entity_index[entity];
        return record.archetype;
    }

    template <typename T> void setComponent(EntityId entity, T &component) {
        T *toSetComp = getComponent<T>(entity);
        if (toSetComp == nullptr)
            return;

        std::memcpy(toSetComp, (void *)&component, sizeof(T));
    }

    template <typename... T> void prepareArchetype() {
        static const ArchSignature_t signature = {ComponentID<T>::_id...};
        getOrCreateArchetype(signature);
    }

    template <typename... T> Archetype *getArchetype() {
        static const ArchSignature_t signature = {ComponentID<T>::_id...};
        return getOrCreateArchetype(signature);
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

        Record &record = entity_index[entity];
        Archetype *currentArchtype = record.archetype;
        Archetype *nextArchtype = nullptr;

        if (currentArchtype == nullptr) {
            std::vector<ComponentId> rootArch = {component};
            nextArchtype = getOrCreateArchetype(rootArch);
        } else {
            ArchetypeEdge &edge = currentArchtype->edges[component];
            if (edge.add != nullptr) {
                nextArchtype = edge.add;
            } else {
                ArchSignature_t newSignature = currentArchtype->typeSet;
                newSignature.push_back(component);
                std::sort(newSignature.begin(), newSignature.end());

                nextArchtype = getOrCreateArchetype(newSignature);
                edge.add = nextArchtype;
                nextArchtype->edges[component].remove = currentArchtype;
            }
        }

        size_t newRow = nextArchtype->entities.size();
        nextArchtype->entities.push_back(entity);

        // FIX: Only resize valid columns
        for (ComponentId cid : nextArchtype->typeSet) {
            size_t compSize = component_size[cid];
            if (compSize > 0) {
                size_t col = component_index[cid][nextArchtype->id].column;
                nextArchtype->components[col].resize((newRow + 1) * compSize);
            }
        }

        if (currentArchtype != nullptr) {
            size_t oldRow = record.row;

            // FIX: Copy old data using explicit column lookups
            for (ComponentId cid : currentArchtype->typeSet) {
                size_t compSize = component_size[cid];
                if (compSize == 0)
                    continue;

                size_t oldCol = component_index[cid][currentArchtype->id].column;
                size_t newCol = component_index[cid][nextArchtype->id].column;

                void *src = &currentArchtype->components[oldCol][oldRow * compSize];
                void *dst = &nextArchtype->components[newCol][newRow * compSize];
                std::memcpy(dst, src, compSize);
            }

            size_t lastRow = currentArchtype->entities.size() - 1;
            EntityId lastEntity = currentArchtype->entities.back();

            if (oldRow != lastRow) {
                // FIX: Swap and Pop using column lookups
                for (ComponentId cid : currentArchtype->typeSet) {
                    size_t compSize = component_size[cid];
                    if (compSize == 0)
                        continue;

                    size_t col = component_index[cid][currentArchtype->id].column;
                    void *src = &currentArchtype->components[col][lastRow * compSize];
                    void *dst = &currentArchtype->components[col][oldRow * compSize];
                    std::memcpy(dst, src, compSize);
                }

                entity_index[lastEntity].row = oldRow;
                currentArchtype->entities[oldRow] = lastEntity;
            }

            // FIX: Shrink vectors using column lookups
            for (ComponentId cid : currentArchtype->typeSet) {
                size_t compSize = component_size[cid];
                if (compSize == 0)
                    continue;

                size_t col = component_index[cid][currentArchtype->id].column;
                currentArchtype->components[col].resize(std::max(lastRow * compSize, (size_t)1));
            }
            currentArchtype->entities.pop_back();
        }

        if (initialValue != nullptr && !std::is_empty_v<T>) {
            size_t newCol = component_index[ComponentID<T>::_id][nextArchtype->id].column;
            std::memcpy(&nextArchtype->components[newCol][newRow * sizeof(T)],
                        (void *)initialValue,
                        sizeof(T));
        }

        record.archetype = nextArchtype;
        record.row = newRow;
    }

    template <typename... Ts> void addComponents(EntityId entity, const Ts *...initialValues) {
        static_assert(sizeof...(Ts) > 0, "Must add at least one component.");
        if constexpr (sizeof...(Ts) == 1) {
            addComponent<Ts...>(entity, initialValues...);
            return;
        }

        ((component_size[ComponentID<Ts>::_id] = std::is_empty_v<Ts> ? 0 : sizeof(Ts)), ...);

        Record &record = entity_index[entity];
        Archetype *currentArchtype = record.archetype;

        ArchSignature_t targetSignature;
        if (currentArchtype != nullptr) {
            targetSignature = currentArchtype->typeSet;
        }

        std::array<ComponentId, sizeof...(Ts)> newComps = {ComponentID<Ts>::_id...};
        for (ComponentId id : newComps) {
            if (std::find(targetSignature.begin(), targetSignature.end(), id) ==
                targetSignature.end()) {
                targetSignature.push_back(id);
            }
        }

        std::sort(targetSignature.begin(), targetSignature.end());
        Archetype *nextArchtype = getOrCreateArchetype(targetSignature);

        if (currentArchtype == nextArchtype) {
            (
                [&]() {
                    if (initialValues != nullptr && !std::is_empty_v<Ts>) {
                        size_t col = component_index[ComponentID<Ts>::_id][nextArchtype->id].column;
                        std::memcpy(&nextArchtype->components[col][record.row * sizeof(Ts)],
                                    (void *)initialValues,
                                    sizeof(Ts));
                    }
                }(),
                ...);
            return;
        }

        size_t newRow = nextArchtype->entities.size();
        nextArchtype->entities.push_back(entity);

        // FIX: resize valid columns
        for (ComponentId cid : nextArchtype->typeSet) {
            size_t compSize = component_size[cid];
            if (compSize > 0) {
                size_t col = component_index[cid][nextArchtype->id].column;
                nextArchtype->components[col].resize((newRow + 1) * compSize);
            }
        }

        if (currentArchtype != nullptr) {
            size_t oldRow = record.row;

            for (ComponentId cid : currentArchtype->typeSet) {
                size_t compSize = component_size[cid];
                if (compSize == 0)
                    continue;

                size_t oldCol = component_index[cid][currentArchtype->id].column;
                size_t newCol = component_index[cid][nextArchtype->id].column;

                void *src = &currentArchtype->components[oldCol][oldRow * compSize];
                void *dst = &nextArchtype->components[newCol][newRow * compSize];
                std::memcpy(dst, src, compSize);
            }

            size_t lastRow = currentArchtype->entities.size() - 1;
            EntityId lastEntity = currentArchtype->entities.back();

            if (oldRow != lastRow) {
                for (ComponentId cid : currentArchtype->typeSet) {
                    size_t compSize = component_size[cid];
                    if (compSize == 0)
                        continue;

                    size_t col = component_index[cid][currentArchtype->id].column;
                    void *src = &currentArchtype->components[col][lastRow * compSize];
                    void *dst = &currentArchtype->components[col][oldRow * compSize];
                    std::memcpy(dst, src, compSize);
                }
                entity_index[lastEntity].row = oldRow;
                currentArchtype->entities[oldRow] = lastEntity;
            }

            for (ComponentId cid : currentArchtype->typeSet) {
                size_t compSize = component_size[cid];
                if (compSize == 0)
                    continue;

                size_t col = component_index[cid][currentArchtype->id].column;
                currentArchtype->components[col].resize(std::max(lastRow * compSize, (size_t)1));
            }
            currentArchtype->entities.pop_back();
        }

        (
            [&]() {
                if (initialValues != nullptr && !std::is_empty_v<Ts>) {
                    ComponentId cid = ComponentID<Ts>::_id;
                    size_t newCol = component_index[cid][nextArchtype->id].column;
                    std::memcpy(&nextArchtype->components[newCol][newRow * sizeof(Ts)],
                                (void *)initialValues,
                                sizeof(Ts));
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

        Record &record = entity_index[entity];
        Archetype *currentArchtype = record.archetype;
        if (currentArchtype == nullptr)
            return;

        Archetype *nextArchtype = nullptr;
        ArchetypeEdge &edge = currentArchtype->edges[component];

        if (edge.remove != nullptr) {
            nextArchtype = edge.remove;
        } else {
            ArchSignature_t newSignature;
            newSignature.reserve(currentArchtype->typeSet.size() - 1);

            for (ComponentId cid : currentArchtype->typeSet) {
                if (cid != component) {
                    newSignature.push_back(cid);
                }
            }

            if (!newSignature.empty()) {
                nextArchtype = getOrCreateArchetype(newSignature);
                edge.remove = nextArchtype;
                nextArchtype->edges[component].add = currentArchtype;
            }
        }

        size_t newRow = 0;
        if (nextArchtype != nullptr) {
            newRow = nextArchtype->entities.size();
            nextArchtype->entities.push_back(entity);

            for (ComponentId cid : nextArchtype->typeSet) {
                size_t compSize = component_size[cid];
                if (compSize > 0) {
                    size_t col = component_index[cid][nextArchtype->id].column;
                    nextArchtype->components[col].resize((newRow + 1) * compSize);
                }
            }
        }

        size_t oldRow = record.row;

        if (nextArchtype != nullptr) {
            for (ComponentId cid : currentArchtype->typeSet) {
                if (cid == component)
                    continue;

                size_t compSize = component_size[cid];
                if (compSize == 0)
                    continue;

                size_t oldCol = component_index[cid][currentArchtype->id].column;
                size_t newCol = component_index[cid][nextArchtype->id].column;

                void *src = &currentArchtype->components[oldCol][oldRow * compSize];
                void *dst = &nextArchtype->components[newCol][newRow * compSize];
                std::memcpy(dst, src, compSize);
            }
        }

        size_t lastRow = currentArchtype->entities.size() - 1;
        EntityId lastEntity = currentArchtype->entities.back();

        if (oldRow != lastRow) {
            for (ComponentId cid : currentArchtype->typeSet) {
                size_t compSize = component_size[cid];
                if (compSize == 0)
                    continue;

                size_t col = component_index[cid][currentArchtype->id].column;
                void *src = &currentArchtype->components[col][lastRow * compSize];
                void *dst = &currentArchtype->components[col][oldRow * compSize];
                std::memcpy(dst, src, compSize);
            }
            entity_index[lastEntity].row = oldRow;
            currentArchtype->entities[oldRow] = lastEntity;
        }

        for (ComponentId cid : currentArchtype->typeSet) {
            size_t compSize = component_size[cid];
            if (compSize == 0)
                continue;

            size_t col = component_index[cid][currentArchtype->id].column;
            currentArchtype->components[col].resize(std::max(lastRow * compSize, (size_t)1));
        }
        currentArchtype->entities.pop_back();

        if (nextArchtype != nullptr) {
            record.archetype = nextArchtype;
            record.row = newRow;
        } else {
            record.archetype = nullptr;
            record.row = 0;
        }
    }

    template <typename _SrcComp, typename _DstComp>
    void replaceComponent(EntityId entity, const _DstComp *initialValue = nullptr) {
        static_assert(ComponentID<_SrcComp>::_id != ComponentID<_DstComp>::_id);

        Record &record = entity_index[entity];
        Archetype *currentArchetype = record.archetype;

        if (currentArchetype == nullptr || !has_component(entity, ComponentID<_SrcComp>::_id))
            return;
        if (has_component(entity, ComponentID<_DstComp>::_id))
            return;

        static const ComponentId srcId = ComponentID<_SrcComp>::_id;
        static const ComponentId dstId = ComponentID<_DstComp>::_id;

        component_size[dstId] = std::is_empty_v<_DstComp> ? 0 : sizeof(_DstComp);

        Archetype *withDstArch = nullptr;
        auto &edge = currentArchetype->edges[srcId];

        if (edge.replace.contains(dstId)) {
            withDstArch = edge.replace[dstId];
        } else {
            ArchSignature_t withDstSignature;
            withDstSignature.reserve(currentArchetype->typeSet.size());
            for (ComponentId compID : currentArchetype->typeSet) {
                if (compID != srcId) {
                    withDstSignature.push_back(compID);
                }
            }
            withDstSignature.push_back(dstId);
            std::sort(withDstSignature.begin(), withDstSignature.end());

            withDstArch = getOrCreateArchetype(withDstSignature);
            edge.replace[dstId] = withDstArch;
        }

        size_t newRow = withDstArch->entities.size();
        withDstArch->entities.push_back(entity);

        for (ComponentId cid : withDstArch->typeSet) {
            size_t compSize = component_size[cid];
            if (compSize > 0) {
                size_t col = component_index[cid][withDstArch->id].column;
                withDstArch->components[col].resize((newRow + 1) * compSize);
            }
        }

        size_t oldRow = record.row;

        for (ComponentId cid : currentArchetype->typeSet) {
            if (cid == srcId)
                continue;

            size_t compSize = component_size[cid];
            if (compSize == 0)
                continue;

            size_t oldCol = component_index[cid][currentArchetype->id].column;
            size_t newCol = component_index[cid][withDstArch->id].column;

            void *src = &currentArchetype->components[oldCol][oldRow * compSize];
            void *dst = &withDstArch->components[newCol][newRow * compSize];
            std::memcpy(dst, src, compSize);
        }

        if (initialValue != nullptr && !std::is_empty_v<_DstComp>) {
            size_t newCol = component_index[dstId][withDstArch->id].column;
            std::memcpy(&withDstArch->components[newCol][newRow * sizeof(_DstComp)],
                        initialValue,
                        sizeof(_DstComp));
        }

        size_t lastRow = currentArchetype->entities.size() - 1;
        EntityId lastEntity = currentArchetype->entities.back();

        if (oldRow != lastRow) {
            for (ComponentId cid : currentArchetype->typeSet) {
                size_t compSize = component_size[cid];
                if (compSize == 0)
                    continue;

                size_t col = component_index[cid][currentArchetype->id].column;
                void *src = &currentArchetype->components[col][lastRow * compSize];
                void *dst = &currentArchetype->components[col][oldRow * compSize];
                std::memcpy(dst, src, compSize);
            }
            entity_index[lastEntity].row = oldRow;
            currentArchetype->entities[oldRow] = lastEntity;
        }

        for (ComponentId cid : currentArchetype->typeSet) {
            size_t compSize = component_size[cid];
            if (compSize == 0)
                continue;

            size_t col = component_index[cid][currentArchetype->id].column;
            currentArchetype->components[col].resize(std::max(lastRow * compSize, (size_t)1));
        }
        currentArchetype->entities.pop_back();

        record.row = newRow;
        record.archetype = withDstArch;
    }

    bool has_component(EntityId entity, ComponentId component) {
        Archetype *archetype = entity_index[entity].archetype;
        if (archetype == nullptr)
            return false;
        return component_index[component].count(archetype->id) != 0;
    }

    template <typename... Components> std::vector<Archetype *> queryArchtypes() {
        if constexpr (sizeof...(Components) == 0)
            return {};

        constexpr auto compileTimeComponents = getSortedArray<Components...>();
        auto components = compileTimeComponents;

        std::sort(components.begin(), components.end(), [this](auto a, auto b) {
            auto itA = component_index.find(a);
            size_t sizeA = (itA != component_index.end()) ? itA->second.size() : 0;
            auto itB = component_index.find(b);
            size_t sizeB = (itB != component_index.end()) ? itB->second.size() : 0;
            return sizeA < sizeB;
        });

        std::vector<Archetype *> queryRes;
        ComponentId firstComponent = components[0];
        auto selectedIt = component_index.find(firstComponent);

        if (selectedIt == component_index.end())
            return {};

        ArchetypeMap &candidates = selectedIt->second;

        for (auto &[archId, archRecord] : candidates) {
            bool matchesAll = true;
            for (int i = 1; i < components.size(); ++i) {
                if (!component_index[components[i]].contains(archId)) {
                    matchesAll = false;
                    break;
                }
            }
            if (matchesAll) {
                queryRes.push_back(archetypeIdIndex[archId]);
            }
        }
        return queryRes;
    }

    template <typename... Components, typename Func> void runSystem(Func &&systemFunction) {
        if constexpr (sizeof...(Components) == 0)
            return;

        using FilteredList = typename FilterEmpty<Components...>::type;
        auto queriedArchetypes = queryArchtypes<Components...>();

        auto executeLoops = [&]<typename... Filtered>(TypeList<Filtered...>) {
            static_assert(
                std::is_invocable_v<Func, Filtered &...>,
                "System lambda parameters do not match the non-empty queried components!");

            for (auto arch : queriedArchetypes) {
                size_t entityCount = arch->entities.size();
                if (entityCount == 0)
                    continue;

                const std::tuple<Filtered *...> componentArrays = {reinterpret_cast<Filtered *>(
                    arch->components[component_index[ComponentID<Filtered>::_id][arch->id].column]
                        .data())...};

                for (size_t i = 0; i < entityCount; ++i) {
                    std::apply(
                        [&](auto *...compArray) {
                            systemFunction(compArray[i]...);
                        },
                        componentArrays);
                }
            }
        };

        executeLoops(FilteredList{});
    }

    template <typename... Components> View<Components...> view();

  private:
    Archetype *getOrCreateArchetype(const ArchSignature_t &signature) {

        auto archIt = archetype_index.find(signature);
        if (archIt != archetype_index.end()) {
            return &archIt->second;
        }

        ArchetypeId newId = archetypeIdCount++;
        auto [insertedIt, success] = archetype_index.try_emplace(signature, newId);
        Archetype &newArchtype = insertedIt->second;
        archetypeIdIndex[newId] = &newArchtype;

        size_t trueSize = 0;
        for (const auto cid : signature) {
            if (component_size[cid] != 0)
                trueSize += 1;
        }

        newArchtype.trueComponentCount = trueSize;
        newArchtype.components.resize(trueSize);

        size_t col = 0;
        for (size_t i = 0; i < signature.size(); ++i) {
            ComponentId cid = signature[i];
            component_index[cid][newArchtype.id] = ArchtypeRecord{col};

            if (component_size[cid] != 0) {
                col++;
            }
        }

        newArchtype.typeSet = signature;

        return &newArchtype;
    }
};

#endif