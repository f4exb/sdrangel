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

#ifndef SDRGUI_GUI_GLVIDEORENDERER_H_
#define SDRGUI_GUI_GLVIDEORENDERER_H_

#include <atomic>
#include <deque>
#include <memory>
#include <vector>

#include <QImage>
#include <QOpenGLFunctions>
#include <QRect>

#include "gui/glvideoframe.h"
#include "gui/glvideointerop.h"
#include "export.h"

class QOpenGLShaderProgram;
class QOpenGLBuffer;
class QOpenGLVertexArrayObject;

// Draws video frames with OpenGL, in the current context: YUV 4:2:0 frames in memory are uploaded a plane at a time,
// and frames on the GPU (D3D11, CUDA, DMA-BUF, IOSurface) are imported without copying through the CPU, where the
// platform and driver allow. Either way, YUV is converted to RGB in a shader, so the CPU doesn't convert pixels.
// Separate from GLVideoWidget, so it can be tested with an offscreen context
class SDRGUI_API GLVideoRenderer : protected QOpenGLFunctions
{
public:
    GLVideoRenderer();
    ~GLVideoRenderer();

    void initialize();      //!< With the context current
    void cleanup();         //!< With the context current, before it's destroyed
    //! With the context current: let go of the frames held, for deinterlacing and by the GPU interops, so a decoder's
    //! surfaces aren't held while nothing is shown
    void releaseFrames();
    //! If GPU frames of the type can be shown. From any thread. False once one has failed to be shown, or while
    //! deinterlacing if the GPU can't deinterlace them, so the decoder deinterlaces them in memory instead
    bool canShow(GLVideoFrame::Type type) const;
    //! Draw the frame into the current framebuffer (width x height pixels), letterboxed to its aspect ratio if
    //! keepAspect, deinterlacing GPU frames that need it if deinterlace. False if it couldn't be shown
    bool drawFrame(const GLVideoFramePtr& frame, int width, int height, bool keepAspect, bool deinterlace);
    //! Draw an image, blended, at rect (pixels from the top left) of the framebuffer of width x height pixels
    void drawImage(const QImage& image, const QRect& rect, int width, int height);
    //! Where the frame is drawn
    static QRect videoRect(const GLVideoFrame& frame, int width, int height, bool keepAspect);
    //! Where a subtitle is drawn, over the picture drawn at videoRect
    static QRect subtitleRect(const GLVideoSubtitle& subtitle, const QRect& videoRect);

private:
    enum Program { ProgramPlanar, ProgramNV12, ProgramNV12Rect, ProgramRGBA, NbPrograms };

    QOpenGLShaderProgram *program(Program program);
    void uploadPlane(int unit, int index, const uint8_t *data, int linesize, int width, int height, bool twoChannels);
    void drawQuad(QOpenGLShaderProgram *program);
    void setYUVUniforms(QOpenGLShaderProgram *program, const GLVideoFrame& frame, bool pixels);
    void drawSubtitle(const GLVideoSubtitlePtr& subtitle, const QRect& videoRect, int width, int height);
    void drawTexture(GLuint texture, const QRect& rect, int width, int height);

    bool m_initialized;
    bool m_core;                    //!< Core profile (or GLES 3), so GLSL 3.30 (300 es) and R8/RG8 textures
    bool m_gles;
    bool m_rg;                      //!< R8/RG8 textures, else luminance (alpha)
    QOpenGLShaderProgram *m_programs[NbPrograms];
    bool m_programFailed[NbPrograms];       //!< To compile or link, so it isn't tried again
    QOpenGLBuffer *m_vertices;
    QOpenGLVertexArrayObject *m_vao;
    GLuint m_textures[3];           //!< Planes uploaded from memory
    int m_textureWidth[3];
    int m_textureHeight[3];
    int m_textureFormat[3];         //!< Internal format, as NV12's UV plane is two channels
    // Images drawn over or instead of the picture (a message, an overlay, a guide), each in a texture of its own, so
    // those drawn together aren't uploaded again for each paint. The least recently drawn's is reused for another
    static const int m_nbImages = 3;
    GLuint m_imageTextures[m_nbImages];
    qint64 m_imageKeys[m_nbImages];         //!< QImage::cacheKey() of the image in each, -1 if none
    quint64 m_imageDrawn[m_nbImages];       //!< When each was last drawn, in m_imageDraws
    quint64 m_imageDraws;
    GLuint m_subtitleTexture;
    GLVideoSubtitlePtr m_subtitle;  //!< In m_subtitleTexture (held, so a new one can't have the same address)
    std::unique_ptr<GLVideoInterop> m_interops[GLVideoFrame::NbTypes];
    std::atomic<unsigned int> m_supported;  //!< Bit per GLVideoFrame::Type
    std::atomic<unsigned int> m_noDeinterlace;  //!< Bit per GLVideoFrame::Type the GPU has been found not to deinterlace
    std::atomic<bool> m_deinterlacing;      //!< As frames were last drawn
    std::deque<GLVideoFramePtr> m_past;     //!< Previous GPU frames, for deinterlacing
    //! The frame last drawn, which a repaint (as for an overlay) draws again without uploading or importing it again
    std::weak_ptr<GLVideoFrame> m_lastFrame;
    bool m_lastDeinterlace;
    std::vector<uint8_t> m_repack;          //!< Rows of a plane without padding, for OpenGL ES 2
};

#endif // SDRGUI_GUI_GLVIDEORENDERER_H_
