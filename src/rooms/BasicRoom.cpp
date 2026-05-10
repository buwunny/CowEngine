#include "rooms/BasicRoom.hpp"
#include "objects/StaticObject.hpp"
#include "../../cow_mesh.hpp"

#include <memory>

BasicRoom::BasicRoom(glm::vec3 position, glm::vec3 front, glm::vec3 up)
{
    auto ground = std::make_unique<Plane>(1000, 1000, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f)), glm::vec4(0.60f, 0.60f, 0.60f, 1.0f), 0.0f);
    // Plane *ceiling = new Plane(100, 100, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 50.0f, 0.0f)), glm::vec4(0.95f, 0.95f, 0.96f, 1.0f), 0.0f);
    auto plane1 = std::make_unique<Plane>(50, 45, glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(50.0f, 25.0f, 27.5f)), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)), glm::vec4(0.80f, 0.90f, 0.95f, 1.0f), 0.0f);
    auto plane2 = std::make_unique<Plane>(50, 45, glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(50.0f, 25.0f, -27.5f)), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)), glm::vec4(0.80f, 0.90f, 0.95f, 1.0f), 0.0f);
    auto plane3 = std::make_unique<Plane>(40, 10, glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(50.0f, 30.0f, 0.0f)), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)), glm::vec4(0.85f, 0.85f, 0.90f, 1.0f), 0.0f);
    auto plane4 = std::make_unique<Plane>(50, 100, glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(-50.0f, 25.0f, 0.0f)), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)), glm::vec4(0.80f, 0.90f, 0.95f, 1.0f), 0.0f);
    auto plane5 = std::make_unique<Plane>(100, 50, glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 25.0f, 50.0f)), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec4(0.75f, 0.90f, 0.80f, 1.0f), 0.0f);
    auto plane6 = std::make_unique<Plane>(100, 50, glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 25.0f, -50.0f)), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec4(0.75f, 0.90f, 0.80f, 1.0f), 0.0f);

    auto cube1 = std::make_unique<Cube>(3, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 15.0f, 0.0f)), glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 10.0f);
    auto cube2 = std::make_unique<Cube>(3, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 15.0f, 0.0f)), glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 10.0f);
    auto cube3 = std::make_unique<Cube>(3, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 15.0f, 0.0f)), glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 10.0f);

    objects.push_back(std::move(ground));
    // objects.push_back(ceiling);
    objects.push_back(std::move(plane1));
    objects.push_back(std::move(plane2));
    objects.push_back(std::move(plane3));
    objects.push_back(std::move(plane4));
    objects.push_back(std::move(plane5));
    objects.push_back(std::move(plane6));
    objects.push_back(std::move(cube1));
    objects.push_back(std::move(cube2));
    objects.push_back(std::move(cube3));

    auto cow = std::make_unique<StaticObject>(cow_mesh_vertices, cow_mesh_vertex_count, cow_mesh_indices, cow_mesh_index_count, 3, glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 10.0f, 5.0f)), glm::vec4(0.8f, 0.6f, 0.5f, 1.0f), 1.0f);
    objects.push_back(std::move(cow));
}