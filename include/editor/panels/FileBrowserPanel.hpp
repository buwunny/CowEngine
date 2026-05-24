#ifndef EDITOR_FILE_BROWSER_PANEL_HPP
#define EDITOR_FILE_BROWSER_PANEL_HPP

#include <imgui.h>

#include "editor/EditorContext.hpp"

namespace editor
{
    class FileBrowserPanel
    {
    public:
        void draw(Context &ctx);
        void refresh(Context &ctx);

    private:
        bool loaded = false;
        ImGuiTextFilter filter;
    };
}

#endif
