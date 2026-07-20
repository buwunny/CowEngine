#ifndef EDITOR_CONTEXT_HPP
#define EDITOR_CONTEXT_HPP

#include <string>
#include <vector>
#include <imgui.h>
#include <glm/glm.hpp>

#include "ecs/Entity.hpp"

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

        // Vaporwave / VFX settings. Read by Application each frame to drive
        // the sky-grid background pass, the main shader's fog + neon
        // intensity, and the bloom post-process chain.
        struct VFX
        {
            // Every visual feature is gated by its own *Enabled flag so the
            // editor boots into a plain wireframe-on-dark look. Users opt in
            // to each effect via the VFX panel.

            // Sky gradient
            bool skyEnabled = false;
            glm::vec3 skyTop = glm::vec3(0.05f, 0.02f, 0.18f);     // deep purple
            glm::vec3 skyMid = glm::vec3(0.85f, 0.18f, 0.55f);     // hot pink
            glm::vec3 skyBottom = glm::vec3(1.00f, 0.55f, 0.20f);  // sunset orange

            // Sun disk + glow
            bool sunEnabled = false;
            // sunPos is used only in screen-anchored mode (NDC: -1..1 on both axes).
            glm::vec2 sunPos = glm::vec2(0.0f, 0.18f);
            float sunRadius = 0.22f;
            glm::vec3 sunColor = glm::vec3(1.0f, 0.85f, 0.45f);
            int sunStripes = 6;
            // When true, the sun is fixed at a world-space direction (set by
            // azimuth + elevation) and projected to screen each frame so it
            // sits on the horizon rather than drifting with the camera.
            bool sunWorldAnchored = true;
            float sunAzimuth = 0.0f;    // degrees around Y, 0 = looking down -Z
            float sunElevation = 6.0f;  // degrees above the horizon

            // Perspective grid floor
            bool gridEnabled = false;
            glm::vec3 gridColor = glm::vec3(1.0f, 0.25f, 0.85f);   // neon magenta
            float gridScale = 4.0f;                                // world units between major lines
            float gridFade = 120.0f;                               // distance at which grid fades to 0
            float gridLineWidth = 0.04f;                           // line thickness (0..1)
            float horizonY = 0.0f;                                 // world-Y of the grid plane

            // Solid black fill behind each wireframe object (legibility — hides sky)
            bool wireframeFill = true;

            // Distance fog applied to the wireframe lines
            bool fogEnabled = false;
            glm::vec3 fogColor = glm::vec3(0.30f, 0.05f, 0.30f);
            float fogStart = 12.0f;
            float fogEnd = 140.0f;

            // Neon brightness boost on wireframe colors (1.0 = pass-through)
            bool neonEnabled = false;
            float neonIntensity = 1.0f;

            // Bloom post-process (also enables HDR tonemap + gamma so neon stays balanced)
            bool bloomEnabled = false;
            float bloomThreshold = 0.55f;
            float bloomIntensity = 1.4f;
            float bloomRadius = 1.0f;                              // blur kernel scale
            int bloomIterations = 5;                               // ping-pong passes per axis pair

            // Retro CRT overlay
            bool scanlinesEnabled = false;
            float scanlineStrength = 0.15f;
        } vfx;

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
