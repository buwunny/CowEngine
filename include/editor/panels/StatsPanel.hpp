#ifndef EDITOR_STATS_PANEL_HPP
#define EDITOR_STATS_PANEL_HPP

#include "editor/EditorContext.hpp"

namespace editor
{
    class StatsPanel
    {
    public:
        void draw(Context &ctx, float deltaSeconds, float fps);
    };
}

#endif
