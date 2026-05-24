#ifndef ECS_COMPONENT_OPS_HPP
#define ECS_COMPONENT_OPS_HPP

#include "ecs/Entity.hpp"
#include "ecs/Components.hpp"

#include <glm/glm.hpp>
#include <memory>
#include <string>

class PhysicsWorld;
class Mesh;
class Scene;

// Add/remove operations for individual components. These are the building
// blocks the editor's Inspector uses for its "Add Component" / per-component
// remove buttons. Each helper handles Bullet teardown and keeps ShapeMarker
// consistent so the entity round-trips through save/load.
//
// Adding a new component type to the editor takes three things:
//   1) declare add/remove helpers here,
//   2) implement them in ComponentOps.cpp,
//   3) register a ComponentDescriptor in EditorUI's table.

namespace ecs
{
    // ---- Renderable -------------------------------------------------------
    // Each `addRenderable*` overload sets the appropriate mesh + ShapeMarker
    // for serialization. Removing the renderable does NOT touch the collider.
    void addRenderableCube(Registry &r, Entity e, int size = 1, const glm::vec4 &color = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
    void addRenderablePlane(Registry &r, Entity e, float length = 10.f, float width = 10.f, const glm::vec4 &color = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
    void addRenderableFromMesh(Registry &r, Entity e, std::shared_ptr<Mesh> mesh,
                               const std::string &meshPath = "",
                               const glm::vec4 &color = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
    void removeRenderable(Registry &r, Entity e);

    // ---- Physics (collider + rigidbody bundle) ----------------------------
    // Adding Physics requires removing any existing Physics first; helpers
    // do that automatically. PhysicsWorld is needed so the new body gets
    // registered (and the old one un-registered) with the dynamics world.
    void addBoxCollider(Registry &r, Entity e, PhysicsWorld *world,
                        const glm::vec3 &halfExtents = glm::vec3(0.5f), float mass = 0.f);
    void addSphereCollider(Registry &r, Entity e, PhysicsWorld *world,
                           float radius = 0.5f, float mass = 0.f);
    void addCapsuleCollider(Registry &r, Entity e, PhysicsWorld *world,
                            float radius = 0.5f, float height = 1.f, float mass = 0.f);

    // Build a convex hull from the entity's current Renderable mesh. Requires
    // the entity to have a Renderable with a valid mesh; no-op otherwise.
    void addConvexHullColliderFromRenderable(Registry &r, Entity e, PhysicsWorld *world, float mass = 0.f);

    void removePhysics(Registry &r, Entity e, PhysicsWorld *world);

    // ---- Script -----------------------------------------------------------
    // Writes Identity::scriptPath. The actual ScriptComponent (compiled
    // bytecode) is attached lazily by the ScriptSystem on next load.
    void addScript(Registry &r, Entity e, const std::string &path = "scripts/new_script.cow");
    void removeScript(Registry &r, Entity e);

    // ---- Empty entity -----------------------------------------------------
    // Create a "blank slate" entity with only Identity + Transform. Callers
    // then attach whatever components they want via the helpers above.
    Entity createEmptyEntity(Registry &r, const std::string &name = "Entity",
                             const glm::mat4 &model = glm::mat4(1.0f));
}

#endif // ECS_COMPONENT_OPS_HPP
