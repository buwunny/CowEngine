#ifndef CUBE_HPP
#define CUBE_HPP

#include "Object.hpp"
#include "../meshes/CubeMesh.hpp"

class Cube : public Object
{
private:
    int size;
public:
    Cube(int size, glm::mat4 model = glm::mat4(1.0f), glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), float mass = 10.0f);
    ~Cube() = default;

    const char *getTypeName() const override { return "Cube"; }

    void render(Window &window, Shader &shader) override;
    void renderTransparent(Window &window, Shader &shader) override;
    void renderFill(Window &window, Shader &shader) override;

    int getSize() const { return size; }
};
#endif // CUBE_HPP