#include "engine/Shader.h"

#include <cstdio>

namespace fs = std::filesystem;

namespace satyr {

Shader::Shader(const ShaderPreprocessor& preprocessor) : m_pp(preprocessor) {}

Shader::~Shader()
{
    if (m_program && glDeleteProgram) glDeleteProgram(m_program);
}

bool Shader::load(const fs::path& vertexFile, const fs::path& fragmentFile)
{
    m_vertexFile = vertexFile;
    m_fragmentFile = fragmentFile;
    return reload();
}

bool Shader::reload()
{
    m_lastError.clear();

    PreprocessedSource vs = m_pp.process(m_vertexFile, m_prelude);
    PreprocessedSource fsrc = m_pp.process(m_fragmentFile, m_prelude);
    trackDependencies({&vs, &fsrc});
    m_directives = fsrc.directives;

    if (!vs.ok() || !fsrc.ok()) {
        m_lastError = !vs.ok() ? vs.error : fsrc.error;
        if (!m_quiet) std::fprintf(stderr, "[shader] %s\n", m_lastError.c_str());
        return false;
    }
    if (m_transform) m_transform(fsrc);

    const GLuint vert = compileStage(GL_VERTEX_SHADER, vs, "vertex");
    if (!vert) return false;
    const GLuint frag = compileStage(GL_FRAGMENT_SHADER, fsrc, "fragment");
    if (!frag) {
        glDeleteShader(vert);
        return false;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);
    glDetachShader(program, vert);
    glDetachShader(program, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(len > 1 ? len : 1), '\0');
        glGetProgramInfoLog(program, len, nullptr, log.data());
        m_lastError = "link error in " + m_pp.displayName(m_fragmentFile) + ":\n" + m_pp.prettifyLog(log, fsrc.files);
        if (!m_quiet) std::fprintf(stderr, "[shader] %s\n", m_lastError.c_str());
        glDeleteProgram(program);
        return false;
    }

    if (m_program) glDeleteProgram(m_program);
    m_program = program;
    m_uniforms.clear();
    if (!m_quiet) std::printf("[shader] loaded %s\n", m_pp.displayName(m_fragmentFile).c_str());
    return true;
}

GLuint Shader::compileStage(GLenum type, const PreprocessedSource& src, const char* label)
{
    const GLuint shader = glCreateShader(type);
    const GLchar* text = src.code.c_str();
    const GLint length = static_cast<GLint>(src.code.size());
    glShaderSource(shader, 1, &text, &length);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(len > 1 ? len : 1), '\0');
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        const fs::path& file = type == GL_VERTEX_SHADER ? m_vertexFile : m_fragmentFile;
        m_lastError = std::string(label) + " shader error in " + m_pp.displayName(file) + ":\n"
                    + m_pp.prettifyLog(log, src.files);
        if (!m_quiet) std::fprintf(stderr, "[shader] %s\n", m_lastError.c_str());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void Shader::trackDependencies(const std::vector<PreprocessedSource*>& sources)
{
    m_deps.clear();
    m_depPaths.clear();
    auto add = [this](const fs::path& p) {
        for (const Dependency& d : m_deps)
            if (d.path == p) return;
        m_deps.push_back({p, mtimeOf(p)});
        m_depPaths.push_back(p);
    };
    for (const PreprocessedSource* s : sources) {
        for (const fs::path& p : s->files) add(p);
        for (const fs::path& p : s->missing) add(p);
    }
    m_dirty = false;
}

fs::file_time_type Shader::mtimeOf(const fs::path& p)
{
    std::error_code ec;
    const auto t = fs::last_write_time(p, ec);
    return ec ? fs::file_time_type{} : t;
}

bool Shader::pollHotReload(double now)
{
    if (now - m_lastCheck < 0.2) return false;
    m_lastCheck = now;

    bool changed = false;
    for (Dependency& d : m_deps) {
        const auto t = mtimeOf(d.path);
        if (t != d.mtime) {
            d.mtime = t;
            changed = true;
        }
    }

    // Wait one more poll after the last change so editors that write in several steps are done.
    if (changed) {
        m_dirty = true;
        return false;
    }
    if (m_dirty) {
        m_dirty = false;
        std::printf("[shader] change detected, reloading...\n");
        reload();
        return true;
    }
    return false;
}

void Shader::bind() const { glUseProgram(m_program); }

GLint Shader::uniform(const std::string& name)
{
    auto it = m_uniforms.find(name);
    if (it != m_uniforms.end()) return it->second;
    const GLint loc = m_program ? glGetUniformLocation(m_program, name.c_str()) : -1;
    m_uniforms.emplace(name, loc);
    return loc;
}

void Shader::set(const std::string& name, int v)          { glUniform1i(uniform(name), v); }
void Shader::set(const std::string& name, float v)        { glUniform1f(uniform(name), v); }
void Shader::set(const std::string& name, const vec2& v)  { glUniform2f(uniform(name), v.x, v.y); }
void Shader::set(const std::string& name, const vec3& v)  { glUniform3f(uniform(name), v.x, v.y, v.z); }
void Shader::set(const std::string& name, float x, float y, float z, float w) { glUniform4f(uniform(name), x, y, z, w); }
void Shader::set(const std::string& name, const mat3& m)  { glUniformMatrix3fv(uniform(name), 1, GL_FALSE, m.data()); }
void Shader::setVec4Array(const std::string& name, const float* values, int count)
{
    if (count > 0) glUniform4fv(uniform(name), count, values);
}

} // namespace satyr
