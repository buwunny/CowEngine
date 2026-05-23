#include "Application.hpp"
#include "ImGuiLayer.hpp"
#include "EditorUI.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdlib>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

Application::Application()
{
}

Application::~Application()
{
    if (gameDepth)
        glDeleteRenderbuffers(1, &gameDepth);
    if (gameColor)
        glDeleteTextures(1, &gameColor);
    if (gameFbo)
        glDeleteFramebuffers(1, &gameFbo);
    delete shader;
    delete scene;
    delete camera;
    delete window;
    delete physics;
    delete imguiLayer;
    delete editorUI;
    delete editorInput;
    delete scriptHost;
}

void Application::init()
{
    physics = new PhysicsWorld();
    window = new Window(1920, 1080, "CowEngine");
    camera = new Camera(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    scene = new Scene();
    bool sceneLoaded = false;
#ifdef __EMSCRIPTEN__
    // Restore script/model files from localStorage into the in-memory FS so that
    // any subsequent script/mesh loads pick up the latest edits or, for game
    // builds, the assets baked into the HTML by GameBuilder.
    Scene::restoreAssetsFromLocalStorage();

    // For game builds: kGameHtmlTemplate writes the exported scene to localStorage
    // ('cowengine_save') synchronously before CowEngine.js loads, so we find it here.
    // For editor builds: this restores the scene from the last editor save.
    EM_ASM({
        var data = localStorage.getItem('cowengine_save');
        if (data)
        {
            Module.ccall('app_set_has_local_storage_data', null, ['number'], [1]);
            Module.ccall('app_set_saved_data', null, ['string'], [data]);
        }
    });
    if (hasLocalStorageData)
        sceneLoaded = scene->loadFromString(pendingLocalStorageData);
#endif
    if (!sceneLoaded && !scene->loadFromJSON("scenes/scene.json"))
        scene->populateDefault();
    scene->addRigidBodiesToWorld(*physics);

    scene->addPlayer(std::make_unique<Player>(camera, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 3.0f, 10.0f))), window, *physics);
    if (scene->getPlayer())
        scene->getPlayer()->setScriptPath("scripts/shoot_cow.cow");

    // Ensure camera is positioned to match the player's initial transform on web builds
    camera->setPosition(glm::vec3(0.0f, 3.0f, 10.0f));

    shader = new Shader("./shaders/vertex.glsl", "./shaders/fragment.glsl");

#ifdef COWENGINE_GAME
    // Game-only build: no editor UI. ImGuiLayer is still constructed because
    // on the web build Window::isKeyPressed reads ImGui's key state. Renders
    // no widgets in game mode, so its presence is effectively free.
    window->setCursorDisabled(true);
    imguiLayer = new ImGuiLayer(window);

    scriptHost = new ScriptHost();
    scriptHost->setContext(scene, window);
    scriptTime = 0.0;
    scene->loadScripts(*scriptHost);
    scriptHost->setTime(0.0);
    scriptHost->setDelta(0.0);
    scene->startScripts(*scriptHost);
#else
    imguiLayer = new ImGuiLayer(window);
    editorUI = new EditorUI();
    editorUI->setCamera(camera);
    editorInput = new InputHandler(camera);

    scriptHost = new ScriptHost();
    scriptHost->setContext(scene, window);
    scriptHost->setLogger([this](const std::string &line)
                          {
        if (editorUI)
            editorUI->addLog("[cow] " + line, ImVec4(0.7f, 0.95f, 0.7f, 1.0f)); });
    editorUI->setScriptHost(scriptHost);
#endif

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
#ifdef COWENGINE_GAME
    {
        double current = getTimeSeconds();
        float delta = static_cast<float>(current - lastFrame);
        lastFrame = current;

        int width = 0, height = 0;
#ifdef __EMSCRIPTEN__
        emscripten_get_canvas_element_size("canvas", &width, &height);
#else
        glfwGetFramebufferSize(window->getWindow(), &width, &height);
        if (window->isKeyPressed(GLFW_KEY_ESCAPE))
            window->close();
#endif
        if (width < 1)
            width = 1;
        if (height < 1)
            height = 1;

        physics->stepSimulation(delta, 10);
        scriptTime += delta;
        scriptHost->setTime(scriptTime);
        scriptHost->setDelta(delta);
        scene->updateScripts(*scriptHost, delta);

        if (scene->getPlayer())
            scene->getPlayer()->processInput(window, delta, physics);

        // Maintain ImGui frame lifecycle (web needs ImGui IO updated for key polling)
        imguiLayer->newFrame();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClearColor(0.06f, 0.06f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader->use();
        glm::mat4 view = glm::lookAt(camera->getPosition(), camera->getPosition() + camera->getFront(), camera->getUp());
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 10000.0f);
        shader->setViewMatrix(view);
        shader->setProjectionMatrix(projection);

        scene->update();
        scene->render(*window, *shader);

        imguiLayer->render();
        window->update();
        return;
    }
#endif

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

    bool testingMode = editorUI && editorUI->isTestingMode();
    if (testingMode != lastTestingMode)
    {
        if (testingMode)
        {
            reloadScripts();
        }
        else
        {
            editorUI->setSelection(nullptr);
            scene->setSelectedObject(nullptr);
            scene->forceReload();
            scene->resetScripts();
            // Pointer-lock during testing can cause key-up events to be missed, leaving
            // ImGui's key state stale. Clear it so editor shortcuts work immediately.
            ImGui::GetIO().ClearInputKeys();
        }

        lastTestingMode = testingMode;
    }
    if (testingMode)
    {
        // Edge detect reload (R)
        bool r = window->isKeyPressed(GLFW_KEY_R);
        if (r && !lastRPressed)
        {
            editorUI->setSelection(nullptr);
            scene->setSelectedObject(nullptr);
            scene->forceReload();
            reloadScripts();
        }
        lastRPressed = r;

        editorUI->setRequestedTab(EditorUI::WorkspaceTab::SceneTab);
        physics->stepSimulation(delta, 10);
        scriptTime += delta;
        scriptHost->setTime(scriptTime);
        scriptHost->setDelta(delta);
        scene->updateScripts(*scriptHost, delta);
    }

    ImGuiIO &io = ImGui::GetIO();
    bool uiCapturing = io.WantCaptureMouse || io.WantCaptureKeyboard;
    bool allowGameInput = editorUI && editorUI->isGameViewInputEnabled();
    bool heiarchyInput = editorUI && editorUI->isHeiarchyInputEnabled();
    if (testingMode && scene->getPlayer() && (!uiCapturing || allowGameInput))
        scene->getPlayer()
            ->processInput(window, delta, physics);
    // Keep firstMouse reset while cursor is free so any new right-click starts clean
    if (!window->isCursorDisabled())
    {
        if (editorInput)
            editorInput->resetFirstMouse();
        if (!testingMode && scene->getPlayer())
            scene->getPlayer()->resetInputState();
    }

    if (!testingMode && editorInput)
    {
        if (allowGameInput || heiarchyInput)
        {
            // Focus camera on selected object when F is pressed
            if (window->isKeyPressed(GLFW_KEY_F) && scene->getSelectedObject())
            {
                glm::vec3 targetPos = scene->getSelectedObject()->getModel()[3];
                glm::vec3 camDir = glm::normalize(camera->getPosition() - targetPos);
                camera->setPosition(targetPos + camDir * 5.0f);
            }
        }
        if (allowGameInput)
        {
            editorInput->setMovementSpeed(editorUI->getCameraSpeed());
            editorInput->processInput(window, delta);
            checkSelection();

            // process mouse when in editor mode and game view is focused, only if cursor is disabled (e.g. on web)
            if (editorUI && editorUI->isGameViewInputEnabled() && window->isCursorDisabled())
            {
#ifdef __EMSCRIPTEN__
                EmscriptenMouseEvent mouseState;
                emscripten_get_mouse_status(&mouseState);
                editorInput->processMouseDelta(static_cast<float>(mouseState.movementX), static_cast<float>(-mouseState.movementY));
#else
                double mouseX, mouseY;
                glfwGetCursorPos(window->getWindow(), &mouseX, &mouseY);
                editorInput->processMouse(window->getWindow(), mouseX, mouseY);
#endif
            }
        }
    }
    // Resize viewport / compute aspect
    int width = 0, height = 0;
#ifdef __EMSCRIPTEN__
    emscripten_get_canvas_element_size("canvas", &width, &height);
#else
    glfwGetFramebufferSize(window->getWindow(), &width, &height);
#endif
    imguiLayer->newFrame();
    float fps = fpsTimer > 0.0 ? static_cast<float>(fpsCount / fpsTimer) : 0.0f;
    if (editorUI)
        editorUI->render(scene, window, physics, delta, fps);

    float vpX = 0.0f, vpY = 0.0f, vpW = static_cast<float>(width), vpH = static_cast<float>(height);
    float scaleX = 1.0f, scaleY = 1.0f;
    if (editorUI && editorUI->getGameViewport(vpX, vpY, vpW, vpH, scaleX, scaleY))
    {
        vpW *= scaleX;
        vpH *= scaleY;
    }

    int targetW = std::max(1, static_cast<int>(vpW));
    int targetH = std::max(1, static_cast<int>(vpH));
    if (targetW < 2 || targetH < 2)
    {
        if (gameFbWidth > 1 && gameFbHeight > 1)
        {
            targetW = gameFbWidth;
            targetH = gameFbHeight;
        }
        else
        {
            targetW = 2;
            targetH = 2;
        }
    }
    if (gameFbo == 0 || targetW != gameFbWidth || targetH != gameFbHeight)
    {
        if (gameDepth)
            glDeleteRenderbuffers(1, &gameDepth);
        if (gameColor)
            glDeleteTextures(1, &gameColor);
        if (gameFbo)
            glDeleteFramebuffers(1, &gameFbo);

        glGenFramebuffers(1, &gameFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, gameFbo);

        glGenTextures(1, &gameColor);
        glBindTexture(GL_TEXTURE_2D, gameColor);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, targetW, targetH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gameColor, 0);

        glGenRenderbuffers(1, &gameDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, gameDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, targetW, targetH);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gameDepth);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        gameFbWidth = targetW;
        gameFbHeight = targetH;
    }

    if (editorUI && gameColor)
        editorUI->setGameTexture(static_cast<ImTextureID>(static_cast<uintptr_t>(gameColor)),
                                 static_cast<float>(gameFbWidth), static_cast<float>(gameFbHeight));

    glBindFramebuffer(GL_FRAMEBUFFER, gameFbo);
    glViewport(0, 0, gameFbWidth, gameFbHeight);
    glClearColor(0.08f, 0.08f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader->use();

    glm::mat4 view = glm::lookAt(camera->getPosition(), camera->getPosition() + camera->getFront(), camera->getUp());
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)gameFbWidth / (float)gameFbHeight, 0.1f, 10000.0f);

    shader->setViewMatrix(view);
    shader->setProjectionMatrix(projection);

    scene->update();
    scene->render(*window, *shader);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(0.06f, 0.06f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    imguiLayer->render();

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

void Application::checkSelection()
{
    // Send raycast to scene to select objects in the editor when hovering in the game view and left-clicking
    if (editorUI && editorUI->isGameViewInputEnabled() && !window->isCursorDisabled())
    {
        ImVec2 mousePos = ImGui::GetMousePos();

        float gameViewportX, gameViewportY, gameViewportW, gameViewportH, scaleX, scaleY;
        if (editorUI->getGameViewport(gameViewportX, gameViewportY, gameViewportW, gameViewportH, scaleX, scaleY))
        {
            float mouseX = mousePos.x - gameViewportX;
            float mouseY = mousePos.y - gameViewportY;
            // Scale mouse correctly
            float scaledW = gameViewportW * scaleX;
            float scaledH = gameViewportH * scaleY;
            float scaledMouseX = mouseX * scaleX;
            float scaledMouseY = mouseY * scaleY;
            if (mouseX >= 0 && mouseY >= 0 && mouseX <= gameViewportW && mouseY <= gameViewportH)
            {
                // Convert mouse coordinates to normalized device coordinates (-1 to 1)
                float ndcX = (mouseX / gameViewportW) * 2.0f - 1.0f;
                float ndcY = 1.0f - (mouseY / gameViewportH) * 2.0f; // Invert Y for NDC

                // Get camera matrices
                glm::mat4 view = glm::lookAt(camera->getPosition(), camera->getPosition() + camera->getFront(), camera->getUp());
                glm::mat4 projection = glm::perspective(glm::radians(45.0f), scaledW / scaledH, 0.1f, 10000.0f);
                // Unproject screen coordinates to world ray
                glm::vec4 rayClip(ndcX, ndcY, -1.0f, 1.0f);
                glm::vec4 rayEye = glm::inverse(projection) * rayClip;
                rayEye.z = -1.0f; // Forward direction in eye space
                rayEye.w = 0.0f;  // Direction vector
                glm::vec3 rayWorld = glm::normalize(glm::vec3(glm::inverse(view) * rayEye));

                // Raycast into the scene
                Object *hitObject = scene->raycast(camera->getPosition(), rayWorld, 10000.0f);
                if (hitObject)
                {
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        if (hitObject == scene->getSelectedObject())
                        {
                            // Only deselect if the mouse click wasn't over the gizmo handles
                            if (!editorUI->isMouseOverGizmo())
                            {
                                scene->setSelectedObject(nullptr);
                                editorUI->setSelection(nullptr);
                            }
                        }
                        else
                        {
                            scene->setSelectedObject(hitObject);
                            editorUI->setSelection(hitObject);
                        }
                    }
                    std::string typeName(hitObject->getTypeName());
                }
                else
                {
                    // Only deselect if the mouse click wasn't over the gizmo handles
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        if (!editorUI->isMouseOverGizmo())
                        {
                            scene->setSelectedObject(nullptr);
                            editorUI->setSelection(nullptr);
                        }
                    }
                }
            }
        }
    }
}

void Application::reloadScripts()
{
    scriptTime = 0.0;
    scene->resetScripts();
    scene->loadScripts(*scriptHost);
    scriptHost->setTime(0.0);
    scriptHost->setDelta(0.0);
    scene->startScripts(*scriptHost);
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
#ifdef __EMSCRIPTEN__
extern "C" EMSCRIPTEN_KEEPALIVE void app_set_has_local_storage_data(int hasData)
{
    if (g_app)
        g_app->setHasLocalStorageData(hasData != 0);
}
#endif
#ifdef __EMSCRIPTEN__
extern "C" EMSCRIPTEN_KEEPALIVE void app_set_saved_data(const char *data)
{
    if (g_app)
        g_app->setPendingLocalStorageData(std::string(data));
}
#endif
#endif