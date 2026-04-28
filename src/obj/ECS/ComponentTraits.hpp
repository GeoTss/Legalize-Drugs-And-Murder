#ifndef COMPONENT_TRAITS_HPP
#define COMPONENT_TRAITS_HPP
#pragma once

#include <cstddef>
#include <memory>
#include <utility>

struct ComponentTraits {
    std::size_t size;
    void (*move_construct)(void* dest, void* src);
    void (*destroy)(void* ptr);
};

template <typename T>
consteval ComponentTraits make_component_traits() {
    return {
        sizeof(T),
        [](void* dest, void* src) {
            std::construct_at(static_cast<T*>(dest), std::move(*static_cast<T*>(src)));
        },
        [](void* ptr) {
            std::destroy_at(static_cast<T*>(ptr));
        }
    };
}
#endif