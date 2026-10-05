#include "engine/Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace satyr {

Renderer::~Renderer() { shutdown(); }

bool Renderer::init()
{
    // Core profile requires a VAO to be bound even when no vertex attributes are used; the
    // vertex shader derives positions from gl_VertexID.
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_DITHER);
    return gl::checkErrors("Renderer::init");
}

void Renderer::shutdown()
{
    if (!glDeleteVertexArrays) return;
    destroyTarget();
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
}

void Renderer::beginFrame(int fbWidth, int fbHeight, float scale)
{
    m_fbW = std::max(fbWidth, 1);
    m_fbH = std::max(fbHeight, 1);
    const bool nativeScale = std::fabs(scale - 1.0f) < 1e-4f;
    m_renderW = nativeScale ? m_fbW : std::max(1, static_cast<int>(std::lround(m_fbW * scale)));
    m_renderH = nativeScale ? m_fbH : std::max(1, static_cast<int>(std::lround(m_fbH * scale)));
    m_offscreen = !nativeScale;

    if (m_offscreen) {
        ensureTarget(m_renderW, m_renderH);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    glViewport(0, 0, m_renderW, m_renderH);
    glBindVertexArray(m_vao);
}

void Renderer::drawFullscreen() { glDrawArrays(GL_TRIANGLES, 0, 3); }

void Renderer::endFrame()
{
    if (!m_offscreen) return;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, m_renderW, m_renderH, 0, 0, m_fbW, m_fbH, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool Renderer::readWindowPixels(std::vector<std::uint8_t>& rgb, int& width, int& height) const
{
    width = m_fbW;
    height = m_fbH;
    rgb.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 3);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    return gl::checkErrors("Renderer::readWindowPixels");
}

void Renderer::ensureTarget(int w, int h)
{
    if (m_fbo && w == m_targetW && h == m_targetH) return;
    destroyTarget();

    glGenTextures(1, &m_color);
    glBindTexture(GL_TEXTURE_2D, m_color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_color, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::fprintf(stderr, "[renderer] offscreen framebuffer %dx%d is incomplete\n", w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_targetW = w;
    m_targetH = h;
}

void Renderer::destroyTarget()
{
    if (m_fbo) { glDeleteFramebuffers(1, &m_fbo); m_fbo = 0; }
    if (m_color) { glDeleteTextures(1, &m_color); m_color = 0; }
    m_targetW = m_targetH = 0;
}

} // namespace satyr
