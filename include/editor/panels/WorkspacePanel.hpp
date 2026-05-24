#ifndef EDITOR_WORKSPACE_PANEL_HPP
#define EDITOR_WORKSPACE_PANEL_HPP

#include <memory>
#include <string>
#include <vector>

#include "editor/EditorContext.hpp"

class CodeEditor;

namespace editor
{
    // Owns the central Workspace window (Scene / Code / Help tabs), the gizmo
    // toolbar overlay, and the testing-mode overlay. Also owns the CodeEditor
    // instance — other panels reach it through Context::codeEditor.
    class WorkspacePanel
    {
    public:
        struct HelpSection
        {
            enum class Kind
            {
                Text,
                Code,
                Table
            } kind = Kind::Text;
            std::string content;
        };

        WorkspacePanel();
        ~WorkspacePanel();

        CodeEditor *codeEditor() { return codeEditor_.get(); }

        // Called once per frame from EditorUI::render before the panels draw.
        // Handles the workspace tab bar and the gizmo overlay inside it.
        void drawWorkspace(Context &ctx);

        // Floating overlays drawn outside the workspace window.
        void drawGizmoToolbar(Context &ctx);
        void drawTestingOverlay(Context &ctx);

        char newScriptName[128] = "scripts/new_script.cow";

    private:
        void drawSceneTab(Context &ctx);
        void drawCodeTab(Context &ctx);
        void drawHelpTab(Context &ctx);

        std::unique_ptr<CodeEditor> codeEditor_;
        std::vector<HelpSection> helpSections;
        bool helpMarkdownLoaded = false;
    };
}

#endif // EDITOR_WORKSPACE_PANEL_HPP
