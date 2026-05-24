#ifndef ECS_PHYSICS_SYNC_SYSTEM_HPP
#define ECS_PHYSICS_SYNC_SYSTEM_HPP

#include "ecs/Entity.hpp"

namespace ecs
{
    // Pull each entity's world transform from Bullet (where available) into
    // its Transform component. Reproduces the Euler-continuity logic that
    // used to live in Object::update so script reads of rotation stay stable
    // through the ±90° singularity.
    void physicsSyncSystem(Registry &r);
}

#endif // ECS_PHYSICS_SYNC_SYSTEM_HPP
