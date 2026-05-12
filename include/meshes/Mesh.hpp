#ifndef MESH_HPP
#define MESH_HPP

#if defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif
#include <glm/glm.hpp>
#include <vector>

class Mesh
{
public:
    Mesh() : VBO(0), VAO(0), EBO(0), model(glm::mat4(1.0f)) {}
    Mesh(const Mesh &) = delete;
    Mesh &operator=(const Mesh &) = delete;

    Mesh(Mesh &&other) noexcept
        : VBO(other.VBO), VAO(other.VAO), EBO(other.EBO), model(other.model), vertices(std::move(other.vertices)), indices(std::move(other.indices))
    {
        other.VBO = other.VAO = other.EBO = 0;
        other.model = glm::mat4(1.0f);
    }

    Mesh &operator=(Mesh &&other) noexcept
    {
        if (this != &other)
        {
            if (EBO)
                glDeleteBuffers(1, &EBO);
            if (VBO)
                glDeleteBuffers(1, &VBO);
            if (VAO)
                glDeleteVertexArrays(1, &VAO);

            VBO = other.VBO;
            VAO = other.VAO;
            EBO = other.EBO;
            model = other.model;
            vertices = std::move(other.vertices);
            indices = std::move(other.indices);

            other.VBO = other.VAO = other.EBO = 0;
            other.model = glm::mat4(1.0f);
        }
        return *this;
    }

    virtual ~Mesh()
    {
        if (EBO)
            glDeleteBuffers(1, &EBO);
        if (VBO)
            glDeleteBuffers(1, &VBO);
        if (VAO)
            glDeleteVertexArrays(1, &VAO);
    }

    virtual void render() = 0;
    // Render as wireframe (default provided below)
    virtual void renderWireframe();

    glm::mat4 getModel() { return model; };
    void setModel(glm::mat4 model) { this->model = model; };

    const std::vector<float> &getVertices() const { return vertices; }
    const std::vector<unsigned int> &getIndices() const { return indices; }
    size_t getVertexCount() const { return vertices.size() / std::max(1, floats_per_vertex); }
    size_t getIndexCount() const { return indices.size(); }
    int getFloatsPerVertex() const { return floats_per_vertex; }

protected:
    unsigned int VBO, VAO, EBO;
    // Optional element buffer for wireframe (edge) rendering
    unsigned int EBO_LINES = 0;
    size_t lineIndexCount = 0;
    glm::mat4 model;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    int floats_per_vertex = 3;
};
// Default implementation: build a line-element buffer from triangle indices and draw GL_LINES.
inline void Mesh::renderWireframe()
{
    if (EBO_LINES == 0)
    {
        std::vector<unsigned int> lineIndices;
        lineIndices.reserve(indices.size() * 2);
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            unsigned int i0 = indices[i];
            unsigned int i1 = indices[i + 1];
            unsigned int i2 = indices[i + 2];
            lineIndices.push_back(i0);
            lineIndices.push_back(i1);
            lineIndices.push_back(i1);
            lineIndices.push_back(i2);
            lineIndices.push_back(i2);
            lineIndices.push_back(i0);
        }
        lineIndexCount = lineIndices.size();
        glGenBuffers(1, &EBO_LINES);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_LINES);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, lineIndices.size() * sizeof(unsigned int), lineIndices.data(), GL_STATIC_DRAW);
    }
    glBindVertexArray(VAO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_LINES);
    glDrawElements(GL_LINES, static_cast<GLsizei>(lineIndexCount), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
#endif // MESH_HPP