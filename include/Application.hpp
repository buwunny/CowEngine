#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <memory>
#include "PhysicsWorld.hpp"
#include "Window.hpp"
#include "Camera.hpp"
#include "Scene.hpp"
#include "Shader.hpp"
#include "ImGuiLayer.hpp"
#include "EditorUI.hpp"
#include "InputHandler.hpp"
#include "script/ScriptHost.hpp"

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

#ifdef __EMSCRIPTEN__
    // flag to see if we have local storage data to load on startup
    void setHasLocalStorageData(bool hasData) { hasLocalStorageData = hasData; };
    void setPendingLocalStorageData(const std::string &data) { pendingLocalStorageData = data; };
#endif

private:
    void checkSelection();
    void reloadScripts();

    PhysicsWorld *physics = nullptr;
    Window *window = nullptr;
    Camera *camera = nullptr;
    Scene *scene = nullptr;
    Shader *shader = nullptr;
    ImGuiLayer *imguiLayer = nullptr;
    EditorUI *editorUI = nullptr;
    InputHandler *editorInput = nullptr;
    ScriptHost *scriptHost = nullptr;
    double scriptTime = 0.0;

    unsigned int gameFbo = 0;
    unsigned int gameColor = 0;
    unsigned int gameDepth = 0;
    int gameFbWidth = 0;
    int gameFbHeight = 0;

    double lastFrame = 0.0;

    // FPS accounting for desktop builds
    int fpsCount = 0;
    double fpsTimer = 0.0;
    bool lastRPressed = false;
    bool lastTestingMode = false;
#ifdef __EMSCRIPTEN__
    // flag to see if we have local storage data to load on startup
    bool hasLocalStorageData = false;
    std::string pendingLocalStorageData;
#endif
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
