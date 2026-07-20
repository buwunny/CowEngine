#ifndef EDITOR_CONTEXT_HPP
#define EDITOR_CONTEXT_HPP

#include <string>
#include <vector>
#include <imgui.h>
#include <glm/glm.hpp>

#include "ecs/Entity.hpp"
#include "render/VfxSettings.hpp"

class Scene;
class Window;
class PhysicsWorld;
class Camera;
class CodeEditor;
class ScriptHost;

namespace editor
{
    enum class GizmoOp
    {
        Translate,
        Rotate,
        Scale
    };

    enum WorkspaceTab
    {
        TabNone = -1,
        SceneTab,
        CodeTab,
        HelpTab
    };

    struct ConsoleLine
    {
        std::string text;
        ImVec4 color;
    };

    struct SelectionState
    {
        ecs::Entity entity = ecs::NullEntity;
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 rotation = glm::vec3(0.0f);
        glm::vec3 scale = glm::vec3(1.0f);
        glm::vec4 color = glm::vec4(1.0f);
        bool hasCache = false;
    };

    // Shared editor state passed by reference to every panel's draw() call.
    // Panels read and mutate this directly instead of going through EditorUI.
    struct Context
    {
        Scene *scene = nullptr;
        Window *window = nullptr;
        PhysicsWorld *physics = nullptr;
        Camera *camera = nullptr;
        ScriptHost *scriptHost = nullptr;
        CodeEditor *codeEditor = nullptr;

        SelectionState selection;

        GizmoOp gizmoOp = GizmoOp::Translate;
        bool gizmoLocal = false;

        WorkspaceTab activeTab = HelpTab;
        WorkspaceTab requestedTab = TabNone;
        WorkspaceTab lastDrawnWorkspaceTab = TabNone;

        bool testingMode = false;
        bool showColliders = true;

        bool showHierarchy = true;
        bool showInspector = true;
        bool showConsole = true;
        bool showStats = true;
        bool showRuntime = true;
        bool showFiles = true;
        bool showGameView = true;
        bool showVfx = true;

        float gameViewportX = 0.0f;
        float gameViewportY = 0.0f;
        float gameViewportW = 0.0f;
        float gameViewportH = 0.0f;
        float framebufferScaleX = 1.0f;
        float framebufferScaleY = 1.0f;
        bool hasGameViewport = false;
        bool gameViewInput = false;
        bool heiarchyInput = false;
        ImTextureID gameTextureId = 0;
        float gameTextureW = 0.0f;
        float gameTextureH = 0.0f;

        float mouseSensitivity = 1.0f;
        float cameraSpeed = 5.0f;

        // Vaporwave / VFX settings. The struct now lives in render/VfxSettings.hpp
        // (imgui-free) so the standalone runtime can use it without the editor;
        // it is aliased here as editor::Context::VFX for existing callers.
        using VFX = ::editor::VFX;
        VFX vfx;

        std::vector<ConsoleLine> consoleLines;
        bool scrollToBottom = false;
        std::string lastSavePath;

        // Discovered asset paths, refreshed by FileBrowserPanel. Other panels
        // (e.g. the inspector's "Static mesh from file" submenu) read these.
        std::vector<std::string> fileBrowserScripts;
        std::vector<std::string> fileBrowserModels;
        std::vector<std::string> fileBrowserScenes;

        void addLog(const std::string &text,
                    const ImVec4 &color = ImVec4(0.85f, 0.85f, 0.85f, 1.0f));

        void setSelection(ecs::Entity e);
        void clearSelection() { setSelection(ecs::NullEntity); }
        void refreshSelectionCache();
        void applySelectionTransform();
        void applySelectionColor();

        void openScriptInCodeEditor(const std::string &path);

        // VFX persistence. On web, the editor's VFX panel state is saved to
        // localStorage under 'cowengine_vfx' so it survives page reloads.
        // On native both calls are no-ops by design.
        std::string serializeVfxToJson() const;
        bool loadVfxFromJson(const std::string &json);
        void saveVfxToLocalStorage();
        void loadVfxFromLocalStorage();
    };

    // Spawn a built-in object kind ("cube", "plane", "cow", "tower") and persist
    // the scene. Shared between HierarchyPanel, RuntimePanel, and the console.
    void addObjectToScene(Context &ctx, const std::string &type);

    // Spawn a static object from a mesh path; used by the file browser.
    void spawnStaticObjectFromMesh(Context &ctx, const std::string &meshPath);
}

#endif // EDITOR_CONTEXT_HPP
