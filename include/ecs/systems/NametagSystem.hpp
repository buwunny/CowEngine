#ifndef ECS_NAMETAG_SYSTEM_HPP
#define ECS_NAMETAG_SYSTEM_HPP

#include "ecs/Entity.hpp"

#include <glm/glm.hpp>

class TextRenderer;

namespace ecs
{
    // Draw every Transform + Nametag entity's label, billboarded at the camera
    // and floating above the entity. Call it inside the scene pass, after
    // renderSystem and before the post-process composite, so labels pick up the
    // same bloom/tonemap as the rest of the frame and are occluded by geometry
    // standing in front of them.
    void nametagSystem(Registry &r, TextRenderer &text, const glm::mat4 &view,
                       const glm::mat4 &projection, const glm::vec3 &camPos);
}

#endif // ECS_NAMETAG_SYSTEM_HPP
