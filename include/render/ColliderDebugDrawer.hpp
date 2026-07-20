#ifndef COLLIDER_DEBUG_DRAWER_HPP
#define COLLIDER_DEBUG_DRAWER_HPP

#if defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif

#include <LinearMath/btIDebugDraw.h>
#include <glm/glm.hpp>
#include <vector>

class Shader;

// btIDebugDraw implementation that accumulates line segments from Bullet's
// debugDrawObject calls and renders them with the engine's shader. Used by
// the editor to visualize the collision shape of the selected object.
class ColliderDebugDrawer : public btIDebugDraw
{
public:
    ColliderDebugDrawer() = default;
    ~ColliderDebugDrawer() override;

    void drawLine(const btVector3 &from, const btVector3 &to, const btVector3 &color) override;
    void drawContactPoint(const btVector3 &, const btVector3 &, btScalar, int, const btVector3 &) override {}
    void reportErrorWarning(const char *) override {}
    void draw3dText(const btVector3 &, const char *) override {}
    void setDebugMode(int mode) override { m_mode = mode; }
    int getDebugMode() const override { return m_mode; }

    void beginFrame() { m_vertices.clear(); }
    void flush(Shader &shader, const glm::vec4 &color);

private:
    void ensureBuffers();

    int m_mode = btIDebugDraw::DBG_DrawWireframe;
    std::vector<float> m_vertices;
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
    size_t m_capacityFloats = 0;
};

#endif
