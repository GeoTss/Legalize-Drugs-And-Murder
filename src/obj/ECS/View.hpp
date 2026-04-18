#ifndef VIEW_HPP
#define VIEW_HPP
#pragma once

#include "Manager.hpp"
#include <vector>


template <typename... Components> struct View {
    Manager *manager;
    std::vector<Archetype *> matchedArchetypes;

    View(Manager *_manager) : manager{_manager} {
        matchedArchetypes = manager->queryArchtypes<Components...>();
    }

    struct Iterator {
        using iterator_category = std::forward_iterator_tag;
        using value_type = EntityId;
        using difference_type = std::ptrdiff_t;
        using pointer = EntityId *;
        using reference = EntityId &;

        const std::vector<Archetype *> *archetypes;
        size_t archIndex;
        size_t entityIndex;

        Iterator(const std::vector<Archetype *> *archs, size_t aIdx, size_t eIdx)
            : archetypes(archs), archIndex(aIdx), entityIndex(eIdx) {
            advanceToValid();
        }

        void advanceToValid() {
            while (archetypes != nullptr && archIndex < archetypes->size() &&
                   entityIndex >= (*archetypes)[archIndex]->entities.size()) {
                archIndex++;
                entityIndex = 0;
            }
        }

        reference operator*() const { return (*archetypes)[archIndex]->entities[entityIndex]; }

        Iterator &operator++() {
            entityIndex++;
            advanceToValid();
            return *this;
        }

        Iterator operator++(int) {
            Iterator temp = *this;
            ++(*this);
            return temp;
        }

        bool operator==(const Iterator &other) const {
            return archIndex == other.archIndex && entityIndex == other.entityIndex;
        }

        bool operator!=(const Iterator &other) const { return !(*this == other); }
    };

    Iterator begin() const { return Iterator(&matchedArchetypes, 0, 0); }

    Iterator end() const { return Iterator(&matchedArchetypes, matchedArchetypes.size(), 0); }

    template <typename T> T* get(EntityId entity) { 
        return manager->getComponent<T>(entity);
    }
};

template <typename... Components> View<Components...> Manager::view() {
    return View<Components...>(this);
}

#endif