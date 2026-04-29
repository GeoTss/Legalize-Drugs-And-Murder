#pragma once
#include <cstdint>

enum struct hunterStates : uint8_t { IDLE, RUNNING, DAMAGED, ATTACKING };
enum struct enemyStates : uint8_t { IDLE, RUNNING, DAMAGED, ATTACKING };
enum struct eventIDs { LIGHT_ATTACK, LIGHTNING };

constexpr int MAP_WIDTH = 60;
constexpr int MAP_HEIGHT = 40;
constexpr float TILE_SIZE = 64.0f;