// GLFW window + OpenGL context + per-frame input snapshot.
#pragma once

#include <string>

struct GLFWwindow;

namespace satyr {

struct InputState {
    static constexpr int kMaxKeys = 512;
    static constexpr int kMaxButtons = 8;

    bool keyDown[kMaxKeys] = {};        // held this frame
    bool keyPressed[kMaxKeys] = {};     // went down this frame
    bool mouseDown[kMaxButtons] = {};
    bool mousePressed[kMaxButtons] = {};
    double mouseX = 0, mouseY = 0;      // window coordinates, origin top-left
    double mouseDX = 0, mouseDY = 0;    // movement since last frame
    double scrollY = 0;                 // wheel movement this frame

    bool key(int k) const       { return k >= 0 && k < kMaxKeys && keyDown[k]; }
    bool pressed(int k) const   { return k >= 0 && k < kMaxKeys && keyPressed[k]; }
    bool mouse(int b) const     { return b >= 0 && b < kMaxButtons && mouseDown[b]; }
    bool mouseClick(int b) const{ return b >= 0 && b < kMaxButtons && mousePressed[b]; }
};

class Window {
public:
    Window() = default;
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Creates the window with a 3.3 core context and loads GL function pointers.
    bool create(int width, int height, const std::string& title);
    void destroy();

    bool shouldClose() const;
    void requestClose();

    // Clears per-frame input state and processes OS events.
    void pollEvents();
    void swapBuffers();

    void setTitle(const std::string& title);
    void setVsync(bool enabled);
    bool vsync() const { return m_vsync; }
    void toggleFullscreen();

    // Hide the cursor and provide unbounded motion (for mouse look).
    void setCursorCaptured(bool captured);
    bool cursorCaptured() const { return m_cursorCaptured; }

    int framebufferWidth() const  { return m_fbWidth; }
    int framebufferHeight() const { return m_fbHeight; }
    // True once after the framebuffer size changed; the flag is cleared by the call.
    bool consumeResize();

    const InputState& input() const { return m_input; }
    GLFWwindow* handle() const { return m_window; }

private:
    static void onKey(GLFWwindow* w, int key, int scancode, int action, int mods);
    static void onMouseButton(GLFWwindow* w, int button, int action, int mods);
    static void onCursorPos(GLFWwindow* w, double x, double y);
    static void onScroll(GLFWwindow* w, double dx, double dy);
    static void onFramebufferSize(GLFWwindow* w, int width, int height);

    GLFWwindow* m_window = nullptr;
    InputState m_input;
    int m_fbWidth = 0, m_fbHeight = 0;
    bool m_resized = false;
    bool m_vsync = true;
    bool m_cursorCaptured = false;
    bool m_skipNextMouseDelta = false;
    bool m_haveLastMouse = false;
    double m_lastMouseX = 0, m_lastMouseY = 0;

    // Saved windowed geometry for fullscreen toggling.
    bool m_fullscreen = false;
    int m_savedX = 0, m_savedY = 0, m_savedW = 0, m_savedH = 0;
};

} // namespace satyr
