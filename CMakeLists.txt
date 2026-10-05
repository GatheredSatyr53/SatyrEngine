cmake_minimum_required(VERSION 3.16)
project(SatyrEngine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SATYR_FETCH_GLFW   "Download and build GLFW with FetchContent instead of using a system install" ON)
option(SATYR_GLFW_WAYLAND "Build GLFW with native Wayland support (needs wayland/xkbcommon dev packages)" OFF)

include(cmake/Dependencies.cmake)

add_executable(satyr
    src/main.cpp
    src/engine/gl.cpp
    src/engine/Window.cpp
    src/engine/Shader.cpp
    src/engine/ShaderPreprocessor.cpp
    src/engine/Camera.cpp
    src/engine/Renderer.cpp
    src/engine/Image.cpp
    src/engine/SceneList.cpp
    src/engine/Physics.cpp
    src/engine/SdfProbe.cpp
)

target_include_directories(satyr PRIVATE src)
target_link_libraries(satyr PRIVATE glfw)

# Shaders are loaded straight from the source tree so hot reload picks up your edits.
target_compile_definitions(satyr PRIVATE
    SATYR_SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders"
    GLFW_INCLUDE_NONE
)

if(MSVC)
    target_compile_options(satyr PRIVATE /W4 /permissive-)
    target_compile_definitions(satyr PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    target_compile_options(satyr PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(satyr PRIVATE stdc++fs)
endif()

set_target_properties(satyr PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
