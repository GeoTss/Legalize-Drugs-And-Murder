#pragma once

#include "ECS/unique_counter.hpp"
#include <memory>
#include <stdint.h>

typedef uint32_t StateID_t;

struct IState {
    StateID_t stateID;
    StateID_t transitionStateID;

    virtual void enter() = 0;
    virtual void update() = 0;
    virtual void exit() = 0;
};

struct StateComponent {
    std::unique_ptr<IState> state;
};