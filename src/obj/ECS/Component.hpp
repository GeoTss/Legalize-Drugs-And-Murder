#ifndef COMPONENT_HPP
#define COMPONENT_HPP
#pragma once

#include <stdint.h>

#include "unique_counter.hpp"

using ComponentId = uint16_t;

template <typename T> struct ComponentID {
    static constexpr ComponentId _id = unique_id<T>();
};

#endif