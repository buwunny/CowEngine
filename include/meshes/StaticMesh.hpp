#ifndef STATIC_MESH_HPP
#define STATIC_MESH_HPP

#include "Mesh.hpp"
#include <cstddef>

class StaticMesh : public Mesh
{
public:
    // verts: flat float array (pos[, normal][, uv])
    // vertex_count: number of vertices
    // indices: index array
    // index_count: number of indices
    // floats_per_vertex: number of floats per vertex (compute with sizeof on array divided by vertex_count)
    StaticMesh(const float *verts, size_t vertex_count, const unsigned int *indices, size_t index_count, int floats_per_vertex);
    ~StaticMesh();

    void render() override;
    void renderWireframe() override;
};

#endif // STATIC_MESH_HPP
