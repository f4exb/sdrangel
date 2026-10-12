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

// IOSurface frames (macOS, VideoToolbox decoding): each NV12 plane is bound to a rectangle texture, without copying

#include "gui/glvideointerop.h"

#if defined(__APPLE__)

#include <IOSurface/IOSurface.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLIOSurface.h>
#include <OpenGL/gl3.h>

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions>

namespace {

class GLVideoInteropIOSurface : public GLVideoInterop
{
public:
    GLVideoInteropIOSurface() :
        m_gl(nullptr),
        m_textures{}
    {
    }

    ~GLVideoInteropIOSurface() override
    {
        if (m_textures[0]) {
            m_gl->glDeleteTextures(2, m_textures);
        }
    }

    bool init(QOpenGLContext *context)
    {
        m_gl = context->functions();
        m_gl->glGenTextures(2, m_textures);
        return true;
    }

    GLVideoFrame::Type type() const override { return GLVideoFrame::IOSurface; }

    bool acquire(const GLVideoFrame& frame, const GLVideoFrame *const *, int, bool, GLuint textures[2], Layout& layout) override
    {
        IOSurfaceRef surface = static_cast<IOSurfaceRef>(frame.m_ioSurface);
        CGLContextObj cgl = CGLGetCurrentContext();

        if (!surface || !cgl || (IOSurfaceGetPlaneCount(surface) < 2)) {
            return false;
        }

        for (int plane = 0; plane < 2; plane++)
        {
            m_gl->glBindTexture(GL_TEXTURE_RECTANGLE, m_textures[plane]);
            const CGLError error = CGLTexImageIOSurface2D(cgl, GL_TEXTURE_RECTANGLE, plane == 0 ? GL_R8 : GL_RG8,
                (GLsizei) IOSurfaceGetWidthOfPlane(surface, plane), (GLsizei) IOSurfaceGetHeightOfPlane(surface, plane),
                plane == 0 ? GL_RED : GL_RG, GL_UNSIGNED_BYTE, surface, plane);

            if (error != kCGLNoError)
            {
                qWarning("GLVideoInteropIOSurface: CGLTexImageIOSurface2D failed: %d", (int) error);
                return false;
            }

            m_gl->glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            m_gl->glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            m_gl->glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            m_gl->glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        m_gl->glBindTexture(GL_TEXTURE_RECTANGLE, 0);
        textures[0] = m_textures[0];
        textures[1] = m_textures[1];
        layout = LayoutNV12Rect;
        return true;
    }

    // The textures hold the surface they were bound to
    void releaseFrames() override
    {
        for (GLuint texture : m_textures)
        {
            m_gl->glBindTexture(GL_TEXTURE_RECTANGLE, texture);
            m_gl->glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_R8, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        }

        m_gl->glBindTexture(GL_TEXTURE_RECTANGLE, 0);
    }

    // The textures, and the surfaces bound to them, go with the context
    void abandon() override
    {
        m_textures[0] = m_textures[1] = 0;
        m_gl = nullptr;
    }

private:
    QOpenGLFunctions *m_gl;
    GLuint m_textures[2];
};

} // namespace

std::unique_ptr<GLVideoInterop> createGLVideoInteropIOSurface(QOpenGLContext *context)
{
    // Rectangle textures are sampled with GLSL 3.30, so a core profile, as SDRangel uses on macOS
    if (context->format().profile() != QSurfaceFormat::CoreProfile) {
        return nullptr;
    }

    std::unique_ptr<GLVideoInteropIOSurface> interop(new GLVideoInteropIOSurface());

    if (!interop->init(context)) {
        return nullptr;
    }

    return std::unique_ptr<GLVideoInterop>(interop.release());
}

#else

std::unique_ptr<GLVideoInterop> createGLVideoInteropIOSurface(QOpenGLContext *)
{
    return nullptr;
}

#endif
