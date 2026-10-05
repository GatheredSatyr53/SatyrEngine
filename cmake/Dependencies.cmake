# Third-party dependencies. The only one is GLFW (window + OpenGL context + input).
# OpenGL functions themselves are loaded by src/engine/gl.cpp, so no GLAD/GLEW is needed.

include(FetchContent)

if(SATYR_FETCH_GLFW)
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
    if(UNIX AND NOT APPLE)
        set(GLFW_BUILD_X11     ON                   CACHE BOOL "" FORCE)
        set(GLFW_BUILD_WAYLAND ${SATYR_GLFW_WAYLAND} CACHE BOOL "" FORCE)
    endif()

    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(glfw)
else()
    find_package(glfw3 3.3 REQUIRED)
endif()
