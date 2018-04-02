#pragma once

//#include <QImage>
#define INCLUDE_GL_CONTEXT_HEADERS
#include "gl_header.h"
#undef INCLUDE_GL_CONTEXT_HEADERS

namespace SimpleRender {

class GLContext {
public:
    GLContext(int h, int w): h_(h), w_(w) {}

    virtual ~GLContext() {}

    virtual void print_info();

protected:
    void init();

    int h_;
    int w_;
};

// Context for EGL (server-side OpenGL on some supported GPUs)
class EGLContext : public GLContext {
public:
    EGLContext(int h, int w, int device=0);
    ~EGLContext();

protected:
    EGLDisplay eglDpy_;
};

inline GLContext* createHeadlessContext(int h, int w, int device=0) {
    return new EGLContext{h, w, device};
};

} // namespace SimpleRender
