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

#ifndef SDRGUI_GUI_GLVIDEOINTEROP_H_
#define SDRGUI_GUI_GLVIDEOINTEROP_H_

#include <memory>

#include <QtGui/qopengl.h>

#include "gui/glvideoframe.h"

class QOpenGLContext;

// Internal to GLVideoRenderer: imports GPU frames of one type into OpenGL textures, in the renderer's context
class GLVideoInterop
{
public:
    //! How the textures acquired are to be sampled
    enum Layout
    {
        LayoutNV12,         //!< Unit 0 Y (R), unit 1 UV (RG)
        LayoutNV12Rect,     //!< As NV12, as rectangle textures (in pixels)
        LayoutRGBA          //!< Unit 0 RGBA, already converted (and deinterlaced) on the GPU
    };

    virtual ~GLVideoInterop() {}
    virtual GLVideoFrame::Type type() const = 0;
    //! Previous frames wanted, for deinterlacing
    virtual int pastFrames() const { return 0; }
    //! False if interlaced frames to be deinterlaced are shown as they are, as the GPU can't deinterlace them. Known
    //! once a frame has been acquired
    virtual bool canDeinterlace() const { return true; }
    //! Make the frame available as textures, which are bound to units 0 and 1. past: the previous frames, most
    //! recent first, of the same type. False if it can't be shown
    virtual bool acquire(const GLVideoFrame& frame, const GLVideoFrame *const *past, int nbPast, bool deinterlace,
        GLuint textures[2], Layout& layout) = 0;
    //! The textures of the frame last acquired, again, as to draw it again, without importing (or deinterlacing) it
    //! again. False if they aren't kept, so it's to be acquired
    virtual bool reacquire(GLuint textures[2], Layout& layout) { (void) textures; (void) layout; return false; }
    //! After drawing
    virtual void release() {}
    //! Let go of whatever is held of the frames acquired (as views of a decoder's surfaces), with nothing shown
    virtual void releaseFrames() {}
    //! Let go of everything without calling OpenGL or its platform's API (WGL, CUDA's OpenGL interop), as the context
    //! isn't current or has gone: what's in the context is left to it. Then it can only be destroyed
    virtual void abandon() = 0;
};

// Each returns null if the platform, driver or context doesn't support it
std::unique_ptr<GLVideoInterop> createGLVideoInteropD3D11(QOpenGLContext *context);
std::unique_ptr<GLVideoInterop> createGLVideoInteropCUDA(QOpenGLContext *context);
std::unique_ptr<GLVideoInterop> createGLVideoInteropDMABuf(QOpenGLContext *context);
std::unique_ptr<GLVideoInterop> createGLVideoInteropIOSurface(QOpenGLContext *context);

#endif // SDRGUI_GUI_GLVIDEOINTEROP_H_
