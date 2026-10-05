// A vertex+fragment program with uniform caching and file-watching hot reload.
#pragma once

#include "engine/Math.h"
#include "engine/ShaderPreprocessor.h"
#include "engine/gl.h"

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace satyr {

class Shader {
public:
    // Applied to the preprocessed fragment source before compilation (see SdfProbe).
    using SourceTransform = std::function<void(PreprocessedSource&)>;

    explicit Shader(const ShaderPreprocessor& preprocessor);
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // Text inserted after #version in both stages; takes effect on the next load()/reload().
    void setPrelude(std::string prelude) { m_prelude = std::move(prelude); }
    void setFragmentTransform(SourceTransform transform) { m_transform = std::move(transform); }
    // Quiet shaders only record errors in lastError() instead of printing them.
    void setQuiet(bool quiet) { m_quiet = quiet; }

    // Compiles and links. On failure the previous program (if any) is kept and lastError() is set.
    bool load(const std::filesystem::path& vertexFile, const std::filesystem::path& fragmentFile);
    bool reload();

    // Rechecks source timestamps a few times per second and recompiles when a file changed.
    // Returns true when a recompilation was attempted this call (check valid()/lastError()).
    bool pollHotReload(double now);

    bool valid() const { return m_program != 0; }
    void bind() const;

    const std::string& lastError() const { return m_lastError; }
    const std::filesystem::path& fragmentFile() const { return m_fragmentFile; }
    const std::vector<std::filesystem::path>& dependencies() const { return m_depPaths; }
    // "#pragma satyr ..." lines from the fragment shader, available even if it failed to compile.
    const std::vector<std::string>& directives() const { return m_directives; }

    GLint uniform(const std::string& name);
    void set(const std::string& name, int v);
    void set(const std::string& name, float v);
    void set(const std::string& name, const vec2& v);
    void set(const std::string& name, const vec3& v);
    void set(const std::string& name, float x, float y, float z, float w);
    void set(const std::string& name, const mat3& m);
    // vec4 array: `values` holds 4 * count floats; name the first element, e.g. "uBodies[0]".
    void setVec4Array(const std::string& name, const float* values, int count);

private:
    struct Dependency {
        std::filesystem::path path;
        std::filesystem::file_time_type mtime{};
    };

    GLuint compileStage(GLenum type, const PreprocessedSource& src, const char* label);
    void trackDependencies(const std::vector<PreprocessedSource*>& sources);
    static std::filesystem::file_time_type mtimeOf(const std::filesystem::path& p);

    const ShaderPreprocessor& m_pp;
    std::string m_prelude = "#define SATYR_ENGINE 1\n";
    SourceTransform m_transform;
    bool m_quiet = false;
    std::filesystem::path m_vertexFile;
    std::filesystem::path m_fragmentFile;
    GLuint m_program = 0;
    std::unordered_map<std::string, GLint> m_uniforms;
    std::vector<Dependency> m_deps;
    std::vector<std::filesystem::path> m_depPaths;
    std::vector<std::string> m_directives;
    std::string m_lastError;
    double m_lastCheck = 0.0;
    bool m_dirty = false;
};

} // namespace satyr
