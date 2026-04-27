#ifndef COMPONENT_FAMILY_HPP
#define COMPONENT_FAMILY_HPP
#pragma once

#include <atomic>
#include <cstdint>

using ComponentId = std::uint16_t;

// Encapsulates the thread-safe, strictly increasing counter.
class ComponentFamily {
    static inline std::atomic<ComponentId> counter{0};

public:
    template <typename T>
    static ComponentId type() noexcept {
        // Guaranteed to execute exactly once per type T across all Translation Units
        static const ComponentId id = counter.fetch_add(1, std::memory_order_relaxed);
        return id;
    }
};

// Replaces your previous stateful metaprogramming struct
template <typename T> 
struct ComponentID {
    // Initialized safely at runtime the first time it is accessed
    static const inline ComponentId _id = ComponentFamily::type<T>();
};

#endif