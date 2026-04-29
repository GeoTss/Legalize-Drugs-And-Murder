#pragma once
#include "ECS/Manager.hpp"
#include "SpriteManager.hpp"
#include "EventDispatcher.hpp"

void initializeCharacterAnimations(SpriteManager &spriteManager, EventDispatcher &eventDispatcher);
void initializeEnemyAnimations(SpriteManager &spriteManager, EventDispatcher &eventDispatcher);

EntityId spawnPlayer(Manager& manager, SpriteManager& spriteManager);
EntityId spawnEnemy(Manager& manager, SpriteManager& spriteManager, float, float y);