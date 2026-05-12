#include "meshes/StaticMesh.hpp"
#include <cstring>
#include <iostream>
#include <vector>

StaticMesh::StaticMesh(const float *verts, size_t vertex_count, const unsigned int *inds, size_t index_count, int floats_per_vertex)
{
    this->floats_per_vertex = floats_per_vertex;
    // Copy into vectors
    vertices.assign(verts, verts + vertex_count * floats_per_vertex);
    indices.assign(inds, inds + index_count);

    // Create GL buffers
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    // Create line element buffer for wireframe rendering (triangle edges)
    glGenBuffers(1, &EBO_LINES);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Build line index buffer (each triangle -> 3 line segments)
    std::vector<unsigned int> lineIndices;
    lineIndices.reserve(indices.size() * 2);
    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        unsigned int i0 = indices[i];
        unsigned int i1 = indices[i + 1];
        unsigned int i2 = indices[i + 2];
        // add segments (i0,i1), (i1,i2), (i2,i0)
        lineIndices.push_back(i0);
        lineIndices.push_back(i1);
        lineIndices.push_back(i1);
        lineIndices.push_back(i2);
        lineIndices.push_back(i2);
        lineIndices.push_back(i0);
    }
    lineIndexCount = lineIndices.size();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_LINES);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, lineIndices.size() * sizeof(unsigned int), lineIndices.data(), GL_STATIC_DRAW);

    GLsizei stride_bytes = static_cast<GLsizei>(floats_per_vertex * sizeof(float));

    // position (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride_bytes, (void *)0);
    glEnableVertexAttribArray(0);

    int offset = 3;
    if (floats_per_vertex == 6)
    {
        // pos + normal
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride_bytes, (void *)(offset * sizeof(float)));
        glEnableVertexAttribArray(1);
    }
    else if (floats_per_vertex == 5)
    {
        // pos + uv
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride_bytes, (void *)(offset * sizeof(float)));
        glEnableVertexAttribArray(2);
    }
    else if (floats_per_vertex == 8)
    {
        // pos + normal + uv
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride_bytes, (void *)(offset * sizeof(float)));
        glEnableVertexAttribArray(1);
        offset += 3;
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride_bytes, (void *)(offset * sizeof(float)));
        glEnableVertexAttribArray(2);
    }
    else if (floats_per_vertex != 3)
    {
        // Unexpected stride: try to be tolerant by placing only position attribute
        std::cerr << "StaticMesh: unexpected floats_per_vertex=" << floats_per_vertex << ", using position-only." << std::endl;
    }

    glBindVertexArray(0);
}

StaticMesh::~StaticMesh()
{
    if (EBO_LINES)
        glDeleteBuffers(1, &EBO_LINES);
}

void StaticMesh::render()
{
    glBindVertexArray(VAO);
    // bind triangle element buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void StaticMesh::renderWireframe()
{
    glBindVertexArray(VAO);
    // bind line element buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_LINES);
    glDrawElements(GL_LINES, static_cast<GLsizei>(lineIndexCount), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
