#include "objects/StaticObject.hpp"
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
        glm::vec3 vmin(verts[0], verts[1], verts[2]);
        glm::vec3 vmax = vmin;
        for (size_t i = 1; i < vertex_count; ++i)
        {
            const float *v = verts + i * floats_per_vertex;
            glm::vec3 p(v[0], v[1], v[2]);
            vmin.x = std::min(vmin.x, p.x);
            vmin.y = std::min(vmin.y, p.y);
            vmin.z = std::min(vmin.z, p.z);
            vmax.x = std::max(vmax.x, p.x);
            vmax.y = std::max(vmax.y, p.y);
            vmax.z = std::max(vmax.z, p.z);
        }

        glm::vec3 center = (vmin + vmax) * 0.5f;
        glm::vec3 halfExtents = (vmax - vmin) * 0.5f;

        // Build convex hull shape centered on origin by subtracting center
        btConvexHullShape *hull = new btConvexHullShape();
        for (size_t i = 0; i < vertex_count; ++i)
        {
            const float *v = verts + i * floats_per_vertex;
            btVector3 p(v[0] - center.x, v[1] - center.y, v[2] - center.z);
            hull->addPoint(p, false);
        }
        hull->optimizeConvexHull();
        hull->recalcLocalAabb();

        collisionShape.reset(hull);

        // Incorporate center offset into model and initial rigidbody transform
        glm::mat4 centerMat = glm::translate(glm::mat4(1.0f), center);
        this->setInitialModel(model * centerMat);

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

        // Activate and log for debugging on web builds
        rigidBody->setActivationState(ACTIVE_TAG);
        std::printf("StaticObject: created rigidBody=%p mass=%.3f objs_vertices=%zu inds=%zu active=%d\n", (void *)rigidBody.get(), (double)mass, vertex_count, index_count, rigidBody->getActivationState());

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
        glm::vec3 vmin(verts[0], verts[1], verts[2]);
        glm::vec3 vmax = vmin;
        for (size_t i = 1; i < vertex_count; ++i)
        {
            const float *v = verts + i * floats_per_vertex;
            glm::vec3 p(v[0], v[1], v[2]);
            vmin.x = std::min(vmin.x, p.x);
            vmin.y = std::min(vmin.y, p.y);
            vmin.z = std::min(vmin.z, p.z);
            vmax.x = std::max(vmax.x, p.x);
            vmax.y = std::max(vmax.y, p.y);
            vmax.z = std::max(vmax.z, p.z);
        }

        glm::vec3 center = (vmin + vmax) * 0.5f;
        glm::vec3 halfExtents = (vmax - vmin) * 0.5f;

        btConvexHullShape *hull = new btConvexHullShape();
        for (size_t i = 0; i < vertex_count; ++i)
        {
            const float *v = verts + i * floats_per_vertex;
            btVector3 p(v[0] - center.x, v[1] - center.y, v[2] - center.z);
            hull->addPoint(p, false);
        }
        hull->optimizeConvexHull();
        hull->recalcLocalAabb();

        collisionShape.reset(hull);

        // Incorporate center offset into model and initial rigidbody transform
        glm::mat4 centerMat = glm::translate(glm::mat4(1.0f), center);
        this->setInitialModel(model * centerMat);

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

void StaticObject::render(Window &window, Shader &shader)
{
    shader.setModelMatrix(model);
    // Draw filled geometry first. Use polygon offset to push fill slightly
    // away from the camera to avoid depth-equal fragments blocking the
    // subsequent wireframe/line overlay (prevents z-fighting on web/GL).
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    window.setPolygonMode(GL_FILL);
    shader.setFragmentColor(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    mesh->render();
    glDisable(GL_POLYGON_OFFSET_FILL);

    // Draw wireframe overlay on top
    window.setPolygonMode(GL_LINE);
    window.setLineWidth(3.0f);
    shader.setFragmentColor(color);
    mesh->renderWireframe();
}

void StaticObject::renderTransparent(Window &window, Shader &shader)
{
    shader.setModelMatrix(model);
    window.setPolygonMode(GL_LINE);
    window.setLineWidth(3.0f);
    shader.setFragmentColor(color);
    mesh->renderWireframe();
}

void StaticObject::renderFill(Window &window, Shader &shader)
{
    shader.setModelMatrix(model);
    window.setPolygonMode(GL_FILL);
    shader.setFragmentColor(color);
    mesh->render();
}