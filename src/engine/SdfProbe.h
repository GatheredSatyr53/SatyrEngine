// Evaluates the scene's distance field on the GPU for a handful of points (the physics bodies).
//
// The probe compiles the scene's fragment shader a second time with SATYR_QUERY_PASS defined
// and its main() replaced: each pixel of a small float render target (query i lives at pixel
// (i % width, i / width)) samples map() and a finite-difference normal for one query point.
// Results are read back through a pixel buffer object one frame later, so the CPU never stalls
// on the GPU.
#pragma once

#include "engine/Physics.h"
#include "engine/Shader.h"
#include "engine/gl.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace satyr {

class ShaderPreprocessor;

class SdfProbe {
public:
    SdfProbe(const ShaderPreprocessor& preprocessor, int capacity);
    ~SdfProbe();
    SdfProbe(const SdfProbe&) = delete;
    SdfProbe& operator=(const SdfProbe&) = delete;

    bool init();

    // Builds the query program for a scene. `prelude` is the same prelude the main shader uses.
    bool load(const std::filesystem::path& vertexFile, const std::filesystem::path& sceneFile, const std::string& prelude);
    bool reload();
    bool valid() const { return m_shader.valid(); }
    const std::string& lastError() const { return m_shader.lastError(); }

    // Issues the GPU query. `setUniforms` uploads the scene uniforms (time, camera, ...) so an
    // animated map() is sampled at the same moment the frame is rendered.
    void submit(const std::vector<vec3>& points, const std::function<void(Shader&)>& setUniforms);
    // Collects the samples of the previous submit() (same order). Returns false if none pending.
    bool fetch(std::vector<SurfaceSample>& out);

    int capacity() const { return m_capacity; }

private:
    void destroy();

    Shader m_shader;
    int m_capacity = 0;
    int m_width = 0;      // texels per row of the query target
    int m_rows = 0;
    int m_pending = 0;
    int m_pendingRows = 0;
    GLuint m_vao = 0;
    GLuint m_inputTex = 0;
    GLuint m_outputTex = 0;
    GLuint m_fbo = 0;
    GLuint m_pbo = 0;
    std::vector<float> m_upload;
};

} // namespace satyr
