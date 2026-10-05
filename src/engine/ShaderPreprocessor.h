// Resolves `#include "file.glsl"` (relative to the including file, then to the shader root),
// supports `#pragma once`, hoists `#version` to the top and emits `#line` directives so
// driver error messages can be mapped back to real files. Lines of the form
// `#pragma satyr <text>` are stripped and collected so scenes can pass hints to the engine
// (e.g. `#pragma satyr camera pos=0,1,4 target=0,0,0`).
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace satyr {

struct PreprocessedSource {
    std::string code;                           // final GLSL handed to the driver
    std::vector<std::filesystem::path> files;   // index == source string number in #line
    std::vector<std::filesystem::path> missing; // includes that could not be opened
    std::vector<std::string> directives;        // text after "#pragma satyr", in file order
    std::string error;                          // non-empty when preprocessing failed

    bool ok() const { return error.empty(); }
};

class ShaderPreprocessor {
public:
    explicit ShaderPreprocessor(std::filesystem::path shaderRoot);

    const std::filesystem::path& root() const { return m_root; }

    // `prelude` is inserted right after the #version line (engine defines etc.).
    PreprocessedSource process(const std::filesystem::path& file, const std::string& prelude = {}) const;

    // Rewrites "0(12)" / "0:12" style locations in a driver info log into "file.glsl:12".
    std::string prettifyLog(const std::string& log, const std::vector<std::filesystem::path>& files) const;

    // Shortens a path to be relative to the shader root when possible (for logs/titles).
    std::string displayName(const std::filesystem::path& file) const;

private:
    struct Context;
    bool expand(const std::filesystem::path& file, int depth, Context& ctx) const;
    std::filesystem::path resolveInclude(const std::string& name, const std::filesystem::path& from) const;

    std::filesystem::path m_root;
};

} // namespace satyr
