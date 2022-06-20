///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include "gui/glshadertextured.h"

GLShaderTextured::GLShaderTextured() :
    m_program(nullptr),
    m_vao(nullptr),
    m_verticesBuf(nullptr),
    m_textureCoordsBuf(nullptr),
    m_texture(nullptr),
    m_textureId(0),
    m_vertexLoc(0),
    m_texCoordLoc(0),
    m_matrixLoc(0),
    m_textureLoc(0),
    m_useImmutableStorage(true)
{ }

GLShaderTextured::~GLShaderTextured()
{
    cleanup();
}

void GLShaderTextured::initializeGL(int majorVersion, int minorVersion)
{
    initializeOpenGLFunctions();
    m_useImmutableStorage = useImmutableStorage();
    qDebug() << "GLShaderTextured::initializeGL: m_useImmutableStorage: " << m_useImmutableStorage;

    m_program = new QOpenGLShaderProgram;
    if ((majorVersion > 3) || ((majorVersion == 3) && (minorVersion >= 3)))
    {
        if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, m_vertexShaderSourceTextured)) {
            qDebug() << "GLShaderTextured::initializeGL: error in vertex shader: " << m_program->log();
        }
        if (!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, m_fragmentShaderSourceTextured)) {
            qDebug() << "GLShaderTextured::initializeGL: error in fragment shader: " << m_program->log();
        }

        m_vao = new QOpenGLVertexArrayObject();
        m_vao->create();
        m_vao->bind();
    }
    else
    {
        if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, m_vertexShaderSourceTextured2)) {
            qDebug() << "GLShaderTextured::initializeGL: error in vertex shader: " << m_program->log();
        }
        if (!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, m_fragmentShaderSourceTextured2)) {
            qDebug() << "GLShaderTextured::initializeGL: error in fragment shader: " << m_program->log();
        }
    }

    m_program->bindAttributeLocation("vertex", 0);
    m_program->bindAttributeLocation("texCoord", 1);

    if (!m_program->link()) {
        qDebug() << "GLShaderTextured::initializeGL: error linking shader: " << m_program->log();
    }

    m_program->bind();
    m_vertexLoc = m_program->attributeLocation("vertex");
    m_texCoordLoc = m_program->attributeLocation("texCoord");
    m_matrixLoc = m_program->uniformLocation("uMatrix");
    m_textureLoc = m_program->uniformLocation("uTexture");
    if (m_vao)
    {
        m_verticesBuf = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
        m_verticesBuf->setUsagePattern(QOpenGLBuffer::DynamicDraw);
        m_verticesBuf->create();
        m_textureCoordsBuf = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
        m_textureCoordsBuf->setUsagePattern(QOpenGLBuffer::DynamicDraw);
        m_textureCoordsBuf->create();
        m_vao->release();
    }
    m_program->release();
}

void GLShaderTextured::initTexture(const QImage& image, QOpenGLTexture::WrapMode wrapMode)
{
    if (m_useImmutableStorage) {
        initTextureImmutable(image, wrapMode);
    } else {
        initTextureMutable(image, wrapMode);
    }
}

void GLShaderTextured::initTextureImmutable(const QImage& image, QOpenGLTexture::WrapMode wrapMode)
{
    if (m_texture) {
        delete m_texture;
    }

    m_texture = new QOpenGLTexture(image);

    m_texture->setMinificationFilter(QOpenGLTexture::Linear);
    m_texture->setMagnificationFilter(QOpenGLTexture::Linear);
    m_texture->setWrapMode(wrapMode);
}

void GLShaderTextured::initTextureMutable(const QImage& image, QOpenGLTexture::WrapMode wrapMode)
{
    if (m_textureId)
    {
        glDeleteTextures(1, &m_textureId);
        m_textureId = 0;
    }

    glGenTextures(1, &m_textureId);
    glBindTexture(GL_TEXTURE_2D, m_textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
        image.width(), image.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, image.constScanLine(0));

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);
}

void GLShaderTextured::subTexture(int xOffset, int yOffset, int width, int height, const void *pixels)
{
    if (m_useImmutableStorage) {
        subTextureImmutable(xOffset, yOffset, width, height, pixels);
    } else {
        subTextureMutable(xOffset, yOffset, width, height, pixels);
    }
}

void GLShaderTextured::subTextureImmutable(int xOffset, int yOffset, int width, int height, const void *pixels)
{
    if (!m_texture)
    {
        qDebug("GLShaderTextured::subTextureImmutable: no texture defined. Doing nothing");
        return;
    }

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    m_texture->bind();
    f->glTexSubImage2D(GL_TEXTURE_2D, 0, xOffset, yOffset, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

void GLShaderTextured::subTextureMutable(int xOffset, int yOffset, int width, int height, const void *pixels)
{
    if (!m_textureId)
    {
        qDebug("GLShaderTextured::subTextureMutable: no texture defined. Doing nothing");
        return;
    }

    glBindTexture(GL_TEXTURE_2D, m_textureId);
    glTexSubImage2D(GL_TEXTURE_2D, 0, xOffset, yOffset, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

void GLShaderTextured::drawSurface(const QMatrix4x4& transformMatrix, GLfloat *textureCoords, GLfloat *vertices, int nbVertices, int nbComponents)
{
    if (m_useImmutableStorage) {
        draw(GL_TRIANGLE_FAN, transformMatrix, textureCoords, vertices, nbVertices, nbComponents);
    } else {
        drawMutable(GL_TRIANGLE_FAN, transformMatrix, textureCoords, vertices, nbVertices, nbComponents);
    }
}

void GLShaderTextured::draw(unsigned int mode, const QMatrix4x4& transformMatrix, GLfloat *textureCoords, GLfloat *vertices, int nbVertices, int nbComponents)
{
    if (!m_texture)
    {
        qDebug("GLShaderTextured::draw: no texture defined. Doing nothing");
        return;
    }

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    m_program->bind();
    m_program->setUniformValue(m_matrixLoc, transformMatrix);
    m_texture->bind();
    m_program->setUniformValue(m_textureLoc, 0); // Use texture unit 0 which magically contains our texture
    if (m_vao)
    {
        m_vao->bind();

        m_verticesBuf->bind();
        m_verticesBuf->allocate(vertices, nbVertices * nbComponents * sizeof(GL_FLOAT));
        m_program->enableAttributeArray(m_vertexLoc);
        m_program->setAttributeBuffer(m_vertexLoc, GL_FLOAT, 0, nbComponents);

        m_textureCoordsBuf->bind();
        m_textureCoordsBuf->allocate(textureCoords, nbVertices * 2 * sizeof(GL_FLOAT));
        m_program->enableAttributeArray(m_texCoordLoc);
        m_program->setAttributeBuffer(m_texCoordLoc, GL_FLOAT, 0, 2);
    }
    else
    {
        f->glEnableVertexAttribArray(m_vertexLoc); // vertex
        f->glVertexAttribPointer(m_vertexLoc, nbComponents, GL_FLOAT, GL_FALSE, 0, vertices);
        f->glEnableVertexAttribArray(m_texCoordLoc); // texture coordinates
        f->glVertexAttribPointer(m_texCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, textureCoords);
    }

    f->glDrawArrays(mode, 0, nbVertices);

    if (m_vao)
    {
        m_vao->release();
    }
    else
    {
       f->glDisableVertexAttribArray(m_vertexLoc);
       f->glDisableVertexAttribArray(m_texCoordLoc);
    }
    m_program->release();
}

void GLShaderTextured::drawMutable(unsigned int mode, const QMatrix4x4& transformMatrix, GLfloat *textureCoords, GLfloat *vertices, int nbVertices, int nbComponents)
{
    if (!m_textureId)
    {
        qDebug("GLShaderTextured::drawMutable: no texture defined. Doing nothing");
        return;
    }

    m_program->bind();
    m_program->setUniformValue(m_matrixLoc, transformMatrix);
    glBindTexture(GL_TEXTURE_2D, m_textureId);
    m_program->setUniformValue(m_textureLoc, 0); // Use texture unit 0 which magically contains our texture
    glEnableVertexAttribArray(m_vertexLoc); // vertex
    glVertexAttribPointer(m_vertexLoc, nbComponents, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(m_texCoordLoc); // texture coordinates
    glVertexAttribPointer(m_texCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, textureCoords);
    glDrawArrays(mode, 0, nbVertices);
    glDisableVertexAttribArray(m_vertexLoc);
    glDisableVertexAttribArray(m_texCoordLoc);
    m_program->release();
}

void GLShaderTextured::cleanup()
{
    delete m_program;
    m_program = nullptr;
    delete m_vao;
    m_vao = nullptr;
    delete m_verticesBuf;
    m_verticesBuf = nullptr;
    delete m_textureCoordsBuf;
    m_textureCoordsBuf = nullptr;
    delete m_texture;
    m_texture = nullptr;

    if (!QOpenGLContext::currentContext()) {
        return;
    }

    if (m_textureId)
    {
        glDeleteTextures(1, &m_textureId);
        m_textureId = 0;
    }
}

bool GLShaderTextured::useImmutableStorage()
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

const QString GLShaderTextured::m_vertexShaderSourceTextured2 = QString(
        "uniform highp mat4 uMatrix;\n"
        "attribute highp vec4 vertex;\n"
        "attribute highp vec2 texCoord;\n"
        "varying mediump vec2 texCoordVar;\n"
        "void main() {\n"
        "    gl_Position = uMatrix * vertex;\n"
        "    texCoordVar = texCoord;\n"
        "}\n"
        );

const QString GLShaderTextured::m_vertexShaderSourceTextured = QString(
        "#version 330\n"
        "uniform highp mat4 uMatrix;\n"
        "in highp vec4 vertex;\n"
        "in highp vec2 texCoord;\n"
        "out mediump vec2 texCoordVar;\n"
        "void main() {\n"
        "    gl_Position = uMatrix * vertex;\n"
        "    texCoordVar = texCoord;\n"
        "}\n"
        );

const QString GLShaderTextured::m_fragmentShaderSourceTextured2 = QString(
        "uniform lowp sampler2D uTexture;\n"
        "varying mediump vec2 texCoordVar;\n"
        "void main() {\n"
        "    gl_FragColor = texture2D(uTexture, texCoordVar);\n"
        "}\n"
        );

const QString GLShaderTextured::m_fragmentShaderSourceTextured = QString(
        "#version 330\n"
        "uniform lowp sampler2D uTexture;\n"
        "in mediump vec2 texCoordVar;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    fragColor = texture(uTexture, texCoordVar);\n"
        "}\n"
        );
