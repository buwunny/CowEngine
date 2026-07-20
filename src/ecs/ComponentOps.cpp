#include "ecs/ComponentOps.hpp"
#include "ecs/Factories.hpp"

#include "core/PhysicsWorld.hpp"
#include "meshes/Mesh.hpp"
#include "meshes/CubeMesh.hpp"
#include "meshes/PlaneMesh.hpp"
#include "meshes/StaticMesh.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <set>
#include <string>

namespace ecs
{
    namespace
    {
        int g_nextOpId = 100000; // separate range from Factories' counter

        // Build the underlying Bullet body for a fresh shape, registering it
        // with the world. Replaces any existing Physics on the entity.
        void emplacePhysics(Registry &r, Entity e, PhysicsWorld *world,
                            std::unique_ptr<btCollisionShape> shape, double mass)
        {
            // Tear down any existing body first so the new one inherits the
            // entity's pose without an orphan AABB lingering in broadphase.
            if (auto *existing = r.try_get<Physics>(e); existing && existing->body)
            {
                if (world)
                    world->removeRigidBody(existing->body.get());
                r.remove<Physics>(e);
            }

            auto *t = r.try_get<Transform>(e);
            glm::mat4 noScale = t ? t->modelNoScale : glm::mat4(1.0f);
            glm::vec3 localScale = t ? t->localScale : glm::vec3(1.0f);

            shape->setLocalScaling(btVector3(localScale.x, localScale.y, localScale.z));

            btVector3 inertia(0, 0, 0);
            if (mass != 0.0)
                shape->calculateLocalInertia(mass, inertia);

            btTransform xf;
            xf.setFromOpenGLMatrix(glm::value_ptr(noScale));
            auto motion = std::make_unique<btDefaultMotionState>(xf);

            btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motion.get(), shape.get(), inertia);
            auto body = std::make_unique<btRigidBody>(rbInfo);
            body->setActivationState(ACTIVE_TAG);
            body->setUserPointer(toUserPointer(e));

            Physics phys;
            phys.shape = std::move(shape);
            phys.motion = std::move(motion);
            phys.body = std::move(body);
            phys.mass = mass;
            r.emplace<Physics>(e, std::move(phys));

            if (world)
                world->addRigidBody(r.get<Physics>(e).body.get());
        }

        void ensureShapeMarker(Registry &r, Entity e, ShapeKind kind)
        {
            auto &sm = r.get_or_emplace<ShapeMarker>(e);
            sm.kind = kind;
        }
    }

    // -------------------- Renderable ----------------------------------------

    void addRenderableCube(Registry &r, Entity e, int size, const glm::vec4 &color)
    {
        Renderable rd;
        rd.mesh = std::make_shared<CubeMesh>(size);
        rd.color = color;
        r.emplace_or_replace<Renderable>(e, std::move(rd));

        auto &sm = r.get_or_emplace<ShapeMarker>(e);
        sm.kind = ShapeKind::Cube;
        sm.cubeSize = size;
    }

    void addRenderablePlane(Registry &r, Entity e, float length, float width, const glm::vec4 &color)
    {
        Renderable rd;
        rd.mesh = std::make_shared<PlaneMesh>(length, width, length / 5.0f, width / 5.0f);
        rd.color = color;
        r.emplace_or_replace<Renderable>(e, std::move(rd));

        auto &sm = r.get_or_emplace<ShapeMarker>(e);
        sm.kind = ShapeKind::Plane;
        sm.planeLength = length;
        sm.planeWidth = width;
    }

    void addRenderableFromMesh(Registry &r, Entity e, std::shared_ptr<Mesh> mesh,
                               const std::string &meshPath, const glm::vec4 &color)
    {
        if (!mesh)
            return;
        Renderable rd;
        rd.mesh = std::move(mesh);
        rd.color = color;
        r.emplace_or_replace<Renderable>(e, std::move(rd));

        ensureShapeMarker(r, e, ShapeKind::Static);
        if (!meshPath.empty())
        {
            auto &ident = r.get_or_emplace<Identity>(e);
            ident.meshPath = meshPath;
        }
    }

    void removeRenderable(Registry &r, Entity e)
    {
        r.remove<Renderable>(e);
    }

    // -------------------- Physics -------------------------------------------

    void addBoxCollider(Registry &r, Entity e, PhysicsWorld *world,
                        const glm::vec3 &halfExtents, float mass)
    {
        emplacePhysics(r, e, world,
                       std::make_unique<btBoxShape>(btVector3(halfExtents.x, halfExtents.y, halfExtents.z)),
                       mass);
    }

    void addSphereCollider(Registry &r, Entity e, PhysicsWorld *world, float radius, float mass)
    {
        emplacePhysics(r, e, world, std::make_unique<btSphereShape>(radius), mass);
    }

    void addCapsuleCollider(Registry &r, Entity e, PhysicsWorld *world, float radius, float height, float mass)
    {
        emplacePhysics(r, e, world, std::make_unique<btCapsuleShape>(radius, height), mass);
    }

    void addConvexHullColliderFromRenderable(Registry &r, Entity e, PhysicsWorld *world, float mass)
    {
        auto *rd = r.try_get<Renderable>(e);
        if (!rd || !rd->mesh)
            return;
        const auto &verts = rd->mesh->getVertices();
        if (verts.empty())
            return;
        int stride = rd->mesh->getFloatsPerVertex();
        if (stride < 3)
            return;
        size_t vertexCount = verts.size() / stride;

        // Same dedupe + margin logic as the StaticObject factory — keeping
        // them in sync matters because either path can produce the collider.
        auto hull = std::make_unique<btConvexHullShape>();
        std::set<std::array<int32_t, 3>> seen;
        for (size_t i = 0; i < vertexCount; ++i)
        {
            const float *v = verts.data() + i * stride;
            std::array<int32_t, 3> key{
                static_cast<int32_t>(std::lround(v[0] * 100000.0f)),
                static_cast<int32_t>(std::lround(v[1] * 100000.0f)),
                static_cast<int32_t>(std::lround(v[2] * 100000.0f))};
            if (!seen.insert(key).second)
                continue;
            hull->addPoint(btVector3(v[0], v[1], v[2]), false);
        }
        hull->setMargin(0.005f);
        hull->optimizeConvexHull();
        hull->recalcLocalAabb();

        emplacePhysics(r, e, world, std::move(hull), mass);
    }

    void removePhysics(Registry &r, Entity e, PhysicsWorld *world)
    {
        auto *p = r.try_get<Physics>(e);
        if (!p)
            return;
        if (world && p->body)
            world->removeRigidBody(p->body.get());
        r.remove<Physics>(e);
    }

    // -------------------- Script --------------------------------------------

    void addScript(Registry &r, Entity e, const std::string &path)
    {
        auto &ident = r.get_or_emplace<Identity>(e);
        ident.scriptPath = path;
        // Drop any compiled bytecode so the ScriptSystem recompiles next load.
        r.remove<ScriptComponent>(e);
    }

    void removeScript(Registry &r, Entity e)
    {
        if (auto *ident = r.try_get<Identity>(e))
            ident->scriptPath.clear();
        r.remove<ScriptComponent>(e);
    }

    // -------------------- Empty entity --------------------------------------

    Entity createEmptyEntity(Registry &r, const std::string &name, const glm::mat4 &model)
    {
        Entity e = r.create();

        Identity ident;
        ident.id = g_nextOpId++;
        ident.name = name.empty() ? ("Entity " + std::to_string(ident.id)) : name;
        r.emplace<Identity>(e, std::move(ident));

        Transform t;
        initializeTransformFromModel(t, model);
        r.emplace<Transform>(e, std::move(t));

        return e;
    }
}
