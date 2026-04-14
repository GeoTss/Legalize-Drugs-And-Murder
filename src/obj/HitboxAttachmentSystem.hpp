#ifndef HITBOX_ATTACHEMENT_SYSTEM
#define HITBOX_ATTACHEMENT_SYSTEM
#pragma once

#include "ECS/Component.hpp"
#include "ECS/Manager.hpp"

struct HitboxAttachmentSystem {
    static void update(Manager &manager) {

        auto hitboxView = manager.view<HitboxComponent>();

        for (auto hitboxEntity : hitboxView) {
            auto& hitbox = hitboxView.get<HitboxComponent>(hitboxEntity);

            if (hitbox.attached == false)
                continue;;

            EntityId srcEntity = hitbox.srcEntity;

            auto srcTransform = manager.getComponent<TransformComponent>(srcEntity);
            if (srcTransform == nullptr)
                continue;

            hitbox.x = srcTransform->pos.x;
            hitbox.y = srcTransform->pos.y;
        }
    }
};

#endif