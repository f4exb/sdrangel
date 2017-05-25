///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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
	m_program(0),
    m_matrixLoc(0),
	m_colorLoc(0)
{ }

GLShaderSimple::~GLShaderSimple()
{
	cleanup();
}

void GLShaderSimple::initializeGL()
{
	m_program = new QOpenGLShaderProgram;

	if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, m_vertexShaderSourceSimple)) {
		qDebug() << "GLShaderSimple::initializeGL: error in vertex shader: " << m_program->log();
	}

	if (!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, m_fragmentShaderSourceColored)) {
		qDebug() << "GLShaderSimple::initializeGL: error in fragment shader: " << m_program->log();
	}

	m_program->bindAttributeLocation("vertex", 0);

	if (!m_program->link()) {
		qDebug() << "GLShaderSimple::initializeGL: error linking shader: " << m_program->log();
	}

	m_program->bind();
	m_matrixLoc = m_program->uniformLocation("uMatrix");
	m_colorLoc = m_program->uniformLocation("uColour");
	m_program->release();
}

void GLShaderSimple::drawPolyline(const QMatrix4x4& transformMatrix, const QVector4D& color, GLfloat *vertices, int nbVertices)
{
	draw(GL_LINE_STRIP, transformMatrix, color, vertices, nbVertices);
}

void GLShaderSimple::drawSegments(const QMatrix4x4& transformMatrix, const QVector4D& color, GLfloat *vertices, int nbVertices)
{
	draw(GL_LINES, transformMatrix, color, vertices, nbVertices);
}

void GLShaderSimple::drawContour(const QMatrix4x4& transformMatrix, const QVector4D& color, GLfloat *vertices, int nbVertices)
{
	draw(GL_LINE_LOOP, transformMatrix, color, vertices, nbVertices);
}

void GLShaderSimple::drawSurface(const QMatrix4x4& transformMatrix, const QVector4D& color, GLfloat *vertices, int nbVertices)
{
	draw(GL_TRIANGLE_FAN, transformMatrix, color, vertices, nbVertices);
}

void GLShaderSimple::draw(unsigned int mode, const QMatrix4x4& transformMatrix, const QVector4D& color, GLfloat *vertices, int nbVertices)
{
	QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
	m_program->bind();
	m_program->setUniformValue(m_matrixLoc, transformMatrix);
	m_program->setUniformValue(m_colorLoc, color);
	f->glEnable(GL_BLEND);
	f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	f->glLineWidth(1.0f);
	f->glEnableVertexAttribArray(0); // vertex
	f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
	f->glDrawArrays(mode, 0, nbVertices);
	f->glDisableVertexAttribArray(0);
	m_program->release();
}

void GLShaderSimple::cleanup()
{
	if (m_program)
	{
		delete m_program;
		m_program = 0;
	}
}

const QString GLShaderSimple::m_vertexShaderSourceSimple = QString(
		"uniform highp mat4 uMatrix;\n"
		"attribute highp vec4 vertex;\n"
		"void main() {\n"
		"    gl_Position = uMatrix * vertex;\n"
		"}\n"
		);

const QString GLShaderSimple::m_fragmentShaderSourceColored = QString(
		"uniform mediump vec4 uColour;\n"
		"void main() {\n"
		"    gl_FragColor = uColour;\n"
		"}\n"
		);
