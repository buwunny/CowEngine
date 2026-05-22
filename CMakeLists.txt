cmake_minimum_required(VERSION 3.10)
project(CowEngine)

set(CMAKE_CXX_STANDARD 17)

# FetchContent used to pull header-only dependencies when not available
include(FetchContent)

# vcpkg support guidance
option(USE_VCPKG "Enable vcpkg usage (requires providing CMAKE_TOOLCHAIN_FILE at configure time)" ON)
if(EMSCRIPTEN)
    # ensure vcpkg checks do not run for web builds
    set(USE_VCPKG OFF CACHE BOOL "Enable vcpkg usage (requires providing CMAKE_TOOLCHAIN_FILE at configure time)" FORCE)
endif()
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
    src/objects/Object.cpp
    src/objects/Cube.cpp
    src/objects/StaticObject.cpp
    src/objects/Plane.cpp
    src/objects/Player.cpp
    src/Scene.cpp
    src/PhysicsWorld.cpp
    src/Camera.cpp
    src/InputHandler.cpp
    src/Window.cpp
    src/Application.cpp
    src/EditorUI.cpp
    src/CodeEditor.cpp
    src/script/CowScript.cpp
    src/script/ScriptHost.cpp
)

# ImGui integration sources (present for both native and web builds)
list(APPEND SOURCES
    src/ImGuiLayer.cpp
    src/imgui_impl_emscripten.cpp
    
)

if(NOT EMSCRIPTEN)
    add_executable(CowEngine ${SOURCES})
endif()

if(NOT EMSCRIPTEN)
    find_package(Bullet REQUIRED)
    find_package(Bullet CONFIG REQUIRED)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(glad CONFIG REQUIRED)
    find_package(OpenGL REQUIRED)
endif()

# GLM and nlohmann_json: prefer package configs, but provide simple FetchContent
# fallbacks for web builds (or when the package isn't available).
find_package(glm CONFIG QUIET)
if(NOT glm_FOUND)
    message(STATUS "GLM package not found; will attempt to fetch via FetchContent")
    FetchContent_Declare(
        glm_repo
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(glm_repo)
endif()

find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json package not found; will attempt to fetch via FetchContent")
    FetchContent_Declare(
        nlohmann_json_repo
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
    )
    FetchContent_MakeAvailable(nlohmann_json_repo)
endif()

# Link imported targets provided by vcpkg / config packages
if(NOT EMSCRIPTEN)
    # Native ImGui docking build
    FetchContent_Declare(
        imgui_repo
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG docking
    )
    FetchContent_MakeAvailable(imgui_repo)

    add_library(imgui_native STATIC
        ${imgui_repo_SOURCE_DIR}/imgui.cpp
        ${imgui_repo_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_repo_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_repo_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_repo_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_repo_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui_native PUBLIC ${imgui_repo_SOURCE_DIR} ${imgui_repo_SOURCE_DIR}/backends)
    target_compile_definitions(imgui_native PRIVATE IMGUI_ENABLE_DOCKING)

    FetchContent_Declare(
        imguizmo_repo
        GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
        GIT_TAG master
    )
    FetchContent_GetProperties(imguizmo_repo)
    if(NOT imguizmo_repo_POPULATED)
        FetchContent_Populate(imguizmo_repo)
    endif()

    add_library(imguizmo_native STATIC ${imguizmo_repo_SOURCE_DIR}/src/ImGuizmo.cpp)
    target_include_directories(imguizmo_native PUBLIC ${imguizmo_repo_SOURCE_DIR} ${imguizmo_repo_SOURCE_DIR}/src ${imgui_repo_SOURCE_DIR})
    target_link_libraries(imguizmo_native PUBLIC imgui_native)
    target_compile_definitions(imguizmo_native PRIVATE IMGUI_ENABLE_DOCKING)

    target_link_libraries(CowEngine PRIVATE
        glad::glad
        glfw
        glm::glm
        OpenGL::GL
        nlohmann_json::nlohmann_json
        imgui_native
        imguizmo_native
        BulletDynamics
        BulletCollision
        LinearMath
    )
    target_compile_definitions(CowEngine PRIVATE USE_IMGUI ASSET_ROOT="${CMAKE_SOURCE_DIR}" GLM_ENABLE_EXPERIMENTAL IMGUI_ENABLE_DOCKING)
    target_include_directories(CowEngine PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/glad/include ${BULLET_INCLUDE_DIRS} /usr/include/bullet ${imguizmo_repo_SOURCE_DIR})

    add_custom_command(TARGET CowEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/src/shaders $<TARGET_FILE_DIR:CowEngine>/shaders
    )
else()
    # Web build: only link header-only / portable libs; concrete JS/Web APIs (Ammo.js, ImGui port) handled at runtime
    # Create a minimal CowEngine target alias for common includes if needed
    # (CowEngine_web will be the actual web executable)
    add_library(cowengine_common INTERFACE)
    target_link_libraries(cowengine_common INTERFACE glm::glm nlohmann_json::nlohmann_json)
    # make sure web target can see glad headers if present locally
endif()

# Web (Emscripten) target --------------------------------------------------
if(EMSCRIPTEN)
    # Use the same source list for web builds (unified codebase)
    set(WEB_SOURCES ${SOURCES})


    # Ensure Bullet is available for web builds by fetching and building it
    # under the Emscripten toolchain so the same physics API works on web.
    # Configure a minimal Bullet build suitable for linking into the web target.
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "" FORCE)
    set(BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
    set(BUILD_CPU_DEMOS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        bullet_repo
        GIT_REPOSITORY https://github.com/bulletphysics/bullet3.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(bullet_repo)

    # Fetch Dear ImGui for web builds and build it into the wasm
    FetchContent_Declare(
        imgui_repo
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG docking
    )
    FetchContent_MakeAvailable(imgui_repo)

    add_executable(CowEngine_web ${WEB_SOURCES})

    # Keep most headers/includes and definitions consistent
    target_include_directories(CowEngine_web PRIVATE ${CMAKE_SOURCE_DIR}/include ${bullet_repo_SOURCE_DIR}/src)
    target_include_directories(CowEngine_web PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_compile_definitions(CowEngine_web PRIVATE ASSET_ROOT="${CMAKE_SOURCE_DIR}" GLM_ENABLE_EXPERIMENTAL EMSCRIPTEN IMGUI_ENABLE_DOCKING)

    # Link libraries including Bullet built for EMSCRIPTEN so we use the
    # same native physics API on web and desktop.
    target_link_libraries(CowEngine_web PRIVATE
        glm::glm
        nlohmann_json::nlohmann_json
        BulletDynamics
        BulletCollision
        LinearMath
    )

    # Emscripten linker flags: WebGL2, memory growth and preload assets
    set(WEB_LINK_FLAGS "-s USE_WEBGL2=1 -s ALLOW_MEMORY_GROWTH=1 -s ASSERTIONS=1 -s USE_GLFW=3")
    # Export `callMain` so start.html can safely call Module.callMain()
    list(APPEND WEB_LINK_FLAGS "-sEXPORTED_RUNTIME_METHODS=[\"callMain\"]")
    list(APPEND WEB_LINK_FLAGS "--preload-file" "${CMAKE_SOURCE_DIR}/scenes@/scenes")
    list(APPEND WEB_LINK_FLAGS "--preload-file" "${CMAKE_SOURCE_DIR}/src/shaders@/shaders")
    list(APPEND WEB_LINK_FLAGS "--preload-file" "${CMAKE_SOURCE_DIR}/models@/models")
    list(APPEND WEB_LINK_FLAGS "--preload-file" "${CMAKE_SOURCE_DIR}/icons/png@/icons/png")
    list(APPEND WEB_LINK_FLAGS "--preload-file" "${CMAKE_SOURCE_DIR}/scripts/cow@/scripts/cow")

    # Join flags into a single string and attach to target
    string(JOIN " " WEB_LINK_FLAGS_STR ${WEB_LINK_FLAGS})
    set_target_properties(CowEngine_web PROPERTIES
        LINK_FLAGS "${WEB_LINK_FLAGS_STR}"
        OUTPUT_NAME "CowEngine"
        SUFFIX ".html"
    )

    # Build a minimal imgui static library from the fetched repo so we can
    # link it into the web target. We include the core sources and the
    # OpenGL3 backend for rendering. Input is provided by a small
    # imgui_impl_emscripten platform file implemented in src/.
    add_library(imgui_web STATIC
        ${imgui_repo_SOURCE_DIR}/imgui.cpp
        ${imgui_repo_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_repo_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_repo_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_repo_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui_web PUBLIC ${imgui_repo_SOURCE_DIR} ${imgui_repo_SOURCE_DIR}/backends)
    target_compile_definitions(imgui_web PRIVATE IMGUI_ENABLE_DOCKING)

    FetchContent_Declare(
        imguizmo_repo
        GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
        GIT_TAG master
    )
    FetchContent_GetProperties(imguizmo_repo)
    if(NOT imguizmo_repo_POPULATED)
        FetchContent_Populate(imguizmo_repo)
    endif()

    add_library(imguizmo_web STATIC ${imguizmo_repo_SOURCE_DIR}/src/ImGuizmo.cpp)
    target_include_directories(imguizmo_web PUBLIC ${imguizmo_repo_SOURCE_DIR} ${imguizmo_repo_SOURCE_DIR}/src ${imgui_repo_SOURCE_DIR})
    target_link_libraries(imguizmo_web PUBLIC imgui_web)
    target_compile_definitions(imguizmo_web PRIVATE IMGUI_ENABLE_DOCKING)

    # Link imgui + imguizmo into the web executable and expose USE_IMGUI to compile-time
    target_link_libraries(CowEngine_web PRIVATE imgui_web imguizmo_web)
    target_compile_definitions(CowEngine_web PRIVATE USE_IMGUI)
    target_include_directories(CowEngine_web PRIVATE ${imguizmo_repo_SOURCE_DIR} ${imguizmo_repo_SOURCE_DIR}/src)

    # Do not attempt to copy shaders at post-build for the web target (preloaded instead)
    # Copy our custom HTML shell into the web output so Pages serves our UI as index.html
    set(CUSTOM_SHELL_SOURCE "${CMAKE_SOURCE_DIR}/src/PlaygroundTemplate.html")
    add_custom_command(TARGET CowEngine_web POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CUSTOM_SHELL_SOURCE} $<TARGET_FILE_DIR:CowEngine_web>/index.html
        COMMAND ${CMAKE_COMMAND} -E copy ${CUSTOM_SHELL_SOURCE} $<TARGET_FILE_DIR:CowEngine_web>/CowEngine.html
    )
endif()
