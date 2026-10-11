///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

// DMA-BUF frames (Linux, VAAPI decoding, deinterlaced by deinterlace_vaapi, exported as DRM PRIME): each plane is
// imported as an EGLImage, bound to a texture, when its surface first comes, and kept for when it comes again. Needs
// an EGL context (Wayland, or X11 with QT_XCB_GL_INTEGRATION=xcb_egl); with GLX this isn't available, and frames are
// copied instead. libEGL is loaded when needed

#include "gui/glvideointerop.h"

#if defined(__linux__) && !defined(ANDROID)

#include <cstdint>
#include <memory>
#include <vector>

#include <QDebug>
#include <QLibrary>
#include <QOpenGLContext>
#include <QOpenGLFunctions>

namespace {

// The parts of EGL (egl.h, eglext.h) used
typedef void *EGLDisplay;
typedef void *EGLContext;
typedef void *EGLImageKHR;
typedef void *EGLClientBuffer;
typedef int32_t EGLint;
typedef unsigned int EGLenum;
typedef unsigned int EGLBoolean;

const EGLint eglNone = 0x3038;
const EGLint eglWidth = 0x3057;
const EGLint eglHeight = 0x3056;
const EGLenum eglLinuxDmaBuf = 0x3270;
const EGLint eglLinuxDrmFourcc = 0x3271;
const EGLint eglDmaBufPlane0Fd = 0x3272;
const EGLint eglDmaBufPlane0Offset = 0x3273;
const EGLint eglDmaBufPlane0Pitch = 0x3274;
const EGLint eglDmaBufPlane0ModifierLo = 0x3443;
const EGLint eglDmaBufPlane0ModifierHi = 0x3444;

// drm_fourcc.h
const uint64_t drmFormatModLinear = 0;
const uint64_t drmFormatModInvalid = 0x00ffffffffffffffULL; // Implicit: the driver's own layout

typedef EGLDisplay (*GetCurrentDisplay)();
typedef EGLContext (*GetCurrentContext)();
typedef void *(*GetProcAddress)(const char *);
typedef const char *(*QueryString)(EGLDisplay, EGLint);
typedef EGLImageKHR (*CreateImage)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLint *);
typedef EGLBoolean (*DestroyImage)(EGLDisplay, EGLImageKHR);
typedef void (*ImageTargetTexture2D)(GLenum, void *);

const EGLint eglExtensions = 0x3055;

class GLVideoInteropDMABuf : public GLVideoInterop
{
public:
    GLVideoInteropDMABuf() :
        m_gl(nullptr),
        m_display(nullptr),
        m_pool(nullptr),
        m_width(0),
        m_height(0),
        m_current(-1),
        m_modifiers(false)
    {
    }

    ~GLVideoInteropDMABuf() override
    {
        releaseImages();
    }

    bool init(QOpenGLContext *context)
    {
        m_library.setFileNameAndVersion("EGL", 1);

        if (!m_library.load()) {
            return false;
        }

        GetCurrentDisplay getDisplay = reinterpret_cast<GetCurrentDisplay>(m_library.resolve("eglGetCurrentDisplay"));
        GetCurrentContext getContext = reinterpret_cast<GetCurrentContext>(m_library.resolve("eglGetCurrentContext"));
        GetProcAddress getProcAddress = reinterpret_cast<GetProcAddress>(m_library.resolve("eglGetProcAddress"));
        QueryString queryString = reinterpret_cast<QueryString>(m_library.resolve("eglQueryString"));

        // Only if Qt's context is EGL
        if (!getDisplay || !getContext || !getProcAddress || !queryString || !getContext()) {
            return false;
        }

        m_display = getDisplay();
        const QByteArray extensions(queryString(m_display, eglExtensions));

        if (!extensions.contains("EGL_EXT_image_dma_buf_import")) {
            return false;
        }

        m_modifiers = extensions.contains("EGL_EXT_image_dma_buf_import_modifiers");
        m_createImage = reinterpret_cast<CreateImage>(getProcAddress("eglCreateImageKHR"));
        m_destroyImage = reinterpret_cast<DestroyImage>(getProcAddress("eglDestroyImageKHR"));
        m_imageTargetTexture = reinterpret_cast<ImageTargetTexture2D>(getProcAddress("glEGLImageTargetTexture2DOES"));

        if (!m_createImage || !m_destroyImage || !m_imageTargetTexture) {
            return false;
        }

        m_gl = context->functions();
        return true;
    }

    GLVideoFrame::Type type() const override { return GLVideoFrame::DMABuf; }

    // The planes of each surface of the pool are imported the first time it comes, and kept, as a decoder (or
    // deinterlacer) cycles through a few dozen. Without the pool, they're imported for each frame
    bool acquire(const GLVideoFrame& frame, const GLVideoFrame *const *, int, bool, GLuint textures[2], Layout& layout) override
    {
        if (!frame.m_dmaBufPool || (frame.m_dmaBufPool != m_pool) || (frame.m_width != m_width) || (frame.m_height != m_height)
            || (m_surfaces.size() >= maxSurfaces))
        {
            releaseImages(); // Another pool's, or the previous frame's, or more than a pool would have
            m_pool = frame.m_dmaBufPool;
            m_poolOwner = frame.m_dmaBufPoolOwner;
            m_width = frame.m_width;
            m_height = frame.m_height;
        }

        m_current = -1;

        for (int i = 0; m_pool && (i < (int) m_surfaces.size()); i++)
        {
            if (m_surfaces[i].m_id == frame.m_dmaBufSurface)
            {
                m_current = i;
                break;
            }
        }

        if (m_current < 0)
        {
            Surface surface;

            if (!import(frame, surface)) {
                return false;
            }

            m_surfaces.push_back(surface);
            m_current = (int) m_surfaces.size() - 1;
        }

        textures[0] = m_surfaces[m_current].m_textures[0];
        textures[1] = m_surfaces[m_current].m_textures[1];
        layout = LayoutNV12;
        return true;
    }

    // The images are kept, as drawing may not have finished with them
    void release() override {}

    bool reacquire(GLuint textures[2], Layout& layout) override
    {
        if ((m_current < 0) || (m_current >= (int) m_surfaces.size())) {
            return false;
        }

        textures[0] = m_surfaces[m_current].m_textures[0];
        textures[1] = m_surfaces[m_current].m_textures[1];
        layout = LayoutNV12;
        return true;
    }

    // The images, and the textures they are bound to, hold the decoder's surfaces
    void releaseFrames() override
    {
        releaseImages();
    }

    void abandon() override
    {
        // The textures go with the context. The images aren't the context's (nor need it current), so are destroyed
        for (Surface& surface : m_surfaces)
        {
            for (EGLImageKHR& image : surface.m_images)
            {
                if (image)
                {
                    m_destroyImage(m_display, image);
                    image = nullptr;
                }
            }
        }

        m_gl = nullptr;
        releaseImages();
    }

private:
    struct Surface
    {
        uint64_t m_id;
        EGLImageKHR m_images[2];
        GLuint m_textures[2];
    };

    static const size_t maxSurfaces = 64;

    // Each plane as an EGLImage, bound to a texture of its own
    bool import(const GLVideoFrame& frame, Surface& surface)
    {
        // A tiled or compressed layout can't be described without the modifiers extension: imported as linear, the
        // picture would be scrambled, so it's copied instead
        if (frame.m_dmaBufHasModifier && !m_modifiers)
        {
            for (int plane = 0; plane < 2; plane++)
            {
                const uint64_t modifier = frame.m_dmaBufModifier[plane];

                if ((modifier != drmFormatModLinear) && (modifier != drmFormatModInvalid))
                {
                    qWarning("GLVideoInteropDMABuf: plane %d has modifier 0x%llx, without EGL_EXT_image_dma_buf_import_modifiers",
                        plane, (unsigned long long) modifier);
                    return false;
                }
            }
        }

        surface.m_id = frame.m_dmaBufSurface;
        surface.m_images[0] = surface.m_images[1] = nullptr;
        m_gl->glGenTextures(2, surface.m_textures);

        for (int plane = 0; plane < 2; plane++)
        {
            std::vector<EGLint> attributes = {
                eglWidth, plane == 0 ? frame.m_width : (frame.m_width + 1) / 2,
                eglHeight, plane == 0 ? frame.m_height : (frame.m_height + 1) / 2,
                eglLinuxDrmFourcc, (EGLint) frame.m_dmaBufFourcc[plane],
                eglDmaBufPlane0Fd, frame.m_dmaBufFd[plane],
                eglDmaBufPlane0Offset, frame.m_dmaBufOffset[plane],
                eglDmaBufPlane0Pitch, frame.m_dmaBufPitch[plane]
            };

            if (m_modifiers && frame.m_dmaBufHasModifier)
            {
                attributes.push_back(eglDmaBufPlane0ModifierLo);
                attributes.push_back((EGLint) (frame.m_dmaBufModifier[plane] & 0xFFFFFFFF));
                attributes.push_back(eglDmaBufPlane0ModifierHi);
                attributes.push_back((EGLint) (frame.m_dmaBufModifier[plane] >> 32));
            }

            attributes.push_back(eglNone);
            surface.m_images[plane] = m_createImage(m_display, nullptr, eglLinuxDmaBuf, nullptr, attributes.data());

            if (!surface.m_images[plane])
            {
                qWarning("GLVideoInteropDMABuf: eglCreateImageKHR failed for plane %d", plane);
                m_gl->glBindTexture(GL_TEXTURE_2D, 0);
                destroy(surface);
                return false;
            }

            m_gl->glBindTexture(GL_TEXTURE_2D, surface.m_textures[plane]);
            m_imageTargetTexture(GL_TEXTURE_2D, surface.m_images[plane]);
            m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        m_gl->glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    }

    // Deleting the textures and images doesn't free the memory while drawing still uses it
    void destroy(Surface& surface)
    {
        m_gl->glDeleteTextures(2, surface.m_textures);

        for (EGLImageKHR& image : surface.m_images)
        {
            if (image)
            {
                m_destroyImage(m_display, image);
                image = nullptr;
            }
        }
    }

    void releaseImages()
    {
        if (m_gl)
        {
            for (Surface& surface : m_surfaces) {
                destroy(surface);
            }
        }

        m_surfaces.clear();
        m_current = -1;
        m_pool = nullptr;
        m_poolOwner.reset();
    }

    QLibrary m_library;
    QOpenGLFunctions *m_gl;
    EGLDisplay m_display;
    CreateImage m_createImage = nullptr;
    DestroyImage m_destroyImage = nullptr;
    ImageTargetTexture2D m_imageTargetTexture = nullptr;
    std::vector<Surface> m_surfaces;    //!< Of m_pool, imported
    const void *m_pool;                 //!< GLVideoFrame::m_dmaBufPool of the surfaces
    std::shared_ptr<void> m_poolOwner;  //!< Keeps m_pool alive while its surfaces are kept
    int m_width;                        //!< Of the surfaces
    int m_height;
    int m_current;                      //!< In m_surfaces, of the frame last acquired, -1 if none
    bool m_modifiers;
};

} // namespace

std::unique_ptr<GLVideoInterop> createGLVideoInteropDMABuf(QOpenGLContext *context)
{
    std::unique_ptr<GLVideoInteropDMABuf> interop(new GLVideoInteropDMABuf());

    if (!interop->init(context)) {
        return nullptr;
    }

    return std::unique_ptr<GLVideoInterop>(interop.release());
}

#else

std::unique_ptr<GLVideoInterop> createGLVideoInteropDMABuf(QOpenGLContext *)
{
    return nullptr;
}

#endif
