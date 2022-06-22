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
#include <QMatrix4x4>
#include <QVector4D>
#include <QDebug>

#include "gui/glshadersimple.h"

GLShaderSimple::GLShaderSimple() :
    m_program(nullptr),
    m_vao(nullptr),
    m_verticesBuf(nullptr),
    m_vertexLoc(0),
    m_matrixLoc(0),
    m_colorLoc(0)
{ }

GLShaderSimple::~GLShaderSimple()
{
    cleanup();
}

void GLShaderSimple::initializeGL(int majorVersion, int minorVersion)
{
    m_program = new QOpenGLShaderProgram;

    if ((majorVersion > 3) || ((majorVersion == 3) && (minorVersion >= 3)))
    {
        if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, m_vertexShaderSourceSimple)) {
            qDebug() << "GLShaderSimple::initializeGL: error in vertex shader: " << m_program->log();
        }
        if (!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, m_fragmentShaderSourceColored)) {
            qDebug() << "GLShaderSimple::initializeGL: error in fragment shader: " << m_program->log();
        }

        m_vao = new QOpenGLVertexArrayObject();
        m_vao->create();
        m_vao->bind();
    }
    else
    {
        if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, m_vertexShaderSourceSimple2)) {
            qDebug() << "GLShaderSimple::initializeGL: error in vertex shader: " << m_program->log();
        }
        if (!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, m_fragmentShaderSourceColored2)) {
            qDebug() << "GLShaderSimple::initializeGL: error in fragment shader: " << m_program->log();
        }
    }

    m_program->bindAttributeLocation("vertex", 0);

    if (!m_program->link()) {
        qDebug() << "GLShaderSimple::initializeGL: error linking shader: " << m_program->log();
    }

    m_program->bind();
    m_vertexLoc = m_program->attributeLocation("vertex");
    m_matrixLoc = m_program->uniformLocation("uMatrix");
    m_colorLoc = m_program->uniformLocation("uColour");
    if (m_vao)
    {
        m_verticesBuf = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
        m_verticesBuf->setUsagePattern(QOpenGLBuffer::DynamicDraw);
        m_verticesBuf->create();
        m_vao->release();
    }
    m_program->release();
}

void GLShaderSimple::drawPoints(const QMatrix4x4& transformMatrix, const QVector4D& color, GLfloat *vertices, int nbVertices, int nbComponents)
{
    draw(GL_POINTS, transformMatrix, color, vertices, nbVertices, nbComponents);
}

void GLShaderSimple::drawPolyline(const QMatrix4x4& transformMatrix, const QVector4D& color, GLfloat *vertices, int nbVertices, int nbComponents)
{
    draw(GL_LINE_STRIP, transformMatrix, color, vertices, nbVertices, nbComponents);
}

void GLShaderSimple::drawSegments(const QMatrix4x4& transformMatrix, const QVector4D& color, GLfloat *vertices, int nbVertices, int nbComponents)
{
    draw(GL_LINES, transformMatrix, color, vertices, nbVertices, nbComponents);
}

void GLShaderSimple::drawContour(const QMatrix4x4& transformMatrix, const QVector4D& color, GLfloat *vertices, int nbVertices, int nbComponents)
{
    draw(GL_LINE_LOOP, transformMatrix, color, vertices, nbVertices, nbComponents);
}

void GLShaderSimple::drawSurface(const QMatrix4x4& transformMatrix, const QVector4D& color, GLfloat *vertices, int nbVertices, int nbComponents)
{
    draw(GL_TRIANGLE_FAN, transformMatrix, color, vertices, nbVertices, nbComponents);
}

void GLShaderSimple::drawSurfaceStrip(const QMatrix4x4& transformMatrix, const QVector4D& color, GLfloat *vertices, int nbVertices, int nbComponents)
{
    draw(GL_TRIANGLE_STRIP, transformMatrix, color, vertices, nbVertices, nbComponents);
}

void GLShaderSimple::draw(unsigned int mode, const QMatrix4x4& transformMatrix, const QVector4D& color, GLfloat *vertices, int nbVertices, int nbComponents)
{
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    m_program->bind();
    m_program->setUniformValue(m_matrixLoc, transformMatrix);
    m_program->setUniformValue(m_colorLoc, color);
    if (m_vao)
    {
        m_vao->bind();

        m_verticesBuf->bind();
        m_verticesBuf->allocate(vertices, nbVertices * nbComponents * sizeof(GL_FLOAT));
        m_program->enableAttributeArray(m_vertexLoc);
        m_program->setAttributeBuffer(m_vertexLoc, GL_FLOAT, 0, nbComponents);
    }
    else
    {
        f->glEnableVertexAttribArray(m_vertexLoc); // vertex
        f->glVertexAttribPointer(m_vertexLoc, nbComponents, GL_FLOAT, GL_FALSE, 0, vertices);
    }

    f->glEnable(GL_BLEND);
    f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    f->glLineWidth(1.0f);
    f->glDrawArrays(mode, 0, nbVertices);

    if (m_vao) {
        m_vao->release();
    } else {
       f->glDisableVertexAttribArray(m_vertexLoc);
    }
    m_program->release();
}

void GLShaderSimple::cleanup()
{
    delete m_program;
    m_program = nullptr;
    delete m_vao;
    m_vao = nullptr;
    delete m_verticesBuf;
    m_verticesBuf = nullptr;
}

const QString GLShaderSimple::m_vertexShaderSourceSimple2 = QString(
        "uniform highp mat4 uMatrix;\n"
        "attribute highp vec4 vertex;\n"
        "void main() {\n"
        "    gl_Position = uMatrix * vertex;\n"
        "}\n"
        );

const QString GLShaderSimple::m_vertexShaderSourceSimple = QString(
        "#version 330\n"
        "uniform highp mat4 uMatrix;\n"
        "in highp vec4 vertex;\n"
        "void main() {\n"
        "    gl_Position = uMatrix * vertex;\n"
        "}\n"
        );

const QString GLShaderSimple::m_fragmentShaderSourceColored2 = QString(
        "uniform mediump vec4 uColour;\n"
        "void main() {\n"
        "    gl_FragColor = uColour;\n"
        "}\n"
        );

const QString GLShaderSimple::m_fragmentShaderSourceColored = QString(
        "#version 330\n"
        "out vec4 fragColor;\n"
        "uniform mediump vec4 uColour;\n"
        "void main() {\n"
        "    fragColor = uColour;\n"
        "}\n"
        );
