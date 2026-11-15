// Минимальный загрузчик OpenGL 3.3 core: объявляет только те функции и
// константы, которыми пользуется игра, и подгружает их через
// SDL_GL_GetProcAddress. Работает одинаково на macOS, Linux и Windows,
// системные заголовки OpenGL не нужны.
#pragma once

#include <cstddef>
#include <cstdint>

#if defined(_WIN32) && !defined(_WIN64)
#  define NORMA_GLAPI __stdcall
#else
#  define NORMA_GLAPI
#endif

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef float GLfloat;
typedef char GLchar;
typedef unsigned char GLubyte;
typedef std::ptrdiff_t GLsizeiptr;
typedef std::ptrdiff_t GLintptr;

enum : GLenum {
    GL_FALSE = 0,
    GL_TRUE = 1,
    GL_TRIANGLES = 0x0004,
    GL_DEPTH_BUFFER_BIT = 0x00000100,
    GL_COLOR_BUFFER_BIT = 0x00004000,
    GL_SRC_ALPHA = 0x0302,
    GL_ONE_MINUS_SRC_ALPHA = 0x0303,
    GL_LEQUAL = 0x0203,
    GL_CULL_FACE = 0x0B44,
    GL_DEPTH_TEST = 0x0B71,
    GL_BLEND = 0x0BE2,
    GL_UNPACK_ALIGNMENT = 0x0CF5,
    GL_PACK_ALIGNMENT = 0x0D05,
    GL_TEXTURE_2D = 0x0DE1,
    GL_UNSIGNED_BYTE = 0x1401,
    GL_FLOAT = 0x1406,
    GL_RED = 0x1903,
    GL_RGB = 0x1907,
    GL_RGBA = 0x1908,
    GL_VENDOR = 0x1F00,
    GL_RENDERER = 0x1F01,
    GL_VERSION = 0x1F02,
    GL_NEAREST = 0x2600,
    GL_LINEAR = 0x2601,
    GL_NEAREST_MIPMAP_LINEAR = 0x2702,
    GL_LINEAR_MIPMAP_LINEAR = 0x2703,
    GL_TEXTURE_MAG_FILTER = 0x2800,
    GL_TEXTURE_MIN_FILTER = 0x2801,
    GL_TEXTURE_WRAP_S = 0x2802,
    GL_TEXTURE_WRAP_T = 0x2803,
    GL_REPEAT = 0x2901,
    GL_R8 = 0x8229,
    GL_RGBA8 = 0x8058,
    GL_CLAMP_TO_EDGE = 0x812F,
    GL_TEXTURE0 = 0x84C0,
    GL_ARRAY_BUFFER = 0x8892,
    GL_STATIC_DRAW = 0x88E4,
    GL_DYNAMIC_DRAW = 0x88E8,
    GL_FRAGMENT_SHADER = 0x8B30,
    GL_VERTEX_SHADER = 0x8B31,
    GL_COMPILE_STATUS = 0x8B81,
    GL_LINK_STATUS = 0x8B82,
    GL_FRAMEBUFFER_COMPLETE = 0x8CD5,
    GL_COLOR_ATTACHMENT0 = 0x8CE0,
    GL_FRAMEBUFFER = 0x8D40
};

// X-макрос: (тип результата, имя, параметры)
#define NORMA_GL_FUNCS(X)                                                                         \
    X(void, glViewport, (GLint x, GLint y, GLsizei w, GLsizei h))                                 \
    X(void, glClearColor, (GLfloat r, GLfloat g, GLfloat b, GLfloat a))                           \
    X(void, glClear, (GLbitfield mask))                                                           \
    X(void, glEnable, (GLenum cap))                                                               \
    X(void, glDisable, (GLenum cap))                                                              \
    X(void, glBlendFunc, (GLenum s, GLenum d))                                                    \
    X(void, glDepthFunc, (GLenum f))                                                              \
    X(void, glDepthMask, (GLboolean flag))                                                        \
    X(GLenum, glGetError, (void))                                                                 \
    X(const GLubyte*, glGetString, (GLenum name))                                                 \
    X(void, glPixelStorei, (GLenum pname, GLint param))                                           \
    X(void, glReadPixels,                                                                         \
      (GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, void* data))           \
    X(void, glGenTextures, (GLsizei n, GLuint* t))                                                \
    X(void, glDeleteTextures, (GLsizei n, const GLuint* t))                                       \
    X(void, glBindTexture, (GLenum target, GLuint t))                                             \
    X(void, glTexImage2D,                                                                         \
      (GLenum target, GLint level, GLint ifmt, GLsizei w, GLsizei h, GLint border, GLenum fmt,    \
       GLenum type, const void* data))                                                            \
    X(void, glTexParameteri, (GLenum target, GLenum pname, GLint param))                          \
    X(void, glGenerateMipmap, (GLenum target))                                                    \
    X(void, glActiveTexture, (GLenum tex))                                                        \
    X(void, glGenBuffers, (GLsizei n, GLuint* b))                                                 \
    X(void, glDeleteBuffers, (GLsizei n, const GLuint* b))                                        \
    X(void, glBindBuffer, (GLenum target, GLuint b))                                              \
    X(void, glBufferData, (GLenum target, GLsizeiptr size, const void* data, GLenum usage))       \
    X(void, glGenVertexArrays, (GLsizei n, GLuint* a))                                            \
    X(void, glDeleteVertexArrays, (GLsizei n, const GLuint* a))                                   \
    X(void, glBindVertexArray, (GLuint a))                                                        \
    X(void, glEnableVertexAttribArray, (GLuint index))                                            \
    X(void, glVertexAttribPointer,                                                                \
      (GLuint index, GLint size, GLenum type, GLboolean norm, GLsizei stride, const void* ptr))   \
    X(void, glDrawArrays, (GLenum mode, GLint first, GLsizei count))                              \
    X(GLuint, glCreateShader, (GLenum type))                                                      \
    X(void, glShaderSource,                                                                       \
      (GLuint shader, GLsizei count, const GLchar* const* str, const GLint* len))                 \
    X(void, glCompileShader, (GLuint shader))                                                     \
    X(void, glGetShaderiv, (GLuint shader, GLenum pname, GLint* params))                          \
    X(void, glGetShaderInfoLog, (GLuint shader, GLsizei max, GLsizei* len, GLchar* log))          \
    X(void, glDeleteShader, (GLuint shader))                                                      \
    X(GLuint, glCreateProgram, (void))                                                            \
    X(void, glAttachShader, (GLuint program, GLuint shader))                                      \
    X(void, glBindAttribLocation, (GLuint program, GLuint index, const GLchar* name))             \
    X(void, glLinkProgram, (GLuint program))                                                      \
    X(void, glGetProgramiv, (GLuint program, GLenum pname, GLint* params))                        \
    X(void, glGetProgramInfoLog, (GLuint program, GLsizei max, GLsizei* len, GLchar* log))        \
    X(void, glUseProgram, (GLuint program))                                                       \
    X(GLint, glGetUniformLocation, (GLuint program, const GLchar* name))                          \
    X(void, glUniform1i, (GLint loc, GLint v))                                                    \
    X(void, glUniform1f, (GLint loc, GLfloat v))                                                  \
    X(void, glUniform2f, (GLint loc, GLfloat a, GLfloat b))                                       \
    X(void, glUniform3f, (GLint loc, GLfloat a, GLfloat b, GLfloat c))                            \
    X(void, glUniform4f, (GLint loc, GLfloat a, GLfloat b, GLfloat c, GLfloat d))                 \
    X(void, glUniform1fv, (GLint loc, GLsizei count, const GLfloat* v))                           \
    X(void, glUniform3fv, (GLint loc, GLsizei count, const GLfloat* v))                           \
    X(void, glUniformMatrix4fv, (GLint loc, GLsizei count, GLboolean transpose, const GLfloat* v))\
    X(void, glGenFramebuffers, (GLsizei n, GLuint* f))                                            \
    X(void, glDeleteFramebuffers, (GLsizei n, const GLuint* f))                                   \
    X(void, glBindFramebuffer, (GLenum target, GLuint f))                                         \
    X(void, glFramebufferTexture2D,                                                               \
      (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level))          \
    X(GLenum, glCheckFramebufferStatus, (GLenum target))

#define NORMA_GL_DECLARE(ret, name, params) extern ret(NORMA_GLAPI* name) params;
NORMA_GL_FUNCS(NORMA_GL_DECLARE)
#undef NORMA_GL_DECLARE

// Загружает указатели на функции. Вызывать после создания контекста.
// Возвращает имя первой ненайденной функции или nullptr при успехе.
const char* loadGL(void* (*getProc)(const char*));
