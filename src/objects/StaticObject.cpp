#include "objects/StaticObject.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <set>
#include <string>

StaticObject::StaticObject(const float *verts, size_t vertex_count, const unsigned int *indices, size_t index_count, int floats_per_vertex, glm::mat4 model, glm::vec4 color, float mass)
{
    setName(std::string("StaticObject ") + std::to_string(getId()));
    mesh = std::make_shared<StaticMesh>(verts, vertex_count, indices, index_count, floats_per_vertex);

    // Compute tight AABB from vertex positions (assumes position is first 3 floats)
    if (vertex_count == 0 || floats_per_vertex < 3)
    {
        // fallback
        collisionShape = std::make_unique<btBoxShape>(btVector3(1.0f, 1.0f, 1.0f));
    }
    else
    {
        // Build the hull in the mesh's original local space — the renderer
        // draws vertices unmodified, so the collider has to share that frame
        // or it ends up visually offset (e.g. the AABB center of an Eiffel
        // Tower model sits high up the spire while its origin is at the base).
        // Dedupe positions: OBJ vertices that share a position but differ in
        // normal/UV would otherwise be added many times, slowing
        // optimizeConvexHull and leaving near-coincident points that can
        // survive its reduction. Quantize to 1e-5 units so float jitter
        // collapses to a single key.
        btConvexHullShape *hull = new btConvexHullShape();
        std::set<std::array<int32_t, 3>> seenPositions;
        for (size_t i = 0; i < vertex_count; ++i)
        {
            const float *v = verts + i * floats_per_vertex;
            std::array<int32_t, 3> key{
                static_cast<int32_t>(std::lround(v[0] * 100000.0f)),
                static_cast<int32_t>(std::lround(v[1] * 100000.0f)),
                static_cast<int32_t>(std::lround(v[2] * 100000.0f))};
            if (!seenPositions.insert(key).second)
                continue;
            hull->addPoint(btVector3(v[0], v[1], v[2]), false);
        }
        // Bullet's default 0.04 unit collision margin inflates the effective
        // contact surface — for sub-meter models this is the dominant source
        // of "loose" hits. Use a small absolute value that still keeps GJK
        // numerically stable.
        hull->setMargin(0.005f);
        hull->optimizeConvexHull();
        hull->recalcLocalAabb();

        collisionShape.reset(hull);

        this->setInitialModel(model);

        btVector3 inertia(0, 0, 0);
        if (mass != 0.0f)
        {
            this->setMass(mass);
        }

        btTransform transform;
        transform.setFromOpenGLMatrix(glm::value_ptr(this->modelNoScale));
        motionState = std::make_unique<btDefaultMotionState>(transform);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState.get(), collisionShape.get(), inertia);
        rigidBody.reset(new btRigidBody(rbInfo));

        this->color = color;

        rigidBody->setActivationState(ACTIVE_TAG);
        rigidBody->setUserPointer(this);
        return;
    }

    // Fallback path (used when vertex data invalid)
    btVector3 localInertia(0, 0, 0);
    if (mass != 0.0f)
    {
        this->setMass(mass);
    }
    btTransform transform;
    transform.setFromOpenGLMatrix(glm::value_ptr(model));
    motionState = std::make_unique<btDefaultMotionState>(transform);
    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState.get(), collisionShape.get(), localInertia);
    rigidBody.reset(new btRigidBody(rbInfo));

    this->setInitialModel(model);
    this->color = color;
}

StaticObject::StaticObject(std::shared_ptr<Mesh> sharedMesh, const float *verts, size_t vertex_count, const unsigned int *indices, size_t index_count, int floats_per_vertex, glm::mat4 model, glm::vec4 color, float mass)
{
    setName(std::string("StaticObject ") + std::to_string(getId()));
    // Use provided shared mesh for rendering; still compute collision shape from vertex data
    mesh = sharedMesh;

    // Compute tight AABB from vertex positions (assumes position is first 3 floats)
    if (vertex_count == 0 || floats_per_vertex < 3)
    {
        // fallback
        collisionShape = std::make_unique<btBoxShape>(btVector3(1.0f, 1.0f, 1.0f));
    }
    else
    {
        // Build the hull in the mesh's original local space so it lines up
        // with the renderer (see the matching block in the other constructor
        // for the full rationale).
        btConvexHullShape *hull = new btConvexHullShape();
        std::set<std::array<int32_t, 3>> seenPositions;
        for (size_t i = 0; i < vertex_count; ++i)
        {
            const float *v = verts + i * floats_per_vertex;
            std::array<int32_t, 3> key{
                static_cast<int32_t>(std::lround(v[0] * 100000.0f)),
                static_cast<int32_t>(std::lround(v[1] * 100000.0f)),
                static_cast<int32_t>(std::lround(v[2] * 100000.0f))};
            if (!seenPositions.insert(key).second)
                continue;
            hull->addPoint(btVector3(v[0], v[1], v[2]), false);
        }
        hull->setMargin(0.005f);
        hull->optimizeConvexHull();
        hull->recalcLocalAabb();

        collisionShape.reset(hull);

        this->setInitialModel(model);

        btVector3 inertia(0, 0, 0);
        if (mass != 0.0f)
        {
            this->setMass(mass);
        }

        btTransform transform;
        transform.setFromOpenGLMatrix(glm::value_ptr(this->modelNoScale));
        motionState = std::make_unique<btDefaultMotionState>(transform);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState.get(), collisionShape.get(), inertia);
        rigidBody.reset(new btRigidBody(rbInfo));

        this->color = color;
        rigidBody->setUserPointer(this);

        return;
    }

    // Fallback path
    btVector3 localInertia(0, 0, 0);
    if (mass != 0.0f)
    {
        collisionShape->calculateLocalInertia(mass, localInertia);
    }
    btTransform transform;
    transform.setFromOpenGLMatrix(glm::value_ptr(this->modelNoScale));
    motionState = std::make_unique<btDefaultMotionState>(transform);
    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState.get(), collisionShape.get(), localInertia);
    rigidBody.reset(new btRigidBody(rbInfo));

    this->setInitialModel(model);
    this->color = color;
}