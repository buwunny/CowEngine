#ifndef ECS_FACTORIES_HPP
#define ECS_FACTORIES_HPP

#include "ecs/Entity.hpp"
#include "ecs/Components.hpp"

#include <glm/glm.hpp>
#include <memory>

class PhysicsWorld;
class Camera;
class Mesh;

namespace ecs
{
    // Build the Transform fields (cached model + modelNoScale + localScale +
    // position/rotation/scale) from the raw model matrix the caller passes
    // in. Mirrors the historical Object::setInitialModel logic.
    void initializeTransformFromModel(Transform &t, const glm::mat4 &model);

    // Apply pos/rot(deg)/scale to a Transform (and to its rigid body if one
    // is present). Mirrors Object::setTransform.
    void applyTransform(Registry &r, Entity e, const glm::vec3 &pos,
                        const glm::vec3 &rotDeg, const glm::vec3 &scale);

    // Set mass on an existing Physics component, recomputing inertia.
    void setMass(Physics &p, double mass);

    // Factories. Each one creates a fresh entity in `r`, attaches the
    // necessary components, registers the rigid body with `physics` (when
    // supplied), and returns the entity id. `physics` may be null during
    // load — Scene::addEntitiesToPhysicsWorld picks up bodies later.
    Entity createCube(Registry &r, PhysicsWorld *physics, int size,
                      const glm::mat4 &model = glm::mat4(1.0f),
                      const glm::vec4 &color = glm::vec4(1.0f),
                      float mass = 10.0f);

    Entity createPlane(Registry &r, PhysicsWorld *physics,
                       float length, float width,
                       const glm::mat4 &model = glm::mat4(1.0f),
                       const glm::vec4 &color = glm::vec4(1.0f),
                       float mass = 0.0f);

    Entity createStaticObject(Registry &r, PhysicsWorld *physics,
                              std::shared_ptr<Mesh> sharedMesh,
                              const float *verts, size_t vertexCount,
                              const unsigned int *indices, size_t indexCount,
                              int floatsPerVertex,
                              const glm::mat4 &model = glm::mat4(1.0f),
                              const glm::vec4 &color = glm::vec4(1.0f),
                              float mass = 0.0f);

    Entity createPlayer(Registry &r, PhysicsWorld *physics, Camera *camera,
                        const glm::mat4 &model = glm::mat4(1.0f));
}

#endif // ECS_FACTORIES_HPP
