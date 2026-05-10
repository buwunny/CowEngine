#ifndef MESH_HPP
#define MESH_HPP

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

#include <glad/glad.h>

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

    glm::mat4 getModel() { return model; };
    void setModel(glm::mat4 model) { this->model = model; };

    const std::vector<float> &getVertices() const { return vertices; }
    const std::vector<unsigned int> &getIndices() const { return indices; }
    size_t getVertexCount() const { return vertices.size() / std::max(1, floats_per_vertex); }
    size_t getIndexCount() const { return indices.size(); }
    int getFloatsPerVertex() const { return floats_per_vertex; }

protected:
    unsigned int VBO, VAO, EBO;
    glm::mat4 model;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    int floats_per_vertex = 3;
};
#endif // MESH_HPP