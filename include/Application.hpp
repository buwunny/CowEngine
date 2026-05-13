#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <memory>
#include "PhysicsWorld.hpp"
#include "Window.hpp"
#include "Camera.hpp"
#include "Scene.hpp"
#include "Shader.hpp"

class Application
{
public:
    Application();
    ~Application();

    // Initialize resources (load scene, create player, shader)
    void init();

    // Single tick: update physics, process input, render
    void tick();

    // Desktop run loop
    void runDesktop();

private:
    PhysicsWorld *physics = nullptr;
    Window *window = nullptr;
    Camera *camera = nullptr;
    Scene *scene = nullptr;
    Shader *shader = nullptr;

    double lastFrame = 0.0;

    // FPS accounting for desktop builds
    int fpsCount = 0;
    double fpsTimer = 0.0;
    bool lastRPressed = false;
};

#endif // APPLICATION_HPP

#ifdef __EMSCRIPTEN__
extern "C"
{
    void app_tick();
    void app_set_global(Application *a);
    void app_run_main_loop();
}
#endif
