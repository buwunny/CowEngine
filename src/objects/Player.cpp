#include "objects/Player.hpp"

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

    // Apply accumulated pointer-lock mouse deltas once per frame with exponential smoothing
    {
        // smoothing rate (higher = faster response, lower = smoother)
        const float smoothingK = 20.0f;
        float alpha = 1.0f - std::exp(-smoothingK * deltaTime);
        smoothedMouseDx += (pendingMouseDx - smoothedMouseDx) * alpha;
        smoothedMouseDy += (pendingMouseDy - smoothedMouseDy) * alpha;
        // multiplier to match desktop sensitivity
        const float pointerLockMultiplier = 5.0f;
        // apply small movements even if below integer thresholds
        if (std::fabs(smoothedMouseDx) > 0.0001f || std::fabs(smoothedMouseDy) > 0.0001f)
        {
            this->camera->look(smoothedMouseDx * pointerLockMultiplier, -smoothedMouseDy * pointerLockMultiplier);
        }
        // clear pending deltas (they've been integrated into smoothed state)
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

void Player::processMouse(float xpos, float ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

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
    player->processMouse(xpos, ypos);
}