#include "engine/gl.h"

#include <cstdio>

#define SATYR_GL_DEFINE(name, ret, args) PFN_##name name = nullptr;
SATYR_GL_FUNCTIONS(SATYR_GL_DEFINE)
#undef SATYR_GL_DEFINE

namespace satyr::gl {

bool load(LoadProc getProc)
{
    bool ok = true;
#define SATYR_GL_LOAD(name, ret, args) \
    name = reinterpret_cast<PFN_##name>(getProc(#name)); \
    if (!name) { std::fprintf(stderr, "[gl] missing function: %s\n", #name); ok = false; }
    SATYR_GL_FUNCTIONS(SATYR_GL_LOAD)
#undef SATYR_GL_LOAD
    return ok;
}

bool checkErrors(const char* where)
{
    bool clean = true;
    for (GLenum err = glGetError(); err != GL_NO_ERROR; err = glGetError()) {
        const char* text = "unknown";
        switch (err) {
            case GL_INVALID_ENUM:                  text = "GL_INVALID_ENUM"; break;
            case GL_INVALID_VALUE:                 text = "GL_INVALID_VALUE"; break;
            case GL_INVALID_OPERATION:             text = "GL_INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY:                 text = "GL_OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: text = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
        }
        std::fprintf(stderr, "[gl] %s: %s (0x%04X)\n", where, text, err);
        clean = false;
    }
    return clean;
}

} // namespace satyr::gl
