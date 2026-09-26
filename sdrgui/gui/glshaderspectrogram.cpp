///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Copyright (C) 2016 Edouard Griffiths, F4EXB.                                  //
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

#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions>
#if defined(ANDROID)
#include <QOpenGLFunctions_ES2>
#else
#include <QOpenGLFunctions_3_3_Core>
#endif
#include <QOpenGLContext>
#include <QtOpenGL>
#include <QImage>
#include <QMatrix4x4>
#include <QVector4D>
#include <QDebug>

#ifdef ANDROID
#include <GLES3/gl3.h>
#endif

#include "gui/glshaderspectrogram.h"
#include "util/colormap.h"

GLShaderSpectrogram::GLShaderSpectrogram() :
    m_programShaded(nullptr),
    m_programSimple(nullptr),
    m_texture(nullptr),
    m_textureId(0),
    m_colorMapTexture(nullptr),
    m_colorMapTextureId(0),
    m_textureWidth(0),
    m_textureHeight(0),
    m_programForLocs(nullptr),
    m_coord2dLoc(0),
    m_textureTransformLoc(0),
    m_vertexTransformLoc(0),
    m_dataTextureLoc(0),
    m_brightnessLoc(0),
    m_colorMapLoc(0),
    m_lightDirLoc(0),
    m_lightPosLoc(0),
    m_useImmutableStorage(true),
    m_vao(nullptr),
    m_vertexBuf(nullptr),
    m_index0Buf(nullptr),
    m_index1Buf(nullptr),
    m_translateX(0.0),
    m_translateY(0.0),
    m_translateZ(0.0),
    m_rotX(-45.0),
    m_rotY(0.0),
    m_rotZ(0.0),
    m_scaleX(1.0),
    m_scaleY(1.0),
    m_scaleZ(1.0),
    m_userScaleZ(1.0),
    m_verticalAngle(45.0f),
    m_aspectRatio(1600.0/1200.0),
    m_lightTranslateX(0.0),
    m_lightTranslateY(0.0),
    m_lightTranslateZ(0.0),
    m_lightRotX(0.0),
    m_lightRotY(0.0),
    m_lightRotZ(0.0),
    m_gridWidthElements(0),
    m_gridHeightElements(0),
    m_gridLineIndexCount(0),
    m_gridTriangleIndexCount(0)
{
}

GLShaderSpectrogram::~GLShaderSpectrogram()
{
    cleanup();
}

void GLShaderSpectrogram::initializeGL(int majorVersion, int minorVersion)
{
    initializeOpenGLFunctions();
    m_useImmutableStorage = useImmutableStorage();
    qDebug() << "GLShaderSpectrogram::initializeGL: m_useImmutableStorage: " << m_useImmutableStorage;

    if ((majorVersion > 3) || ((majorVersion == 3) && (minorVersion >= 3)))
    {
        m_programShaded = new QOpenGLShaderProgram;
        if (!m_programShaded->addShaderFromSourceCode(QOpenGLShader::Vertex, m_vertexShader)) {
            qDebug() << "GLShaderSpectrogram::initializeGL: error in vertex shader: " << m_programShaded->log();
        }
        if (!m_programShaded->addShaderFromSourceCode(QOpenGLShader::Geometry, m_geometryShader)) {
            qDebug() << "GLShaderSpectrogram::initializeGL: error in geometry shader: " << m_programShaded->log();
        }
        if (!m_programShaded->addShaderFromSourceCode(QOpenGLShader::Fragment, m_fragmentShaderShaded)) {
            qDebug() << "GLShaderSpectrogram::initializeGL: error in fragment shader: " << m_programShaded->log();
        }
        if (!m_programShaded->link()) {
            qDebug() << "GLShaderSpectrogram::initializeGL: error linking shader: " << m_programShaded->log();
        }

        m_programSimple = new QOpenGLShaderProgram;
        if (!m_programSimple->addShaderFromSourceCode(QOpenGLShader::Vertex, m_vertexShader)) {
            qDebug() << "GLShaderSpectrogram::initializeGL: error in vertex shader: " << m_programSimple->log();
        }
        if (!m_programSimple->addShaderFromSourceCode(QOpenGLShader::Fragment, m_fragmentShaderSimple)) {
            qDebug() << "GLShaderSpectrogram::initializeGL: error in fragment shader: " << m_programSimple->log();
        }
        if (!m_programSimple->link()) {
            qDebug() << "GLShaderSpectrogram::initializeGL: error linking shader: " << m_programSimple->log();
        }

        m_vao = new QOpenGLVertexArrayObject();
        m_vao->create();
        m_vao->bind();
    }
    else
    {
        m_programSimple = new QOpenGLShaderProgram;
        if (!m_programSimple->addShaderFromSourceCode(QOpenGLShader::Vertex, m_vertexShader2)) {
            qDebug() << "GLShaderSpectrogram::initializeGL: error in vertex shader: " << m_programSimple->log();
        }
        if (!m_programSimple->addShaderFromSourceCode(QOpenGLShader::Fragment, m_fragmentShaderSimple2)) {
            qDebug() << "GLShaderSpectrogram::initializeGL: error in fragment shader: " << m_programSimple->log();
        }
        if (!m_programSimple->link()) {
            qDebug() << "GLShaderSpectrogram::initializeGL: error linking shader: " << m_programSimple->log();
        }
    }

    m_vertexBuf = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    m_vertexBuf->setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_vertexBuf->create();
    m_index0Buf = new QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
    m_index0Buf->setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_index0Buf->create();
    m_index1Buf = new QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
    m_index1Buf->setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_index1Buf->create();

    if (m_vao) {
        m_vao->release();
    }
}

void GLShaderSpectrogram::initGrid(int width, int height)
{
    // There is one vertex per texture sample where possible. The final frequency
    // sample is the extra endpoint written by GLSpectrumView, so W samples form
    // W-1 cells. Keeping the time vertices aligned with the H texture rows stops
    // a stored spectrum being resampled as it advances through the circular texture.
    int gridWidthElements = std::max(1, std::min(width - 1, 4096));
    int gridHeightElements = std::max(1, std::min(height - 1, 4096));

    if ((gridWidthElements == m_gridWidthElements) && (gridHeightElements == m_gridHeightElements)) {
        return;
    }

    m_gridWidthElements = gridWidthElements;
    m_gridHeightElements = gridHeightElements;
    qDebug() << "GLShaderSpectrogram::initGrid: requested: " << width - 1 << "x" << height - 1
             << " actual: " << m_gridWidthElements << "x" << m_gridHeightElements;
    int verticesPerRow = m_gridWidthElements + 1;

    // Grid vertices
    std::vector<QVector2D> vertices((m_gridWidthElements + 1) * (m_gridHeightElements + 1));

    for (int y = 0; y <= m_gridHeightElements; y++)
    {
        for (int x = 0; x <= m_gridWidthElements; x++)
        {
            vertices[y * verticesPerRow + x].setX(x / (float)m_gridWidthElements);
            vertices[y * verticesPerRow + x].setY(y / (float)m_gridHeightElements);
        }
    }

    if (m_vao) {
        m_vao->bind();
    }

    m_vertexBuf->bind();
    m_vertexBuf->allocate(&vertices[0], vertices.size() * sizeof(QVector2D));

    if (m_vao)
    {
        m_programShaded->enableAttributeArray(m_coord2dLoc);
        m_programShaded->setAttributeBuffer(m_coord2dLoc, GL_FLOAT, 0, 2);
        m_programSimple->enableAttributeArray(m_coord2dLoc);
        m_programSimple->setAttributeBuffer(m_coord2dLoc, GL_FLOAT, 0, 2);
        m_vao->release();
    }

    // Create an array of indices into the vertex array that traces both horizontal and vertical lines
    std::vector<GLuint> lineIndices;
    lineIndices.reserve(
        2 * (m_gridWidthElements * (m_gridHeightElements + 1)
            + m_gridHeightElements * (m_gridWidthElements + 1)));

    for (int y = 0; y <= m_gridHeightElements; y++) {
        for (int x = 0; x < m_gridWidthElements; x++) {
            lineIndices.push_back(y * verticesPerRow + x);
            lineIndices.push_back(y * verticesPerRow + x + 1);
        }
    }

    for (int x = 0; x <= m_gridWidthElements; x++) {
        for (int y = 0; y < m_gridHeightElements; y++) {
            lineIndices.push_back(y * verticesPerRow + x);
            lineIndices.push_back((y + 1) * verticesPerRow + x);
        }
    }

    m_gridLineIndexCount = (int) lineIndices.size();
    m_index0Buf->bind();
    m_index0Buf->allocate(lineIndices.data(), m_gridLineIndexCount * sizeof(GLuint));

    // Create an array of indices that describes all the triangles needed to create a completely filled surface
    std::vector<GLuint> triangleIndices;
    triangleIndices.reserve(m_gridWidthElements * m_gridHeightElements * 6);

    for (int y = 0; y < m_gridHeightElements; y++) {
        for (int x = 0; x < m_gridWidthElements; x++) {
            triangleIndices.push_back(y * verticesPerRow + x);
            triangleIndices.push_back(y * verticesPerRow + x + 1);
            triangleIndices.push_back((y + 1) * verticesPerRow + x + 1);

            triangleIndices.push_back(y * verticesPerRow + x);
            triangleIndices.push_back((y + 1) * verticesPerRow + x + 1);
            triangleIndices.push_back((y + 1) * verticesPerRow + x);
        }
    }

    m_gridTriangleIndexCount = (int) triangleIndices.size();
    m_index1Buf->bind();
    m_index1Buf->allocate(triangleIndices.data(), m_gridTriangleIndexCount * sizeof(GLuint));

    if (!m_vao)
    {
        QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
        f->glBindBuffer(GL_ARRAY_BUFFER, 0);
        f->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}

void GLShaderSpectrogram::initColorMapTexture(const QString &colorMapName)
{
    if (m_useImmutableStorage) {
        initColorMapTextureImmutable(colorMapName);
    } else {
        initColorMapTextureMutable(colorMapName);
    }
}

void GLShaderSpectrogram::initColorMapTextureImmutable(const QString &colorMapName)
{
    if (!m_colorMapTexture)
    {
        m_colorMapTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
        m_colorMapTexture->setFormat(QOpenGLTexture::RGB32F);
        m_colorMapTexture->setSize(256, 1);
        m_colorMapTexture->allocateStorage();
        m_colorMapTexture->setMinificationFilter(QOpenGLTexture::Linear);
        m_colorMapTexture->setMagnificationFilter(QOpenGLTexture::Linear);
        m_colorMapTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
    }

    GLfloat *colorMap = (GLfloat *)ColorMap::getColorMap(colorMapName);
    if (colorMap) {
        m_colorMapTexture->setData(QOpenGLTexture::RGB, QOpenGLTexture::Float32, colorMap);
    } else {
        qDebug() << "GLShaderSpectrogram::initColorMapTextureImmutable: colorMap " << colorMapName << " not supported";
    }
}

void GLShaderSpectrogram::initColorMapTextureMutable(const QString &colorMapName)
{
    if (m_colorMapTextureId)
    {
        glDeleteTextures(1, &m_colorMapTextureId);
        m_colorMapTextureId = 0;
    }

    glGenTextures(1, &m_colorMapTextureId);
    glBindTexture(GL_TEXTURE_2D, m_colorMapTextureId); // Use 2D texture as 1D not supported in OpenGL ES on ARM
    GLfloat *colorMap = (GLfloat *)ColorMap::getColorMap(colorMapName);
    if (colorMap) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 1, 0, GL_RGB, GL_FLOAT, colorMap);
    } else {
        qDebug() << "GLShaderSpectrogram::initColorMapTextureMutable: colorMap " << colorMapName << " not supported";
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, QOpenGLTexture::Repeat);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, QOpenGLTexture::Repeat);
}

void GLShaderSpectrogram::initTexture(const QImage& image)
{
    if (m_useImmutableStorage) {
        initTextureImmutable(image);
    } else {
        initTextureMutable(image);
    }
    m_textureWidth = image.width();
    m_textureHeight = image.height();
    initGrid(m_textureWidth, m_textureHeight);
}

void GLShaderSpectrogram::initTextureImmutable(const QImage& image)
{
    if (m_texture) {
        delete m_texture;
    }

    m_texture = new QOpenGLTexture(image);

    m_texture->setMinificationFilter(QOpenGLTexture::Linear);
    m_texture->setMagnificationFilter(QOpenGLTexture::Linear);
    m_texture->setWrapMode(QOpenGLTexture::DirectionS, QOpenGLTexture::ClampToEdge);
    m_texture->setWrapMode(QOpenGLTexture::DirectionT, QOpenGLTexture::Repeat);
}

void GLShaderSpectrogram::initTextureMutable(const QImage& image)
{
    if (m_textureId)
    {
        glDeleteTextures(1, &m_textureId);
        m_textureId = 0;
    }

    glGenTextures(1, &m_textureId);
    glBindTexture(GL_TEXTURE_2D, m_textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
        image.width(), image.height(), 0, GL_RED, GL_UNSIGNED_BYTE, image.constScanLine(0));

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, QOpenGLTexture::ClampToEdge);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, QOpenGLTexture::Repeat);
}

void GLShaderSpectrogram::subTexture(int xOffset, int yOffset, int width, int height, const void *pixels)
{
    if (m_useImmutableStorage) {
        subTextureImmutable(xOffset, yOffset, width, height, pixels);
    } else {
        subTextureMutable(xOffset, yOffset, width, height, pixels);
    }
}

void GLShaderSpectrogram::subTextureImmutable(int xOffset, int yOffset, int width, int height, const void *pixels)
{
    if (!m_texture)
    {
        qDebug("GLShaderSpectrogram::subTextureImmutable: no texture defined. Doing nothing");
        return;
    }

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    m_texture->bind();
    f->glTexSubImage2D(GL_TEXTURE_2D, 0, xOffset, yOffset, width, height, GL_RED, GL_UNSIGNED_BYTE, pixels);
}

void GLShaderSpectrogram::subTextureMutable(int xOffset, int yOffset, int width, int height, const void *pixels)
{
    if (!m_textureId)
    {
        qDebug("GLShaderSpectrogram::subTextureMutable: no texture defined. Doing nothing");
        return;
    }

    glBindTexture(GL_TEXTURE_2D, m_textureId);
    glTexSubImage2D(GL_TEXTURE_2D, 0, xOffset, yOffset, width, height, GL_RED, GL_UNSIGNED_BYTE, pixels);
}

void GLShaderSpectrogram::drawSurface(SpectrumSettings::SpectrogramStyle style, const QMatrix4x4& vertexTransform, int texturePosition, bool invert)
{
    if ((m_useImmutableStorage && !m_texture) || (!m_useImmutableStorage && !m_textureId))
    {
        qDebug("GLShaderSpectrogram::drawSurface: no texture defined. Doing nothing");
        return;
    }

    QOpenGLShaderProgram *program;
    if (style == SpectrumSettings::Shaded) {
        program = m_programShaded;
    } else {
        program = m_programSimple;
    }
    if (!program) {
        return;
    }

    float direction = invert ? 1.0f : -1.0f;
    float textureScaleX = (m_textureWidth - 1.0f) / m_textureWidth;
    float textureScaleY = (m_textureHeight - 1.0f) / m_textureHeight;
    float textureOffsetX = 0.5f / m_textureWidth;
    float textureOffsetY = (texturePosition + direction * 0.5f) / m_textureHeight;
    QMatrix4x4 textureTransform(
        textureScaleX, 0.0, 0.0, textureOffsetX,
        0.0, direction * textureScaleY, 0.0, textureOffsetY,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0);        // Use this to move texture for each row of data

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    program->bind();

    if (program != m_programForLocs)
    {
        m_coord2dLoc = program->attributeLocation("coord2d");
        m_textureTransformLoc = program->uniformLocation("textureTransform");
        m_vertexTransformLoc = program->uniformLocation("vertexTransform");
        m_dataTextureLoc = program->uniformLocation("dataTexture");
        m_brightnessLoc = program->uniformLocation("brightness");
        m_colorMapLoc = program->uniformLocation("colorMap");
        m_lightDirLoc = program->uniformLocation("lightDir");
        m_lightPosLoc = program->uniformLocation("lightPos");
        m_programForLocs = program;
    }

    program->setUniformValue(m_vertexTransformLoc, vertexTransform);
    program->setUniformValue(m_textureTransformLoc, textureTransform);

    if (m_useImmutableStorage)
    {
        m_texture->bind();
        glActiveTexture(GL_TEXTURE1);
        m_colorMapTexture->bind();
        glActiveTexture(GL_TEXTURE0);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, m_textureId);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_colorMapTextureId);
        glActiveTexture(GL_TEXTURE0);
    }

    program->setUniformValue(m_dataTextureLoc, 0);         // set uniform to texture unit?
    program->setUniformValue(m_colorMapLoc, 1);

    if (style == SpectrumSettings::Outline)
    {
        // When drawing the outline, slightly darken the triangles
        // so grid is a bit more visible
        program->setUniformValue(m_brightnessLoc, 0.85f);
    }
    else
    {
        program->setUniformValue(m_brightnessLoc, 1.0f);
    }

    if (style == SpectrumSettings::Shaded)
    {
        QMatrix4x4 matrix;
        matrix.rotate(m_lightRotX, 1.0f, 0.0f, 0.0f);
        matrix.rotate(m_lightRotY, 0.0f, 1.0f, 0.0f);
        matrix.rotate(m_lightRotZ, 0.0f, 0.0f, 1.0f);
        QVector3D vector = matrix * QVector3D(0.0f, 0.0f, -1.0f);
        GLfloat lightDir[3] = {vector.x(), vector.y(), vector.z()};
        GLfloat lightPos[3] = {m_lightTranslateX, m_lightTranslateY, m_lightTranslateZ};
        program->setUniformValueArray(m_lightDirLoc, lightDir, 1, 3);
        program->setUniformValueArray(m_lightPosLoc, lightPos, 1, 3);
    }

    f->glEnable(GL_DEPTH_TEST);

    f->glPolygonOffset(1, 0);
    f->glEnable(GL_POLYGON_OFFSET_FILL);

    if (m_vao)
    {
        m_vao->bind();
    }
    else
    {
        m_vertexBuf->bind();
        f->glEnableVertexAttribArray(m_coord2dLoc);
        f->glVertexAttribPointer(m_coord2dLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
    }
    m_index1Buf->bind();

    switch (style)
    {
    case SpectrumSettings::Points:
        f->glDrawElements(GL_POINTS, m_gridTriangleIndexCount, GL_UNSIGNED_INT, 0);
        break;
    case SpectrumSettings::Lines:
        f->glDrawElements(GL_LINES, m_gridTriangleIndexCount, GL_UNSIGNED_INT, 0);
        break;
    case SpectrumSettings::Solid:
    case SpectrumSettings::Outline:
    case SpectrumSettings::Shaded:
        f->glDrawElements(GL_TRIANGLES, m_gridTriangleIndexCount, GL_UNSIGNED_INT, 0);
        break;
    }

    f->glPolygonOffset(0, 0);
    f->glDisable(GL_POLYGON_OFFSET_FILL);

    if (style == SpectrumSettings::Outline)
    {
        // Draw the outline
        program->setUniformValue(m_brightnessLoc, 1.5f);
        m_index0Buf->bind();
        f->glDrawElements(GL_LINES, m_gridLineIndexCount, GL_UNSIGNED_INT, 0);
    }

    if (m_vao)
    {
        m_vao->release();
        m_index0Buf->release();
    }
    else
    {
        f->glDisableVertexAttribArray(m_coord2dLoc);
        // Need to do this, otherwise nothing else is drawn by other shaders
        f->glBindBuffer(GL_ARRAY_BUFFER, 0);
        f->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    // If this is left enabled, channel markers aren't drawn properly
    f->glDisable(GL_DEPTH_TEST);

    program->release();
}

void GLShaderSpectrogram::cleanup()
{
    delete m_vao;
    m_vao = nullptr;
    delete m_programShaded;
    m_programShaded = nullptr;
    delete m_programSimple;
    m_programSimple = nullptr;
    m_programForLocs = nullptr;
    delete m_texture;
    m_texture = nullptr;
    delete m_colorMapTexture;
    m_colorMapTexture = nullptr;
    delete m_vertexBuf;
    m_vertexBuf = nullptr;
    delete m_index0Buf;
    m_index0Buf = nullptr;
    delete m_index1Buf;
    m_index1Buf = nullptr;

    if (!QOpenGLContext::currentContext()) {
        return;
    }

    if (m_textureId)
    {
        glDeleteTextures(1, &m_textureId);
        m_textureId = 0;
    }

    if (m_colorMapTextureId)
    {
        glDeleteTextures(1, &m_colorMapTextureId);
        m_colorMapTextureId = 0;
    }
}

bool GLShaderSpectrogram::useImmutableStorage()
{
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    QSurfaceFormat sf = ctx->format();

    if (sf.version() >= qMakePair(4, 2)
        || ctx->hasExtension(QByteArrayLiteral("GL_ARB_texture_storage"))
        || ctx->hasExtension(QByteArrayLiteral("GL_EXT_texture_storage")))
    {
        void (QOPENGLF_APIENTRYP glTexStorage2D)(
            GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height);
        glTexStorage2D = reinterpret_cast<void (QOPENGLF_APIENTRYP)(
            GLenum, GLsizei, GLenum, GLsizei, GLsizei)>(ctx->getProcAddress("glTexStorage2D"));
        int data = 0;
        GLuint textureId;
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data);
        GLenum err = glGetError();
        glDeleteTextures(1, &textureId);
        return err == GL_NO_ERROR;
    }

    return false;
}

void GLShaderSpectrogram::translateX(float distance)
{
    m_translateX += distance;
}

void GLShaderSpectrogram::translateY(float distance)
{
    m_translateY += distance;
}

void GLShaderSpectrogram::translateZ(float distance)
{
    m_translateZ += distance;
}

void GLShaderSpectrogram::rotateX(float degrees)
{
    m_rotX += degrees;
}

void GLShaderSpectrogram::rotateY(float degrees)
{
    m_rotY += degrees;
}

void GLShaderSpectrogram::rotateZ(float degrees)
{
    m_rotZ += degrees;
}

void GLShaderSpectrogram::setScaleX(float factor)
{
    m_scaleX = factor;
}

void GLShaderSpectrogram::setScaleY(float factor)
{
    m_scaleY = factor;
}

void GLShaderSpectrogram::setScaleZ(float factor)
{
    m_scaleZ = factor;
}

void GLShaderSpectrogram::userScaleZ(float factor)
{
    m_userScaleZ *= factor;
    if ((m_userScaleZ < 0.1) && (factor < 1.0)) {
        m_userScaleZ = 0.0;
    } else if ((m_userScaleZ == 0.0) && (factor > 1.0)) {
        m_userScaleZ = 0.1;
    }
}

void GLShaderSpectrogram::reset()
{
    // Don't reset m_scaleX, m_scaleY and m_scaleZ, m_aspectRatio
    // as they are not set directly by user, but depend on window size
    m_translateX = 0.0f;
    m_translateY = 0.0f;
    m_translateZ = 0.0f;
    m_rotX = -45.0f;
    m_rotY = 0.0f;
    m_rotZ = 0.0f;
    m_userScaleZ = 1.0f;
    m_verticalAngle = 45.0f;
    m_lightTranslateX = 0.0f;
    m_lightTranslateY = 0.0f;
    m_lightTranslateZ = 0.0f;
    m_lightRotX = 0.0f;
    m_lightRotY = 0.0f;
    m_lightRotZ = 0.0f;
    setPerspective();
}

void GLShaderSpectrogram::setAspectRatio(float aspectRatio)
{
    m_aspectRatio = aspectRatio;
    setPerspective();
}

void GLShaderSpectrogram::verticalAngle(float degrees)
{
    m_verticalAngle += degrees;
    m_verticalAngle = std::max(1.0f, m_verticalAngle);
    m_verticalAngle = std::min(179.0f, m_verticalAngle);
    setPerspective();
}

void GLShaderSpectrogram::setPerspective()
{
    m_perspective.setToIdentity();
    m_perspective.perspective(m_verticalAngle, m_aspectRatio, 0.1, 7.0);
}

void GLShaderSpectrogram::lightTranslateX(float distance)
{
    m_lightTranslateX += distance;
}

void GLShaderSpectrogram::lightTranslateY(float distance)
{
    m_lightTranslateY += distance;
}

void GLShaderSpectrogram::lightTranslateZ(float distance)
{
    m_lightTranslateZ += distance;
}

void GLShaderSpectrogram::lightRotateX(float degrees)
{
    m_lightRotX += degrees;
}

void GLShaderSpectrogram::lightRotateY(float degrees)
{
    m_lightRotY += degrees;
}

void GLShaderSpectrogram::lightRotateZ(float degrees)
{
    m_lightRotZ += degrees;
}

void GLShaderSpectrogram::applyTransform(QMatrix4x4 &matrix)
{
    // Note that translation to the origin and rotation
    // needs to be performed in reverse order to what you
    // might normally expect
    // See: https://bugreports.qt.io/browse/QTBUG-20752
    matrix.translate(0.0f, 0.0f, -1.65f);                       // Camera position
    matrix.translate(m_translateX, m_translateY, m_translateZ); // User camera position adjustment
    matrix.rotate(m_rotX, 1.0f, 0.0f, 0.0f);                    // User rotation
    matrix.rotate(m_rotY, 0.0f, 1.0f, 0.0f);
    matrix.rotate(m_rotZ, 0.0f, 0.0f, 1.0f);
    matrix.scale(m_scaleX, m_scaleY, m_scaleZ * m_userScaleZ);  // Scaling
    matrix.translate(-0.5f, -0.5f, 0.0f);                       // Centre at origin for correct rotation
    applyPerspective(matrix);
}

void GLShaderSpectrogram::applyPerspective(QMatrix4x4 &matrix)
{
    matrix = m_perspective * matrix;
}

const QString GLShaderSpectrogram::m_vertexShader2 = QString(
    "attribute vec2 coord2d;\n"
    "varying vec4 coord;\n"
    "varying highp float lightDistance;\n"
    "uniform mat4 textureTransform;\n"
    "uniform mat4 vertexTransform;\n"
    "uniform sampler2D dataTexture;\n"
    "uniform vec3 lightPos;\n"
    "void main(void) {\n"
    "   coord = textureTransform * vec4(coord2d, 0, 1);\n"
    "   coord.z = (texture2D(dataTexture, coord.xy).r);\n"
    "   gl_Position = vertexTransform * vec4(coord2d, coord.z, 1);\n"
    "   lightDistance = length(lightPos - gl_Position.xyz);\n"
    "}\n"
    );

const QString GLShaderSpectrogram::m_vertexShader = QString(
    "#version 330\n"
    "in vec2 coord2d;\n"
    "out vec4 coord;\n"
    "out float lightDistance;\n"
    "uniform mat4 textureTransform;\n"
    "uniform mat4 vertexTransform;\n"
    "uniform sampler2D dataTexture;\n"
    "uniform vec3 lightPos;\n"
    "void main(void) {\n"
    "   coord = textureTransform * vec4(coord2d, 0, 1);\n"
    "   coord.z = (texture(dataTexture, coord.xy).r);\n"
    "   gl_Position = vertexTransform * vec4(coord2d, coord.z, 1);\n"
    "   lightDistance = length(lightPos - gl_Position.xyz);\n"
    "}\n"
    );

// We need to use a geometry shader to calculate the normals, as they are only
// determined after z has been calculated for the vertices in the vertex shader
const QString GLShaderSpectrogram::m_geometryShader = QString(
    "#version 330\n"
    "layout(triangles) in;\n"
    "layout(triangle_strip, max_vertices=3) out;\n"
    "in vec4 coord[];\n"
    "in float lightDistance[];\n"
    "out vec4 coord2;\n"
    "out vec3 normal;\n"
    "out float lightDistance2;\n"
    "void main(void) {\n"
    "    vec3 a = (gl_in[1].gl_Position - gl_in[0].gl_Position).xyz;\n"
    "    vec3 b = (gl_in[2].gl_Position - gl_in[0].gl_Position).xyz;\n"
    "    vec3 N = normalize(cross(b, a));\n"
    "    for(int i=0; i < gl_in.length(); ++i)\n"
    "    {\n"
    "        gl_Position = gl_in[i].gl_Position;\n"
    "        normal = N;\n"
    "        coord2 = coord[i];\n"
    "        lightDistance2 = lightDistance[i];\n"
    "        EmitVertex( );\n"
    "    }\n"
    "    EndPrimitive( );\n"
    "}\n"
    );

const QString GLShaderSpectrogram::m_fragmentShaderShaded = QString(
    "#version 330\n"
    "in vec4 coord2;\n"
    "in vec3 normal;\n"
    "in float lightDistance2;\n"
    "out vec4 fragColor;\n"
    "uniform sampler2D colorMap;\n"
    "uniform vec3 lightDir;\n"
    "void main(void) {\n"
    "    float factor;\n"
    "    if (gl_FrontFacing)\n"
    "        factor = 1.0;\n"
    "    else\n"
    "        factor = 0.5;\n"
    "    float ambient = 0.4;\n"
    "    vec3 color;\n"
    "    color.r = texture(colorMap, vec2(coord2.z, 0)).r;\n"
    "    color.g = texture(colorMap, vec2(coord2.z, 0)).g;\n"
    "    color.b = texture(colorMap, vec2(coord2.z, 0)).b;\n"
    "    float cosTheta = max(0.0, dot(normal, lightDir));\n"
    "    float d2 = max(1.0, lightDistance2*lightDistance2);\n"
    "    vec3 relection = (ambient * color + color * cosTheta / d2) * factor;\n"
    "    fragColor[0] = relection.r;\n"
    "    fragColor[1] = relection.g;\n"
    "    fragColor[2] = relection.b;\n"
    "    fragColor[3] = 1.0;\n"
    "}\n"
    );

const QString GLShaderSpectrogram::m_fragmentShaderSimple2 = QString(
    "varying highp vec4 coord;\n"
    "uniform highp float brightness;\n"
    "uniform sampler2D colorMap;\n"
    "void main(void) {\n"
    "    highp float factor;\n"
    "    if (gl_FrontFacing)\n"
    "        factor = 1.0;\n"
    "    else\n"
    "        factor = 0.5;\n"
    "    gl_FragColor[0] = texture2D(colorMap, vec2(coord.z, 0)).r * brightness * factor;\n"
    "    gl_FragColor[1] = texture2D(colorMap, vec2(coord.z, 0)).g * brightness * factor;\n"
    "    gl_FragColor[2] = texture2D(colorMap, vec2(coord.z, 0)).b * brightness * factor;\n"
    "    gl_FragColor[3] = 1.0;\n"
    "}\n"
    );

const QString GLShaderSpectrogram::m_fragmentShaderSimple = QString(
    "#version 330\n"
    "in vec4 coord;\n"
    "out vec4 fragColor;\n"
    "uniform float brightness;\n"
    "uniform sampler2D colorMap;\n"
    "void main(void) {\n"
    "    float factor;\n"
    "    if (gl_FrontFacing)\n"
    "        factor = 1.0;\n"
    "    else\n"
    "        factor = 0.5;\n"
    "    fragColor[0] = texture(colorMap, vec2(coord.z, 0)).r * brightness * factor;\n"
    "    fragColor[1] = texture(colorMap, vec2(coord.z, 0)).g * brightness * factor;\n"
    "    fragColor[2] = texture(colorMap, vec2(coord.z, 0)).b * brightness * factor;\n"
    "    fragColor[3] = 1.0;\n"
    "}\n"
    );
