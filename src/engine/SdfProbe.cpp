#include "engine/SdfProbe.h"
#include "engine/ShaderPreprocessor.h"

#include <algorithm>
#include <cstdio>
#include <regex>

namespace fs = std::filesystem;

namespace satyr {

namespace {

// Replaces the scene's main() with one that samples map() for the query point of this pixel.
const char* kQueryMain = R"GLSL(
uniform sampler2D uSatyrQueryPoints;
layout(location = 0) out vec4 satyrQueryOut;

void main()
{
    vec3 p = texelFetch(uSatyrQueryPoints, ivec2(int(gl_FragCoord.x), 0), 0).xyz;
    const vec2 e = vec2(0.001, 0.0);
    float d = map(p).x;
    vec3 n = vec3(map(p + e.xyy).x - map(p - e.xyy).x,
                  map(p + e.yxy).x - map(p - e.yxy).x,
                  map(p + e.yyx).x - map(p - e.yyx).x);
    float len = length(n);
    n = len > 0.0 ? n / len : vec3(0.0, 1.0, 0.0);
    satyrQueryOut = vec4(n, d);
}
)GLSL";

void makeQueryPass(PreprocessedSource& src)
{
    // The scene's main() becomes an ordinary (unused) function...
    static const std::regex mainRe(R"(\bvoid\s+main\s*\(\s*(?:void)?\s*\))");
    src.code = std::regex_replace(src.code, mainRe, "void satyrSceneMain()");
    // ...and its fragment outputs become plain globals, leaving satyrQueryOut as the only output.
    static const std::regex outRe(R"((?:layout\s*\([^)]*\)\s*)?\bout\s+(float|vec[234])\s+(\w+)\s*;)");
    src.code = std::regex_replace(src.code, outRe, "$1 $2;");

    const int index = static_cast<int>(src.files.size());
    src.files.push_back("<satyr query pass>");
    src.code += "\n#line 1 " + std::to_string(index) + "\n";
    src.code += kQueryMain;
}

} // namespace

SdfProbe::SdfProbe(const ShaderPreprocessor& preprocessor, int capacity)
    : m_shader(preprocessor), m_capacity(std::max(capacity, 1))
{
    m_shader.setQuiet(true);
    m_shader.setFragmentTransform(makeQueryPass);
}

SdfProbe::~SdfProbe() { destroy(); }

bool SdfProbe::init()
{
    glGenVertexArrays(1, &m_vao);

    auto makeTexture = [this](GLuint& tex) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_capacity, 1, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };
    makeTexture(m_inputTex);
    makeTexture(m_outputTex);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_outputTex, 0);
    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!complete) std::fprintf(stderr, "[probe] float framebuffer is incomplete; physics disabled\n");

    glGenBuffers(1, &m_pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, static_cast<GLsizeiptr>(m_capacity) * 4 * sizeof(float), nullptr, GL_STREAM_READ);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    m_upload.resize(static_cast<size_t>(m_capacity) * 4);
    return complete && gl::checkErrors("SdfProbe::init");
}

void SdfProbe::destroy()
{
    if (!glDeleteBuffers) return;
    if (m_pbo) { glDeleteBuffers(1, &m_pbo); m_pbo = 0; }
    if (m_fbo) { glDeleteFramebuffers(1, &m_fbo); m_fbo = 0; }
    if (m_inputTex) { glDeleteTextures(1, &m_inputTex); m_inputTex = 0; }
    if (m_outputTex) { glDeleteTextures(1, &m_outputTex); m_outputTex = 0; }
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
}

bool SdfProbe::load(const fs::path& vertexFile, const fs::path& sceneFile, const std::string& prelude)
{
    m_shader.setPrelude(prelude + "#define SATYR_QUERY_PASS 1\n");
    m_pending = 0;
    return m_shader.load(vertexFile, sceneFile);
}

bool SdfProbe::reload()
{
    m_pending = 0;
    return m_shader.reload();
}

void SdfProbe::submit(const std::vector<vec3>& points, const std::function<void(Shader&)>& setUniforms)
{
    const int n = static_cast<int>(std::min<size_t>(points.size(), static_cast<size_t>(m_capacity)));
    m_pending = 0;
    if (n == 0 || !m_shader.valid() || !m_fbo) return;

    for (int i = 0; i < n; ++i) {
        m_upload[static_cast<size_t>(i) * 4 + 0] = points[static_cast<size_t>(i)].x;
        m_upload[static_cast<size_t>(i) * 4 + 1] = points[static_cast<size_t>(i)].y;
        m_upload[static_cast<size_t>(i) * 4 + 2] = points[static_cast<size_t>(i)].z;
        m_upload[static_cast<size_t>(i) * 4 + 3] = 0.0f;
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_inputTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, n, 1, GL_RGBA, GL_FLOAT, m_upload.data());

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, n, 1);
    m_shader.bind();
    if (setUniforms) setUniforms(m_shader);
    m_shader.set("uSatyrQueryPoints", 0);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Asynchronous readback: the copy into the PBO completes on the GPU's own schedule.
    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbo);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, n, 1, GL_RGBA, GL_FLOAT, nullptr);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_pending = n;
}

bool SdfProbe::fetch(std::vector<SurfaceSample>& out)
{
    out.clear();
    if (m_pending == 0) return false;

    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbo);
    const auto* data = static_cast<const float*>(
        glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, static_cast<GLsizeiptr>(m_pending) * 4 * sizeof(float), GL_MAP_READ_BIT));
    if (data) {
        out.resize(static_cast<size_t>(m_pending));
        for (int i = 0; i < m_pending; ++i) {
            const float* v = data + static_cast<size_t>(i) * 4;
            SurfaceSample& s = out[static_cast<size_t>(i)];
            s.normal = vec3(v[0], v[1], v[2]);
            s.distance = v[3];
            s.valid = true;
        }
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    m_pending = 0;
    return !out.empty();
}

} // namespace satyr
