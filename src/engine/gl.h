// Minimal OpenGL 3.3 core loader.
//
// Only the handful of functions the engine actually uses are declared here, which keeps the
// project free of GLAD/GLEW. To use another GL function, add a line to SATYR_GL_FUNCTIONS
// below (the typedef follows the official gl.xml signature) and it is loaded automatically.
#pragma once

#include <cstddef>
#include <cstdint>

#if defined(_WIN32) && !defined(SATYR_GL_APIENTRY)
#define SATYR_GL_APIENTRY __stdcall
#else
#define SATYR_GL_APIENTRY
#endif

// ---- Types -----------------------------------------------------------------------------------
typedef unsigned int   GLenum;
typedef unsigned char  GLboolean;
typedef unsigned int   GLbitfield;
typedef signed char    GLbyte;
typedef short          GLshort;
typedef int            GLint;
typedef int            GLsizei;
typedef unsigned char  GLubyte;
typedef unsigned short GLushort;
typedef unsigned int   GLuint;
typedef float          GLfloat;
typedef float          GLclampf;
typedef double         GLdouble;
typedef char           GLchar;
typedef void           GLvoid;
typedef std::ptrdiff_t GLintptr;
typedef std::ptrdiff_t GLsizeiptr;

// ---- Constants -------------------------------------------------------------------------------
#define GL_FALSE                          0
#define GL_TRUE                           1
#define GL_NO_ERROR                       0
#define GL_INVALID_ENUM                   0x0500
#define GL_INVALID_VALUE                  0x0501
#define GL_INVALID_OPERATION              0x0502
#define GL_OUT_OF_MEMORY                  0x0505
#define GL_INVALID_FRAMEBUFFER_OPERATION  0x0506

#define GL_TRIANGLES                      0x0004
#define GL_DEPTH_BUFFER_BIT               0x00000100
#define GL_COLOR_BUFFER_BIT               0x00004000
#define GL_DEPTH_TEST                     0x0B71
#define GL_CULL_FACE                      0x0B44
#define GL_BLEND                          0x0BE2
#define GL_DITHER                         0x0BD0
#define GL_UNPACK_ALIGNMENT               0x0CF5
#define GL_PACK_ALIGNMENT                 0x0D05
#define GL_TEXTURE_2D                     0x0DE1
#define GL_UNSIGNED_BYTE                  0x1401
#define GL_FLOAT                          0x1406
#define GL_RGB                            0x1907
#define GL_RGBA                           0x1908
#define GL_RGBA8                          0x8058
#define GL_RGBA16F                        0x881A
#define GL_RGBA32F                        0x8814
#define GL_R32F                           0x822E
#define GL_RED                            0x1903
#define GL_PIXEL_PACK_BUFFER              0x88EB
#define GL_STREAM_READ                    0x88E1
#define GL_MAP_READ_BIT                   0x0001
#define GL_CONSTANT_ALPHA                 0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA       0x8004
#define GL_NEAREST                        0x2600
#define GL_LINEAR                         0x2601
#define GL_TEXTURE_MAG_FILTER             0x2800
#define GL_TEXTURE_MIN_FILTER             0x2801
#define GL_TEXTURE_WRAP_S                 0x2802
#define GL_TEXTURE_WRAP_T                 0x2803
#define GL_CLAMP_TO_EDGE                  0x812F
#define GL_TEXTURE0                       0x84C0
#define GL_VENDOR                         0x1F00
#define GL_RENDERER                       0x1F01
#define GL_VERSION                        0x1F02
#define GL_SHADING_LANGUAGE_VERSION       0x8B8C
#define GL_MAJOR_VERSION                  0x821B
#define GL_MINOR_VERSION                  0x821C

#define GL_FRAGMENT_SHADER                0x8B30
#define GL_VERTEX_SHADER                  0x8B31
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_INFO_LOG_LENGTH                0x8B84

#define GL_FRAMEBUFFER                    0x8D40
#define GL_READ_FRAMEBUFFER               0x8CA8
#define GL_DRAW_FRAMEBUFFER               0x8CA9
#define GL_COLOR_ATTACHMENT0              0x8CE0
#define GL_FRAMEBUFFER_COMPLETE           0x8CD5
#define GL_BACK                           0x0405
#define GL_FRONT                          0x0404
#define GL_FRAMEBUFFER_SRGB               0x8DB9

// ---- Function list ---------------------------------------------------------------------------
// X(name, return type, (arguments))
#define SATYR_GL_FUNCTIONS(X) \
    X(glGetString,              const GLubyte*, (GLenum name)) \
    X(glGetIntegerv,            void, (GLenum pname, GLint* data)) \
    X(glGetError,               GLenum, (void)) \
    X(glEnable,                 void, (GLenum cap)) \
    X(glDisable,                void, (GLenum cap)) \
    X(glViewport,               void, (GLint x, GLint y, GLsizei width, GLsizei height)) \
    X(glClearColor,             void, (GLfloat r, GLfloat g, GLfloat b, GLfloat a)) \
    X(glClear,                  void, (GLbitfield mask)) \
    X(glFinish,                 void, (void)) \
    X(glBlendFunc,              void, (GLenum sfactor, GLenum dfactor)) \
    X(glBlendColor,             void, (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)) \
    X(glCreateShader,           GLuint, (GLenum type)) \
    X(glShaderSource,           void, (GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length)) \
    X(glCompileShader,          void, (GLuint shader)) \
    X(glGetShaderiv,            void, (GLuint shader, GLenum pname, GLint* params)) \
    X(glGetShaderInfoLog,       void, (GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog)) \
    X(glDeleteShader,           void, (GLuint shader)) \
    X(glCreateProgram,          GLuint, (void)) \
    X(glAttachShader,           void, (GLuint program, GLuint shader)) \
    X(glDetachShader,           void, (GLuint program, GLuint shader)) \
    X(glLinkProgram,            void, (GLuint program)) \
    X(glGetProgramiv,           void, (GLuint program, GLenum pname, GLint* params)) \
    X(glGetProgramInfoLog,      void, (GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog)) \
    X(glUseProgram,             void, (GLuint program)) \
    X(glDeleteProgram,          void, (GLuint program)) \
    X(glGetUniformLocation,     GLint, (GLuint program, const GLchar* name)) \
    X(glUniform1i,              void, (GLint location, GLint v0)) \
    X(glUniform1f,              void, (GLint location, GLfloat v0)) \
    X(glUniform2f,              void, (GLint location, GLfloat v0, GLfloat v1)) \
    X(glUniform3f,              void, (GLint location, GLfloat v0, GLfloat v1, GLfloat v2)) \
    X(glUniform4f,              void, (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)) \
    X(glUniform4fv,             void, (GLint location, GLsizei count, const GLfloat* value)) \
    X(glUniformMatrix3fv,       void, (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)) \
    X(glGenVertexArrays,        void, (GLsizei n, GLuint* arrays)) \
    X(glBindVertexArray,        void, (GLuint array)) \
    X(glDeleteVertexArrays,     void, (GLsizei n, const GLuint* arrays)) \
    X(glDrawArrays,             void, (GLenum mode, GLint first, GLsizei count)) \
    X(glGenTextures,            void, (GLsizei n, GLuint* textures)) \
    X(glBindTexture,            void, (GLenum target, GLuint texture)) \
    X(glActiveTexture,          void, (GLenum texture)) \
    X(glTexImage2D,             void, (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels)) \
    X(glTexSubImage2D,          void, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels)) \
    X(glTexParameteri,          void, (GLenum target, GLenum pname, GLint param)) \
    X(glDeleteTextures,         void, (GLsizei n, const GLuint* textures)) \
    X(glGenFramebuffers,        void, (GLsizei n, GLuint* framebuffers)) \
    X(glBindFramebuffer,        void, (GLenum target, GLuint framebuffer)) \
    X(glFramebufferTexture2D,   void, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)) \
    X(glCheckFramebufferStatus, GLenum, (GLenum target)) \
    X(glDeleteFramebuffers,     void, (GLsizei n, const GLuint* framebuffers)) \
    X(glBlitFramebuffer,        void, (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)) \
    X(glReadBuffer,             void, (GLenum src)) \
    X(glReadPixels,             void, (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels)) \
    X(glPixelStorei,            void, (GLenum pname, GLint param)) \
    X(glGenBuffers,             void, (GLsizei n, GLuint* buffers)) \
    X(glBindBuffer,             void, (GLenum target, GLuint buffer)) \
    X(glBufferData,             void, (GLenum target, GLsizeiptr size, const void* data, GLenum usage)) \
    X(glDeleteBuffers,          void, (GLsizei n, const GLuint* buffers)) \
    X(glMapBufferRange,         void*, (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)) \
    X(glUnmapBuffer,            GLboolean, (GLenum target))

// Declare a function pointer for every entry: `extern PFN_glClear glClear;`
#define SATYR_GL_DECLARE(name, ret, args) \
    typedef ret (SATYR_GL_APIENTRY *PFN_##name) args; \
    extern PFN_##name name;
SATYR_GL_FUNCTIONS(SATYR_GL_DECLARE)
#undef SATYR_GL_DECLARE

namespace satyr::gl {

using LoadProc = void* (*)(const char* name);

// Resolve every function pointer through `getProc` (normally glfwGetProcAddress).
// Returns false and logs the missing names if any function could not be loaded.
bool load(LoadProc getProc);

// Print any pending GL errors with a tag. Returns true if there were none.
bool checkErrors(const char* where);

} // namespace satyr::gl
