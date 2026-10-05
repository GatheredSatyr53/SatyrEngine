#include "engine/Window.h"
#include "engine/gl.h"

#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstring>

namespace satyr {

namespace {
void glfwErrorCallback(int code, const char* description)
{
    std::fprintf(stderr, "[glfw] error %d: %s\n", code, description);
}
} // namespace

Window::~Window() { destroy(); }

bool Window::create(int width, int height, const std::string& title)
{
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        std::fprintf(stderr, "[window] glfwInit failed\n");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE); // required on macOS
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

    m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_window) {
        std::fprintf(stderr, "[window] glfwCreateWindow failed (is OpenGL 3.3 available?)\n");
        glfwTerminate();
        return false;
    }

    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, onKey);
    glfwSetMouseButtonCallback(m_window, onMouseButton);
    glfwSetCursorPosCallback(m_window, onCursorPos);
    glfwSetScrollCallback(m_window, onScroll);
    glfwSetFramebufferSizeCallback(m_window, onFramebufferSize);

    glfwMakeContextCurrent(m_window);
    setVsync(true);

    if (!gl::load(reinterpret_cast<gl::LoadProc>(glfwGetProcAddress))) {
        std::fprintf(stderr, "[window] failed to load OpenGL 3.3 functions\n");
        destroy();
        return false;
    }

    glfwGetFramebufferSize(m_window, &m_fbWidth, &m_fbHeight);
    glfwGetCursorPos(m_window, &m_input.mouseX, &m_input.mouseY);
    m_lastMouseX = m_input.mouseX;
    m_lastMouseY = m_input.mouseY;
    m_haveLastMouse = true;

    std::printf("[gl] vendor:   %s\n", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
    std::printf("[gl] renderer: %s\n", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    std::printf("[gl] version:  %s\n", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    std::printf("[gl] glsl:     %s\n", reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));
    return true;
}

void Window::destroy()
{
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
        glfwTerminate();
    }
}

bool Window::shouldClose() const { return !m_window || glfwWindowShouldClose(m_window); }
void Window::requestClose() { if (m_window) glfwSetWindowShouldClose(m_window, GLFW_TRUE); }

void Window::pollEvents()
{
    std::memset(m_input.keyPressed, 0, sizeof(m_input.keyPressed));
    std::memset(m_input.mousePressed, 0, sizeof(m_input.mousePressed));
    m_input.mouseDX = m_input.mouseDY = 0.0;
    m_input.scrollY = 0.0;
    glfwPollEvents();
}

void Window::swapBuffers() { glfwSwapBuffers(m_window); }

void Window::setTitle(const std::string& title) { glfwSetWindowTitle(m_window, title.c_str()); }

void Window::setVsync(bool enabled)
{
    m_vsync = enabled;
    glfwSwapInterval(enabled ? 1 : 0);
}

void Window::toggleFullscreen()
{
    if (!m_fullscreen) {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
        if (!mode) return;
        glfwGetWindowPos(m_window, &m_savedX, &m_savedY);
        glfwGetWindowSize(m_window, &m_savedW, &m_savedH);
        glfwSetWindowMonitor(m_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        m_fullscreen = true;
    } else {
        glfwSetWindowMonitor(m_window, nullptr, m_savedX, m_savedY, m_savedW, m_savedH, 0);
        m_fullscreen = false;
    }
    // Changing the monitor recreates the swap chain on some drivers; re-apply the interval.
    setVsync(m_vsync);
}

void Window::setCursorCaptured(bool captured)
{
    if (captured == m_cursorCaptured) return;
    m_cursorCaptured = captured;
    glfwSetInputMode(m_window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (captured && glfwRawMouseMotionSupported())
        glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    // The cursor position jumps when the mode changes; don't feed that jump into the camera.
    m_skipNextMouseDelta = true;
}

bool Window::consumeResize()
{
    const bool r = m_resized;
    m_resized = false;
    return r;
}

// ---- Callbacks -------------------------------------------------------------------------------

void Window::onKey(GLFWwindow* w, int key, int /*scancode*/, int action, int /*mods*/)
{
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
    if (!self || key < 0 || key >= InputState::kMaxKeys) return;
    if (action == GLFW_PRESS) {
        self->m_input.keyDown[key] = true;
        self->m_input.keyPressed[key] = true;
    } else if (action == GLFW_RELEASE) {
        self->m_input.keyDown[key] = false;
    }
}

void Window::onMouseButton(GLFWwindow* w, int button, int action, int /*mods*/)
{
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
    if (!self || button < 0 || button >= InputState::kMaxButtons) return;
    if (action == GLFW_PRESS) {
        self->m_input.mouseDown[button] = true;
        self->m_input.mousePressed[button] = true;
    } else if (action == GLFW_RELEASE) {
        self->m_input.mouseDown[button] = false;
    }
}

void Window::onCursorPos(GLFWwindow* w, double x, double y)
{
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
    if (!self) return;
    self->m_input.mouseX = x;
    self->m_input.mouseY = y;
    if (self->m_skipNextMouseDelta || !self->m_haveLastMouse) {
        self->m_skipNextMouseDelta = false;
        self->m_haveLastMouse = true;
    } else {
        self->m_input.mouseDX += x - self->m_lastMouseX;
        self->m_input.mouseDY += y - self->m_lastMouseY;
    }
    self->m_lastMouseX = x;
    self->m_lastMouseY = y;
}

void Window::onScroll(GLFWwindow* w, double /*dx*/, double dy)
{
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
    if (self) self->m_input.scrollY += dy;
}

void Window::onFramebufferSize(GLFWwindow* w, int width, int height)
{
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
    if (!self) return;
    self->m_fbWidth = width;
    self->m_fbHeight = height;
    self->m_resized = true;
}

} // namespace satyr
