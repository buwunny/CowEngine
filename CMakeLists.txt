cmake_minimum_required(VERSION 3.10)
project(CowEngine)

set(CMAKE_CXX_STANDARD 17)

# vcpkg support guidance
option(USE_VCPKG "Enable vcpkg usage (requires providing CMAKE_TOOLCHAIN_FILE at configure time)" ON)
if(USE_VCPKG)
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        message(FATAL_ERROR "USE_VCPKG is ON but CMAKE_TOOLCHAIN_FILE is not set. Configure CMake with -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake")
    endif()
    if(NOT CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
        message(WARNING "CMAKE_TOOLCHAIN_FILE does not look like a vcpkg toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
    endif()
    message(STATUS "vcpkg toolchain in use: ${CMAKE_TOOLCHAIN_FILE}")
    set(VCPKG_SUGGESTED_PACKAGES "glfw3 glm nlohmann-json bullet3 imgui glad")
    message(STATUS "If you haven't already, install dependencies with vcpkg: ${VCPKG_SUGGESTED_PACKAGES}")
endif()

set(SOURCES
    src/main.cpp
    src/Shader.cpp
    src/meshes/PlaneMesh.cpp
    src/meshes/CubeMesh.cpp
    src/meshes/CircleMesh.cpp
    src/meshes/StaticMesh.cpp
    src/meshes/AssetManager.cpp
    src/objects/Cube.cpp
    src/objects/StaticObject.cpp
    src/objects/Plane.cpp
    src/objects/Player.cpp
    src/Scene.cpp
    src/PhysicsWorld.cpp
    src/Camera.cpp
    src/InputHandler.cpp
    src/Window.cpp
)

add_executable(CowEngine ${SOURCES})

find_package(Bullet REQUIRED)
find_package(Bullet CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(imgui CONFIG REQUIRED)

# Link imported targets provided by vcpkg / config packages
target_link_libraries(CowEngine PRIVATE
    BulletDynamics
    BulletCollision
    LinearMath
    glad::glad
    glfw
    glm::glm
    OpenGL::GL
    nlohmann_json::nlohmann_json
    imgui::imgui
)

target_compile_definitions(CowEngine PRIVATE USE_IMGUI ASSET_ROOT=\"${CMAKE_SOURCE_DIR}\" GLM_ENABLE_EXPERIMENTAL)

target_include_directories(CowEngine PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(CowEngine PRIVATE USE_IMGUI ASSET_ROOT=\"${CMAKE_SOURCE_DIR}\" GLM_ENABLE_EXPERIMENTAL)

target_include_directories(CowEngine PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/glad/include ${BULLET_INCLUDE_DIRS} /usr/include/bullet)


add_custom_command(TARGET CowEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/src/shaders $<TARGET_FILE_DIR:CowEngine>/shaders
)