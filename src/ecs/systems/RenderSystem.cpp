#include "ecs/systems/RenderSystem.hpp"
#include "ecs/Components.hpp"

#include "Window.hpp"
#include "Shader.hpp"
#include "meshes/Mesh.hpp"

#if defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif

namespace ecs
{
    namespace
    {
        bool s_fillEnabled = true;
    }

    void setWireframeFillEnabled(bool enabled) { s_fillEnabled = enabled; }
    bool isWireframeFillEnabled() { return s_fillEnabled; }

    void renderSystem(Registry &r, Window &window, Shader &shader)
    {
        auto view = r.view<Transform, Renderable>();
        for (auto e : view)
        {
            auto &t = view.get<Transform>(e);
            auto &rd = view.get<Renderable>(e);
            if (!rd.mesh)
                continue;

            shader.setModelMatrix(t.model);

            bool selected = r.all_of<Selected>(e);
            bool hovered = r.all_of<Hovered>(e);

            if (s_fillEnabled || selected || hovered)
            {
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(1.0f, 1.0f);
                window.setPolygonMode(GL_FILL);

                glm::vec4 fillColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                if (selected)
                    fillColor = glm::vec4(0.10f, 0.10f, 0.14f, 0.95f);
                else if (hovered)
                    fillColor = glm::vec4(0.05f, 0.05f, 0.05f, 1.0f);

                shader.setFragmentColor(fillColor);
                rd.mesh->render();
                glDisable(GL_POLYGON_OFFSET_FILL);
            }

            window.setPolygonMode(GL_LINE);
            window.setLineWidth(rd.lineWidth);
            shader.setFragmentColor(rd.color);
            rd.mesh->renderWireframe();
        }
    }

    void renderTransparentSystem(Registry &r, Window &window, Shader &shader)
    {
        auto view = r.view<Transform, Renderable>();
        for (auto e : view)
        {
            auto &t = view.get<Transform>(e);
            auto &rd = view.get<Renderable>(e);
            if (!rd.mesh)
                continue;
            shader.setModelMatrix(t.model);
            window.setPolygonMode(GL_LINE);
            window.setLineWidth(rd.lineWidth);
            shader.setFragmentColor(rd.color);
            rd.mesh->renderWireframe();
        }
    }

    void renderFillSystem(Registry &r, Window &window, Shader &shader)
    {
        auto view = r.view<Transform, Renderable>();
        for (auto e : view)
        {
            auto &t = view.get<Transform>(e);
            auto &rd = view.get<Renderable>(e);
            if (!rd.mesh)
                continue;
            shader.setModelMatrix(t.model);
            window.setPolygonMode(GL_FILL);
            shader.setFragmentColor(rd.color);
            rd.mesh->render();
        }
    }
}
