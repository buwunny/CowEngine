#ifndef ECS_ENTITY_HPP
#define ECS_ENTITY_HPP

#include <entt/entt.hpp>
#include <cstdint>

namespace ecs
{
    using Registry = entt::registry;
    using Entity = entt::entity;
    constexpr Entity NullEntity = entt::null;

    // Pack an entity id into a void* for Bullet's user pointer. Use +1 so that
    // a valid handle is never zero (Bullet treats null user pointers as
    // "unassigned" — and entt::null is the max uint32, which casts back fine
    // but we never store the null handle in a rigid body anyway).
    inline void *toUserPointer(Entity e)
    {
        return reinterpret_cast<void *>(static_cast<uintptr_t>(static_cast<uint32_t>(e)) + 1u);
    }

    inline Entity fromUserPointer(const void *p)
    {
        if (!p)
            return NullEntity;
        uintptr_t v = reinterpret_cast<uintptr_t>(p);
        if (v == 0)
            return NullEntity;
        return static_cast<Entity>(static_cast<uint32_t>(v - 1u));
    }
}

#endif // ECS_ENTITY_HPP
