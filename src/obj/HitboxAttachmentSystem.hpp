#ifndef HITBOX_ATTACHEMENT_SYSTEM
#define HITBOX_ATTACHEMENT_SYSTEM
#pragma once

#include "ECS/Component.hpp"
#include "ECS/Manager.hpp"

struct HitboxAttachmentSystem {
    static void update(Manager &manager) {

        auto hitboxView = manager.view<HitboxComponent>();

        for (auto hitboxEntity : hitboxView) {
            auto hitbox = hitboxView.get<HitboxComponent>(hitboxEntity);

            if (hitbox->attached == false)
                continue;
            ;

            EntityId srcEntity = hitbox->srcEntity;

            auto srcTransform = manager.getComponent<TransformComponent>(srcEntity);
            auto srcStats = manager.getComponent<StatsComponent>(srcEntity);

            if (srcTransform == nullptr || srcStats == nullptr)
                continue;

            float finalWidth = hitbox->width * srcStats->hitboxScale;
            float finalHeight = hitbox->height * srcStats->hitboxScale;

            float dirMultiplier = (srcTransform->facingDirection == -1) ? -1.0f : 1.0f;

            float startingOffsetX = hitbox->offsetX * dirMultiplier;

            float spawnX = (srcTransform->pos.x + startingOffsetX) - (finalWidth / 2.0f);
            float spawnY = (srcTransform->pos.y + hitbox->offsetY);

            hitbox->x = spawnX;
            hitbox->y = spawnY;
        }
    }
};

#endif