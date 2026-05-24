#ifndef ECS_ENTITY_HANDLE_HPP
#define ECS_ENTITY_HANDLE_HPP

#include "ecs/Entity.hpp"

namespace ecs
{
    // Convenience wrapper bundling a registry pointer with an entity id.
    // The wrapper is cheap to copy and remains valid only as long as the
    // referenced registry outlives it. Use valid() before any accessor.
    class EntityHandle
    {
    public:
        EntityHandle() = default;
        EntityHandle(Registry *reg, Entity e) : reg_(reg), e_(e) {}

        Entity entity() const { return e_; }
        Registry *registry() const { return reg_; }

        bool valid() const { return reg_ && reg_->valid(e_); }
        explicit operator bool() const { return valid(); }
        bool operator==(const EntityHandle &o) const { return reg_ == o.reg_ && e_ == o.e_; }
        bool operator!=(const EntityHandle &o) const { return !(*this == o); }

        template <typename T, typename... Args>
        T &add(Args &&...args) { return reg_->emplace_or_replace<T>(e_, std::forward<Args>(args)...); }

        template <typename T>
        void remove() { reg_->remove<T>(e_); }

        template <typename T>
        bool has() const { return reg_->all_of<T>(e_); }

        template <typename T>
        T *tryGet() { return reg_->try_get<T>(e_); }

        template <typename T>
        const T *tryGet() const { return reg_->try_get<T>(e_); }

        template <typename T>
        T &get() { return reg_->get<T>(e_); }

        template <typename T>
        const T &get() const { return reg_->get<T>(e_); }

    private:
        Registry *reg_ = nullptr;
        Entity e_ = NullEntity;
    };
}

#endif // ECS_ENTITY_HANDLE_HPP
