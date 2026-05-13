#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>

#include <btBulletDynamicsCommon.h>
#include <btBulletCollisionCommon.h>
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
#include "../cow_mesh.hpp"
#include <cstdlib>

float deltaTime = 0.0f;
float lastFrame = 0.0f;
// Emscripten requires a non-blocking main loop (use emscripten_set_main_loop).
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>

static PhysicsWorld *g_physics = nullptr;
static Window *g_window = nullptr;
static Camera *g_playerCamera = nullptr;
static Scene *g_scene = nullptr;
static Shader *g_shader = nullptr;

static int g_fpsCount = 0;
static double g_fpsTimer = 0.0;
static bool g_lastRPressed = false;
static double g_lastFrame = 0.0;

void main_loop()
{
    // Manual reload key (R) edge-detect
    bool r = g_window->isKeyPressed(GLFW_KEY_R);
    if (r && !g_lastRPressed)
    {
        g_scene->forceReload();
    }
    g_lastRPressed = r;

    // Check for scene file changes and reload if needed
    g_scene->checkReload();

    double currentFrame = emscripten_get_now() / 1000.0;
    float delta = (float)(currentFrame - g_lastFrame);
    g_lastFrame = currentFrame;

    // Update FPS counters
    g_fpsCount++;
    g_fpsTimer += delta;
    if (g_fpsTimer >= 0.5)
    {
        g_fpsCount = 0;
        g_fpsTimer = 0.0;
    }

    g_physics->stepSimulation(delta, 10);

    if (g_scene->getPlayer())
        g_scene->getPlayer()->processInput(g_window, delta, g_physics);

    // Resize viewport to canvas size
    int w = 0, h = 0;
    emscripten_get_canvas_element_size("canvas", &w, &h);
    if (w > 0 && h > 0)
    {
        glViewport(0, 0, w, h);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    g_shader->use();

    glm::mat4 view = glm::lookAt(g_playerCamera->getPosition(), g_playerCamera->getPosition() + g_playerCamera->getFront(), g_playerCamera->getUp());
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)w / (float)h, 0.1f, 1000.0f);
    g_shader->setViewMatrix(view);
    g_shader->setProjectionMatrix(projection);

    g_scene->update();
    g_scene->render(*g_window, *g_shader);

    g_window->update();
}

// Expose a spawn function to JavaScript so the page can add cows at runtime.
extern "C"
{
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_KEEPALIVE
    void spawnCow()
    {
        if (!g_scene)
            return;
        auto &assetManager = AssetManager::instance();
        auto cowMesh = assetManager.loadStaticMeshFromOBJ("models/cow.obj", "cow");
        if (!cowMesh)
            cowMesh = assetManager.loadStaticMeshFromArrays("cow", cow_mesh_vertices, cow_mesh_vertex_count, cow_mesh_indices, cow_mesh_index_count, 3);

        glm::vec3 pos(0.0f, 10.0f, 0.0f);
        if (g_playerCamera)
        {
            pos = g_playerCamera->getPosition() + g_playerCamera->getFront() * 5.0f;
            pos.y += 2.0f;
        }

        glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
        float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        float g = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        float b = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        glm::vec4 color = glm::vec4(r, g, b, 1.0f);
        float mass = 1.0f;

        if (cowMesh)
        {
            const auto &verts = cowMesh->getVertices();
            const auto &inds = cowMesh->getIndices();
            int stride = cowMesh->getFloatsPerVertex();
            auto obj = std::make_unique<StaticObject>(cowMesh, verts.data(), verts.size() / stride, inds.data(), inds.size(), stride, model, color, mass);
            g_scene->addObject(std::move(obj));
        }
    }
#endif
}

int main()
{
    // Allocate main objects on the heap and store pointers for the emscripten loop
    g_physics = new PhysicsWorld();
    g_window = new Window(1920, 1080, "CowEngine");
    g_playerCamera = new Camera(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    g_scene = new Scene();
    if (!g_scene->loadFromJSON("scenes/scene.json"))
        g_scene->populateDefault();
    g_scene->addRigidBodiesToWorld(*g_physics);

    g_scene->addPlayer(std::make_unique<Player>(g_playerCamera, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 3.0f, 10.0f))), g_window, *g_physics);

    // Ensure camera is positioned to match the player's initial transform on web builds
    g_playerCamera->setPosition(glm::vec3(0.0f, 3.0f, 10.0f));

    g_shader = new Shader("./shaders/vertex.glsl", "./shaders/fragment.glsl");

    g_lastFrame = emscripten_get_now() / 1000.0;

    // Start the browser-friendly main loop
    emscripten_set_main_loop(main_loop, 0, 1);

    return 0;
}
#else
int main()
{
    PhysicsWorld physics;

    Window window(1920, 1080, "CowEngine");
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
            oss << "CowEngine - FPS: " << std::fixed << std::setprecision(1) << fps;
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
#endif