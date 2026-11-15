#include "gl_min.h"

#define NORMA_GL_DEFINE(ret, name, params) ret(NORMA_GLAPI* name) params = nullptr;
NORMA_GL_FUNCS(NORMA_GL_DEFINE)
#undef NORMA_GL_DEFINE

const char* loadGL(void* (*getProc)(const char*)) {
#define NORMA_GL_LOAD(ret, name, params)                                  \
    name = reinterpret_cast<ret(NORMA_GLAPI*) params>(getProc(#name));    \
    if (!name) return #name;
    NORMA_GL_FUNCS(NORMA_GL_LOAD)
#undef NORMA_GL_LOAD
    return nullptr;
}
