#include "canvas-egl.h"
#include "log.h"
#include "options.h"
#include "util.h"

#include <fstream>
#include <sstream>


#include <hwcomposerwindow/hwcomposer_window.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <malloc.h>
#include <sync/sync.h>


hwc_display_contents_1_t **mList = NULL;
hwc_composer_device_1_t *hwcDevicePtr = 0;

bool
CanvasEGL::make_current()
{
    if (!ensure_egl_surface())
        return false;

    if (!ensure_egl_context())
        return false;

    if (egl_context_ == eglGetCurrentContext())
        return true;

    if (!eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_)) {
        Log::error("Error: eglMakeCurrent failed with error %d\n", eglGetError());
        return false;
    }

    if (!eglSwapInterval(egl_display_, 0))
        Log::info("** Failed to set swap interval. Results may be bounded above by refresh rate.\n");

    init_gl_extensions();

    return true;
}

void
CanvasEGL::get_glvisualinfo(GLVisualInfo &gl_visinfo)
{
    if (!ensure_egl_config())
        return;

    eglGetConfigAttrib(egl_display_, egl_config_, EGL_BUFFER_SIZE, &gl_visinfo.buffer_size);
    eglGetConfigAttrib(egl_display_, egl_config_, EGL_RED_SIZE, &gl_visinfo.red_size);
    eglGetConfigAttrib(egl_display_, egl_config_, EGL_GREEN_SIZE, &gl_visinfo.green_size);
    eglGetConfigAttrib(egl_display_, egl_config_, EGL_BLUE_SIZE, &gl_visinfo.blue_size);
    eglGetConfigAttrib(egl_display_, egl_config_, EGL_ALPHA_SIZE, &gl_visinfo.alpha_size);
    eglGetConfigAttrib(egl_display_, egl_config_, EGL_DEPTH_SIZE, &gl_visinfo.depth_size);
}

/*******************
 * Private methods *
 *******************/

bool
CanvasEGL::ensure_egl_display()
{
    if (egl_display_)
        return true;

    Log::debug("ensure_egl_display\n");

    egl_display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!egl_display_) {
        Log::error("eglGetDisplay() failed with error: %d\n",
                   eglGetError());
        return false;
    }
    if (!eglInitialize(egl_display_, NULL, NULL)) {
        Log::error("eglInitialize() failed with error: %d\n",
                   eglGetError());
        return false;
        egl_display_ = 0;
    }

    return true;
}

bool
CanvasEGL::ensure_egl_config()
{
    static const EGLint attribs[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 8,
#ifdef USE_GLESv2
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        //EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
#elif USE_GL
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
#endif
        EGL_NONE
    };
    EGLint num_configs;
    EGLint vid;

    if (egl_config_)
        return true;

    int err;
    hw_module_t *hwcModule = 0;

    err = hw_get_module(HWC_HARDWARE_MODULE_ID, (const hw_module_t **) &hwcModule);
    if(err)
    {
        Log::error("hw_get_module() failed!");
	return false;
    }

    err = hwc_open_1(hwcModule, &hwcDevicePtr);
    if(err)
    {
        Log::error("hwc_open_1 failed!");
	return false;
    }


    hwcDevicePtr->blank(hwcDevicePtr, 0, 0);

    uint32_t configs[5];
    size_t numConfigs = 5;

    err = hwcDevicePtr->getDisplayConfigs(hwcDevicePtr, 0, configs, &numConfigs);
    if(err)
    {
        Log::error("getDisplayConfigs failed!");
	return false;
    }



    int32_t attr_values[2];
    uint32_t attributes[] = { HWC_DISPLAY_WIDTH, HWC_DISPLAY_HEIGHT, HWC_DISPLAY_NO_ATTRIBUTE };

    hwcDevicePtr->getDisplayAttributes(hwcDevicePtr, 0,
                    configs[0], attributes, attr_values);

    printf("width: %i height: %i\n", attr_values[0], attr_values[1]);
    screen_width_ = attr_values[0];
    screen_height_ = attr_values[1];

    native_window_ = new HWComposerNativeWindow(attr_values[0], attr_values[1], HAL_PIXEL_FORMAT_RGBA_8888);

    if (!ensure_egl_display())
        return false;


    Log::debug("ensure_egl_config\n");

    if (!eglChooseConfig(egl_display_, attribs, &egl_config_,
                         1, &num_configs))
    {
        Log::error("eglChooseConfig() failed with error: %d\n",
                     eglGetError());
        return false;
    }

    if (!eglGetConfigAttrib(egl_display_, egl_config_,
                            EGL_NATIVE_VISUAL_ID, &vid))
    {
        Log::error("eglGetConfigAttrib() failed with error: %d\n",
                   eglGetError());
        return false;
    }

    if (Options::show_debug) {
        int buf, red, green, blue, alpha, depth, id, native_id;
        eglGetConfigAttrib(egl_display_, egl_config_, EGL_CONFIG_ID, &id);
        eglGetConfigAttrib(egl_display_, egl_config_, EGL_NATIVE_VISUAL_ID, &native_id);
        eglGetConfigAttrib(egl_display_, egl_config_, EGL_BUFFER_SIZE, &buf);
        eglGetConfigAttrib(egl_display_, egl_config_, EGL_RED_SIZE, &red);
        eglGetConfigAttrib(egl_display_, egl_config_, EGL_GREEN_SIZE, &green);
        eglGetConfigAttrib(egl_display_, egl_config_, EGL_BLUE_SIZE, &blue);
        eglGetConfigAttrib(egl_display_, egl_config_, EGL_ALPHA_SIZE, &alpha);
        eglGetConfigAttrib(egl_display_, egl_config_, EGL_DEPTH_SIZE, &depth);
        Log::debug("EGL chosen config ID: 0x%x Native Visual ID: 0x%x\n"
                   "  Buffer: %d bits\n"
                   "     Red: %d bits\n"
                   "   Green: %d bits\n"
                   "    Blue: %d bits\n"
                   "   Alpha: %d bits\n"
                   "   Depth: %d bits\n",
                   id, native_id,
                   buf, red, green, blue, alpha, depth);
    }

    return true;
}

bool
CanvasEGL::reset_context()
{
    if (!ensure_egl_display())
        return false;

    if (!egl_context_)
        return true;

    if (eglDestroyContext(egl_display_, egl_context_) == EGL_FALSE) {
        Log::debug("eglDestroyContext() failed with error: 0x%x\n",
                   eglGetError());
    }

    egl_context_ = 0;
    return true;
}

bool
CanvasEGL::ensure_egl_context()
{
    if (egl_context_)
        return true;

    if (!ensure_egl_display())
        return false;

    if (!ensure_egl_config())
        return false;

    Log::debug("ensure_egl_context\n");

    static const EGLint ctx_attribs[] = {
#ifdef USE_GLESv2
        EGL_CONTEXT_CLIENT_VERSION, 2,
#endif
        EGL_NONE
    };

    egl_context_ = eglCreateContext(egl_display_, egl_config_,
                                    EGL_NO_CONTEXT, ctx_attribs);
    if (!egl_context_) {
        Log::error("eglCreateContext() failed with error: 0x%x\n",
                     eglGetError());
        return false;
    }

    return true;
}

bool
CanvasEGL::ensure_egl_surface()
{
    if (egl_surface_)
        return true;

    if (!ensure_egl_display())
        return false;

    if (!ensure_egl_config())
        return false;

    Log::debug("ensure_egl_surface\n");

#ifdef USE_GLESv2
    eglBindAPI(EGL_OPENGL_ES_API);
#elif USE_GL
    eglBindAPI(EGL_OPENGL_API);
#endif

    egl_surface_ = eglCreateWindowSurface((EGLDisplay) egl_display_, egl_config_, (EGLNativeWindowType) static_cast<ANativeWindow *> (native_window_), NULL);

    if (egl_surface_ == EGL_NO_SURFACE) {
        Log::error("eglCreateWindowSurface failed with error: %d\n",
                     eglGetError());
        return false;
    }

    size_t size = sizeof(hwc_display_contents_1_t) + 2 * sizeof(hwc_layer_1_t);
    hwc_display_contents_1_t *list = (hwc_display_contents_1_t *) malloc(size);
    mList = (hwc_display_contents_1_t **) malloc(HWC_NUM_DISPLAY_TYPES * sizeof(hwc_display_contents_1_t *));
    const hwc_rect_t r = { 0, 0, screen_width_, screen_height_ };


    int counter = 0;
    for (; counter < HWC_NUM_DISPLAY_TYPES; counter++)
            mList[counter] = list;

    hwc_layer_1_t *layer = &list->hwLayers[0];
    memset(layer, 0, sizeof(hwc_layer_1_t));
    layer->compositionType = HWC_FRAMEBUFFER;
    layer->hints = 0;
    layer->flags = 0;
    layer->handle = 0;
    layer->transform = 0;
    layer->blending = HWC_BLENDING_NONE;
    layer->sourceCrop = r;
    layer->displayFrame = r;
    layer->visibleRegionScreen.numRects = 1;
    layer->visibleRegionScreen.rects = &layer->displayFrame;
    layer->acquireFenceFd = -1;
    layer->releaseFenceFd = -1;
    layer = &list->hwLayers[1];
    memset(layer, 0, sizeof(hwc_layer_1_t));
    layer->compositionType = HWC_FRAMEBUFFER_TARGET;
    layer->hints = 0;
    layer->flags = 0;
    layer->handle = 0;
    layer->transform = 0;
    layer->blending = HWC_BLENDING_NONE;
    layer->sourceCrop = r;
    layer->displayFrame = r;
    layer->visibleRegionScreen.numRects = 1;
    layer->visibleRegionScreen.rects = &layer->displayFrame;
    layer->acquireFenceFd = -1;
    layer->releaseFenceFd = -1;

    list->retireFenceFd = -1;
    list->flags = HWC_GEOMETRY_CHANGED;
    list->numHwLayers = 2;


    return true;
}

void
CanvasEGL::init_gl_extensions()
{
#if USE_GLESv2
    if (GLExtensions::support("GL_OES_mapbuffer")) {
        GLExtensions::MapBuffer =
            reinterpret_cast<PFNGLMAPBUFFEROESPROC>(eglGetProcAddress("glMapBufferOES"));
        GLExtensions::UnmapBuffer =
            reinterpret_cast<PFNGLUNMAPBUFFEROESPROC>(eglGetProcAddress("glUnmapBufferOES"));
    }
#elif USE_GL
    GLExtensions::MapBuffer = glMapBuffer;
    GLExtensions::UnmapBuffer = glUnmapBuffer;
#endif
}

bool
CanvasEGL::reset()
{
    release_fbo();

    if (!reset_context())
        return false;

    if (!do_make_current())
        return false;

    if (!supports_gl2()) {
        Log::error("Glmark2 needs OpenGL(ES) version >= 2.0 to run"
                   " (but version string is: '%s')!\n",
                   glGetString(GL_VERSION));
        return false;
    }

    glViewport(0, 0, width_, height_);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    clear();

    return true;
}

bool
CanvasEGL::init()
{
    return reset();
}


void
CanvasEGL::visible(bool visible)
{
    (void) visible;
}

void
CanvasEGL::clear()
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.5f);
#if USE_GL
    glClearDepth(1.0f);
#elif USE_GLESv2
    glClearDepthf(1.0f);
#endif
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void
CanvasEGL::update()
{
    Options::FrameEnd m = Options::frame_end;

    if (m == Options::FrameEndDefault) {
        if (offscreen_)
            m = Options::FrameEndFinish;
        else
            m = Options::FrameEndSwap;
    }

    switch(m) {
        case Options::FrameEndSwap:
            swap_buffers();
            break;
        case Options::FrameEndFinish:
            glFinish();
            break;
        case Options::FrameEndReadPixels:
            read_pixel(width_ / 2, height_ / 2);
            break;
        case Options::FrameEndNone:
        default:
            break;
    }
}

void
CanvasEGL::print_info()
{
    do_make_current();

    std::stringstream ss;

    ss << "    OpenGL Information" << std::endl;
    ss << "    GL_VENDOR:     " << glGetString(GL_VENDOR) << std::endl;
    ss << "    GL_RENDERER:   " << glGetString(GL_RENDERER) << std::endl;
    ss << "    GL_VERSION:    " << glGetString(GL_VERSION) << std::endl;

    Log::info("%s", ss.str().c_str());
}

Canvas::Pixel
CanvasEGL::read_pixel(int x, int y)
{
    uint8_t pixel[4];

    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);

    return Canvas::Pixel(pixel[0], pixel[1], pixel[2], pixel[3]);
}

void
CanvasEGL::write_to_file(std::string &filename)
{
    char *pixels = new char[width_ * height_ * 4];

    for (int i = 0; i < height_; i++) {
        glReadPixels(0, i, width_, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                     &pixels[(height_ - i - 1) * width_ * 4]);
    }

    std::ofstream output (filename.c_str(), std::ios::out | std::ios::binary);
    output.write(pixels, 4 * width_ * height_);

    delete [] pixels;
}

bool
CanvasEGL::should_quit()
{
    return false;
}

void
CanvasEGL::resize(int width, int height)
{
    resize_no_viewport(width, height);
    glViewport(0, 0, width_, height_);
}

bool
CanvasEGL::supports_gl2()
{
    std::string gl_version_str(reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    int gl_major(0);

    size_t point_pos(gl_version_str.find('.'));

    if (point_pos != std::string::npos) {
        point_pos--;

        size_t start_pos(gl_version_str.rfind(' ', point_pos));
        if (start_pos == std::string::npos)
            start_pos = 0;
        else
            start_pos++;

        gl_major = Util::fromString<int>(
                gl_version_str.substr(start_pos, point_pos - start_pos + 1)
                );
    }

    return gl_major >= 2;
}

/*******************
 * Private methods *
 *******************/

void
CanvasEGL::resize_no_viewport(int width, int height)
{
    width_ = width;
    height_ = height;

    if (color_renderbuffer_) {
        glBindRenderbuffer(GL_RENDERBUFFER, color_renderbuffer_);
        glRenderbufferStorage(GL_RENDERBUFFER, gl_color_format_,
                              width_, height_);
    }

    if (depth_renderbuffer_) {
        glBindRenderbuffer(GL_RENDERBUFFER, depth_renderbuffer_);
        glRenderbufferStorage(GL_RENDERBUFFER, gl_depth_format_,
                              width_, height_);
    }

    projection_ = LibMatrix::Mat4::perspective(60.0, width_ / static_cast<float>(height_),
                                               1.0, 1024.0);
}

bool
CanvasEGL::do_make_current()
{
    if (!make_current())
        return false;

    if (offscreen_) {
        if (!ensure_fbo())
            return false;

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    }

    return true;
}

bool
CanvasEGL::ensure_gl_formats()
{
    if (gl_color_format_ && gl_depth_format_)
        return true;

    Log::debug("ensure_egl_formats\n");

    GLVisualInfo gl_visinfo;
    get_glvisualinfo(gl_visinfo);

    gl_color_format_ = 0;
    gl_depth_format_ = 0;

    bool supports_rgba8(false);
    bool supports_rgb8(false);
    bool supports_depth24(false);
    bool supports_depth32(false);

#if USE_GLESv2
    if (GLExtensions::support("GL_ARM_rgba8"))
        supports_rgba8 = true;

    if (GLExtensions::support("GL_OES_rgb8_rgba8")) {
        supports_rgba8 = true;
        supports_rgb8 = true;
    }

    if (GLExtensions::support("GL_OES_depth24"))
        supports_depth24 = true;

    if (GLExtensions::support("GL_OES_depth32"))
        supports_depth32 = true;
#elif USE_GL
    supports_rgba8 = true;
    supports_rgb8 = true;
    supports_depth24 = true;
    supports_depth32 = true;
#endif

    if (gl_visinfo.buffer_size == 32) {
        if (supports_rgba8)
            gl_color_format_ = GL_RGBA8;
        else
            gl_color_format_ = GL_RGBA4;
    }
    else if (gl_visinfo.buffer_size == 24) {
        if (supports_rgb8)
            gl_color_format_ = GL_RGB8;
        else
            gl_color_format_ = GL_RGB565;
    }
    else if (gl_visinfo.buffer_size == 16) {
        if (gl_visinfo.red_size == 4 && gl_visinfo.green_size == 4 &&
            gl_visinfo.blue_size == 4 && gl_visinfo.alpha_size == 4)
        {
            gl_color_format_ = GL_RGBA4;
        }
        else if (gl_visinfo.red_size == 5 && gl_visinfo.green_size == 5 &&
                 gl_visinfo.blue_size == 5 && gl_visinfo.alpha_size == 1)
        {
            gl_color_format_ = GL_RGB5_A1;
        }
        else if (gl_visinfo.red_size == 5 && gl_visinfo.green_size == 6 &&
                 gl_visinfo.blue_size == 5 && gl_visinfo.alpha_size == 0)
        {
            gl_color_format_ = GL_RGB565;
        }
    }

    if (gl_visinfo.depth_size == 32 && supports_depth32)
        gl_depth_format_ = GL_DEPTH_COMPONENT32;
    else if (gl_visinfo.depth_size >= 24 && supports_depth24)
        gl_depth_format_ = GL_DEPTH_COMPONENT24;
    else if (gl_visinfo.depth_size == 16)
        gl_depth_format_ = GL_DEPTH_COMPONENT16;

    Log::debug("Selected Renderbuffer ColorFormat: %s DepthFormat: %s\n",
               get_gl_format_str(gl_color_format_),
               get_gl_format_str(gl_depth_format_));

    return (gl_color_format_ && gl_depth_format_);
}

bool
CanvasEGL::ensure_fbo()
{
    if (!fbo_) {
        if (!ensure_gl_formats())
            return false;

        /* Create a texture for the color attachment  */
        glGenRenderbuffers(1, &color_renderbuffer_);
        glBindRenderbuffer(GL_RENDERBUFFER, color_renderbuffer_);
        glRenderbufferStorage(GL_RENDERBUFFER, gl_color_format_,
                              width_, height_);

        /* Create a renderbuffer for the depth attachment */
        glGenRenderbuffers(1, &depth_renderbuffer_);
        glBindRenderbuffer(GL_RENDERBUFFER, depth_renderbuffer_);
        glRenderbufferStorage(GL_RENDERBUFFER, gl_depth_format_,
                              width_, height_);

        /* Create a FBO and set it up */
        glGenFramebuffers(1, &fbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_RENDERBUFFER, color_renderbuffer_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, depth_renderbuffer_);
    }

    return true;
}

void
CanvasEGL::release_fbo()
{
    glDeleteFramebuffers(1, &fbo_);
    glDeleteRenderbuffers(1, &color_renderbuffer_);
    glDeleteRenderbuffers(1, &depth_renderbuffer_);
    fbo_ = 0;
    color_renderbuffer_ = 0;
    depth_renderbuffer_ = 0;

    gl_color_format_ = 0;
    gl_depth_format_ = 0;
}

const char *
CanvasEGL::get_gl_format_str(GLenum f)
{
    const char *str;

    switch(f) {
        case GL_RGBA8: str = "GL_RGBA8"; break;
        case GL_RGB8: str = "GL_RGB8"; break;
        case GL_RGBA4: str = "GL_RGBA4"; break;
        case GL_RGB5_A1: str = "GL_RGB5_A1"; break;
        case GL_RGB565: str = "GL_RGB565"; break;
        case GL_DEPTH_COMPONENT16: str = "GL_DEPTH_COMPONENT16"; break;
        case GL_DEPTH_COMPONENT24: str = "GL_DEPTH_COMPONENT24"; break;
        case GL_DEPTH_COMPONENT32: str = "GL_DEPTH_COMPONENT32"; break;
        case GL_NONE: str = "GL_NONE"; break;
        default: str = "Unknown"; break;
    }

    return str;
}

void CanvasEGL::swap_buffers()
{
	int oldretire = -1, oldrelease = -1, oldrelease2 = -1;

		eglSwapBuffers ( (EGLDisplay) egl_display_, egl_surface_ );  // get the rendered buffer to the screen

		HWComposerNativeWindowBuffer *front;	
		native_window_->lockFrontBuffer(&front);	

		mList[0]->hwLayers[1].handle = front->handle;
		mList[0]->hwLayers[0].handle = NULL;
		mList[0]->hwLayers[0].flags = HWC_SKIP_LAYER;

		oldretire = mList[0]->retireFenceFd;
		oldrelease = mList[0]->hwLayers[1].releaseFenceFd;
		oldrelease2 = mList[0]->hwLayers[0].releaseFenceFd;

		int err = hwcDevicePtr->prepare(hwcDevicePtr, HWC_NUM_DISPLAY_TYPES, mList);
		if(err)
		{
			printf("prepare() failed!");
		}		

		err = hwcDevicePtr->set(hwcDevicePtr, HWC_NUM_DISPLAY_TYPES, mList);
		//assert(err == 0);
		
		//assert(mList[0]->hwLayers[0].releaseFenceFd == -1);
	
		native_window_->unlockFrontBuffer(front);
		if (oldrelease != -1)
		{
			sync_wait(oldrelease, -1);
			close(oldrelease);
		}
		if (oldrelease2 != -1)
		{
			sync_wait(oldrelease2, -1);
			close(oldrelease2);
		}
		if (oldretire != -1)
		{
			sync_wait(oldretire, -1);
			close(oldretire);
		}
}


