#include <iostream>

#include <bullet/btBulletDynamicsCommon.h>
#include <bullet/btBulletCollisionCommon.h>

#include "Shader.hpp"
#include "objects/Cube.hpp"
#include "objects/Plane.hpp"
#include "objects/Player.hpp"
#include "rooms/BasicRoom.hpp"
#include "Camera.hpp"
#include "InputHandler.hpp"
#include "Window.hpp"
#include "objects/StaticObject.hpp"
#include "../../cow_mesh.hpp"

float deltaTime = 0.0f;
float lastFrame = 0.0f;

int main()
{
    btDefaultCollisionConfiguration *collisionConfiguration = new btDefaultCollisionConfiguration();
    btCollisionDispatcher *dispatcher = new btCollisionDispatcher(collisionConfiguration);
    btBroadphaseInterface *overlappingPairCache = new btDbvtBroadphase();
    btSequentialImpulseConstraintSolver *solver = new btSequentialImpulseConstraintSolver();
    btDiscreteDynamicsWorld *dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, overlappingPairCache, solver, collisionConfiguration);
    dynamicsWorld->setGravity(btVector3(0, -9.81 * 2, 0));

    Window window(1920, 1080, "Spinning Cube");
    Camera playerCamera(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    Player player(&playerCamera, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 3.0f, 10.0f)));
    glfwSetWindowUserPointer(window.getWindow(), &player);
    glfwSetCursorPosCallback(window.getWindow(), player.mouse_callback);

    // Create and populate objects safely. Use push_back so vector has
    // valid elements and iterate over the vector when using them.
    std::vector<Object *> objects;
    int numObjects = 50;
    objects.reserve(numObjects);

    // Create a few larger/static cubes first (heap-allocated so they live)
    objects.push_back(new Cube(3, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 15.0f, 0.0f)), glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 10.0f));
    objects.push_back(new Cube(2, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 3.0f, 0.0f)), glm::vec4(0.0f, 0.5f, 0.5f, 1.0f), 1.0f));
    objects.push_back(new Cube(2, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 6.0f, 0.0f)), glm::vec4(0.5f, 0.5f, 0.0f, 1.0f), 1.0f));
    objects.push_back(new Cube(2, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 9.0f, 0.0f)), glm::vec4(0.5f, 0.0f, 0.5f, 1.0f), 1.0f));

    // Fill remaining objects (start at index 4)
    for (int i = 4; i < numObjects; i++)
    {
        float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        float g = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        float b = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        // objects.push_back(new Cube(2, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 3.0f + i * 3.0f, 0.0f)), glm::vec4(r, g, b, 1.0f), 1.0f));
        objects.push_back(new StaticObject(cow_mesh_vertices, cow_mesh_vertex_count, cow_mesh_indices, cow_mesh_index_count, 3, glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 10.0f, 5.0f)), glm::vec4(r, g, b, 1.0f), 1.0f));
    }

    // Add all created objects to the physics world
    for (auto obj : objects)
    {
        if (obj)
            dynamicsWorld->addRigidBody(obj->getRigidBody());
    }

    BasicRoom room(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    room.addRigidBodiesToWorld(dynamicsWorld);
    dynamicsWorld->addRigidBody(player.getRigidBody());

    Shader shader("./shaders/vertex.glsl", "./shaders/fragment.glsl");

    while (!window.shouldClose())
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        dynamicsWorld->stepSimulation(deltaTime, 10);

        player.processInput(&window, deltaTime, dynamicsWorld);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();

        glm::mat4 view = glm::lookAt(playerCamera.getPosition(), playerCamera.getPosition() + playerCamera.getFront(), playerCamera.getUp());
        int width, height;
        glfwGetFramebufferSize(window.getWindow(), &width, &height);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f);
        shader.setViewMatrix(view);
        shader.setProjectionMatrix(projection);

        for (auto obj : objects)
        {
            if (!obj)
                continue;
            btTransform trans;
            obj->getRigidBody()->getMotionState()->getWorldTransform(trans);
            btScalar matrix[16];
            trans.getOpenGLMatrix(matrix);
            glm::mat4 modelMatrix = glm::make_mat4(matrix);
            obj->setModel(modelMatrix);
            obj->render(window, shader);
        }

        player.update();
        room.update();
        room.render(window, shader);

        window.update();
    }

    delete dynamicsWorld;
    delete solver;
    delete overlappingPairCache;
    delete dispatcher;
    delete collisionConfiguration;

    return 0;
}