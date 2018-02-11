#include <cassert>
#include <iostream>

#include "gl_context.h"

using namespace std;

namespace {

const EGLint EGLconfigAttribs[] = {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
    EGL_BLUE_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_RED_SIZE, 8,
    EGL_DEPTH_SIZE, 24,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
    EGL_NONE
};

const EGLint EGLpbufferAttribs[] = {
    EGL_WIDTH, 9,
    EGL_HEIGHT, 9,
    EGL_NONE,
};

}

namespace SimpleRender {

void GLContext::init() {
    glViewport(0, 0, w_, h_);
}

void GLContext::print_info() {
    assert(glGetString(GL_VERSION));
    cerr << "----------- OpenGL Context Info --------------" << endl;
    cerr << "GL Version: " << glGetString(GL_VERSION) << endl;
    cerr << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
    cerr << "Vendor: " << glGetString(GL_VENDOR) << endl;
    cerr << "Renderer: " << glGetString(GL_RENDERER) << endl;
    cerr << "----------------------------------------------" << endl;
}

// https://devblogs.nvidia.com/parallelforall/egl-eye-opengl-visualization-without-x-server/
EGLContext::EGLContext(int h, int w, int device): GLContext{h, w} {
    auto checkError = [](EGLBoolean succ) {
        EGLint err = eglGetError();
        if (err != EGL_SUCCESS) {
            fprintf(stderr,"EGL error: %d\n", err);
            fflush(stderr);
            exit(1);
        }
        if (!succ) {
            fprintf(stderr,"EGL failed\n");
            fflush(stderr);
            exit(1);
        }
    };

    // 1. Initialize EGL
    {
        static const int MAX_DEVICES = 8;
        EGLDeviceEXT eglDevs[MAX_DEVICES];
        EGLint numDevices;
        PFNEGLQUERYDEVICESEXTPROC eglQueryDevicesEXT =
          (PFNEGLQUERYDEVICESEXTPROC) eglGetProcAddress("eglQueryDevicesEXT");
        PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT =
          (PFNEGLGETPLATFORMDISPLAYEXTPROC) eglGetProcAddress("eglGetPlatformDisplayEXT");
        if (!eglQueryDevicesEXT or !eglGetPlatformDisplayEXT) {
            fprintf(stderr, "Failed to get function pointer of "
                            "eglQueryDevicesEXT/eglGetPlatformDisplayEXT! "
                            "Maybe EGL extensions are unsupported.");
            fflush(stderr);
            exit(1);
        }

        eglQueryDevicesEXT(MAX_DEVICES, eglDevs, &numDevices);
        cerr << "[EGL] Detected " << numDevices << " devices. "
             << "Using device " << device << endl;
        assert(device < numDevices);
        eglDpy_ = eglGetPlatformDisplayEXT(
                EGL_PLATFORM_DEVICE_EXT, eglDevs[device], 0);
    }

    EGLint major, minor;

    EGLBoolean succ = eglInitialize(eglDpy_, &major, &minor);
    if (!succ) {
        fprintf(stderr, "Failed to initialize EGL display!");
        fflush(stderr);
        exit(1);
    }
    checkError(succ);

    // 2. Select an appropriate configuration
    EGLint numConfigs;
    EGLConfig eglCfg;

    succ = eglChooseConfig(eglDpy_, EGLconfigAttribs, &eglCfg, 1, &numConfigs);
    checkError(succ);
    if (numConfigs != 1) {
        fprintf(stderr, "Cannot create configs for EGL! "
                        "You driver may not support EGL.");
        fflush(stderr);
        exit(1);
    }

    // 3. Create a surface
    /*
     *EGLSurface eglSurf = eglCreatePbufferSurface(eglDpy_, eglCfg, EGLpbufferAttribs);
     *checkError(succ);
     */
    EGLSurface eglSurf = EGL_NO_SURFACE;

    // 4. Bind the API
    succ = eglBindAPI(EGL_OPENGL_API);
    checkError(succ);

    // 5. Create a context and make it current
    ::EGLContext eglCtx = eglCreateContext(
            eglDpy_, eglCfg, (::EGLContext)0, NULL);
    checkError(succ);
    succ = eglMakeCurrent(eglDpy_, eglSurf, eglSurf, eglCtx);
    if (!succ) {
        fprintf(stderr, "Failed to make EGL context current!");
        fflush(stderr);
        exit(1);
    }
    checkError(succ);

    this->init();
}

EGLContext::~EGLContext() {
    // 6. Terminate EGL when finished
    eglTerminate(eglDpy_);
}

} // namespace SimpleRender
