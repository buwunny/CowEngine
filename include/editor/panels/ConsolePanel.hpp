#ifndef EDITOR_CONSOLE_PANEL_HPP
#define EDITOR_CONSOLE_PANEL_HPP

#include <string>
#include <vector>
#include <imgui.h>

#include "editor/EditorContext.hpp"

namespace editor
{
    class ConsolePanel
    {
    public:
        ConsolePanel();
        void draw(Context &ctx);

    private:
        void execCommand(Context &ctx, const std::string &commandLine);
        static int textEditCallback(ImGuiInputTextCallbackData *data);

        std::vector<std::string> consoleHistory;
        int historyPos = -1;
        char consoleInput[256] = {};
        bool autoScroll = true;
    };
}

#endif
