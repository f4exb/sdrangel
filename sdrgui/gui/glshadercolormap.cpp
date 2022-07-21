///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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
#include <QOpenGLContext>
#include <QImage>
#include <QMatrix4x4>
#include <QVector4D>
#include <QDebug>

#include "gui/glshadercolormap.h"
#include "util/colormap.h"

GLShaderColorMap::GLShaderColorMap() :
    m_program(nullptr),
    m_vao(nullptr),
    m_verticesBuf(nullptr),
    m_colorMapTexture(nullptr),
    m_colorMapTextureId(0),
    m_vertexLoc(0),
    m_matrixLoc(0),
    m_colorMapLoc(0),
    m_scaleLoc(0),
    m_alphaLoc(0),
    m_useImmutableStorage(true)
{ }

GLShaderColorMap::~GLShaderColorMap()
{
    cleanup();
}

void GLShaderColorMap::initializeGL(int majorVersion, int minorVersion)
{
    initializeOpenGLFunctions();
    m_useImmutableStorage = useImmutableStorage();
    qDebug() << "GLShaderColorMap::initializeGL: m_useImmutableStorage: " << m_useImmutableStorage;

    m_program = new QOpenGLShaderProgram;
    if ((majorVersion > 3) || ((majorVersion == 3) && (minorVersion >= 3)))
    {
        if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, m_vertexShaderSourceColorMap)) {
            qDebug() << "GLShaderColorMap::initializeGL: error in vertex shader: " << m_program->log();
        }
        if (!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, m_fragmentShaderSourceColorMap)) {
            qDebug() << "GLShaderColorMap::initializeGL: error in fragment shader: " << m_program->log();
        }

        m_vao = new QOpenGLVertexArrayObject();
        m_vao->create();
        m_vao->bind();
    }
    else
    {
        if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, m_vertexShaderSourceColorMap2)) {
            qDebug() << "GLShaderColorMap::initializeGL: error in vertex shader: " << m_program->log();
        }
        if (!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, m_fragmentShaderSourceColorMap2)) {
            qDebug() << "GLShaderColorMap::initializeGL: error in fragment shader: " << m_program->log();
        }
    }

    m_program->bindAttributeLocation("vertex", 0);

    if (!m_program->link()) {
        qDebug() << "GLShaderColorMap::initializeGL: error linking shader: " << m_program->log();
    }

    m_program->bind();
    m_vertexLoc = m_program->attributeLocation("vertex");
    m_matrixLoc = m_program->uniformLocation("uMatrix");
    m_colorMapLoc = m_program->uniformLocation("colorMap");
    m_scaleLoc = m_program->uniformLocation("scale");
    m_alphaLoc = m_program->uniformLocation("alpha");
    if (m_vao)
    {
        m_verticesBuf = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
        m_verticesBuf->setUsagePattern(QOpenGLBuffer::DynamicDraw);
        m_verticesBuf->create();
        m_vao->release();
    }
    m_program->release();
}

void GLShaderColorMap::initColorMapTexture(const QString &colorMapName)
{
    if (m_useImmutableStorage) {
        initColorMapTextureImmutable(colorMapName);
    } else {
        initColorMapTextureMutable(colorMapName);
    }
}

void GLShaderColorMap::initColorMapTextureImmutable(const QString &colorMapName)
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
        qDebug() << "GLShaderColorMap::initColorMapTextureImmutable: colorMap " << colorMapName << " not supported";
    }
}

void GLShaderColorMap::initColorMapTextureMutable(const QString &colorMapName)
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
        qDebug() << "GLShaderColorMap::initColorMapTextureMutable: colorMap " << colorMapName << " not supported";
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, QOpenGLTexture::Repeat);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, QOpenGLTexture::Repeat);
}

void GLShaderColorMap::drawSurfaceStrip(const QMatrix4x4& transformMatrix, GLfloat *vertices, int nbVertices, float scale, float alpha)
{
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    m_program->bind();
    m_program->setUniformValue(m_matrixLoc, transformMatrix);
    if (m_useImmutableStorage) {
        m_colorMapTexture->bind();
    } else {
        glBindTexture(GL_TEXTURE_2D, m_colorMapTextureId);
    }
    m_program->setUniformValue(m_colorMapLoc, 0); // Texture unit 0 for color map
    m_program->setUniformValue(m_scaleLoc, scale);
    m_program->setUniformValue(m_alphaLoc, alpha);
    if (m_vao)
    {
        m_vao->bind();

        m_verticesBuf->bind();
        m_verticesBuf->allocate(vertices, nbVertices * 2 * sizeof(GL_FLOAT));
        m_program->enableAttributeArray(m_vertexLoc);
        m_program->setAttributeBuffer(m_vertexLoc, GL_FLOAT, 0, 2);
    }
    else
    {
        f->glEnableVertexAttribArray(m_vertexLoc);
        f->glVertexAttribPointer(m_vertexLoc, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    }

    f->glEnable(GL_BLEND);
    f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    f->glDrawArrays(GL_TRIANGLE_STRIP, 0, nbVertices);

    if (m_vao)
    {
        m_vao->release();
    }
    else
    {
       f->glDisableVertexAttribArray(m_vertexLoc);
    }
    m_program->release();
}

void GLShaderColorMap::cleanup()
{
    delete m_program;
    m_program = nullptr;
    delete m_vao;
    m_vao = nullptr;
    delete m_verticesBuf;
    m_verticesBuf = nullptr;
    delete m_colorMapTexture;
    m_colorMapTexture = nullptr;

    if (!QOpenGLContext::currentContext()) {
        return;
    }

    if (m_colorMapTextureId)
    {
        glDeleteTextures(1, &m_colorMapTextureId);
        m_colorMapTextureId = 0;
    }
}

bool GLShaderColorMap::useImmutableStorage()
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

const QString GLShaderColorMap::m_vertexShaderSourceColorMap2 = QString(
        "uniform highp mat4 uMatrix;\n"
        "attribute highp vec4 vertex;\n"
        "varying highp float y;\n"
        "void main() {\n"
        "    gl_Position = uMatrix * vertex;\n"
        "    y = vertex.y;\n"
        "}\n"
        );

const QString GLShaderColorMap::m_vertexShaderSourceColorMap = QString(
        "#version 330\n"
        "uniform highp mat4 uMatrix;\n"
        "in highp vec4 vertex;\n"
        "out float y;\n"
        "void main() {\n"
        "    gl_Position = uMatrix * vertex;\n"
        "    y = vertex.y;\n"
        "}\n"
        );

const QString GLShaderColorMap::m_fragmentShaderSourceColorMap2 = QString(
        "uniform highp float alpha;\n"
        "uniform highp float scale;\n"
        "uniform highp sampler2D colorMap;\n"
        "varying highp float y;\n"
        "void main() {\n"
        "    gl_FragColor = vec4(texture2D(colorMap, vec2(1.0-(y/scale), 0)).rgb, alpha);\n"
        "}\n"
        );

const QString GLShaderColorMap::m_fragmentShaderSourceColorMap = QString(
        "#version 330\n"
        "uniform float alpha;\n"
        "uniform float scale;\n"
        "uniform sampler2D colorMap;\n"
        "in float y;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "   fragColor = vec4(texture(colorMap, vec2(1.0-(y/scale), 0)).rgb, alpha);\n"
        "}\n"
        );
