///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
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

#include "glshadercolors.h"

GLShaderColors::GLShaderColors() :
	m_program(nullptr),
    m_matrixLoc(0),
    m_alphaLoc(0)
{ }

GLShaderColors::~GLShaderColors()
{
	cleanup();
}

void GLShaderColors::initializeGL()
{
	m_program = new QOpenGLShaderProgram;

	if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, m_vertexShaderSourceSimple)) {
		qDebug() << "GLShaderColors::initializeGL: error in vertex shader: " << m_program->log();
	}

	if (!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, m_fragmentShaderSourceColored)) {
		qDebug() << "GLShaderColors::initializeGL: error in fragment shader: " << m_program->log();
	}

	m_program->bindAttributeLocation("vertex", 0);
	m_program->bindAttributeLocation("v_color", 1);

	if (!m_program->link()) {
		qDebug() << "GLShaderColors::initializeGL: error linking shader: " << m_program->log();
	}

	m_program->bind();
	m_matrixLoc = m_program->uniformLocation("uMatrix");
    m_alphaLoc = m_program->uniformLocation("uAlpha");
	m_program->release();
}

void GLShaderColors::drawPoints(const QMatrix4x4& transformMatrix, GLfloat *vertices, GLfloat *colors, GLfloat alpha, int nbVertices)
{
    draw(GL_POINTS, transformMatrix, vertices, colors, alpha, nbVertices);
}

void GLShaderColors::drawPolyline(const QMatrix4x4& transformMatrix, GLfloat *vertices, GLfloat *colors, GLfloat alpha, int nbVertices)
{
	draw(GL_LINE_STRIP, transformMatrix, vertices, colors, alpha, nbVertices);
}

void GLShaderColors::drawSegments(const QMatrix4x4& transformMatrix, GLfloat *vertices, GLfloat *colors, GLfloat alpha, int nbVertices)
{
	draw(GL_LINES, transformMatrix, vertices, colors, alpha, nbVertices);
}

void GLShaderColors::drawContour(const QMatrix4x4& transformMatrix, GLfloat *vertices, GLfloat *colors, GLfloat alpha, int nbVertices)
{
	draw(GL_LINE_LOOP, transformMatrix, vertices, colors, alpha, nbVertices);
}

void GLShaderColors::drawSurface(const QMatrix4x4& transformMatrix, GLfloat *vertices, GLfloat *colors, GLfloat alpha, int nbVertices)
{
	draw(GL_TRIANGLE_FAN, transformMatrix, vertices, colors, alpha, nbVertices);
}

void GLShaderColors::draw(unsigned int mode, const QMatrix4x4& transformMatrix, GLfloat *vertices, GLfloat *colors, GLfloat alpha, int nbVertices)
{
	QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
	m_program->bind();
	m_program->setUniformValue(m_matrixLoc, transformMatrix);
    m_program->setUniformValue(m_alphaLoc, alpha);
	f->glEnable(GL_BLEND);
	f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	f->glLineWidth(1.0f);
	f->glEnableVertexAttribArray(0); // vertex
	f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    f->glEnableVertexAttribArray(1); // colors
    f->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, colors);
	f->glDrawArrays(mode, 0, nbVertices);
	f->glDisableVertexAttribArray(0);
    f->glDisableVertexAttribArray(1);
	m_program->release();
}

void GLShaderColors::cleanup()
{
	if (m_program)
	{
		delete m_program;
		m_program = nullptr;
	}
}

const QString GLShaderColors::m_vertexShaderSourceSimple = QString(
		"uniform highp mat4 uMatrix;\n"
		"attribute highp vec4 vertex;\n"
        "attribute vec3 v_color;\n"
        "varying vec3 f_color;\n"
        "void main() {\n"
		"    gl_Position = uMatrix * vertex;\n"
        "    f_color = v_color;\n"
		"}\n"
		);

const QString GLShaderColors::m_fragmentShaderSourceColored = QString(
        "uniform mediump float uAlpha;\n"
        "varying vec3 f_color;\n"
		"void main() {\n"
		"    gl_FragColor = vec4(f_color.r, f_color.g, f_color.b, uAlpha);\n"
		"}\n"
		);
