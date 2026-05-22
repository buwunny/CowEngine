#ifndef EDITOR_UI_HPP
#define EDITOR_UI_HPP

#include <string>
#include <vector>
#include <memory>
#include <imgui.h>
#include <glm/glm.hpp>

class Scene;
class Window;
class PhysicsWorld;
class Object;
class Camera;
class CodeEditor;
class ScriptHost;

class EditorUI
{
public:
    enum class GizmoOp
    {
        Translate,
        Rotate,
        Scale
    };
    enum WorkspaceTab
    {
        None = -1,
        SceneTab,
        CodeTab,
        HelpTab
    };

public:
    EditorUI();
    ~EditorUI();

    void render(Scene *scene, Window *window, PhysicsWorld *physics, float deltaSeconds, float fps);
    void setScriptHost(ScriptHost *host) { scriptHostRef = host; }
    CodeEditor *getCodeEditor() { return codeEditor.get(); }
    void addLog(const std::string &text, const ImVec4 &color = ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    bool getGameViewport(float &x, float &y, float &w, float &h, float &scaleX, float &scaleY) const;
    bool isTestingMode() const { return testingMode; }
    bool isGameViewInputEnabled() const { return gameViewInput; }
    void setGameTexture(ImTextureID textureId, float width, float height);
    void setSelection(Object *object);
    void setRequestedTab(WorkspaceTab tab);
    void setVisible(bool visible) { showUI = visible; }
    bool isVisible() const { return showUI; }

    void setCamera(Camera *cam) { cameraRef = cam; }
    GizmoOp getGizmoOp() const { return gizmoOp; }
    bool isMouseOverGizmo() const;

private:
    struct ConsoleLine
    {
        std::string text;
        ImVec4 color;
    };

    struct SelectionState
    {
        Object *object = nullptr;
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 rotation = glm::vec3(0.0f);
        glm::vec3 scale = glm::vec3(1.0f);
        glm::vec4 color = glm::vec4(1.0f);
        bool hasCache = false;
    };

    void drawMainMenu();
    void drawDockspace();
    void drawWorkspace(Scene *scene);
    void drawSceneTab(Scene *scene);
    void drawCodeTab(Scene *scene);
    void drawHelpTab();
    void drawGizmoToolbar();
    void drawTestingOverlay();
    void drawSceneHierarchy(Scene *scene);
    void drawInspector(Scene *scene);
    void drawStats(Scene *scene, float deltaSeconds, float fps);
    void drawConsole(Scene *scene);
    void drawRuntime(Scene *scene);

    void refreshSelectionCache();
    void applySelectionTransform();
    void applySelectionColor();

    void execCommand(const std::string &commandLine, Scene *scene);
    static int consoleTextEditCallback(ImGuiInputTextCallbackData *data);

    void addObjectToScene(Scene *scene, const std::string &type);

    GizmoOp gizmoOp = GizmoOp::Translate;
    bool gizmoLocal = false;
    Camera *cameraRef = nullptr;
    PhysicsWorld *physicsRef = nullptr;

    bool showUI = true;
    bool showHierarchy = true;
    bool showInspector = true;
    bool showConsole = true;
    bool showStats = true;
    bool showRuntime = true;
    bool showGameView = true;
    bool testingMode = false;
    bool dockLayoutBuilt = false;
    WorkspaceTab activeTab = WorkspaceTab::HelpTab;
    WorkspaceTab requestedTab = WorkspaceTab::None;

    SelectionState selection;

    ImGuiTextFilter hierarchyFilter;

    std::vector<ConsoleLine> consoleLines;
    std::vector<std::string> consoleHistory;
    int historyPos = -1;
    char consoleInput[256] = {};
    bool autoScroll = true;
    bool scrollToBottom = false;
    std::string lastSavePath;

    float mouseSensitivity = 1.0f;

    float gameViewportX = 0.0f;
    float gameViewportY = 0.0f;
    float gameViewportW = 0.0f;
    float gameViewportH = 0.0f;
    float framebufferScaleX = 1.0f;
    float framebufferScaleY = 1.0f;
    bool hasGameViewport = false;
    bool gameViewInput = false;

    Window *windowRef = nullptr;

    ImTextureID gameTextureId = 0;
    float gameTextureW = 0.0f;
    float gameTextureH = 0.0f;

    std::unique_ptr<CodeEditor> codeEditor;
    ScriptHost *scriptHostRef = nullptr;
    char newScriptName[128] = "scripts/cow/new_script.cow";
    std::string helpMarkdown;
    bool helpMarkdownLoaded = false;
};

#endif // EDITOR_UI_HPP
