// Draws one fullscreen triangle per frame, optionally into a scaled offscreen target that is
// then blitted to the window (render at 50% for heavy fractals, or 200% for supersampled shots).
#pragma once

#include "engine/gl.h"

#include <cstdint>
#include <vector>

namespace satyr {

class Renderer {
public:
    Renderer() = default;
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool init();
    void shutdown();

    // Binds the render target for this frame. `scale` is the render resolution relative to
    // the window framebuffer; 1.0 draws straight into the default framebuffer.
    void beginFrame(int fbWidth, int fbHeight, float scale);
    void drawFullscreen();
    // Resolves the offscreen target to the window (no-op when scale == 1).
    void endFrame();

    int renderWidth() const  { return m_renderW; }
    int renderHeight() const { return m_renderH; }

    // Reads the default framebuffer as tightly packed RGB8, bottom row first (GL convention).
    bool readWindowPixels(std::vector<std::uint8_t>& rgb, int& width, int& height) const;

private:
    void ensureTarget(int w, int h);
    void destroyTarget();

    GLuint m_vao = 0;
    GLuint m_fbo = 0;
    GLuint m_color = 0;
    int m_targetW = 0, m_targetH = 0;
    int m_fbW = 0, m_fbH = 0;
    int m_renderW = 0, m_renderH = 0;
    bool m_offscreen = false;
};

} // namespace satyr
