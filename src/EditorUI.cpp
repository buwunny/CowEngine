#define IMGUI_DEFINE_MATH_OPERATORS
#ifndef IMGUI_ENABLE_DOCKING
#define IMGUI_ENABLE_DOCKING
#endif
#include "EditorUI.hpp"

#include "Camera.hpp"
#include "Scene.hpp"
#include "Window.hpp"
#include "PhysicsWorld.hpp"
#include "objects/Object.hpp"
#include "objects/Player.hpp"
#include "objects/StaticObject.hpp"
#include "meshes/AssetManager.hpp"
#include "../cow_mesh.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui_internal.h>
#include <ImGuizmo.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif
#include <imgui.h>
#include <vector>
#include <cmath>
#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb_image.h"

EditorUI::EditorUI()
{
    addLog("Editor UI ready. Type 'help' for commands.");
}

void EditorUI::render(Scene *scene, Window *window, PhysicsWorld *physics, float deltaSeconds, float fps)
{
    if (!showUI)
        return;

    ImGuizmo::BeginFrame();
    windowRef = window;
    physicsRef = physics;

    ImGuiIO &io = ImGui::GetIO();
    framebufferScaleX = io.DisplayFramebufferScale.x;
    framebufferScaleY = io.DisplayFramebufferScale.y;
    hasGameViewport = false;

    // Tool shortcuts — run every editor frame, not gated on selection or game viewport
    if (!testingMode && !io.WantTextInput)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_1))
            gizmoOp = GizmoOp::Translate;
        if (ImGui::IsKeyPressed(ImGuiKey_2))
            gizmoOp = GizmoOp::Rotate;
        if (ImGui::IsKeyPressed(ImGuiKey_3))
            gizmoOp = GizmoOp::Scale;
    }

    drawDockspace();
    drawMainMenu();

    if (testingMode)
    {
        drawTestingOverlay();
        showGameView = true;
    }

    if (showGameView)
        drawGameView(scene);

    if (showGameView && !testingMode)
        drawGizmoToolbar();

    if (!testingMode)
    {
        if (showHierarchy)
            drawSceneHierarchy(scene);
        if (showInspector)
            drawInspector(scene);
        if (showStats)
            drawStats(scene, deltaSeconds, fps);
        if (showConsole)
            drawConsole(scene);
        if (showRuntime)
            drawRuntime(scene);
    }
}

void EditorUI::addLog(const std::string &text, const ImVec4 &color)
{
    consoleLines.push_back({text, color});
    scrollToBottom = true;
}

void EditorUI::drawMainMenu()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Mode"))
        {
            if (!testingMode && ImGui::MenuItem("Start Testing"))
                testingMode = true;
            if (testingMode && ImGui::MenuItem("Stop Testing"))
                testingMode = false;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Windows"))
        {
            ImGui::MenuItem("Game View", nullptr, &showGameView);
            ImGui::MenuItem("Scene Hierarchy", nullptr, &showHierarchy);
            ImGui::MenuItem("Inspector", nullptr, &showInspector);
            ImGui::MenuItem("Debug Console", nullptr, &showConsole);
            ImGui::MenuItem("Stats", nullptr, &showStats);
            ImGui::MenuItem("Runtime", nullptr, &showRuntime);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void EditorUI::drawDockspace()
{
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    float menuBarHeight = ImGui::GetFrameHeight();
    ImVec2 dockPos(viewport->Pos.x, viewport->Pos.y + menuBarHeight);
    ImVec2 dockSize(viewport->Size.x, viewport->Size.y - menuBarHeight);
    ImGui::SetNextWindowPos(dockPos);
    ImGui::SetNextWindowSize(dockSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                             ImGuiWindowFlags_NoBackground;

    ImGui::Begin("DockSpaceHost", nullptr, flags);
    ImGuiID dockspaceId = ImGui::GetID("DockSpace");
    ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockFlags);

    if (!dockLayoutBuilt)
    {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_None);
        ImGui::DockBuilderSetNodeSize(dockspaceId, dockSize);

        ImGuiID dockMain = dockspaceId;
        ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.22f, nullptr, &dockMain);
        ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.26f, nullptr, &dockMain);
        ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.28f, nullptr, &dockMain);

        ImGuiID dockRightBottom = ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Down, 0.45f, nullptr, &dockRight);

        ImGui::DockBuilderDockWindow("Game View", dockMain);
        ImGui::DockBuilderDockWindow("Scene Hierarchy", dockLeft);
        ImGui::DockBuilderDockWindow("Inspector", dockRight);
        ImGui::DockBuilderDockWindow("Runtime", dockRightBottom);
        ImGui::DockBuilderDockWindow("Debug Console", dockBottom);
        ImGui::DockBuilderDockWindow("Stats", dockBottom);

        ImGui::DockBuilderFinish(dockspaceId);
        dockLayoutBuilt = true;
    }
    ImGui::End();
}

void EditorUI::drawGizmoToolbar()
{
    if (!hasGameViewport)
        return;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoScrollbar;

    ImGui::SetNextWindowPos(ImVec2(gameViewportX + 8.0f, gameViewportY + 8.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.78f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0.0f, 0.0f));

    if (ImGui::Begin("##GizmoToolbar", nullptr, flags))
    {
        const ImVec2 btnSize(26.0f, 26.0f);
        // Accent color for the active tool's background (shows in frame padding + transparent icon areas)
        const ImVec4 activeBg(0.30f, 0.45f, 0.72f, 1.0f);
        const ImVec4 inactiveBg(0.0f, 0.0f, 0.0f, 0.0f);
        const ImVec4 tintOn(1.0f, 1.0f, 1.0f, 1.0f);
        const ImVec4 tintOff(0.60f, 0.60f, 0.60f, 0.75f);
        // Icon textures (lazy-loaded)
        static bool iconsLoaded = false;
        static GLuint texTranslate = 0, texRotate = 0, texScale = 0, texWorld = 0, texLocal = 0;

        auto generateIconPixels = [](int size, unsigned char r, unsigned char g, unsigned char b)
        {
            std::vector<unsigned char> pixels(size * size * 4);
            int cx = size / 2;
            int cy = size / 2;
            float radius = size * 0.42f;
            for (int y = 0; y < size; ++y)
            {
                for (int x = 0; x < size; ++x)
                {
                    int idx = (y * size + x) * 4;
                    float dx = x - cx + 0.5f;
                    float dy = y - cy + 0.5f;
                    float d = std::sqrt(dx * dx + dy * dy);
                    if (d <= radius)
                    {
                        pixels[idx + 0] = r;
                        pixels[idx + 1] = g;
                        pixels[idx + 2] = b;
                        pixels[idx + 3] = 0xFF;
                    }
                    else
                    {
                        pixels[idx + 0] = 0x00;
                        pixels[idx + 1] = 0x00;
                        pixels[idx + 2] = 0x00;
                        pixels[idx + 3] = 0x00;
                    }
                }
            }
            return pixels;
        };

        auto loadPNGTexture = [&](const std::string &path) -> GLuint
        {
            int w = 0, h = 0, comp = 0;
            unsigned char *data = stbi_load(path.c_str(), &w, &h, &comp, 4);
            if (!data)
                return 0;
            GLuint tex = 0;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);
            stbi_image_free(data);
            return tex;
        };

        auto ensureIcons = [&]()
        {
            if (iconsLoaded)
                return;
            const int ICON_SIZE = 32;
            std::string base = std::string("icons/png/");
            GLuint t1 = loadPNGTexture(base + "up-down-left-right-solid-full.png");
            GLuint t2 = loadPNGTexture(base + "rotate-solid-full.png");
            GLuint t3 = loadPNGTexture(base + "expand-solid-full.png");
            GLuint t4 = loadPNGTexture(base + "globe-solid-full.png");
            GLuint t5 = loadPNGTexture(base + "location-dot-solid-full.png");

            auto createTex = [&](const std::vector<unsigned char> &px)
            {
                GLuint tex = 0;
                glGenTextures(1, &tex);
                glBindTexture(GL_TEXTURE_2D, tex);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ICON_SIZE, ICON_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glBindTexture(GL_TEXTURE_2D, 0);
                return tex;
            };

            if (t1 && t2 && t3 && t4 && t5)
            {
                texTranslate = t1;
                texRotate = t2;
                texScale = t3;
                texWorld = t4;
                texLocal = t5;
            }
            else
            {
                // Fallback to generated icons if PNG loading fails
                texTranslate = createTex(generateIconPixels(ICON_SIZE, 0x33, 0x99, 0xFF)); // blue
                texRotate = createTex(generateIconPixels(ICON_SIZE, 0x33, 0xCC, 0x66));    // green
                texScale = createTex(generateIconPixels(ICON_SIZE, 0xFF, 0x99, 0x33));     // orange
                texWorld = createTex(generateIconPixels(ICON_SIZE, 0x66, 0xCC, 0xFF));     // light blue
                texLocal = createTex(generateIconPixels(ICON_SIZE, 0xCC, 0xCC, 0x66));     // yellow
            }

            iconsLoaded = true;
        };

        ensureIcons();

        // Frame padding makes the accent bg visible around the icon image
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.0f, 3.0f));

        auto toolBtnTex = [&](const char *id, GLuint tex, const char *tooltip, GizmoOp op)
        {
            bool active = (gizmoOp == op);
            if (ImGui::ImageButton(id, (ImTextureID)(uintptr_t)tex, btnSize,
                                   ImVec2(0, 0), ImVec2(1, 1),
                                   active ? activeBg : inactiveBg,
                                   active ? tintOn : tintOff))
                gizmoOp = op;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", tooltip);
        };

        toolBtnTex("##gizmo_translate", texTranslate, "Translate (1)", GizmoOp::Translate);
        ImGui::SameLine();
        toolBtnTex("##gizmo_rotate", texRotate, "Rotate (2)", GizmoOp::Rotate);
        ImGui::SameLine();
        toolBtnTex("##gizmo_scale", texScale, "Scale (3)", GizmoOp::Scale);

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        auto modeBtnTex = [&](const char *id, GLuint tex, const char *tooltip, bool local)
        {
            bool active = (gizmoLocal == local);
            if (ImGui::ImageButton(id, (ImTextureID)(uintptr_t)tex, btnSize,
                                   ImVec2(0, 0), ImVec2(1, 1),
                                   active ? activeBg : inactiveBg,
                                   active ? tintOn : tintOff))
                gizmoLocal = local;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", tooltip);
        };
        modeBtnTex("##gizmo_world", texWorld, "World space", false);
        ImGui::SameLine();
        modeBtnTex("##gizmo_local", texLocal, "Local space", true);

        ImGui::PopStyleVar(); // FramePadding
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
}

void EditorUI::drawGameView(Scene *scene)
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar;
    ImGui::SetNextWindowSize(ImVec2(900.0f, 600.0f), ImGuiCond_FirstUseEver);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.08f, 0.05f));
    ImGui::Begin("Game View", &showGameView, flags);

    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
    ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
    ImVec2 contentSize = ImVec2(contentMax.x - contentMin.x, contentMax.y - contentMin.y);

    gameViewportX = windowPos.x + contentMin.x;
    gameViewportY = windowPos.y + contentMin.y;
    gameViewportW = contentSize.x;
    gameViewportH = contentSize.y;
    hasGameViewport = (gameViewportW > 1.0f && gameViewportH > 1.0f);

    gameViewInput = (ImGui::IsWindowHovered(ImGuiFocusedFlags_RootAndChildWindows) || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) && hasGameViewport;

    if (gameTextureId && hasGameViewport)
    {
        ImGui::Image(gameTextureId, contentSize, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
    }
    else
    {
        ImGui::TextUnformatted("Game View (render target not ready)");
    }

    // Gizmo overlay — only in editor mode with a selection and a camera
    if (!testingMode && selection.object && cameraRef && hasGameViewport)
    {
        if (!selection.hasCache)
            refreshSelectionCache();

        glm::mat4 view = glm::lookAt(cameraRef->getPosition(),
                                     cameraRef->getPosition() + cameraRef->getFront(),
                                     cameraRef->getUp());
        glm::mat4 proj = glm::perspective(glm::radians(45.0f),
                                          gameViewportW / gameViewportH, 0.1f, 1000.0f);

        float viewF[16], projF[16], modelF[16];
        memcpy(viewF, glm::value_ptr(view), sizeof(viewF));
        memcpy(projF, glm::value_ptr(proj), sizeof(projF));

        // Rebuild model matrix from cached euler angles so gizmo reflects inspector values
        glm::mat4 modelMat = glm::translate(glm::mat4(1.0f), selection.position);
        modelMat = glm::rotate(modelMat, glm::radians(selection.rotation.y), glm::vec3(0, 1, 0));
        modelMat = glm::rotate(modelMat, glm::radians(selection.rotation.x), glm::vec3(1, 0, 0));
        modelMat = glm::rotate(modelMat, glm::radians(selection.rotation.z), glm::vec3(0, 0, 1));
        modelMat = glm::scale(modelMat, selection.scale);
        memcpy(modelF, glm::value_ptr(modelMat), sizeof(modelF));

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetRect(gameViewportX, gameViewportY, gameViewportW, gameViewportH);

        ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
        if (gizmoOp == GizmoOp::Rotate)
            op = ImGuizmo::ROTATE;
        else if (gizmoOp == GizmoOp::Scale)
            op = ImGuizmo::SCALE;
        ImGuizmo::MODE mode = gizmoLocal ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

        if (ImGuizmo::Manipulate(viewF, projF, op, mode, modelF))
        {
            float pos[3], rot[3], sc[3];
            ImGuizmo::DecomposeMatrixToComponents(modelF, pos, rot, sc);
            selection.position = glm::vec3(pos[0], pos[1], pos[2]);
            selection.rotation = glm::vec3(rot[0], rot[1], rot[2]);
            selection.scale = glm::vec3(sc[0], sc[1], sc[2]);
            applySelectionTransform();
        }
    }

    if (gameViewInput)
    {
        if (!testingMode)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
            {
                if (windowRef)
                    windowRef->setCursorDisabled(true);
            }
            else
            {
                if (windowRef)
                    windowRef->setCursorDisabled(false);
            }
        }
        else
        {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                if (windowRef)
                    windowRef->setCursorDisabled(true);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                if (windowRef)
                    windowRef->setCursorDisabled(false);
            }
        }
    }
    else
    {
        if (windowRef)
            windowRef->setCursorDisabled(false);
    }

    ImGui::End();
    ImGui::PopStyleColor();
}

void EditorUI::drawTestingOverlay()
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    flags |= ImGuiWindowFlags_NoDocking;
    ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + 20.0f, vp->Pos.y + 20.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::Begin("TestingOverlay", nullptr, flags);
    ImGui::TextUnformatted("Testing Mode");
    if (ImGui::Button("Stop Testing"))
        testingMode = false;
    ImGui::End();
}

void EditorUI::drawSceneHierarchy(Scene *scene)
{
    ImGui::Begin("Scene Hierarchy", &showHierarchy);

    if (!scene)
    {
        ImGui::TextUnformatted("No scene loaded.");
        ImGui::End();
        return;
    }

    const std::string &path = scene->getScenePath();
    ImGui::Text("Scene: %s", path.empty() ? "<unsaved>" : path.c_str());
    if (ImGui::Button("Reload"))
    {
        setSelection(nullptr);
        scene->setSelectedObject(nullptr);
        scene->forceReload();
        addLog("Scene reload requested.", ImVec4(0.9f, 0.8f, 0.4f, 1.0f));
    }
    ImGui::SameLine();
    if (ImGui::Button("Save"))
    {
        std::string savePath = path.empty() ? "scenes/scene.json" : path;
        if (scene->saveToJSON(savePath))
        {
            lastSavePath = savePath;
            addLog("Scene saved to " + savePath, ImVec4(0.6f, 0.9f, 0.6f, 1.0f));
        }
        else
        {
            addLog("Scene save failed for " + savePath, ImVec4(0.9f, 0.5f, 0.5f, 1.0f));
        }
    }

    hierarchyFilter.Draw("Filter");
    ImGui::Separator();

    if (scene->getPlayer())
    {
        Object *player = scene->getPlayer();
        std::string label = player->getName() + " [" + player->getTypeName() + "]";
        if (hierarchyFilter.PassFilter(label.c_str()))
        {
            bool selected = selection.object == player;
            if (ImGui::Selectable(label.c_str(), selected))
            {
                setSelection(player);
            }
        }
    }

    const size_t count = scene->getObjectCount();
    for (size_t i = 0; i < count; ++i)
    {
        Object *obj = scene->getObjectByIndex(i);
        if (!obj)
            continue;
        std::string label = obj->getName() + " [" + obj->getTypeName() + "]";
        if (!hierarchyFilter.PassFilter(label.c_str()))
            continue;
        bool selected = selection.object == obj;
        if (ImGui::Selectable(label.c_str(), selected))
        {
            setSelection(obj);
        }
    }

    ImGui::End();
}

void EditorUI::drawInspector(Scene *scene)
{
    ImGui::Begin("Inspector", &showInspector);

    if (!scene)
    {
        ImGui::TextUnformatted("No scene loaded.");
        ImGui::End();
        return;
    }

    if (!selection.object)
    {
        ImGui::TextUnformatted("Select an object to inspect.");
        ImGui::End();
        return;
    }

    if (!selection.hasCache)
        refreshSelectionCache();

    ImGui::Text("Type: %s", selection.object->getTypeName());

    std::string name = selection.object->getName();
    char nameBuffer[256] = {};
    std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", name.c_str());
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
    {
        selection.object->setName(std::string(nameBuffer));
    }

    if (ImGui::DragFloat3("Position", &selection.position.x, 0.1f))
        applySelectionTransform();
    if (ImGui::DragFloat3("Rotation", &selection.rotation.x, 0.5f))
        applySelectionTransform();
    if (ImGui::DragFloat3("Scale", &selection.scale.x, 0.05f, 0.001f, 1000.0f))
        applySelectionTransform();

    if (ImGui::ColorEdit4("Color", &selection.color.r))
        applySelectionColor();

    if (selection.object->getRigidBody())
    {
        btRigidBody *rb = selection.object->getRigidBody();
        float mass = 0.0f;
        if (rb->getInvMass() > 0.0f)
            mass = 1.0f / rb->getInvMass();
        btVector3 v = rb->getLinearVelocity();
        ImGui::Separator();
        if (ImGui::InputFloat("Mass", &mass, 0.1f, 1.0f))
        {
            selection.object->setMass(mass);
        }
        ImGui::Text("Velocity: %.2f %.2f %.2f", v.getX(), v.getY(), v.getZ());
    }

    ImGui::Separator();
    if (ImGui::Button("Delete"))
    {
        if (selection.object == scene->getPlayer())
        {
            scene->removePlayer();
        }
        else
        {
            // Find and remove the object from the scene
            scene->deleteObject(selection.object);
        }
        setSelection(nullptr);
    }

    ImGui::End();
}

void EditorUI::drawStats(Scene *scene, float deltaSeconds, float fps)
{
    ImGui::Begin("Stats", &showStats);
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Frame: %.2f ms", deltaSeconds * 1000.0f);
    if (scene)
    {
        ImGui::Text("Objects: %zu", scene->getObjectCount());
        ImGui::Text("Has Player: %s", scene->getPlayer() ? "Yes" : "No");
    }
    ImGui::End();
}

void EditorUI::drawConsole(Scene *scene)
{
    ImGui::Begin("Debug Console", &showConsole);

    if (ImGui::Button("Clear"))
        consoleLines.clear();
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoScroll);

    ImGui::Separator();
    ImGui::BeginChild("ConsoleScroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false);

    for (const auto &line : consoleLines)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, line.color);
        ImGui::TextUnformatted(line.text.c_str());
        ImGui::PopStyleColor();
    }

    if (scrollToBottom || (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
        ImGui::SetScrollHereY(1.0f);
    scrollToBottom = false;

    ImGui::EndChild();

    bool reclaimFocus = false;
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory;
    if (ImGui::InputText("Command", consoleInput, sizeof(consoleInput), flags, &EditorUI::consoleTextEditCallback, this))
    {
        std::string commandLine(consoleInput);
        if (!commandLine.empty())
            execCommand(commandLine, scene);
        std::fill(std::begin(consoleInput), std::end(consoleInput), 0);
        reclaimFocus = true;
    }

    ImGui::SetItemDefaultFocus();
    if (reclaimFocus)
        ImGui::SetKeyboardFocusHere(-1);

    ImGui::End();
}

void EditorUI::drawRuntime(Scene *scene)
{
    ImGui::Begin("Runtime", &showRuntime);

    if (!scene)
    {
        ImGui::TextUnformatted("No scene loaded.");
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("Mode");
    if (!testingMode)
    {
        if (ImGui::Button("Start Testing"))
        {
            std::string savePath = "scenes/scene.json";
            if (scene->saveToJSON(savePath))
            {
                lastSavePath = savePath;
                addLog("Scene saved to " + savePath, ImVec4(0.6f, 0.9f, 0.6f, 1.0f));
            }
            else
            {
                addLog("Scene save failed for " + savePath, ImVec4(0.9f, 0.5f, 0.5f, 1.0f));
            }
            testingMode = true;
        }
    }
    else
    {
        if (ImGui::Button("Stop Testing"))
            testingMode = false;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Tools");
    if (ImGui::Button("Spawn Cow"))
        addObjectToScene(scene, "cow");
    if (ImGui::Button("Spawn Cube"))
        addObjectToScene(scene, "cube");
    if (ImGui::Button("Spawn Plane"))
        addObjectToScene(scene, "plane");

    ImGui::Separator();
    ImGui::TextUnformatted("Input");
    if (ImGui::SliderFloat("Mouse Sensitivity", &mouseSensitivity, 0.1f, 10.0f))
    {
#if defined(__EMSCRIPTEN__)
        std::ostringstream ss;
        ss << "Module.mouseSensitivity = " << mouseSensitivity;
        emscripten_run_script(ss.str().c_str());
#endif
    }

    ImGui::End();
}

bool EditorUI::getGameViewport(float &x, float &y, float &w, float &h, float &scaleX, float &scaleY) const
{
    if (!hasGameViewport)
        return false;
    x = gameViewportX;
    y = gameViewportY;
    w = gameViewportW;
    h = gameViewportH;
    scaleX = framebufferScaleX;
    scaleY = framebufferScaleY;
    return true;
}

void EditorUI::setGameTexture(ImTextureID textureId, float width, float height)
{
    gameTextureId = textureId;
    gameTextureW = width;
    gameTextureH = height;
}

void EditorUI::setSelection(Object *object)
{
    if (selection.object == object)
        return;
    selection.object = object;
    selection.hasCache = false;
}

void EditorUI::refreshSelectionCache()
{
    if (!selection.object)
        return;
    selection.object->getTransform(selection.position, selection.rotation, selection.scale);
    selection.color = selection.object->getColor();
    selection.hasCache = true;
}

void EditorUI::applySelectionTransform()
{
    if (!selection.object)
        return;
    selection.object->setTransform(selection.position, selection.rotation, selection.scale);
    // After manually moving an object in the editor, Bullet's broadphase AABB cache is stale
    // (stepSimulation isn't called in editor mode). Update it so raycasts find the new position.
    if (physicsRef && selection.object->getRigidBody())
        physicsRef->updateSingleAabb(selection.object->getRigidBody());
}

void EditorUI::applySelectionColor()
{
    if (!selection.object)
        return;
    selection.object->setColor(selection.color);
}

bool EditorUI::isMouseOverGizmo() const
{
    return ImGuizmo::IsOver() || ImGuizmo::IsUsing();
}

void EditorUI::execCommand(const std::string &commandLine, Scene *scene)
{
    addLog("> " + commandLine, ImVec4(0.7f, 0.8f, 1.0f, 1.0f));

    consoleHistory.push_back(commandLine);
    historyPos = -1;

    if (!scene)
    {
        addLog("No scene loaded.", ImVec4(0.9f, 0.5f, 0.5f, 1.0f));
        return;
    }

    if (commandLine == "help")
    {
        addLog("Commands: help, clear, reload, save, save_as <path>, spawn_cow");
        return;
    }

    if (commandLine == "clear")
    {
        consoleLines.clear();
        return;
    }

    if (commandLine == "reload")
    {
        setSelection(nullptr);
        scene->setSelectedObject(nullptr);
        scene->forceReload();
        addLog("Scene reload requested.");
        return;
    }

    if (commandLine == "spawn_cow")
    {
        addObjectToScene(scene, "cow");
        return;
    }

    if (commandLine == "save")
    {
        std::string savePath = lastSavePath;
        if (savePath.empty())
            savePath = scene->getScenePath().empty() ? "scenes/scene.json" : scene->getScenePath();
        if (scene->saveToJSON(savePath))
        {
            lastSavePath = savePath;
            addLog("Scene saved to " + savePath, ImVec4(0.6f, 0.9f, 0.6f, 1.0f));
        }
        else
        {
            addLog("Scene save failed for " + savePath, ImVec4(0.9f, 0.5f, 0.5f, 1.0f));
        }
        return;
    }

    const std::string prefix = "save_as ";
    if (commandLine.rfind(prefix, 0) == 0)
    {
        std::string path = commandLine.substr(prefix.size());
        if (path.empty())
        {
            addLog("save_as requires a path.", ImVec4(0.9f, 0.5f, 0.5f, 1.0f));
            return;
        }
        if (scene->saveToJSON(path))
        {
            lastSavePath = path;
            addLog("Scene saved to " + path, ImVec4(0.6f, 0.9f, 0.6f, 1.0f));
        }
        else
        {
            addLog("Scene save failed for " + path, ImVec4(0.9f, 0.5f, 0.5f, 1.0f));
        }
        return;
    }

    addLog("Unknown command: " + commandLine, ImVec4(0.9f, 0.5f, 0.5f, 1.0f));
}

int EditorUI::consoleTextEditCallback(ImGuiInputTextCallbackData *data)
{
    EditorUI *ui = static_cast<EditorUI *>(data->UserData);
    if (!ui)
        return 0;

    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
    {
        const int prevHistoryPos = ui->historyPos;
        if (data->EventKey == ImGuiKey_UpArrow)
        {
            if (ui->historyPos == -1)
                ui->historyPos = static_cast<int>(ui->consoleHistory.size()) - 1;
            else if (ui->historyPos > 0)
                ui->historyPos--;
        }
        else if (data->EventKey == ImGuiKey_DownArrow)
        {
            if (ui->historyPos != -1)
            {
                if (++ui->historyPos >= static_cast<int>(ui->consoleHistory.size()))
                    ui->historyPos = -1;
            }
        }

        if (prevHistoryPos != ui->historyPos)
        {
            const char *historyStr = (ui->historyPos >= 0) ? ui->consoleHistory[ui->historyPos].c_str() : "";
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, historyStr);
        }
    }

    return 0;
}

void EditorUI::addObjectToScene(Scene *scene, const std::string &type)
{
    if (!scene)
        return;

    if (type == "cube")
    {
        scene->addObject(std::make_unique<Cube>(1, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 0.0f)), glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 1.0f));
    }
    else if (type == "plane")
    {
        scene->addObject(std::make_unique<Plane>(100, 100, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 0.0f)), glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 0.0f));
    }
    else if (type == "cow")
    {
        auto &assetManager = AssetManager::instance();
        auto cowMesh = assetManager.loadStaticMeshFromOBJ("models/cow.obj", "cow");
        if (cowMesh)
        {
            scene->addObject(std::make_unique<StaticObject>(cowMesh, cow_mesh_vertices, cow_mesh_vertex_count, cow_mesh_indices, cow_mesh_index_count, 3, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 0.0f)), glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 1.0f));
        }
    }
}
