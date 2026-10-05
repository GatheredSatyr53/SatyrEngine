// Draws one fullscreen triangle per frame into a float offscreen target that is then blitted to
// the window. The target can be scaled (render at 50% for heavy fractals, 200% for supersampled
// shots) and can accumulate: with a blend weight below 1 the new frame is averaged into what is
// already there, which is how the engine converges anti-aliasing while the camera is still.
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

    // Binds the render target for this frame. `scale` is the render resolution relative to the
    // window framebuffer. `blendWeight` < 1 averages the new frame into the existing contents:
    // result = new * weight + old * (1 - weight).
    void beginFrame(int fbWidth, int fbHeight, float scale, float blendWeight = 1.0f);
    void drawFullscreen();
    // Resolves the offscreen target to the window.
    void endFrame();
    // Shows the existing target again without rendering (used once accumulation has converged).
    void present(int fbWidth, int fbHeight, float scale);

    int renderWidth() const  { return m_renderW; }
    int renderHeight() const { return m_renderH; }
    // True when the last beginFrame()/present() had to (re)create the target, i.e. its contents are undefined.
    bool targetWasRecreated() const { return m_recreated; }

    // Reads the default framebuffer as tightly packed RGB8, bottom row first (GL convention).
    bool readWindowPixels(std::vector<std::uint8_t>& rgb, int& width, int& height) const;

private:
    void setupFrame(int fbWidth, int fbHeight, float scale);
    void ensureTarget(int w, int h);
    void destroyTarget();

    GLuint m_vao = 0;
    GLuint m_fbo = 0;
    GLuint m_color = 0;
    int m_targetW = 0, m_targetH = 0;
    int m_fbW = 0, m_fbH = 0;
    int m_renderW = 0, m_renderH = 0;
    bool m_recreated = false;
};

} // namespace satyr
