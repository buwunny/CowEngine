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
        if (selection.object && !selection.object->getScriptPath().empty())
        {
            codeEditor->openFile(selection.object->getScriptPath());
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
    ImGui::TextUnformatted("Script");
    {
        char scriptBuf[256] = {};
        std::snprintf(scriptBuf, sizeof(scriptBuf), "%s", selection.object->getScriptPath().c_str());
        if (ImGui::InputText("##scriptPath", scriptBuf, sizeof(scriptBuf)))
        {
            selection.object->setScriptPath(scriptBuf);
            // Drop any previously compiled script so the new path takes effect.
            selection.object->setScript(nullptr);
        }
        ImGui::SameLine();
        if (ImGui::Button("Edit"))
        {
            std::string path = selection.object->getScriptPath();
            if (path.empty())
            {
                addLog("Set a script path first (e.g. scripts/spin.cow).",
                       ImVec4(0.9f, 0.7f, 0.4f, 1.0f));
            }
            else if (codeEditor)
            {
                codeEditor->openFile(path);
                requestedTab = WorkspaceTab::CodeTab;
            }
        }
    }

    if (std::string(selection.object->getTypeName()) == "StaticObject")
    {
        ImGui::Separator();
        ImGui::TextUnformatted("Model");
        char meshBuf[256] = {};
        std::snprintf(meshBuf, sizeof(meshBuf), "%s", selection.object->getMeshPath().c_str());
        if (ImGui::InputText("##meshPath", meshBuf, sizeof(meshBuf)))
        {
            selection.object->setMeshPath(meshBuf);
        }
        ImGui::TextDisabled("Reload the scene to apply.");
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
        std::vector<fs::path> candidates = { fs::path(rel) };
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
        if (dir.empty()) return out;

        std::error_code ec;
        for (auto it = fs::recursive_directory_iterator(dir, ec);
             !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
        {
            const fs::directory_entry &entry = *it;
            if (!entry.is_regular_file(ec)) continue;
            if (entry.path().extension() != ext) continue;
            fs::path rel = fs::relative(entry.path(), dir, ec);
            if (ec) continue;
            out.push_back(rootRel + "/" + rel.generic_string());
        }
        std::sort(out.begin(), out.end());
        return out;
    }
} // anonymous namespace

void EditorUI::refreshFileBrowser()
{
    fileBrowserScripts = scanFiles("scripts", ".cow");
    fileBrowserModels  = scanFiles("models",  ".obj");
    fileBrowserScenes  = scanFiles("scenes",  ".json");
    fileBrowserLoaded  = true;
}

void EditorUI::spawnStaticObjectFromMesh(Scene *scene, const std::string &meshPath)
{
    if (!scene) return;
    auto &am = AssetManager::instance();
    // Use the filename stem as the cache key so the same mesh isn't loaded twice.
    std::string key = fs::path(meshPath).stem().string();
    auto mesh = am.loadStaticMeshFromOBJ(meshPath, key);
    if (!mesh)
    {
        addLog("Failed to load mesh: " + meshPath, ImVec4(0.95f, 0.5f, 0.5f, 1.0f));
        return;
    }
    auto obj = std::make_unique<StaticObject>(
        mesh, mesh->getVertices().data(), mesh->getVertexCount(),
        mesh->getIndices().data(), mesh->getIndexCount(), mesh->getFloatsPerVertex(),
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 0.0f)),
        glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 1.0f);
    obj->setMeshPath(meshPath);
    obj->setName(key);
    scene->addObject(std::move(obj));
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
                           const std::function<void(const std::string &)> &onActivate) {
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

    drawSection("Scripts", fileBrowserScripts, [&](const std::string &path) {
        if (codeEditor)
        {
            codeEditor->openFile(path);
            requestedTab = WorkspaceTab::CodeTab;
        }
    });

    drawSection("Models", fileBrowserModels, [&](const std::string &path) {
        spawnStaticObjectFromMesh(scene, path);
    });

    drawSection("Scenes", fileBrowserScenes, [&](const std::string &path) {
        if (scene && scene->loadFromJSON(path))
        {
            setSelection(nullptr);
            addLog("Loaded scene " + path, ImVec4(0.7f, 0.95f, 0.7f, 1.0f));
        }
        else
        {
            addLog("Failed to load scene " + path, ImVec4(0.95f, 0.5f, 0.5f, 1.0f));
        }
    });

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
            auto cow = std::make_unique<StaticObject>(cowMesh, cowMesh.get()->getVertices().data(), cowMesh.get()->getVertexCount(), cowMesh.get()->getIndices().data(), cowMesh.get()->getIndexCount(), cowMesh.get()->getFloatsPerVertex(), glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 0.0f)), glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 1.0f);
            cow.get()->setMeshPath("models/cow.obj");
            scene->addObject(std::move(cow));
        }
    }
    else if (type == "tower")
    {
        auto &assetManager = AssetManager::instance();
        auto towerMesh = assetManager.loadStaticMeshFromOBJ("models/eiffel_tower.obj", "tower");
        if (towerMesh)
        {
            auto tower = std::make_unique<StaticObject>(towerMesh, towerMesh.get()->getVertices().data(), towerMesh.get()->getVertexCount(), towerMesh.get()->getIndices().data(), towerMesh.get()->getIndexCount(), towerMesh.get()->getFloatsPerVertex(), glm::translate(glm::mat4(0.0f), glm::vec3(0.0f, 0.0f, 0.0f)), glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 0.0f);
            tower.get()->setTransform(glm::vec3(0.0f), glm::vec3(-90.0f, 0.0f, 0.0f), glm::vec3(0.1f));
            tower.get()->setMeshPath("models/eiffel_tower.obj");
            scene->addObject(std::move(tower));
        }
    }
    scene->saveToJSON("scenes/scene.json");
}