#define IMGUI_DEFINE_MATH_OPERATORS
#ifndef IMGUI_ENABLE_DOCKING
#define IMGUI_ENABLE_DOCKING
#endif
#include "EditorUI.hpp"

#include "Camera.hpp"
#include "Scene.hpp"
#include "Window.hpp"
#include "PhysicsWorld.hpp"
#if !defined(COWENGINE_GAME)
#include "GameBuilder.hpp"
#endif
#include "ecs/Components.hpp"
#include "ecs/Factories.hpp"
#include "ecs/ComponentOps.hpp"
#include "meshes/AssetManager.hpp"
#include "meshes/StaticMesh.hpp"
#include "CodeEditor.hpp"
#include "ImGuiLayer.hpp"
#include "script/CowScript.hpp"
#include "script/ScriptHost.hpp"

#include <filesystem>
#include <functional>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <vector>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui_internal.h>
#include <ImGuizmo.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif
#include <imgui.h>
#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb/stb_image.h"
#include "../third_party/imgui_markdown/imgui_markdown.h" // https://github.com/enkisoftware/imgui_markdown

EditorUI::EditorUI()
{
    addLog("Editor UI ready. Type 'help' for commands.");
    codeEditor = std::make_unique<CodeEditor>();
    codeEditor->setLogger([this](const std::string &line)
                          { addLog(line, ImVec4(0.7f, 0.85f, 1.0f, 1.0f)); });
}

EditorUI::~EditorUI() = default;

void EditorUI::render(Scene *scene, Window *window, PhysicsWorld *physics, float deltaSeconds, float fps)
{
    if (!showUI)
        return;

    ImGuizmo::BeginFrame();
    windowRef = window;
    sceneRef = scene;
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
    drawMainMenu(scene);

    if (testingMode)
    {
        drawTestingOverlay();
        showGameView = true;
    }

    if (showGameView)
        drawWorkspace(scene);

    if (showGameView && !testingMode && activeTab == WorkspaceTab::SceneTab)
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
        if (showFiles)
            drawFileBrowser(scene);
    }
}

void EditorUI::addLog(const std::string &text, const ImVec4 &color)
{
    consoleLines.push_back({text, color});
    scrollToBottom = true;
}

void EditorUI::drawMainMenu(Scene *scene)
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
            ImGui::MenuItem("Workspace", nullptr, &showGameView);
            ImGui::MenuItem("Scene Hierarchy", nullptr, &showHierarchy);
            ImGui::MenuItem("Inspector", nullptr, &showInspector);
            ImGui::MenuItem("Debug Console", nullptr, &showConsole);
            ImGui::MenuItem("Stats", nullptr, &showStats);
            ImGui::MenuItem("Runtime", nullptr, &showRuntime);
            ImGui::MenuItem("Files", nullptr, &showFiles);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Scene Tab"))
                requestedTab = WorkspaceTab::SceneTab;
            if (ImGui::MenuItem("Code Tab"))
                requestedTab = WorkspaceTab::CodeTab;
            if (ImGui::MenuItem("Help Tab"))
                requestedTab = WorkspaceTab::HelpTab;
            ImGui::EndMenu();
        }
#if !defined(COWENGINE_GAME)
        if (ImGui::BeginMenu("Build"))
        {
            auto runBuild = [&](GameBuilder::Target target)
            {
                if (!scene)
                {
                    addLog("No active scene to build.", ImVec4(1.0f, 0.55f, 0.55f, 1.0f));
                    return;
                }
                GameBuilder::Result result = GameBuilder::build(
                    target,
                    scene,
                    [this](const std::string &line)
                    {
                        addLog(line, ImVec4(0.7f, 0.85f, 1.0f, 1.0f));
                    });
                if (result.ok)
                    addLog(result.message, ImVec4(0.6f, 0.9f, 0.6f, 1.0f));
                else
                    addLog(result.message, ImVec4(1.0f, 0.55f, 0.55f, 1.0f));
            };

            const GameBuilder::Target targets[] = {
                GameBuilder::Target::Linux,
                GameBuilder::Target::Windows,
                GameBuilder::Target::Web,
            };

            for (GameBuilder::Target target : targets)
            {
                bool available = GameBuilder::isTargetAvailable(target);
                if (ImGui::MenuItem(GameBuilder::targetLabel(target), nullptr, false, available))
                    runBuild(target);
            }
            ImGui::EndMenu();
        }
#endif
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

        ImGui::DockBuilderDockWindow("Workspace", dockMain);
        ImGui::DockBuilderDockWindow("Scene Hierarchy", dockLeft);
        ImGui::DockBuilderDockWindow("Inspector", dockRight);
        ImGui::DockBuilderDockWindow("Runtime", dockRightBottom);
        ImGui::DockBuilderDockWindow("Debug Console", dockBottom);
        ImGui::DockBuilderDockWindow("Stats", dockBottom);
        ImGui::DockBuilderDockWindow("Files", dockBottom);

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
            std::string base = std::string("engine_assets/icons/png/");
            GLuint t1 = loadPNGTexture(base + "up-down-left-right-solid-full.png");
            GLuint t2 = loadPNGTexture(base + "rotate-solid-full.png");
            GLuint t3 = loadPNGTexture(base + "expand-solid-full.png");
            // GLuint t4 = loadPNGTexture(base + "globe-solid-full.png");
            // GLuint t5 = loadPNGTexture(base + "location-dot-solid-full.png");

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

            if (t1 && t2 && t3) // && t4 && t5)
            {
                texTranslate = t1;
                texRotate = t2;
                texScale = t3;
                // texWorld = t4;
                // texLocal = t5;
            }
            else
            {
                // Fallback to generated icons if PNG loading fails
                texTranslate = createTex(generateIconPixels(ICON_SIZE, 0x33, 0x99, 0xFF)); // blue
                texRotate = createTex(generateIconPixels(ICON_SIZE, 0x33, 0xCC, 0x66));    // green
                texScale = createTex(generateIconPixels(ICON_SIZE, 0xFF, 0x99, 0x33));     // orange
                // texWorld = createTex(generateIconPixels(ICON_SIZE, 0x66, 0xCC, 0xFF));     // light blue
                // texLocal = createTex(generateIconPixels(ICON_SIZE, 0xCC, 0xCC, 0x66));     // yellow
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

        // ImGui::SameLine();
        // ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        // ImGui::SameLine();

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
        // modeBtnTex("##gizmo_world", texWorld, "World space", false);
        // ImGui::SameLine();
        // modeBtnTex("##gizmo_local", texLocal, "Local space", true);

        ImGui::PopStyleVar(); // FramePadding
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
}

void EditorUI::drawWorkspace(Scene *scene)
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar;
    ImGui::SetNextWindowSize(ImVec2(900.0f, 600.0f), ImGuiCond_FirstUseEver);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.08f, 0.05f));
    ImGui::Begin("Workspace", &showGameView, flags);

    if (ImGui::BeginTabBar("##WorkspaceTabs", ImGuiTabBarFlags_None))
    {
        ImGuiTabItemFlags sceneFlags = (requestedTab == WorkspaceTab::SceneTab) ? ImGuiTabItemFlags_SetSelected : 0;
        ImGuiTabItemFlags codeFlags = (requestedTab == WorkspaceTab::CodeTab) ? ImGuiTabItemFlags_SetSelected : 0;
        ImGuiTabItemFlags helpFlags = (requestedTab == WorkspaceTab::HelpTab) ? ImGuiTabItemFlags_SetSelected : 0;
        requestedTab = WorkspaceTab::None;

        if (ImGui::BeginTabItem("Scene", nullptr, sceneFlags))
        {
            activeTab = WorkspaceTab::SceneTab;
            showGameView = true;
            drawSceneTab(scene);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Code", nullptr, codeFlags))
        {
            activeTab = WorkspaceTab::CodeTab;
            // Focus the editor surface the first frame we enter the Code tab so
            // the user can immediately start typing without an extra click.
            if (lastDrawnWorkspaceTab != WorkspaceTab::CodeTab && codeEditor)
                codeEditor->requestEditorFocus();
            drawCodeTab(scene);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Help", nullptr, helpFlags))
        {
            activeTab = WorkspaceTab::HelpTab;
            drawHelpTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    lastDrawnWorkspaceTab = activeTab;

    ImGui::End();
    ImGui::PopStyleColor();
}

void EditorUI::drawSceneTab(Scene *scene)
{
    ImVec2 cursorScreen = ImGui::GetCursorScreenPos();
    ImVec2 contentSize = ImGui::GetContentRegionAvail();

    gameViewportX = cursorScreen.x;
    gameViewportY = cursorScreen.y;
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

    // Gizmo overlay only in editor mode with a selection and a camera
    if (!testingMode && selection.entity != ecs::NullEntity && cameraRef && hasGameViewport)
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

        // Rebuild model matrix using the same Rz*Ry*Rx convention as setTransform / ImGuizmo
        glm::mat4 model = glm::translate(glm::mat4(1.0f), selection.position);
        model = glm::rotate(model, glm::radians(selection.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::rotate(model, glm::radians(selection.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(selection.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::scale(model, selection.scale);
        memcpy(modelF, glm::value_ptr(model), sizeof(modelF));

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

            if (gizmoOp == GizmoOp::Rotate)
            {
                // DecomposeMatrixToComponents always returns Y in [-90°, 90°].
                // For Rz*Ry*Rx, every rotation has a complementary representation:
                //   (X, Y, Z)  ↔  (X+180°, 180°-Y, Z+180°)
                // Pick whichever is closer to the previously stored angles so the
                // sequence stays continuous and doesn't flip at the ±90° singularity.
                auto norm180 = [](float a) -> float
                {
                    while (a > 180.f)
                        a -= 360.f;
                    while (a < -180.f)
                        a += 360.f;
                    return a;
                };
                float cX = norm180(rot[0] + 180.f);
                float cY = (rot[1] >= 0.f ? 180.f : -180.f) - rot[1];
                float cZ = norm180(rot[2] + 180.f);
                float d1 = std::abs(norm180(rot[0] - selection.rotation.x)) + std::abs(norm180(rot[1] - selection.rotation.y)) + std::abs(norm180(rot[2] - selection.rotation.z));
                float d2 = std::abs(norm180(cX - selection.rotation.x)) + std::abs(norm180(cY - selection.rotation.y)) + std::abs(norm180(cZ - selection.rotation.z));
                if (d2 < d1)
                {
                    rot[0] = cX;
                    rot[1] = cY;
                    rot[2] = cZ;
                }
            }

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
}

void EditorUI::drawCodeTab(Scene *scene)
{
    (void)scene;
    if (!codeEditor)
        return;

    // Toolbar across the top of the code tab.
    if (ImGui::Button("New"))
    {
        ImGui::OpenPopup("##NewScript");
    }
    ImGui::SameLine();
    if (ImGui::Button("Open from selection"))
    {
        ecs::Identity *ident = (sceneRef && selection.entity != ecs::NullEntity)
                                   ? sceneRef->registry().try_get<ecs::Identity>(selection.entity)
                                   : nullptr;
        if (ident && !ident->scriptPath.empty())
        {
            codeEditor->openFile(ident->scriptPath);
        }
        else
        {
            addLog("Select an object with a script first (or attach one in the Inspector).",
                   ImVec4(0.9f, 0.7f, 0.4f, 1.0f));
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save"))
    {
        codeEditor->saveActive();
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply (recompile)"))
    {
        if (scene && scriptHostRef && codeEditor->hasActiveBuffer())
        {
            codeEditor->saveActive();
            scene->resetScripts();
            int n = scene->loadScripts(*scriptHostRef);
            addLog("Recompiled " + std::to_string(n) + " script(s).",
                   ImVec4(0.7f, 0.95f, 0.7f, 1.0f));
        }
    }
    ImGui::SameLine();
    if(ImGui::Button("Attach to selection"))
    {
        if (sceneRef && selection.entity != ecs::NullEntity)
        {
            if (codeEditor->hasActiveBuffer())
            {
                std::string path = codeEditor->activePath();
                auto &ident = sceneRef->registry().get<ecs::Identity>(selection.entity);
                ident.scriptPath = path;
                sceneRef->registry().remove<ecs::ScriptComponent>(selection.entity);
                addLog("Attached script to " + ident.name + ": " + path,
                       ImVec4(0.7f, 0.95f, 0.7f, 1.0f));
            }
            else
            {
                addLog("Open a script in the editor first to attach it.",
                       ImVec4(0.9f, 0.7f, 0.4f, 1.0f));
            }
        }
        else
        {
            addLog("Select an object in the Scene tab first to attach the script to.",
                   ImVec4(0.9f, 0.7f, 0.4f, 1.0f));
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Ctrl+S to save");

    if (ImGui::BeginPopup("##NewScript"))
    {
        ImGui::Text("Path for new .cow file:");
        ImGui::SetNextItemWidth(360.0f);
        ImGui::InputText("##newScriptPath", newScriptName, sizeof(newScriptName));
        if (ImGui::Button("Create"))
        {
            // Make sure the directory exists.
            try
            {
                std::filesystem::path p(newScriptName);
                if (p.has_parent_path())
                    std::filesystem::create_directories(p.parent_path());
            }
            catch (...)
            {
            }
            codeEditor->openFile(newScriptName);
            codeEditor->saveActive();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::Separator();
    codeEditor->render();
}

// ---------------------------------------------------------------------------
// Help tab helpers
// ---------------------------------------------------------------------------

namespace
{
    // Same palette as CodeEditor so highlighted blocks feel consistent.
    ImU32 helpTokenColor(cowscript::TokenKind k)
    {
        switch (k)
        {
        case cowscript::TokenKind::Comment:
            return IM_COL32(110, 145, 110, 255);
        case cowscript::TokenKind::Keyword:
            return IM_COL32(198, 120, 221, 255);
        case cowscript::TokenKind::Number:
            return IM_COL32(255, 175, 100, 255);
        case cowscript::TokenKind::String:
            return IM_COL32(152, 195, 121, 255);
        case cowscript::TokenKind::Builtin:
            return IM_COL32(86, 180, 233, 255);
        case cowscript::TokenKind::Operator:
            return IM_COL32(200, 200, 220, 255);
        case cowscript::TokenKind::Punctuation:
            return IM_COL32(180, 180, 200, 255);
        default:
            return IM_COL32(220, 220, 220, 255);
        }
    }

    // Draw a syntax-highlighted CowScript code block and advance the ImGui cursor past it.
    void renderHelpCodeBlock(const std::string &code)
    {
        // Strip a single trailing newline so we don't render a spurious blank line.
        const char *src = code.c_str();
        size_t srcLen = code.size();
        if (srcLen && src[srcLen - 1] == '\n')
            --srcLen;

        float lineH = ImGui::GetTextLineHeightWithSpacing();
        float lineH2 = ImGui::GetTextLineHeight();

        int numLines = 1;
        for (size_t i = 0; i < srcLen; ++i)
            if (src[i] == '\n')
                ++numLines;

        const float padX = 12.0f;
        const float padY = 7.0f;
        float availW = ImGui::GetContentRegionAvail().x;
        float blockH = numLines * lineH - (lineH - lineH2) + padY * 2.0f;

        ImVec2 tl = ImGui::GetCursorScreenPos();
        ImDrawList *dl = ImGui::GetWindowDrawList();
        ImFont *fnt = ImGui::GetFont();
        float fs = ImGui::GetFontSize();

        // Background + blue left accent bar.
        dl->AddRectFilled(tl, ImVec2(tl.x + availW, tl.y + blockH),
                          IM_COL32(22, 22, 32, 255), 5.0f);
        dl->AddRectFilled(tl, ImVec2(tl.x + 3.0f, tl.y + blockH),
                          IM_COL32(86, 156, 214, 180), 2.5f);

        // Tokenise the trimmed source and draw each span.
        std::string trimmed(src, srcLen);
        auto tokens = cowscript::highlight(trimmed);

        float cx = tl.x + padX;
        float cy = tl.y + padY;

        for (auto &tok : tokens)
        {
            if (tok.length <= 0)
                continue;
            ImU32 col = helpTokenColor(tok.kind);

            const char *p = trimmed.c_str() + tok.start;
            const char *end = p + tok.length;

            while (p < end)
            {
                const char *nl = static_cast<const char *>(memchr(p, '\n', end - p));
                if (!nl)
                    nl = end;

                if (nl > p)
                {
                    dl->AddText(fnt, fs, ImVec2(cx, cy), col, p, nl);
                    cx += fnt->CalcTextSizeA(fs, FLT_MAX, 0.0f, p, nl).x;
                }

                if (nl < end)
                {
                    cy += lineH;
                    cx = tl.x + padX;
                    p = nl + 1;
                }
                else
                    break;
            }
        }

        // Advance ImGui's layout cursor past the block + a small gap.
        ImGui::Dummy(ImVec2(availW, blockH + 6.0f));
    }

    // Colour headings; delegate everything else to the default callback.
    void helpFormatCallback(const ImGui::MarkdownFormatInfo &info, bool start)
    {
        ImGui::defaultMarkdownFormatCallback(info, start);
        if (info.type != ImGui::MarkdownFormatType::HEADING)
            return;
        if (start)
        {
            switch (info.level)
            {
            case 1:
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 0.85f, 0.40f, 1.0f));
                break; // gold
            case 2:
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.83f, 1.00f, 1.0f));
                break; // sky blue
            case 3:
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 1.00f, 0.72f, 1.0f));
                break; // mint
            default:
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
                break;
            }
        }
        else
        {
            ImGui::PopStyleColor();
        }
    }

    // Returns true if a markdown table row is a separator (|---|---|).
    bool isSeparatorRow(const std::string &line)
    {
        bool hasDash = false;
        for (char c : line)
        {
            if (c == '-')
            {
                hasDash = true;
                continue;
            }
            if (c == '|' || c == ':' || c == ' ' || c == '\t')
                continue;
            return false;
        }
        return hasDash;
    }

    // Split a pipe-delimited table row into trimmed cell strings.
    std::vector<std::string> parseTableRow(const std::string &line)
    {
        std::vector<std::string> cells;
        size_t pos = (!line.empty() && line[0] == '|') ? 1 : 0;
        while (pos <= line.size())
        {
            size_t next = line.find('|', pos);
            if (next == std::string::npos)
                next = line.size();
            std::string cell = line.substr(pos, next - pos);
            auto s = cell.find_first_not_of(" \t");
            auto e = cell.find_last_not_of(" \t");
            cells.push_back(s == std::string::npos ? "" : cell.substr(s, e - s + 1));
            pos = next + 1;
        }
        // Drop trailing empty cell left by a closing `|`.
        if (!cells.empty() && cells.back().empty())
            cells.pop_back();
        return cells;
    }

    // Render a markdown table using ImGui::BeginTable.
    void renderHelpTable(const std::string &content)
    {
        using namespace std;
        vector<vector<string>> rows;
        size_t pos = 0;
        while (pos <= content.size())
        {
            size_t nl = content.find('\n', pos);
            if (nl == string::npos)
                nl = content.size();
            if (nl > pos)
            {
                string line = content.substr(pos, nl - pos);
                if (!isSeparatorRow(line))
                    rows.push_back(parseTableRow(line));
            }
            pos = nl + 1;
        }
        if (rows.empty() || rows[0].empty())
            return;

        int cols = (int)rows[0].size();
        ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_SizingStretchProp |
                                ImGuiTableFlags_NoHostExtendX;

        // Use the content pointer as a stable per-table ID.
        ImGui::PushID(content.c_str());
        if (ImGui::BeginTable("##t", cols, flags))
        {
            // First row becomes the column headers.
            for (auto &cell : rows[0])
                ImGui::TableSetupColumn(cell.c_str());
            ImGui::TableHeadersRow();

            for (size_t r = 1; r < rows.size(); ++r)
            {
                ImGui::TableNextRow();
                for (int c = 0; c < cols; ++c)
                {
                    ImGui::TableSetColumnIndex(c);
                    const string &cell = (c < (int)rows[r].size()) ? rows[r][c] : "";
                    // Strip backticks — imgui_markdown doesn't support inline code
                    // and they look noisy inside table cells.
                    string text;
                    text.reserve(cell.size());
                    for (char ch : cell)
                        if (ch != '`')
                            text += ch;
                    ImGui::TextWrapped("%s", text.c_str());
                }
            }
            ImGui::EndTable();
        }
        ImGui::PopID();
        ImGui::Spacing();
    }

    // Split markdown into Text / Code / Table sections by scanning line-by-line.
    std::vector<EditorUI::HelpSection> splitHelpMarkdown(const std::string &md)
    {
        using Kind = EditorUI::HelpSection::Kind;
        std::vector<EditorUI::HelpSection> out;

        enum class Mode
        {
            Text,
            Code,
            Table
        } mode = Mode::Text;
        std::string buf;

        auto flush = [&](Kind k)
        {
            if (!buf.empty())
            {
                out.push_back({k, std::move(buf)});
                buf.clear();
            }
        };

        size_t pos = 0;
        while (pos <= md.size())
        {
            size_t nl = md.find('\n', pos);
            bool hasNl = (nl != std::string::npos);
            if (!hasNl)
                nl = md.size();

            std::string line = md.substr(pos, nl - pos);
            pos = hasNl ? nl + 1 : md.size() + 1;

            if (mode == Mode::Code)
            {
                if (line.substr(0, 3) == "```")
                {
                    flush(Kind::Code);
                    mode = Mode::Text;
                }
                else
                    buf += line + "\n";
            }
            else if (!line.empty() && line[0] == '|')
            {
                if (mode == Mode::Text)
                    flush(Kind::Text);
                mode = Mode::Table;
                buf += line + "\n";
            }
            else if (line.substr(0, 3) == "```")
            {
                if (mode == Mode::Table)
                    flush(Kind::Table);
                else
                    flush(Kind::Text);
                mode = Mode::Code;
            }
            else
            {
                if (mode == Mode::Table)
                    flush(Kind::Table);
                mode = Mode::Text;
                // imgui_markdown only recognises *** and ___ as horizontal rules,
                // not --- (standard CommonMark). Remap standalone --- to ***.
                buf += (line == "---" ? "***" : line) + "\n";
            }
        }
        if (mode == Mode::Text)
            flush(Kind::Text);
        else if (mode == Mode::Code)
            flush(Kind::Code);
        else
            flush(Kind::Table);

        return out;
    }
} // anonymous namespace

void EditorUI::drawHelpTab()
{
    if (!helpMarkdownLoaded)
    {
        std::string raw;
        std::ifstream in("src/help.md");
        if (!in)
        {
            raw = "Help file not found: src/help.md";
        }
        else
        {
            std::ostringstream ss;
            ss << in.rdbuf();
            raw = ss.str();
            if (raw.empty())
                raw = "(No help content available.)";
        }
        helpSections = splitHelpMarkdown(raw);
        helpMarkdownLoaded = true;
    }

    ImGui::MarkdownConfig mdConfig{};
    mdConfig.formatCallback = helpFormatCallback;
    mdConfig.formatFlags = ImGuiMarkdownFormatFlags_GithubStyle;
    mdConfig.headingFormats[0] = {ImGuiLayer::fontH1, true};  // H1: bold 28px + separator
    mdConfig.headingFormats[1] = {ImGuiLayer::fontH2, true};  // H2: bold 24px + separator
    mdConfig.headingFormats[2] = {ImGuiLayer::fontH3, false}; // H3: semibold 18px

    ImGui::BeginChild("HelpRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (auto &section : helpSections)
    {
        switch (section.kind)
        {
        case HelpSection::Kind::Code:
            renderHelpCodeBlock(section.content);
            break;
        case HelpSection::Kind::Table:
            renderHelpTable(section.content);
            break;
        default:
            ImGui::Markdown(section.content.c_str(), section.content.size(), mdConfig);
            break;
        }
    }
    ImGui::EndChild();
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
    heiarchyInput = (ImGui::IsWindowHovered(ImGuiFocusedFlags_RootAndChildWindows) || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) && showHierarchy;

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
        clearSelection();
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

    auto typeName = [](ecs::ShapeKind k) -> const char *
    {
        switch (k)
        {
            case ecs::ShapeKind::Cube: return "Cube";
            case ecs::ShapeKind::Plane: return "Plane";
            case ecs::ShapeKind::Static: return "StaticObject";
            case ecs::ShapeKind::Player: return "Player";
        }
        return "Entity";
    };

    auto drawEntityRow = [&](ecs::Entity e)
    {
        auto &reg = scene->registry();
        auto *ident = reg.try_get<ecs::Identity>(e);
        auto *sm = reg.try_get<ecs::ShapeMarker>(e);
        std::string label = (ident ? ident->name : std::string("Entity")) +
                            " [" + (sm ? typeName(sm->kind) : "Entity") + "]";
        if (!hierarchyFilter.PassFilter(label.c_str()))
            return;
        bool selected = selection.entity == e;
        if (ImGui::Selectable(label.c_str(), selected))
            setSelection(e);
    };

    if (scene->hasPlayer())
        drawEntityRow(scene->getPlayerEntity());

    scene->forEachEntity([&](ecs::Entity e) { drawEntityRow(e); });

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Inspector component descriptors
//
// Each entry knows how to ask "is this component on the entity?", "draw its
// section in the inspector" (returning false if the user clicked Remove), and
// "add a default instance to the entity". Adding a new component type to the
// editor's UI is one new entry in inspectorDescriptors() below.
// ---------------------------------------------------------------------------

namespace
{
    struct ComponentDescriptor
    {
        const char *name;
        // True if the entity currently has this component.
        std::function<bool(EditorUI &, Scene &, ecs::Entity)> has;
        // Draw the inspector section. Returns true to keep the component,
        // false if the user clicked the section's Remove button.
        std::function<bool(EditorUI &, Scene &, ecs::Entity)> draw;
        // Attach a default instance. May be null for components that can't
        // be added through the generic popup (e.g. PlayerController, which
        // needs camera wiring done by Scene::addPlayer).
        std::function<void(EditorUI &, Scene &, ecs::Entity)> add;
    };

    bool drawCollapsingWithRemove(const char *label, bool &requestRemove)
    {
        ImGui::PushID(label);
        bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        // Right-aligned "X" button on the header row.
        float btnW = ImGui::GetFrameHeight();
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - btnW - 4.0f);
        if (ImGui::SmallButton("X"))
            requestRemove = true;
        ImGui::PopID();
        return open;
    }

    // -- Identity ----------------------------------------------------------
    bool drawIdentity(EditorUI &, Scene &scene, ecs::Entity e)
    {
        auto *ident = scene.registry().try_get<ecs::Identity>(e);
        if (!ident)
            return true;
        char nameBuffer[256] = {};
        std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", ident->name.c_str());
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
            ident->name = nameBuffer;
        ImGui::Text("ID: %d", ident->id);
        return true; // never removable
    }

    // -- Transform ---------------------------------------------------------
    bool drawTransform(EditorUI &ui, Scene &scene, ecs::Entity e)
    {
        (void)e; (void)scene;
        if (ImGui::DragFloat3("Position", &ui.selectionPositionRef().x, 0.1f))
            ui.applySelectionTransformPublic();
        if (ImGui::DragFloat3("Rotation", &ui.selectionRotationRef().x, 0.5f))
            ui.applySelectionTransformPublic();
        if (ImGui::DragFloat3("Scale", &ui.selectionScaleRef().x, 0.05f, 0.001f, 1000.0f))
            ui.applySelectionTransformPublic();
        return true; // never removable
    }

    // -- Renderable --------------------------------------------------------
    bool drawRenderable(EditorUI &ui, Scene &scene, ecs::Entity e)
    {
        auto *rd = scene.registry().try_get<ecs::Renderable>(e);
        if (!rd)
            return true;
        if (ImGui::ColorEdit4("Color", &ui.selectionColorRef().r))
            ui.applySelectionColorPublic();

        auto *sm = scene.registry().try_get<ecs::ShapeMarker>(e);
        if (sm && sm->kind == ecs::ShapeKind::Cube)
            ImGui::Text("Mesh: Cube (size %d)", sm->cubeSize);
        else if (sm && sm->kind == ecs::ShapeKind::Plane)
            ImGui::Text("Mesh: Plane (%.1f x %.1f)", sm->planeLength, sm->planeWidth);
        else if (sm && sm->kind == ecs::ShapeKind::Static)
        {
            ImGui::Text("Mesh: StaticMesh");
            if (auto *ident = scene.registry().try_get<ecs::Identity>(e))
            {
                char meshBuf[256] = {};
                std::snprintf(meshBuf, sizeof(meshBuf), "%s", ident->meshPath.c_str());
                if (ImGui::InputText("Path", meshBuf, sizeof(meshBuf)))
                    ident->meshPath = meshBuf;
                ImGui::TextDisabled("Reload the scene to apply path changes.");
            }
        }

        ImGui::Text("Line width:");
        ImGui::SameLine();
        float lw = static_cast<float>(rd->lineWidth);
        if (ImGui::DragFloat("##linewidth", &lw, 0.1f, 0.1f, 10.0f))
            rd->lineWidth = lw;
        return true;
    }

    // -- Physics -----------------------------------------------------------
    bool drawPhysics(EditorUI &ui, Scene &scene, ecs::Entity e)
    {
        auto *p = scene.registry().try_get<ecs::Physics>(e);
        if (!p || !p->body)
            return true;
        float mass = 0.0f;
        if (p->body->getInvMass() > 0.0f)
            mass = 1.0f / p->body->getInvMass();
        if (ImGui::InputFloat("Mass", &mass, 0.1f, 1.0f))
            ecs::setMass(*p, mass);
        btVector3 v = p->body->getLinearVelocity();
        ImGui::Text("Velocity: %.2f %.2f %.2f", v.getX(), v.getY(), v.getZ());
        ImGui::Checkbox("Show Collider", &ui.showCollidersRef());
        return true;
    }

    // -- Script ------------------------------------------------------------
    bool drawScript(EditorUI &ui, Scene &scene, ecs::Entity e)
    {
        auto *ident = scene.registry().try_get<ecs::Identity>(e);
        if (!ident)
            return true;
        char scriptBuf[256] = {};
        std::snprintf(scriptBuf, sizeof(scriptBuf), "%s", ident->scriptPath.c_str());
        if (ImGui::InputText("Path", scriptBuf, sizeof(scriptBuf)))
        {
            ident->scriptPath = scriptBuf;
            scene.registry().remove<ecs::ScriptComponent>(e);
        }
        ImGui::SameLine();
        if (ImGui::Button("Edit"))
        {
            if (ident->scriptPath.empty())
                ui.addLog("Set a script path first (e.g. scripts/spin.cow).",
                          ImVec4(0.9f, 0.7f, 0.4f, 1.0f));
            else
                ui.openScriptInCodeEditor(ident->scriptPath);
        }
        if (scene.registry().all_of<ecs::ScriptComponent>(e))
            ImGui::TextDisabled("Compiled");
        else
            ImGui::TextDisabled("Not yet compiled (Apply in Code tab to compile).");
        return true;
    }
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

    if (selection.entity == ecs::NullEntity || !scene->registry().valid(selection.entity))
    {
        ImGui::TextUnformatted("Select an object to inspect.");
        ImGui::End();
        return;
    }

    if (!selection.hasCache)
        refreshSelectionCache();

    auto &reg = scene->registry();
    auto *sm = reg.try_get<ecs::ShapeMarker>(selection.entity);

    const char *typeStr = "Entity";
    if (sm)
    {
        switch (sm->kind)
        {
            case ecs::ShapeKind::Cube: typeStr = "Cube"; break;
            case ecs::ShapeKind::Plane: typeStr = "Plane"; break;
            case ecs::ShapeKind::Static: typeStr = "StaticObject"; break;
            case ecs::ShapeKind::Player: typeStr = "Player"; break;
        }
    }
    ImGui::Text("Type: %s", typeStr);
    ImGui::Separator();

    struct InspectorEntry
    {
        const char *name;
        bool (*has)(Scene &, ecs::Entity);
        bool (*draw)(EditorUI &, Scene &, ecs::Entity);
        void (*remove)(EditorUI &, Scene &, ecs::Entity);
        bool removable;
    };

    static const InspectorEntry entries[] = {
        {"Identity",
         [](Scene &s, ecs::Entity e) { return s.registry().all_of<ecs::Identity>(e); },
         &drawIdentity,
         [](EditorUI &, Scene &, ecs::Entity) {},
         false},
        {"Transform",
         [](Scene &s, ecs::Entity e) { return s.registry().all_of<ecs::Transform>(e); },
         &drawTransform,
         [](EditorUI &, Scene &, ecs::Entity) {},
         false},
        {"Renderable (Mesh)",
         [](Scene &s, ecs::Entity e) { return s.registry().all_of<ecs::Renderable>(e); },
         &drawRenderable,
         [](EditorUI &, Scene &s, ecs::Entity e) { ecs::removeRenderable(s.registry(), e); },
         true},
        {"Physics (Collider + RigidBody)",
         [](Scene &s, ecs::Entity e) { return s.registry().all_of<ecs::Physics>(e); },
         &drawPhysics,
         [](EditorUI &, Scene &s, ecs::Entity e) { ecs::removePhysics(s.registry(), e, s.physicsWorld()); },
         true},
        {"Script",
         [](Scene &s, ecs::Entity e)
         {
             auto *id = s.registry().try_get<ecs::Identity>(e);
             return (id && !id->scriptPath.empty()) || s.registry().all_of<ecs::ScriptComponent>(e);
         },
         &drawScript,
         [](EditorUI &, Scene &s, ecs::Entity e) { ecs::removeScript(s.registry(), e); },
         true},
    };

    for (const auto &entry : entries)
    {
        if (!entry.has(*scene, selection.entity))
            continue;
        bool requestRemove = false;
        bool open;
        if (entry.removable)
            open = drawCollapsingWithRemove(entry.name, requestRemove);
        else
        {
            ImGui::PushID(entry.name);
            open = ImGui::CollapsingHeader(entry.name, ImGuiTreeNodeFlags_DefaultOpen);
            ImGui::PopID();
        }
        if (open)
        {
            ImGui::PushID(entry.name);
            entry.draw(*this, *scene, selection.entity);
            ImGui::PopID();
        }
        if (requestRemove && entry.removable)
            entry.remove(*this, *scene, selection.entity);
    }

    ImGui::Separator();
    drawAddComponentPopup(scene, entries, sizeof(entries) / sizeof(entries[0]));

    ImGui::Separator();
    if (ImGui::Button("Delete Entity"))
    {
        ecs::Entity e = selection.entity;
        clearSelection();
        if (e == scene->getPlayerEntity())
            scene->removePlayer();
        else
            scene->destroyEntity(e);
    }

    ImGui::End();
}

void EditorUI::drawAddComponentPopup(Scene *scene, const void *entriesPtr, size_t entryCount)
{
    if (ImGui::Button("+ Add Component"))
        ImGui::OpenPopup("##AddComponent");

    if (!ImGui::BeginPopup("##AddComponent"))
        return;

    // Re-derive what the entity is missing using the same descriptor table
    // the inspector uses, so this menu stays in sync automatically.
    struct InspectorEntry
    {
        const char *name;
        bool (*has)(Scene &, ecs::Entity);
        bool (*draw)(EditorUI &, Scene &, ecs::Entity);
        void (*remove)(EditorUI &, Scene &, ecs::Entity);
        bool removable;
    };
    auto *entries = static_cast<const InspectorEntry *>(entriesPtr);
    auto &reg = scene->registry();
    ecs::Entity e = selection.entity;

    if (!reg.all_of<ecs::Renderable>(e))
    {
        if (ImGui::BeginMenu("Renderable (Mesh)"))
        {
            if (ImGui::MenuItem("Cube"))
                ecs::addRenderableCube(reg, e);
            if (ImGui::MenuItem("Plane"))
                ecs::addRenderablePlane(reg, e);
            if (ImGui::BeginMenu("Static mesh from file"))
            {
                if (fileBrowserModels.empty())
                    ImGui::TextDisabled("(no .obj files found — refresh Files panel)");
                for (const auto &path : fileBrowserModels)
                {
                    if (ImGui::MenuItem(path.c_str()))
                    {
                        auto &am = AssetManager::instance();
                        std::string key = std::filesystem::path(path).stem().string();
                        auto mesh = am.loadStaticMeshFromOBJ(path, key);
                        if (mesh)
                            ecs::addRenderableFromMesh(reg, e, mesh, path);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
    }

    if (!reg.all_of<ecs::Physics>(e))
    {
        if (ImGui::BeginMenu("Physics (Collider + RigidBody)"))
        {
            if (ImGui::MenuItem("Box collider"))
                ecs::addBoxCollider(reg, e, scene->physicsWorld());
            if (ImGui::MenuItem("Sphere collider"))
                ecs::addSphereCollider(reg, e, scene->physicsWorld());
            if (ImGui::MenuItem("Capsule collider"))
                ecs::addCapsuleCollider(reg, e, scene->physicsWorld());
            if (ImGui::MenuItem("Convex hull from current mesh", nullptr, false,
                                reg.all_of<ecs::Renderable>(e)))
                ecs::addConvexHullColliderFromRenderable(reg, e, scene->physicsWorld());
            ImGui::EndMenu();
        }
    }

    auto *ident = reg.try_get<ecs::Identity>(e);
    bool hasScript = ident && !ident->scriptPath.empty();
    if (!hasScript && ImGui::MenuItem("Script"))
        ecs::addScript(reg, e);

    // Suppress unused-variable warnings for non-popup code paths.
    (void)entries;
    (void)entryCount;

    ImGui::EndPopup();
}

void EditorUI::drawStats(Scene *scene, float deltaSeconds, float fps)
{
    ImGui::Begin("Stats", &showStats);
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Frame: %.2f ms", deltaSeconds * 1000.0f);
    if (scene)
    {
        size_t count = 0;
        const_cast<Scene *>(scene)->forEachEntity([&](ecs::Entity) { ++count; });
        ImGui::Text("Entities: %zu", count);
        ImGui::Text("Has Player: %s", scene->hasPlayer() ? "Yes" : "No");
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

// ---------------------------------------------------------------------------
// File browser
// ---------------------------------------------------------------------------

namespace
{
    namespace fs = std::filesystem;

    // Locate an asset directory by trying the relative path first, then
    // ASSET_ROOT/<rel> on native, then a couple of common parent dirs.
    fs::path resolveAssetDir(const std::string &rel)
    {
        std::vector<fs::path> candidates = {fs::path(rel)};
#if defined(ASSET_ROOT) && !defined(__EMSCRIPTEN__)
        candidates.emplace_back(fs::path(ASSET_ROOT) / rel);
#endif
        candidates.emplace_back(fs::path("./") / rel);
        candidates.emplace_back(fs::path("../") / rel);
        candidates.emplace_back(fs::path("/") / rel); // Emscripten preload root
        for (auto &c : candidates)
        {
            std::error_code ec;
            if (fs::exists(c, ec) && fs::is_directory(c, ec))
                return c;
        }
        return {};
    }

    // Recursively scan `rootRel` for files with `ext` (e.g. ".cow") and return
    // paths formatted as "<rootRel>/<sub/path>" (forward slashes) for the UI
    // and for assignment into engine APIs.
    std::vector<std::string> scanFiles(const std::string &rootRel, const std::string &ext)
    {
        std::vector<std::string> out;
        fs::path dir = resolveAssetDir(rootRel);
        if (dir.empty())
            return out;

        std::error_code ec;
        for (auto it = fs::recursive_directory_iterator(dir, ec);
             !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
        {
            const fs::directory_entry &entry = *it;
            if (!entry.is_regular_file(ec))
                continue;
            if (entry.path().extension() != ext)
                continue;
            fs::path rel = fs::relative(entry.path(), dir, ec);
            if (ec)
                continue;
            out.push_back(rootRel + "/" + rel.generic_string());
        }
        std::sort(out.begin(), out.end());
        return out;
    }
} // anonymous namespace

void EditorUI::refreshFileBrowser()
{
    fileBrowserScripts = scanFiles("scripts", ".cow");
    fileBrowserModels = scanFiles("models", ".obj");
    fileBrowserScenes = scanFiles("scenes", ".json");
    fileBrowserLoaded = true;
}

void EditorUI::spawnStaticObjectFromMesh(Scene *scene, const std::string &meshPath)
{
    if (!scene)
        return;
    std::string key = fs::path(meshPath).stem().string();
    ecs::Entity e = scene->spawnStaticFromAsset(meshPath, key,
                                                glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 0.0f)),
                                                glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 1.0f);
    if (e == ecs::NullEntity)
    {
        addLog("Failed to load mesh: " + meshPath, ImVec4(0.95f, 0.5f, 0.5f, 1.0f));
        return;
    }
    scene->registry().get<ecs::Identity>(e).name = key;
    addLog("Spawned " + meshPath, ImVec4(0.7f, 0.95f, 0.7f, 1.0f));
}

void EditorUI::drawFileBrowser(Scene *scene)
{
    ImGui::Begin("Files", &showFiles);

    if (!fileBrowserLoaded)
        refreshFileBrowser();

    if (ImGui::Button("Refresh"))
        refreshFileBrowser();
    ImGui::SameLine();
    fileBrowserFilter.Draw("##fbfilter", 180.0f);
    ImGui::SameLine();
    ImGui::TextDisabled("(double-click to open / spawn / load)");

    ImGui::Separator();

    auto drawSection = [&](const char *label, const std::vector<std::string> &entries,
                           const std::function<void(const std::string &)> &onActivate)
    {
        char header[64];
        std::snprintf(header, sizeof(header), "%s (%zu)", label, entries.size());
        if (!ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen))
            return;
        ImGui::Indent();
        if (entries.empty())
            ImGui::TextDisabled("No files found.");
        for (const auto &path : entries)
        {
            if (!fileBrowserFilter.PassFilter(path.c_str()))
                continue;
            ImGui::PushID(path.c_str());
            ImGui::Selectable(path.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick);
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                onActivate(path);
            ImGui::PopID();
        }
        ImGui::Unindent();
    };

    drawSection("Scripts", fileBrowserScripts, [&](const std::string &path)
                {
        if (codeEditor)
        {
            codeEditor->openFile(path);
            requestedTab = WorkspaceTab::CodeTab;
        } });

    drawSection("Models", fileBrowserModels, [&](const std::string &path)
                { spawnStaticObjectFromMesh(scene, path);
                requestedTab = WorkspaceTab::SceneTab; });

    drawSection("Scenes", fileBrowserScenes, [&](const std::string &path)
                {
        if (scene && scene->loadFromJSON(path))
        {
            clearSelection();
            addLog("Loaded scene " + path, ImVec4(0.7f, 0.95f, 0.7f, 1.0f));
            requestedTab = WorkspaceTab::SceneTab;
        }
        else
        {
            addLog("Failed to load scene " + path, ImVec4(0.95f, 0.5f, 0.5f, 1.0f));
        } });

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
    if (ImGui::Button("Spawn Empty Entity"))
    {
        ecs::Entity e = scene->createEmpty("Entity",
                                           glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 0.0f)));
        setSelection(e);
        addLog("Spawned empty entity — use the Inspector's Add Component menu to attach mesh / physics / script.",
               ImVec4(0.7f, 0.95f, 0.7f, 1.0f));
    }
    if (ImGui::Button("Spawn Cow"))
        addObjectToScene(scene, "cow");
    if (ImGui::Button("Spawn Cube"))
        addObjectToScene(scene, "cube");
    if (ImGui::Button("Spawn Plane"))
        addObjectToScene(scene, "plane");
    if (ImGui::Button("Spawn Eiffel Tower"))
        addObjectToScene(scene, "tower");

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

    ImGui::Separator();
    ImGui::TextUnformatted("Camera Speed");

    ImGui::DragFloat("##CameraSpeed", &cameraSpeed, 0.1f);

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

void EditorUI::setRequestedTab(WorkspaceTab tab)
{
    requestedTab = tab;
}

void EditorUI::setSelection(ecs::Entity entity)
{
    if (selection.entity == entity)
        return;
    if (sceneRef)
        sceneRef->setSelectedEntity(entity);
    selection.entity = entity;
    selection.hasCache = false;
}

void EditorUI::refreshSelectionCache()
{
    if (!sceneRef || selection.entity == ecs::NullEntity)
        return;
    auto &reg = sceneRef->registry();
    if (!reg.valid(selection.entity))
        return;
    if (auto *t = reg.try_get<ecs::Transform>(selection.entity))
    {
        selection.position = glm::vec3(t->position);
        selection.rotation = glm::vec3(t->rotation);
        selection.scale = glm::vec3(t->scale);
    }
    if (auto *rd = reg.try_get<ecs::Renderable>(selection.entity))
        selection.color = rd->color;
    selection.hasCache = true;
}

void EditorUI::applySelectionTransform()
{
    if (!sceneRef || selection.entity == ecs::NullEntity)
        return;
    ecs::applyTransform(sceneRef->registry(), selection.entity,
                        selection.position, selection.rotation, selection.scale);
    // Refresh Bullet's broadphase AABB so editor-mode raycasts find the new pose
    // (stepSimulation isn't called in editor mode).
    if (physicsRef)
    {
        if (auto *p = sceneRef->registry().try_get<ecs::Physics>(selection.entity); p && p->body)
            physicsRef->updateSingleAabb(p->body.get());
    }
}

void EditorUI::applySelectionColor()
{
    if (!sceneRef || selection.entity == ecs::NullEntity)
        return;
    if (auto *rd = sceneRef->registry().try_get<ecs::Renderable>(selection.entity))
        rd->color = selection.color;
}

void EditorUI::openScriptInCodeEditor(const std::string &path)
{
    if (path.empty() || !codeEditor)
        return;
    codeEditor->openFile(path);
    requestedTab = WorkspaceTab::CodeTab;
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
        clearSelection();
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
        scene->spawnCube(1,
                         glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 0.0f)),
                         glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 1.0f);
    }
    else if (type == "plane")
    {
        scene->spawnPlane(100, 100,
                          glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 0.0f)),
                          glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 0.0f);
    }
    else if (type == "cow")
    {
        scene->spawnStaticFromAsset("models/cow.obj", "cow",
                                    glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 0.0f)),
                                    glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 1.0f);
    }
    else if (type == "tower")
    {
        ecs::Entity e = scene->spawnStaticFromAsset(
            "models/eiffel_tower.obj", "tower",
            glm::translate(glm::mat4(0.0f), glm::vec3(0.0f, 0.0f, 0.0f)),
            glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 0.0f);
        if (e != ecs::NullEntity)
            ecs::applyTransform(scene->registry(), e,
                                glm::vec3(0.0f), glm::vec3(-90.0f, 0.0f, 0.0f), glm::vec3(0.1f));
    }
    scene->saveToJSON("scenes/scene.json");
}