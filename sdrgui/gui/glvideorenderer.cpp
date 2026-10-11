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

#include <algorithm>
#include <cmath>
#include <cstring>

#include <QDebug>
#include <QGenericMatrix>
#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QVector2D>

#include "gui/glvideorenderer.h"

#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#endif
#ifndef GL_RED
#define GL_RED 0x1903
#endif
#ifndef GL_RG
#define GL_RG 0x8227
#endif
#ifndef GL_R8
#define GL_R8 0x8229
#endif
#ifndef GL_RG8
#define GL_RG8 0x822B
#endif
#ifndef GL_TEXTURE_RECTANGLE
#define GL_TEXTURE_RECTANGLE 0x84F5
#endif
#ifndef GL_LUMINANCE
#define GL_LUMINANCE 0x1909
#endif
#ifndef GL_LUMINANCE_ALPHA
#define GL_LUMINANCE_ALPHA 0x190A
#endif

// A quad over the viewport: position, then texture coordinate, with the top of the image at the top
static const GLfloat quad[] = {
    -1.0f, -1.0f, 0.0f, 1.0f,
     1.0f, -1.0f, 1.0f, 1.0f,
    -1.0f,  1.0f, 0.0f, 0.0f,
     1.0f,  1.0f, 1.0f, 0.0f
};

GLVideoRenderer::GLVideoRenderer() :
    m_initialized(false),
    m_core(false),
    m_gles(false),
    m_rg(false),
    m_programs{},
    m_programFailed{},
    m_vertices(nullptr),
    m_vao(nullptr),
    m_textures{},
    m_textureWidth{},
    m_textureHeight{},
    m_textureFormat{},
    m_imageTextures{},
    m_imageKeys{-1, -1, -1},
    m_imageDrawn{},
    m_imageDraws(0),
    m_subtitleTexture(0),
    m_supported((1u << GLVideoFrame::Planar420) | (1u << GLVideoFrame::NV12)),
    m_noDeinterlace(0),
    m_deinterlacing(false),
    m_lastDeinterlace(false)
{
}

GLVideoRenderer::~GLVideoRenderer()
{
    // cleanup() should have been called with the context current. If not, GL objects are left to the context, which
    // may not be current or may have gone, so the interops let go of the rest without calling OpenGL. Before the
    // frames they may use go
    if (m_initialized)
    {
        qWarning("GLVideoRenderer::~GLVideoRenderer: cleanup() wasn't called");

        for (auto& interop : m_interops)
        {
            if (interop)
            {
                interop->abandon();
                interop.reset();
            }
        }
    }

    for (QOpenGLShaderProgram *&p : m_programs)
    {
        delete p;
        p = nullptr;
    }

    delete m_vertices;
    delete m_vao;
}

void GLVideoRenderer::initialize()
{
    if (m_initialized) {
        return;
    }

    QOpenGLContext *context = QOpenGLContext::currentContext();
    initializeOpenGLFunctions();
    const QSurfaceFormat format = context->format();
    m_gles = context->isOpenGLES();
    m_core = m_gles ? (format.majorVersion() >= 3) : (format.profile() == QSurfaceFormat::CoreProfile);
    m_rg = m_core || (!m_gles && (format.majorVersion() >= 3))
        || context->hasExtension(m_gles ? "GL_EXT_texture_rg" : "GL_ARB_texture_rg");

    m_vao = new QOpenGLVertexArrayObject();

    if (!m_vao->create())
    {
        delete m_vao;
        m_vao = nullptr; // Compatibility contexts may not have them, and don't need them
    }

    if (m_vao) {
        m_vao->bind();
    }

    m_vertices = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    m_vertices->create();
    m_vertices->bind();
    m_vertices->allocate(quad, sizeof(quad));
    m_vertices->release();

    if (m_vao) {
        m_vao->release();
    }

    glGenTextures(3, m_textures);
    glGenTextures(m_nbImages, m_imageTextures);
    glGenTextures(1, &m_subtitleTexture);

    // GPU frames: those this platform, driver and context can import
    unsigned int supported = 0;
    m_interops[GLVideoFrame::D3D11] = createGLVideoInteropD3D11(context);
    m_interops[GLVideoFrame::IOSurface] = createGLVideoInteropIOSurface(context);

    // Their UV planes are two channel (RG) textures, which the NV12 shader samples as such only with m_rg
    if (m_rg)
    {
        m_interops[GLVideoFrame::CUDA] = createGLVideoInteropCUDA(context);
        m_interops[GLVideoFrame::DMABuf] = createGLVideoInteropDMABuf(context);
    }

    for (int t = 0; t < GLVideoFrame::NbTypes; t++)
    {
        if (m_interops[t]) {
            supported |= 1u << t;
        }
    }

    supported |= (1u << GLVideoFrame::Planar420) | (1u << GLVideoFrame::NV12);
    m_supported = supported;
    m_initialized = true;
    qDebug("GLVideoRenderer::initialize: OpenGL%s %d.%d %s, GPU frames:%s%s%s%s", m_gles ? " ES" : "",
        format.majorVersion(), format.minorVersion(), m_core ? "core" : "compatibility",
        m_interops[GLVideoFrame::D3D11] ? " D3D11" : "", m_interops[GLVideoFrame::CUDA] ? " CUDA" : "",
        m_interops[GLVideoFrame::DMABuf] ? " DMA-BUF" : "", m_interops[GLVideoFrame::IOSurface] ? " IOSurface" : "");
}

void GLVideoRenderer::cleanup()
{
    if (!m_initialized) {
        return;
    }

    // The interops first, while the frames they may use are still alive
    for (auto& interop : m_interops) {
        interop.reset();
    }

    m_past.clear();
    m_lastFrame.reset();

    m_supported = (1u << GLVideoFrame::Planar420) | (1u << GLVideoFrame::NV12);
    m_noDeinterlace = 0;
    glDeleteTextures(3, m_textures);
    glDeleteTextures(m_nbImages, m_imageTextures);
    glDeleteTextures(1, &m_subtitleTexture);
    // What was in them: the textures made for another context are allocated again
    std::fill(std::begin(m_textures), std::end(m_textures), 0);
    std::fill(std::begin(m_textureWidth), std::end(m_textureWidth), 0);
    std::fill(std::begin(m_textureHeight), std::end(m_textureHeight), 0);
    std::fill(std::begin(m_textureFormat), std::end(m_textureFormat), 0);
    std::fill(std::begin(m_imageTextures), std::end(m_imageTextures), 0);
    std::fill(std::begin(m_imageKeys), std::end(m_imageKeys), -1);
    m_subtitleTexture = 0;
    m_subtitle.reset();

    for (QOpenGLShaderProgram *&p : m_programs)
    {
        delete p;
        p = nullptr;
    }

    std::fill(std::begin(m_programFailed), std::end(m_programFailed), false); // Another context's may compile
    delete m_vertices;
    m_vertices = nullptr;
    delete m_vao;
    m_vao = nullptr;
    m_initialized = false;
}

void GLVideoRenderer::releaseFrames()
{
    m_past.clear();
    m_lastFrame.reset();
    m_noDeinterlace = 0; // The interops' devices are let go of too: the next may deinterlace

    if (!m_initialized) {
        return;
    }

    for (auto& interop : m_interops)
    {
        if (interop) {
            interop->releaseFrames();
        }
    }
}

bool GLVideoRenderer::canShow(GLVideoFrame::Type type) const
{
    unsigned int supported = m_supported.load();

    if (m_deinterlacing.load()) {
        supported &= ~m_noDeinterlace.load();
    }

    return (supported >> type) & 1u;
}

// Shaders: GLSL 3.30 core (or 3.00 ES), else 1.20 for compatibility contexts of older drivers. Null if it doesn't
// compile, which is only tried once (and warned of)
QOpenGLShaderProgram *GLVideoRenderer::program(Program which)
{
    if (m_programs[which] || m_programFailed[which]) {
        return m_programs[which];
    }

    QString header;

    if (m_gles) {
        header = m_core ? "#version 300 es\nprecision mediump float;\n#define IN in\n#define OUT out\n"
            : "#version 100\nprecision mediump float;\n#define IN varying\n#define OUT varying\n";
    } else {
        header = m_core ? "#version 330 core\n#define IN in\n#define OUT out\n" : "#version 120\n#define IN varying\n#define OUT varying\n";
    }

    const bool modern = m_core;
    const QString vertex = header + (modern ? "in vec2 position;\nin vec2 texCoord;\n" : "attribute vec2 position;\nattribute vec2 texCoord;\n")
        + "OUT vec2 vTexCoord;\n"
          "void main() {\n"
          "    vTexCoord = texCoord;\n"
          "    gl_Position = vec4(position, 0.0, 1.0);\n"
          "}\n";

    QString fragment = header;

    if (modern) {
        fragment += "out vec4 fragColor;\n#define TEX texture\n";
    } else {
        fragment += "#define fragColor gl_FragColor\n#define TEX texture2D\n";
    }

    // YUV to RGB: remove the offsets and scale to 0..1 (luma) and -0.5..0.5 (chroma), then the matrix. Chroma is
    // sampled at vTexCoord * cScale + cOffset, for where its samples are (see setYUVUniforms)
    const QString yuv =
        "uniform mat3 yuvMatrix;\n"
        "uniform vec2 yRange;\n"
        "uniform vec2 cRange;\n"
        "uniform vec2 cScale;\n"
        "uniform vec2 cOffset;\n"
        "vec4 rgb(float y, vec2 c) {\n"
        "    vec3 v = vec3((y - yRange.x) * yRange.y, (c - vec2(cRange.x)) * cRange.y);\n"
        "    return vec4(clamp(yuvMatrix * v, 0.0, 1.0), 1.0);\n"
        "}\n";
    const QString uv = m_rg ? "rg" : "ra";

    switch (which)
    {
    case ProgramPlanar:
        fragment += "IN vec2 vTexCoord;\nuniform sampler2D tex0;\nuniform sampler2D tex1;\nuniform sampler2D tex2;\n" + yuv +
            "void main() {\n"
            "    vec2 c = vTexCoord * cScale + cOffset;\n"
            "    fragColor = rgb(TEX(tex0, vTexCoord).r, vec2(TEX(tex1, c).r, TEX(tex2, c).r));\n"
            "}\n";
        break;
    case ProgramNV12:
        fragment += "IN vec2 vTexCoord;\nuniform sampler2D tex0;\nuniform sampler2D tex1;\n" + yuv +
            "void main() {\n"
            "    fragColor = rgb(TEX(tex0, vTexCoord).r, TEX(tex1, vTexCoord * cScale + cOffset)." + uv + ");\n"
            "}\n";
        break;
    case ProgramNV12Rect:
        // Rectangle textures are sampled in pixels (macOS IOSurfaces), which may be larger than the picture
        fragment += "IN vec2 vTexCoord;\nuniform sampler2DRect tex0;\nuniform sampler2DRect tex1;\nuniform vec2 size0;\n" + yuv +
            "void main() {\n"
            "    fragColor = rgb(texture(tex0, vTexCoord * size0).r, texture(tex1, vTexCoord * cScale + cOffset).rg);\n"
            "}\n";
        break;
    default:
        fragment += "IN vec2 vTexCoord;\nuniform sampler2D tex0;\n"
            "void main() {\n"
            "    fragColor = TEX(tex0, vTexCoord);\n"
            "}\n";
        break;
    }

    QOpenGLShaderProgram *p = new QOpenGLShaderProgram();
    bool ok = p->addShaderFromSourceCode(QOpenGLShader::Vertex, vertex)
        && p->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment);

    if (ok)
    {
        p->bindAttributeLocation("position", 0);
        p->bindAttributeLocation("texCoord", 1);
        ok = p->link();
    }

    if (!ok)
    {
        qWarning() << "GLVideoRenderer::program: shader" << which << "failed:" << p->log();
        delete p;
        m_programFailed[which] = true;
        return nullptr;
    }

    p->bind();
    p->setUniformValue("tex0", 0);
    p->setUniformValue("tex1", 1);
    p->setUniformValue("tex2", 2);
    p->release();
    m_programs[which] = p;
    return p;
}

// pixels: the chroma texture is sampled in pixels (a rectangle texture), else is (width + 1) / 2 x (height + 1) / 2
void GLVideoRenderer::setYUVUniforms(QOpenGLShaderProgram *p, const GLVideoFrame& frame, bool pixels)
{
    // From the luma coefficients: R = Y + 2 (1 - Kr) Cr, B = Y + 2 (1 - Kb) Cb, G = (Y - Kr R - Kb B) / Kg
    float kr, kb;

    switch (frame.m_matrix)
    {
    case GLVideoFrame::BT709:
        kr = 0.2126f;
        kb = 0.0722f;
        break;
    case GLVideoFrame::BT2020:
        kr = 0.2627f;
        kb = 0.0593f;
        break;
    default:
        kr = 0.299f;
        kb = 0.114f;
        break;
    }

    const float kg = 1.0f - kr - kb;
    const float values[9] = {
        1.0f, 0.0f, 2.0f * (1.0f - kr),
        1.0f, -2.0f * kb * (1.0f - kb) / kg, -2.0f * kr * (1.0f - kr) / kg,
        1.0f, 2.0f * (1.0f - kb), 0.0f
    };
    p->setUniformValue("yuvMatrix", QMatrix3x3(values));

    // Chroma sample i is at luma sample 2i (left sited) or 2i + 0.5 (centre), so luma at u (0 to 1 across the
    // picture) has chroma at u * width / 2 + 0.25 or u * width / 2 chroma texels (texel centres at i + 0.5).
    // Likewise vertically, where only top left is co-sited
    const float cw = pixels ? 1.0f : (float) ((frame.m_width + 1) / 2);
    const float ch = pixels ? 1.0f : (float) ((frame.m_height + 1) / 2);
    const bool left = frame.m_chromaLocation != GLVideoFrame::ChromaCenter;
    const bool top = frame.m_chromaLocation == GLVideoFrame::ChromaTopLeft;
    p->setUniformValue("cScale", QVector2D(frame.m_width / (2.0f * cw), frame.m_height / (2.0f * ch)));
    p->setUniformValue("cOffset", QVector2D(left ? 0.25f / cw : 0.0f, top ? 0.25f / ch : 0.0f));

    if (frame.m_fullRange)
    {
        p->setUniformValue("yRange", QVector2D(0.0f, 1.0f));
        p->setUniformValue("cRange", QVector2D(0.5f, 1.0f));
    }
    else
    {
        p->setUniformValue("yRange", QVector2D(16.0f / 255.0f, 255.0f / 219.0f));
        p->setUniformValue("cRange", QVector2D(128.0f / 255.0f, 255.0f / 224.0f));
    }
}

// A plane in memory into one of the textures: one channel (Y, U, V) or two (interleaved UV)
void GLVideoRenderer::uploadPlane(int unit, int index, const uint8_t *data, int linesize, int width, int height, bool twoChannels)
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_textures[index]);
    const GLenum format = twoChannels ? (m_rg ? GL_RG : GL_LUMINANCE_ALPHA) : (m_rg ? GL_RED : GL_LUMINANCE);
    // OpenGL ES 2's internal format is the format (GL_EXT_texture_rg has no sized ones for glTexImage2D)
    const GLint internal = m_gles && !m_core ? (GLint) format
        : twoChannels ? (m_rg ? GL_RG8 : GL_LUMINANCE_ALPHA) : (m_rg ? GL_R8 : GL_LUMINANCE);
    const int bytesPerPixel = twoChannels ? 2 : 1;
    const bool rowLength = !m_gles || m_core;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    if (rowLength)
    {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, linesize / bytesPerPixel);
    }
    else if (linesize != width * bytesPerPixel)
    {
        // OpenGL ES 2 can't skip the padding at the ends of rows, so the rows are packed together
        const size_t row = (size_t) width * bytesPerPixel;
        m_repack.resize(row * height);

        for (int y = 0; y < height; y++) {
            std::memcpy(m_repack.data() + row * y, data + (ptrdiff_t) linesize * y, row);
        }

        data = m_repack.data();
    }

    if ((m_textureWidth[index] != width) || (m_textureHeight[index] != height) || (m_textureFormat[index] != internal))
    {
        glTexImage2D(GL_TEXTURE_2D, 0, internal, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        m_textureWidth[index] = width;
        m_textureHeight[index] = height;
        m_textureFormat[index] = internal;
    }
    else
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, data);
    }

    if (rowLength) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
}

void GLVideoRenderer::drawQuad(QOpenGLShaderProgram *p)
{
    if (m_vao) {
        m_vao->bind();
    }

    m_vertices->bind();
    p->enableAttributeArray(0);
    p->enableAttributeArray(1);
    p->setAttributeBuffer(0, GL_FLOAT, 0, 2, 4 * sizeof(GLfloat));
    p->setAttributeBuffer(1, GL_FLOAT, 2 * sizeof(GLfloat), 2, 4 * sizeof(GLfloat));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    p->disableAttributeArray(0);
    p->disableAttributeArray(1);
    m_vertices->release();

    if (m_vao) {
        m_vao->release();
    }
}

QRect GLVideoRenderer::videoRect(const GLVideoFrame& frame, int width, int height, bool keepAspect)
{
    if (!keepAspect || (frame.m_width <= 0) || (frame.m_height <= 0)) {
        return QRect(0, 0, width, height);
    }

    const double aspect = frame.m_width * (frame.m_sampleAspectRatio > 0.0 ? frame.m_sampleAspectRatio : 1.0) / frame.m_height;
    int w = width, h = (int) std::lround(width / aspect);

    if (h > height)
    {
        h = height;
        w = (int) std::lround(height * aspect);
    }

    return QRect((width - w) / 2, (height - h) / 2, w, h);
}

QRect GLVideoRenderer::subtitleRect(const GLVideoSubtitle& subtitle, const QRect& videoRect)
{
    const double sx = videoRect.width() / (double) std::max(1, subtitle.m_canvasWidth);
    const double sy = videoRect.height() / (double) std::max(1, subtitle.m_canvasHeight);
    const int x0 = videoRect.x() + (int) std::lround(subtitle.m_x * sx);
    const int y0 = videoRect.y() + (int) std::lround(subtitle.m_y * sy);
    const int x1 = videoRect.x() + (int) std::lround((subtitle.m_x + subtitle.m_width) * sx);
    const int y1 = videoRect.y() + (int) std::lround((subtitle.m_y + subtitle.m_height) * sy);
    return QRect(x0, y0, x1 - x0, y1 - y0);
}

bool GLVideoRenderer::drawFrame(const GLVideoFramePtr& frame, int width, int height, bool keepAspect, bool deinterlace)
{
    if (!m_initialized || !frame || (frame->m_width <= 0) || (frame->m_height <= 0)) {
        return false;
    }

    m_deinterlacing = deinterlace;
    const QRect r = videoRect(*frame, width, height, keepAspect);
    glViewport(r.x(), height - r.y() - r.height(), r.width(), r.height());
    glDisable(GL_BLEND);
    QOpenGLShaderProgram *p = nullptr;
    GLVideoInterop *interop = nullptr;
    bool ok = true;
    // Drawn again, as when the overlay changes: its textures still have it
    const bool again = (m_lastFrame.lock() == frame) && (deinterlace == m_lastDeinterlace);

    if ((frame->m_type == GLVideoFrame::Planar420) || (frame->m_type == GLVideoFrame::NV12))
    {
        const bool planar = frame->m_type == GLVideoFrame::Planar420;
        p = program(planar ? ProgramPlanar : ProgramNV12);

        if (!p) {
            return false;
        }

        if (again)
        {
            for (int i = 0; i < (planar ? 3 : 2); i++)
            {
                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D, m_textures[i]);
            }
        }
        else if (planar)
        {
            uploadPlane(0, 0, frame->m_data[0], frame->m_linesize[0], frame->m_width, frame->m_height, false);
            uploadPlane(1, 1, frame->m_data[1], frame->m_linesize[1], (frame->m_width + 1) / 2, (frame->m_height + 1) / 2, false);
            uploadPlane(2, 2, frame->m_data[2], frame->m_linesize[2], (frame->m_width + 1) / 2, (frame->m_height + 1) / 2, false);
        }
        else
        {
            uploadPlane(0, 0, frame->m_data[0], frame->m_linesize[0], frame->m_width, frame->m_height, false);
            uploadPlane(1, 1, frame->m_data[1], frame->m_linesize[1], (frame->m_width + 1) / 2, (frame->m_height + 1) / 2, true);
        }

        p->bind();
        setYUVUniforms(p, *frame, false);
    }
    else
    {
        interop = m_interops[frame->m_type].get();

        // Not canShow(), as frames the GPU can't deinterlace are still shown, until the decoder deinterlaces them
        if (!interop || !((m_supported.load() >> frame->m_type) & 1u)) {
            return false;
        }

        // Previous frames of the same type, for deinterlacing
        while (!m_past.empty() && (m_past.front()->m_type != frame->m_type)) {
            m_past.clear();
        }

        GLuint textures[2] = {0, 0};
        GLVideoInterop::Layout layout;

        if (!again || !interop->reacquire(textures, layout))
        {
            // The frame itself isn't one of its previous frames, if it's drawn again
            const int first = !m_past.empty() && (m_past.front() == frame) ? 1 : 0;
            const GLVideoFrame *past[4];
            const int nbPast = std::min((int) m_past.size() - first, std::min(4, interop->pastFrames()));

            for (int i = 0; i < nbPast; i++) {
                past[i] = m_past[first + i].get();
            }

            if (!interop->acquire(*frame, past, nbPast, deinterlace, textures, layout))
            {
                qWarning("GLVideoRenderer::drawFrame: GPU frames of type %d can't be shown: falling back", frame->m_type);
                m_supported &= ~(1u << frame->m_type);
                m_past.clear();
                m_lastFrame.reset();
                return false;
            }

            const bool deinterlaceFrame = deinterlace && frame->m_interlaced;

            // Shown as it is: the decoder deinterlaces them in memory instead, while deinterlacing's on
            if (deinterlaceFrame && !interop->canDeinterlace() && !((m_noDeinterlace.load() >> frame->m_type) & 1u))
            {
                qWarning("GLVideoRenderer::drawFrame: GPU frames of type %d can't be deinterlaced: falling back while deinterlacing", frame->m_type);
                m_noDeinterlace |= 1u << frame->m_type;
            }

            // Previous frames are kept (holding the decoder's surfaces) only while they're used
            if (!deinterlaceFrame || !interop->canDeinterlace() || (interop->pastFrames() <= 0))
            {
                m_past.clear();
            }
            else if (first == 0)
            {
                m_past.push_front(frame);

                while ((int) m_past.size() > interop->pastFrames()) {
                    m_past.pop_back();
                }
            }
        }

        const GLenum target = layout == GLVideoInterop::LayoutNV12Rect ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(target, textures[0]);

        if (layout != GLVideoInterop::LayoutRGBA)
        {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(target, textures[1]);
        }

        p = program(layout == GLVideoInterop::LayoutRGBA ? ProgramRGBA : layout == GLVideoInterop::LayoutNV12 ? ProgramNV12 : ProgramNV12Rect);

        // As if it couldn't be acquired: the decoder copies them to memory instead
        if (!p)
        {
            qWarning("GLVideoRenderer::drawFrame: GPU frames of type %d can't be shown: falling back", frame->m_type);
            interop->release();
            m_supported &= ~(1u << frame->m_type);
            m_past.clear();
            m_lastFrame.reset();
            return false;
        }

        p->bind();

        if (layout != GLVideoInterop::LayoutRGBA) {
            setYUVUniforms(p, *frame, layout == GLVideoInterop::LayoutNV12Rect);
        }

        if (layout == GLVideoInterop::LayoutNV12Rect) {
            p->setUniformValue("size0", QVector2D(frame->m_width, frame->m_height));
        }
    }

    m_lastFrame = frame;
    m_lastDeinterlace = deinterlace;
    drawQuad(p);
    p->release();

    if (interop) {
        interop->release();
    }

    glActiveTexture(GL_TEXTURE0);
    glViewport(0, 0, width, height);

    if (frame->m_subtitle) {
        drawSubtitle(frame->m_subtitle, r, width, height);
    }

    return ok;
}

// Uploaded once, and drawn with every frame it's attached to
void GLVideoRenderer::drawSubtitle(const GLVideoSubtitlePtr& subtitle, const QRect& videoRect, int width, int height)
{
    if ((subtitle->m_width <= 0) || (subtitle->m_height <= 0)
        || (subtitle->m_rgba.size() < (size_t) subtitle->m_width * subtitle->m_height * 4)) {
        return;
    }

    if (subtitle != m_subtitle)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_subtitleTexture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, subtitle->m_width, subtitle->m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, subtitle->m_rgba.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        m_subtitle = subtitle;
    }

    drawTexture(m_subtitleTexture, subtitleRect(*subtitle, videoRect), width, height);
}

void GLVideoRenderer::drawImage(const QImage& image, const QRect& rect, int width, int height)
{
    if (!m_initialized || image.isNull()) {
        return;
    }

    // Its texture, if it's in one, else the least recently drawn's
    const qint64 key = image.cacheKey();
    int index = 0;

    for (int i = 0; i < m_nbImages; i++)
    {
        if (m_imageKeys[i] == key)
        {
            index = i;
            break;
        }

        if (m_imageDrawn[i] < m_imageDrawn[index]) {
            index = i;
        }
    }

    if (m_imageKeys[index] != key)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_imageTextures[index]);
        const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba.width(), rgba.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.constBits());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        m_imageKeys[index] = key;
    }

    m_imageDrawn[index] = ++m_imageDraws;
    drawTexture(m_imageTextures[index], rect, width, height);
}

// Premultiplied RGBA, blended
void GLVideoRenderer::drawTexture(GLuint texture, const QRect& rect, int width, int height)
{
    QOpenGLShaderProgram *p = program(ProgramRGBA);

    if (!p) {
        return;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glViewport(rect.x(), height - rect.y() - rect.height(), rect.width(), rect.height());
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    p->bind();
    drawQuad(p);
    p->release();
    glDisable(GL_BLEND);
    glViewport(0, 0, width, height);
}
