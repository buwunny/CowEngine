#include "Application.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdlib>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
#include "../cow_mesh.hpp"

Application::Application()
{
}

Application::~Application()
{
    delete shader;
    delete scene;
    delete camera;
    delete window;
    delete physics;
}

void Application::init()
{
    physics = new PhysicsWorld();
    window = new Window(1920, 1080, "CowEngine");
    camera = new Camera(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    scene = new Scene();
    if (!scene->loadFromJSON("scenes/scene.json"))
        scene->populateDefault();
    scene->addRigidBodiesToWorld(*physics);

    scene->addPlayer(std::make_unique<Player>(camera, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 3.0f, 10.0f))), window, *physics);

    // Ensure camera is positioned to match the player's initial transform on web builds
    camera->setPosition(glm::vec3(0.0f, 3.0f, 10.0f));

    shader = new Shader("./shaders/vertex.glsl", "./shaders/fragment.glsl");

#ifdef __EMSCRIPTEN__
    lastFrame = emscripten_get_now() / 1000.0;
#else
    lastFrame = glfwGetTime();
#endif
}

static double getTimeSeconds()
{
#ifdef __EMSCRIPTEN__
    return emscripten_get_now() / 1000.0;
#else
    return glfwGetTime();
#endif
}

void Application::tick()
{
    // Edge detect reload (R)
    bool r = window->isKeyPressed(GLFW_KEY_R);
    if (r && !lastRPressed)
    {
        scene->forceReload();
    }
    lastRPressed = r;

    // Hot-reload
    scene->checkReload();

    double current = getTimeSeconds();
    float delta = static_cast<float>(current - lastFrame);
    lastFrame = current;

    // FPS bookkeeping
    fpsCount++;
    fpsTimer += delta;
    if (fpsTimer >= 0.5)
    {
        fpsCount = 0;
        fpsTimer = 0.0;
    }

    physics->stepSimulation(delta, 10);

    if (scene->getPlayer())
        scene->getPlayer()->processInput(window, delta, physics);

    // Resize viewport / compute aspect
    int width = 0, height = 0;
#ifdef __EMSCRIPTEN__
    emscripten_get_canvas_element_size("canvas", &width, &height);
#else
    glfwGetFramebufferSize(window->getWindow(), &width, &height);
#endif
    if (width > 0 && height > 0)
        glViewport(0, 0, width, height);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader->use();

    glm::mat4 view = glm::lookAt(camera->getPosition(), camera->getPosition() + camera->getFront(), camera->getUp());
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f);

    shader->setViewMatrix(view);
    shader->setProjectionMatrix(projection);

    scene->update();
    scene->render(*window, *shader);

    window->update();
}

void Application::runDesktop()
{
    // Desktop main loop
    while (!window->shouldClose())
    {
        tick();

        // Update window title with FPS periodically
        // Note: fps count/timer already handled in tick; compute instantaneous fps occasionally
        // (Kept minimal to avoid excessive allocations)
        // Sleep/yield not added; rely on vsync or GL swap
    }
}

// Expose a web-friendly tick function when building with Emscripten
#ifdef __EMSCRIPTEN__
static Application *g_app = nullptr;
extern "C"
{
    EMSCRIPTEN_KEEPALIVE
    void app_tick()
    {
        if (g_app)
            g_app->tick();
    }

    EMSCRIPTEN_KEEPALIVE
    void spawnCow()
    {
        Scene *s = Scene::getCurrent();
        if (!s)
            return;
        auto &assetManager = AssetManager::instance();
        auto cowMesh = assetManager.loadStaticMeshFromOBJ("models/cow.obj", "cow");
        if (!cowMesh)
            cowMesh = assetManager.loadStaticMeshFromArrays("cow", cow_mesh_vertices, cow_mesh_vertex_count, cow_mesh_indices, cow_mesh_index_count, 3);

        glm::vec3 pos(0.0f, 10.0f, 0.0f);
        if (s->getPlayer())
        {
            Camera *cam = s->getPlayer()->getCamera();
            if (cam)
            {
                pos = cam->getPosition() + cam->getFront() * 5.0f;
                pos.y += 2.0f;
            }
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
            s->addObject(std::move(obj));
        }
    }
}
#ifdef __EMSCRIPTEN__
extern "C" EMSCRIPTEN_KEEPALIVE void app_run_main_loop()
{
    if (g_app)
        emscripten_set_main_loop(app_tick, 0, 1);
}
#endif
#ifdef __EMSCRIPTEN__
extern "C" EMSCRIPTEN_KEEPALIVE void app_set_global(Application *a)
{
    g_app = a;
}
#endif
#endif
