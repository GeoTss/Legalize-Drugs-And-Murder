#ifndef COMPONENT_FAMILY_HPP
#define COMPONENT_FAMILY_HPP
#pragma once

#include <atomic>
#include <cstdint>

using ComponentId = std::uint16_t;

class ComponentFamily {
    static inline std::atomic<ComponentId> counter{0};

public:
    template <typename T>
    static ComponentId type() noexcept {
        static const ComponentId id = counter.fetch_add(1, std::memory_order_relaxed);
        return id;
    }
};

template <typename T> 
struct ComponentID {
    static const inline ComponentId _id = ComponentFamily::type<T>();
};

#endif