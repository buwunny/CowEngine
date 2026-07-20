#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <memory>
#include "core/EngineConfig.hpp"
#include "core/PhysicsWorld.hpp"
#include "platform/Window.hpp"
#include "core/Camera.hpp"
#include "core/Scene.hpp"
#include "render/Shader.hpp"
#include "script/ScriptHost.hpp"
#include "render/PostFX.hpp"
#include "render/VfxSettings.hpp"

#if ENGINE_WITH_EDITOR
// Editor-only subsystems. In a standalone (ENGINE_WITH_EDITOR=0) build these
// headers — and the ImGui/ImGuizmo dependency they drag in — are never included.
#include "platform/ImGuiLayer.hpp"
#include "app/EditorUI.hpp"
#include "core/InputHandler.hpp"
#include "render/ColliderDebugDrawer.hpp"
#include "editor/EditorContext.hpp"
#endif

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
    void reloadScripts();

    PhysicsWorld *physics = nullptr;
    Window *window = nullptr;
    Camera *camera = nullptr;
    Scene *scene = nullptr;
    Shader *shader = nullptr;
#if ENGINE_WITH_EDITOR
    void checkSelection();

    ImGuiLayer *imguiLayer = nullptr;
    EditorUI *editorUI = nullptr;
    InputHandler *editorInput = nullptr;
    ColliderDebugDrawer *colliderDebug = nullptr;
#endif
    ScriptHost *scriptHost = nullptr;
    PostFX *postfx = nullptr;
    editor::VFX gameVfx;  // default VFX settings used in standalone game builds
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
    bool prevCursorDisabled = false;
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
