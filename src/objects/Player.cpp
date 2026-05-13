#include "objects/Player.hpp"
#include "meshes/AssetManager.hpp"
#include "Scene.hpp"
#include "../cow_mesh.hpp"
#include <cstdlib>
#include <glm/gtc/matrix_transform.hpp>

Player::Player(Camera *camera, glm::mat4 model)
{
    mass = 10.0f;
    movementSpeed = 100.0f;
    lastX = 1920 / 2;
    lastY = 1080 / 2;
    firstMouse = true;
    lastTabPressed = false;
    this->camera = camera;
    inputHandler = std::make_unique<InputHandler>(camera);
    collisionShape = std::make_unique<btCapsuleShape>(0.5f, 1.0f);
    btVector3 localInertia(0, 0, 0);
    collisionShape->calculateLocalInertia(mass, localInertia);

    btTransform transform;
    transform.setFromOpenGLMatrix(glm::value_ptr(model));
    motionState = std::make_unique<btDefaultMotionState>(transform);

    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState.get(), collisionShape.get(), localInertia);
    rigidBody.reset(new btRigidBody(rbInfo));
    rigidBody->setAngularFactor(btVector3(0, 0, 0));

    rigidBody->setCcdMotionThreshold(0.5);
    rigidBody->setCcdSweptSphereRadius(0.4);

    rigidBody->setFriction(1.0f);
}

bool Player::isOnGround(PhysicsWorld *physics)
{
    btVector3 start = rigidBody->getWorldTransform().getOrigin();
    btVector3 end = start - btVector3(0, 1.05f, 0);
    btCollisionWorld::ClosestRayResultCallback rayCallback(start, end);
    physics->rayTest(start, end, rayCallback);
    return rayCallback.hasHit();
}

void Player::processInput(Window *window, float deltaTime, PhysicsWorld *physics)
{
    // Handle Tab toggle first so we can relock even when cursor is currently unlocked
    bool tab = window->isKeyPressed(GLFW_KEY_TAB);
    if (tab && !lastTabPressed)
    {
        bool disabled = window->toggleCursor();
        if (!disabled)
            firstMouse = true; // reset mouse so movement doesn't jump when re-enabled
    }
    lastTabPressed = tab;

    // If cursor is unlocked (normal), don't process movement or mouse control
    if (!window->isCursorDisabled())
        return;

    // Spawn small cow projectile on C key (edge detect)
    {
        bool c = window->isKeyPressed(GLFW_KEY_C);
        if (c && !lastCPressed)
        {
            Scene *sc = Scene::getCurrent();
            if (sc)
            {
                auto &assetManager = AssetManager::instance();
                auto cowMesh = assetManager.loadStaticMeshFromOBJ("models/cow.obj", "cow");
                if (!cowMesh)
                    cowMesh = assetManager.loadStaticMeshFromArrays("cow", cow_mesh_vertices, cow_mesh_vertex_count, cow_mesh_indices, cow_mesh_index_count, 3);

                glm::vec3 pos = camera->getPosition() + camera->getFront() * 2.0f;
                pos.y += 1.0f;
                float scale = 0.35f;
                glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
                model = glm::scale(model, glm::vec3(scale));

                float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                float g = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                float b = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                glm::vec4 color = glm::vec4(r, g, b, 1.0f);

                float mass = 0.5f;
                if (cowMesh)
                {
                    const auto &verts = cowMesh->getVertices();
                    const auto &inds = cowMesh->getIndices();
                    int stride = cowMesh->getFloatsPerVertex();
                    auto obj = std::make_unique<StaticObject>(cowMesh, verts.data(), verts.size() / stride, inds.data(), inds.size(), stride, model, color, mass);
                    btRigidBody *rb = obj->getRigidBody();
                    sc->addObject(std::move(obj));
                    if (rb)
                    {
                        float speed = 50.0f;
                        glm::vec3 dir = glm::normalize(camera->getFront());
                        rb->setLinearVelocity(btVector3(dir.x * speed, dir.y * speed, dir.z * speed));
                    }
                }
            }
        }
        lastCPressed = c;
    }

    // Apply accumulated pointer-lock mouse deltas once per frame (no smoothing)
    {
        // multiplier to match desktop sensitivity
        const float pointerLockMultiplier = 5.0f;
        // apply small movements even if below integer thresholds
        if (std::fabs(pendingMouseDx) > 0.0001f || std::fabs(pendingMouseDy) > 0.0001f)
        {
            this->camera->look(pendingMouseDx * pointerLockMultiplier, -pendingMouseDy * pointerLockMultiplier);
        }
        // clear pending deltas
        pendingMouseDx = 0.0f;
        pendingMouseDy = 0.0f;
    }

    float cameraSpeed = movementSpeed * deltaTime;

    btVector3 velocity = rigidBody->getLinearVelocity();
    btVector3 forwardDir = btVector3(camera->getFront().x, 0, camera->getFront().z).normalized();
    btVector3 rightDir = btVector3(camera->getRight().x, 0, camera->getRight().z).normalized();

    bool isOnGround = this->isOnGround(physics);
    float adjustedSpeed = isOnGround ? cameraSpeed : cameraSpeed * 0.1f;

    if (window->isKeyPressed(GLFW_KEY_W))
        velocity += forwardDir * adjustedSpeed;
    if (window->isKeyPressed(GLFW_KEY_S))
        velocity -= forwardDir * adjustedSpeed;
    if (window->isKeyPressed(GLFW_KEY_A))
        velocity -= rightDir * adjustedSpeed;
    if (window->isKeyPressed(GLFW_KEY_D))
        velocity += rightDir * adjustedSpeed;
    if (window->isKeyPressed(GLFW_KEY_ESCAPE))
        window->close();
    if (window->isKeyPressed(GLFW_KEY_SPACE) && isOnGround)
        velocity.setY(10.0f);

    rigidBody->activate(true);
    float maxSpeed = 10.0f;
    if (window->isKeyPressed(GLFW_KEY_LEFT_SHIFT))
        maxSpeed *= 2;
    if (velocity.length() > maxSpeed)
    {
        velocity = velocity.normalized() * maxSpeed;
    }
    rigidBody->setLinearVelocity(velocity);

    btTransform trans;
    rigidBody->getMotionState()->getWorldTransform(trans);
    btVector3 pos = trans.getOrigin();
    camera->setPosition(glm::vec3(pos.getX(), pos.getY() + 1.0f, pos.getZ()));
}

void Player::processMouse(GLFWwindow *window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = static_cast<float>(xpos - lastX);
    float yoffset = static_cast<float>(lastY - ypos);
    lastX = xpos;
    lastY = ypos;

    // Apply device-pixel scaling and global sensitivity, plus pointer-lock multiplier
    float dpr = getDevicePixelRatioFor(window);
    double sens = getMouseSensitivityFor(window);
    const float pointerLockMultiplier = 5.0f;
    xoffset = xoffset * dpr * static_cast<float>(sens) * pointerLockMultiplier;
    yoffset = yoffset * dpr * static_cast<float>(sens) * pointerLockMultiplier;

    this->camera->look(xoffset, yoffset);
}

void Player::processMouseDelta(float dx, float dy)
{
    // Accumulate deltas; they will be applied once per frame in processInput
    pendingMouseDx += dx;
    pendingMouseDy += dy;
}

void Player::resetInputState()
{
    firstMouse = true;
    lastTabPressed = false;
}

void Player::mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    Player *player = static_cast<Player *>(glfwGetWindowUserPointer(window));
    if (!player)
        return;
    // Only process mouse movement when cursor is disabled (captured)
    int mode = glfwGetInputMode(window, GLFW_CURSOR);
    if (mode != GLFW_CURSOR_DISABLED)
        return;
    player->processMouse(window, xpos, ypos);
}