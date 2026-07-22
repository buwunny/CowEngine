#ifndef PLATFORM_GL_HEADLESS_HPP
#define PLATFORM_GL_HEADLESS_HPP

// No-op OpenGL shim for the headless authoritative server (ENGINE_HEADLESS).
// The server reuses the engine's Scene/Factories/Mesh code so its simulation is
// byte-for-byte the client's, but it has no GL context. Mesh constructors still
// populate CPU-side vertex/index data (used for convex-hull collision shapes);
// only the GPU upload/draw calls are stubbed away here. render() is never called
// on the server. This header stands in for <glad/glad.h> / <GLES3/gl3.h> and
// covers exactly the GL surface the mesh code touches — see the grep in the
// Phase 3 notes; extend it if a new GL call is compiled into the server.

#include <cstddef>

typedef unsigned int GLuint;
typedef int GLint;
typedef unsigned int GLenum;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLboolean;
typedef void GLvoid;
typedef std::ptrdiff_t GLsizeiptr;
typedef char GLchar;

enum
{
    GL_ARRAY_BUFFER = 0x8892,
    GL_ELEMENT_ARRAY_BUFFER = 0x8893,
    GL_STATIC_DRAW = 0x88E4,
    GL_FLOAT = 0x1406,
    GL_FALSE = 0,
    GL_TRUE = 1,
    GL_TRIANGLES = 0x0004,
    GL_LINES = 0x0001,
    GL_UNSIGNED_INT = 0x1405,
};

inline void glGenBuffers(GLsizei n, GLuint *p)
{
    for (GLsizei i = 0; i < n; ++i)
        p[i] = 0;
}
inline void glGenVertexArrays(GLsizei n, GLuint *p)
{
    for (GLsizei i = 0; i < n; ++i)
        p[i] = 0;
}
inline void glBindBuffer(GLenum, GLuint) {}
inline void glBindVertexArray(GLuint) {}
inline void glBufferData(GLenum, GLsizeiptr, const void *, GLenum) {}
inline void glDeleteBuffers(GLsizei, const GLuint *) {}
inline void glDeleteVertexArrays(GLsizei, const GLuint *) {}
inline void glEnableVertexAttribArray(GLuint) {}
inline void glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *) {}
inline void glDrawElements(GLenum, GLsizei, GLenum, const void *) {}
inline void glDrawArrays(GLenum, GLint, GLsizei) {}

#endif // PLATFORM_GL_HEADLESS_HPP
