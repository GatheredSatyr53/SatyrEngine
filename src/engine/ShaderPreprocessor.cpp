#include "engine/ShaderPreprocessor.h"

#include <algorithm>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace satyr {

namespace {

std::string ltrim(const std::string& s)
{
    const size_t i = s.find_first_not_of(" \t\r");
    return i == std::string::npos ? std::string{} : s.substr(i);
}

bool startsWith(const std::string& s, const char* prefix)
{
    return s.compare(0, std::char_traits<char>::length(prefix), prefix) == 0;
}

// Parses `#include "name"` or `#include <name>`; returns false if the line is not an include.
bool parseInclude(const std::string& trimmed, std::string& outName)
{
    if (!startsWith(trimmed, "#include")) return false;
    const size_t open = trimmed.find_first_of("\"<", 8);
    if (open == std::string::npos) return false;
    const char closeCh = trimmed[open] == '"' ? '"' : '>';
    const size_t close = trimmed.find(closeCh, open + 1);
    if (close == std::string::npos) return false;
    outName = trimmed.substr(open + 1, close - open - 1);
    return !outName.empty();
}

} // namespace

struct ShaderPreprocessor::Context {
    PreprocessedSource* out = nullptr;
    std::string versionLine;
    std::vector<fs::path> includeStack;
    std::set<fs::path> onceFiles;
};

ShaderPreprocessor::ShaderPreprocessor(fs::path shaderRoot) : m_root(std::move(shaderRoot)) {}

PreprocessedSource ShaderPreprocessor::process(const fs::path& file, const std::string& prelude) const
{
    PreprocessedSource result;
    Context ctx;
    ctx.out = &result;

    std::string body;
    {
        PreprocessedSource bodyOut;
        ctx.out = &bodyOut;
        if (!expand(file, 0, ctx)) {
            result.error = bodyOut.error;
            result.files = std::move(bodyOut.files);
            result.missing = std::move(bodyOut.missing);
            return result;
        }
        body = std::move(bodyOut.code);
        result.files = std::move(bodyOut.files);
        result.missing = std::move(bodyOut.missing);
        result.directives = std::move(bodyOut.directives);
    }

    std::string code = ctx.versionLine.empty() ? std::string("#version 330 core") : ctx.versionLine;
    code += "\n";
    if (!prelude.empty()) {
        code += prelude;
        if (prelude.back() != '\n') code += "\n";
    }
    code += body;
    result.code = std::move(code);
    return result;
}

bool ShaderPreprocessor::expand(const fs::path& file, int depth, Context& ctx) const
{
    PreprocessedSource& out = *ctx.out;

    if (depth > 32) {
        out.error = "include depth exceeded at " + displayName(file);
        return false;
    }
    if (std::find(ctx.includeStack.begin(), ctx.includeStack.end(), file) != ctx.includeStack.end()) {
        out.error = "cyclic include of " + displayName(file);
        return false;
    }

    std::ifstream in(file, std::ios::binary);
    if (!in) {
        out.missing.push_back(file);
        out.error = "cannot open " + displayName(file);
        return false;
    }

    const int index = static_cast<int>(out.files.size());
    out.files.push_back(file);
    ctx.includeStack.push_back(file);

    out.code += "#line 1 " + std::to_string(index) + "\n";

    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::string trimmed = ltrim(line);

        std::string includeName;
        if (startsWith(trimmed, "#version")) {
            // Only the first #version encountered (normally the root file's) survives; it is
            // hoisted to the very top because GLSL demands it be the first line.
            if (ctx.versionLine.empty()) ctx.versionLine = trimmed;
            out.code += "// " + trimmed + "\n";
        } else if (startsWith(trimmed, "#pragma once")) {
            ctx.onceFiles.insert(file);
            out.code += "// #pragma once\n";
        } else if (startsWith(trimmed, "#pragma satyr")) {
            const std::string text = ltrim(trimmed.substr(13));
            if (!text.empty()) out.directives.push_back(text);
            out.code += "// " + trimmed + "\n";
        } else if (parseInclude(trimmed, includeName)) {
            const fs::path target = resolveInclude(includeName, file);
            if (ctx.onceFiles.count(target)) {
                out.code += "// #include \"" + includeName + "\" (already included)\n";
            } else {
                out.code += "// begin " + displayName(target) + "\n";
                if (!expand(target, depth + 1, ctx)) {
                    if (out.error.find(" (included from ") == std::string::npos)
                        out.error += " (included from " + displayName(file) + ":" + std::to_string(lineNo) + ")";
                    ctx.includeStack.pop_back();
                    return false;
                }
                out.code += "#line " + std::to_string(lineNo + 1) + " " + std::to_string(index) + "\n";
            }
        } else {
            out.code += line + "\n";
        }
    }

    ctx.includeStack.pop_back();
    return true;
}

fs::path ShaderPreprocessor::resolveInclude(const std::string& name, const fs::path& from) const
{
    const fs::path rel(name);
    if (rel.is_absolute()) return rel.lexically_normal();

    std::error_code ec;
    const fs::path nearby = (from.parent_path() / rel).lexically_normal();
    if (fs::exists(nearby, ec)) return nearby;

    return (m_root / rel).lexically_normal();
}

std::string ShaderPreprocessor::prettifyLog(const std::string& log, const std::vector<fs::path>& files) const
{
    // Matches the leading "<string>(<line>)" (NVIDIA) and "<string>:<line>" (Mesa, AMD, Intel) forms.
    static const std::regex locRe(R"(\b(\d+)([:(])(\d+))");

    std::ostringstream result;
    std::istringstream in(log);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::smatch m;
        if (std::regex_search(line, m, locRe)) {
            const size_t fileIndex = static_cast<size_t>(std::stoul(m[1].str()));
            if (fileIndex < files.size()) {
                const std::string replacement = displayName(files[fileIndex]) + ":" + m[3].str();
                const std::string tail = line.substr(static_cast<size_t>(m.position(0) + m.length(0)));
                line = line.substr(0, static_cast<size_t>(m.position(0))) + replacement
                     + (m[2].str() == "(" && !tail.empty() && tail[0] == ')' ? tail.substr(1) : tail);
            }
        }
        result << "    " << line << "\n";
    }
    return result.str();
}

std::string ShaderPreprocessor::displayName(const fs::path& file) const
{
    std::error_code ec;
    const fs::path rel = fs::relative(file, m_root, ec);
    if (ec || rel.empty() || rel.native().rfind("..", 0) == 0) return file.generic_string();
    return rel.generic_string();
}

} // namespace satyr
