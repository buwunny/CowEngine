#ifndef PLANE_HPP
#define PLANE_HPP

#include "Object.hpp"
#include "../meshes/PlaneMesh.hpp"

class Plane : public Object
{
private:
    float length;
    float width;

public:
    Plane(float length, float width, glm::mat4 model = glm::mat4(1.0f), glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), float mass = 0.0f);
    ~Plane() = default;

    const char *getTypeName() const override { return "Plane"; }

    void render(Window &window, Shader &shader) override;
    void renderTransparent(Window &window, Shader &shader) override;
    void renderFill(Window &window, Shader &shader) override;

    float getLength() const { return length; }
    float getWidth() const { return width; }
};
#endif // PLANE_HPP