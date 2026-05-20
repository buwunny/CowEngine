#ifndef EDITOR_UI_HPP
#define EDITOR_UI_HPP

#include <string>
#include <vector>
#include <imgui.h>
#include <glm/glm.hpp>

class Scene;
class Window;
class PhysicsWorld;
class Object;
class EditorUI
{
public:
    EditorUI();

    void render(Scene *scene, Window *window, PhysicsWorld *physics, float deltaSeconds, float fps);
    void addLog(const std::string &text, const ImVec4 &color = ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    bool getGameViewport(float &x, float &y, float &w, float &h, float &scaleX, float &scaleY) const;
    bool isTestingMode() const { return testingMode; }
    bool isGameViewInputEnabled() const { return gameViewInput; }
    void setGameTexture(ImTextureID textureId, float width, float height);
    void setSelection(Object *object);

    void setVisible(bool visible) { showUI = visible; }
    bool isVisible() const { return showUI; }

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
    void drawGameView(Scene *scene);
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

    bool showUI = true;
    bool showHierarchy = true;
    bool showInspector = true;
    bool showConsole = true;
    bool showStats = true;
    bool showRuntime = true;
    bool showGameView = true;
    bool testingMode = false;
    bool dockLayoutBuilt = false;

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
};

#endif // EDITOR_UI_HPP
