#ifndef ANIMATION_SYSTEM_HPP
#define ANIMATION_SYSTEM_HPP
#pragma once

#include "../ECS/Manager.hpp"
#include "../ECS/View.hpp"
#include <iostream>
#include <stdint.h>

struct AnimationStateComponent {
    enum struct State { Idle, Run } currentState;
    uint32_t currentFrame;
    float stateTimer;
};

struct AnimationSystem {
    void update(Manager &manager) {
        auto view = manager.view<AnimationStateComponent>();

        for (auto entity : view) {
            auto &state = view.get<AnimationStateComponent>(entity);

            switch (state.currentState) {
            case AnimationStateComponent::State::Idle:
                std::cout << "Entity: " << entity << " in Idle state\n";
                break;
            case AnimationStateComponent::State::Run:
                break;
            }
        }
    }
};

#endif