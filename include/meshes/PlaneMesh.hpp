#ifndef PLANE_MESH_HPP
#define PLANE_MESH_HPP

#include "Mesh.hpp"

class PlaneMesh : public Mesh
{
public:
    PlaneMesh(float x, float z, int xSubdivisions, int zSubdivisions);
    ~PlaneMesh() = default;

    void render();
};
#endif // PLANE_HPP