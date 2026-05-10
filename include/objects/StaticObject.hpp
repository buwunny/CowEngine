#ifndef STATICOBJECT_HPP
#define STATICOBJECT_HPP

#include "Object.hpp"
#include "../meshes/StaticMesh.hpp"

class StaticObject : public Object
{
public:
    StaticObject(const float *verts, size_t vertex_count, const unsigned int *indices, size_t index_count, int floats_per_vertex, glm::mat4 model = glm::mat4(1.0f), glm::vec4 color = glm::vec4(1.0f), float mass = 0.0f);
    StaticObject(std::shared_ptr<Mesh> sharedMesh, const float *verts, size_t vertex_count, const unsigned int *indices, size_t index_count, int floats_per_vertex, glm::mat4 model = glm::mat4(1.0f), glm::vec4 color = glm::vec4(1.0f), float mass = 0.0f);
    ~StaticObject() = default;

    void render(Window &window, Shader &shader) override;
    void renderTransparent(Window &window, Shader &shader) override;
    void renderFill(Window &window, Shader &shader) override;
};

#endif // STATICOBJECT_HPP
