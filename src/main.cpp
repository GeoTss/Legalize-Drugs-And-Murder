#include "obj/AnimationSystem/AnimationSystem.hpp"
#include "obj/ECS/Manager.hpp"
#include <iostream>
#include <memory>
#include <raylib.h>
#include <raymath.h>

struct Position {
    Vector2 pos;

};

struct Health {
    float health;
};

struct NothingEffectTag {};
struct PoisonEffectTag {};

int main() {

    std::cout << "Your asset path: " << ASSET_PATH << "\n";

    Manager manager;

    auto hunter = manager.addEntity();

    std::cout << "Position component ID: " << ComponentID<Position>::_id << "\n";
    std::cout << "AnimationStateComponent ID: " << ComponentID<AnimationStateComponent>::_id
              << "\n";
    std::cout << "Health component ID: " << ComponentID<Health>::_id << "\n";
    std::cout << "NothingEffectTag component ID: " << ComponentID<NothingEffectTag>::_id << "\n";
    std::cout << "PoisonEffectTag component ID: " << ComponentID<PoisonEffectTag>::_id << "\n\n";

    manager.addComponents<Position, AnimationStateComponent>(hunter);
    Position positionComp = {{100, 100}};
    manager.setComponent<Position>(hunter, positionComp);

    AnimationStateComponent stateComp = {
        .currentState = AnimationStateComponent::State::Idle, .currentFrame = 0, .stateTimer = 0};
    manager.setComponent<AnimationStateComponent>(hunter, stateComp);

    Health healthComp = {.health = 100.f};
    manager.addComponent<Health>(hunter, &healthComp);
    // manager.setComponent<Health>(hunter, healthComp);

    manager.addComponent<NothingEffectTag>(hunter);

    std::cout << *manager.getArchetype(hunter) << "\n";

    AnimationSystem animationSystem;

    animationSystem.update(manager);

    Health *hunterHealth = manager.getComponent<Health>(hunter);
    std::cout << "Hunter's health before poison: " << hunterHealth->health << "\n";

    manager.replaceComponent<NothingEffectTag, PoisonEffectTag>(hunter);

    manager.runSystem<Health, PoisonEffectTag>([](Health &_health) {
        _health.health -= 10.f;
    });

    hunterHealth = manager.getComponent<Health>(hunter);
    std::cout << "Hunter's health after poison: " << hunterHealth->health << "\n";

    std::cout << *manager.getArchetype(hunter) << "\n";

    manager.removeComponent<PoisonEffectTag>(hunter);

    std::cout << *manager.getArchetype(hunter) << "\n";

    manager.runSystem<Health, PoisonEffectTag>([](Health &_health) {
        _health.health -= 10.f;
    });

    hunterHealth = manager.getComponent<Health>(hunter);
    std::cout << "Hunter's health after removing poison effect: " << hunterHealth->health << "\n";

    return 0;
}