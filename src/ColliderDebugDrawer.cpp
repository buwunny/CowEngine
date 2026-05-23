#include "ColliderDebugDrawer.hpp"
#include "Shader.hpp"

ColliderDebugDrawer::~ColliderDebugDrawer()
{
    if (m_vbo)
        glDeleteBuffers(1, &m_vbo);
    if (m_vao)
        glDeleteVertexArrays(1, &m_vao);
}

void ColliderDebugDrawer::drawLine(const btVector3 &from, const btVector3 &to, const btVector3 &)
{
    m_vertices.push_back(from.x());
    m_vertices.push_back(from.y());
    m_vertices.push_back(from.z());
    m_vertices.push_back(to.x());
    m_vertices.push_back(to.y());
    m_vertices.push_back(to.z());
}

void ColliderDebugDrawer::ensureBuffers()
{
    if (m_vao)
        return;
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void ColliderDebugDrawer::flush(Shader &shader, const glm::vec4 &color)
{
    if (m_vertices.empty())
        return;
    ensureBuffers();
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    const size_t bytes = m_vertices.size() * sizeof(float);
    if (m_vertices.size() > m_capacityFloats)
    {
        glBufferData(GL_ARRAY_BUFFER, bytes, m_vertices.data(), GL_DYNAMIC_DRAW);
        m_capacityFloats = m_vertices.size();
    }
    else
    {
        glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, m_vertices.data());
    }
    // Lines come from Bullet in world space; the shader expects model * v.
    shader.setModelMatrix(glm::mat4(1.0f));
    shader.setFragmentColor(color);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_vertices.size() / 3));
    glBindVertexArray(0);
}
