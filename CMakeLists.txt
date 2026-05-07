cmake_minimum_required(VERSION 3.10)
project(OpenGLProject)

set(CMAKE_CXX_STANDARD 17)

set(SOURCES
    src/main.cpp
    src/Shader.cpp
    src/meshes/PlaneMesh.cpp
    src/meshes/CubeMesh.cpp
    src/meshes/CircleMesh.cpp
    src/objects/Cube.cpp
    src/objects/Plane.cpp
    src/objects/Player.cpp
    src/rooms/BasicRoom.cpp
    src/Camera.cpp
    src/InputHandler.cpp
    src/Window.cpp
)

add_executable(OpenGLProject ${SOURCES})

if(WIN32)
    set(CMAKE_PREFIX_PATH "C:/msys64/mingw64")
endif()

find_package(Bullet REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
if(EXISTS ${CMAKE_SOURCE_DIR}/glad/include/glad/glad.h AND EXISTS ${CMAKE_SOURCE_DIR}/glad/src/glad.c)
    add_library(glad STATIC ${CMAKE_SOURCE_DIR}/glad/src/glad.c)
    target_include_directories(glad PUBLIC ${CMAKE_SOURCE_DIR}/glad/include)
else()
    find_package(glad CONFIG REQUIRED)
endif()
find_package(OpenGL REQUIRED)

target_include_directories(OpenGLProject PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/glad/include ${BULLET_INCLUDE_DIRS} /usr/include/bullet)

target_link_libraries(OpenGLProject PRIVATE ${BULLET_LIBRARIES} glad glfw glm OpenGL::GL)


add_custom_command(TARGET OpenGLProject POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/src/shaders $<TARGET_FILE_DIR:OpenGLProject>/shaders
)