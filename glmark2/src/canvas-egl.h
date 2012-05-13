#ifndef GLMARK2_CANVAS_EGL_H_
#define GLMARK2_CANVAS_EGL_H_

#include <EGL/egl.h>
#include "canvas.h"

class CanvasEGL : public Canvas
{
public:
    CanvasEGL(int width, int height) :
        Canvas(width, height),
        gl_color_format_(0),
        gl_depth_format_(0),
        color_renderbuffer_(0),
        depth_renderbuffer_(0),
        fbo_(0),
        screen_width_(0),
        screen_height_(0),
        egl_display_(EGL_NO_DISPLAY),
        egl_surface_(EGL_NO_SURFACE),
        egl_config_(0),
        egl_context_(EGL_NO_CONTEXT)
        {}

    ~CanvasEGL() {}

    virtual bool init();
    virtual bool reset();
    virtual void visible(bool visible);
    virtual void clear();
    virtual void update();
    virtual void print_info();
    virtual Pixel read_pixel(int x, int y);
    virtual void write_to_file(std::string &filename);
    virtual bool should_quit();
    virtual void resize(int width, int height);

private:
    struct GLVisualInfo {
        int buffer_size;
        int red_size;
        int green_size;
        int blue_size;
        int alpha_size;
        int depth_size;
    };

    GLenum gl_color_format_;
    GLenum gl_depth_format_;
    GLuint color_renderbuffer_;
    GLuint depth_renderbuffer_;
    GLuint fbo_;
    EGL_DISPMANX_WINDOW_T native_window_;
    uint32_t screen_width_, screen_height_;
    EGLDisplay egl_display_;
    EGLSurface egl_surface_;
    EGLConfig egl_config_;
    EGLContext egl_context_;

    bool make_current();
    void get_glvisualinfo(GLVisualInfo &gl_visinfo);
    bool ensure_egl_display();
    bool ensure_egl_config();
    bool reset_context();
    bool ensure_egl_context();
    bool ensure_egl_surface();
    void init_gl_extensions();
    bool supports_gl2();
    void resize_no_viewport(int width, int height);
    bool do_make_current();
    bool ensure_gl_formats();
    bool ensure_fbo();
    void release_fbo();
    const char *get_gl_format_str(GLenum f);

    void swap_buffers() { eglSwapBuffers(egl_display_, egl_surface_); }
};

#endif // GLMARK2_CANVAS_EGL_H_
