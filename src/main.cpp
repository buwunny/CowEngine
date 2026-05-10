#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>

#include <bullet/btBulletDynamicsCommon.h>
#include <bullet/btBulletCollisionCommon.h>
#include "PhysicsWorld.hpp"

#include "Shader.hpp"
#include "objects/Cube.hpp"
#include "objects/Plane.hpp"
#include "objects/Player.hpp"
#include "Camera.hpp"
#include "InputHandler.hpp"
#include "Window.hpp"
#include "objects/StaticObject.hpp"
#include "Scene.hpp"
#include "meshes/AssetManager.hpp"
#include <memory>

float deltaTime = 0.0f;
float lastFrame = 0.0f;

int main()
{
    PhysicsWorld physics;

    Window window(1920, 1080, "Spinning Cube");
    Camera playerCamera(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Create player via Scene so Scene owns it and registers callbacks
    // Player will be created and registered below after scene population

    // Create scene and populate default objects (or load from scene.json)
    Scene scene;
    if (!scene.loadFromJSON("scenes/scene.json"))
        scene.populateDefault();
    scene.addRigidBodiesToWorld(physics);

    // Create and register player with the scene
    scene.addPlayer(std::make_unique<Player>(&playerCamera, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 3.0f, 10.0f))), &window, physics);

    // BasicRoom logic moved into Scene::populateDefault()

    Shader shader("./shaders/vertex.glsl", "./shaders/fragment.glsl");

    // FPS counter: frames and accumulated time
    int fpsCount = 0;
    double fpsTimer = 0.0;

    bool lastRPressed = false;
    while (!window.shouldClose())
    {
        // Manual reload key (R) edge-detect
        bool r = window.isKeyPressed(GLFW_KEY_R);
        if (r && !lastRPressed)
        {
            scene.forceReload();
        }
        lastRPressed = r;

        // Check for scene file changes and reload if needed
        scene.checkReload();
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Update FPS counters and window title periodically
        fpsCount++;
        fpsTimer += deltaTime;
        if (fpsTimer >= 0.5)
        {
            double fps = fpsCount / fpsTimer;
            std::ostringstream oss;
            oss << "OpenGLProject - FPS: " << std::fixed << std::setprecision(1) << fps;
            std::string title = oss.str();
            glfwSetWindowTitle(window.getWindow(), title.c_str());
            fpsCount = 0;
            fpsTimer = 0.0;
        }

        physics.stepSimulation(deltaTime, 10);

        if (scene.getPlayer())
            scene.getPlayer()->processInput(&window, deltaTime, &physics);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();

        glm::mat4 view = glm::lookAt(playerCamera.getPosition(), playerCamera.getPosition() + playerCamera.getFront(), playerCamera.getUp());
        int width, height;
        glfwGetFramebufferSize(window.getWindow(), &width, &height);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f);
        shader.setViewMatrix(view);
        shader.setProjectionMatrix(projection);

        scene.update();
        scene.render(window, shader);

        // player is updated by scene.update(); scene contains room objects

        window.update();
    }

    return 0;
}