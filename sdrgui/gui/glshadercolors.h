///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// Inspired by:                                                                  //
// https://en.wikibooks.org/wiki/OpenGL_Programming/Modern_OpenGL_Tutorial_03    //                                                                             //
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

#ifndef INCLUDE_GUI_GLSHADERCOLORS_H_
#define INCLUDE_GUI_GLSHADERCOLORS_H_

#include <QString>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>

#include "export.h"

class QOpenGLShaderProgram;
class QMatrix4x4;
class QVector4D;

class SDRGUI_API GLShaderColors
{
public:
	GLShaderColors();
	~GLShaderColors();

	void initializeGL(int majorVersion, int minorVersion);
    void drawPoints(const QMatrix4x4& transformMatrix, GLfloat *vertices, GLfloat *colors, GLfloat alpha, int nbVertices);
	void drawPolyline(const QMatrix4x4& transformMatrix, GLfloat *vertices, GLfloat *colors, GLfloat alpha, int nbVertices);
	void drawSegments(const QMatrix4x4& transformMatrix, GLfloat *vertices, GLfloat *colors, GLfloat alpha, int nbVertices);
	void drawContour(const QMatrix4x4& transformMatrix, GLfloat *vertices, GLfloat *colors, GLfloat alpha, int nbVertices);
	void drawSurface(const QMatrix4x4& transformMatrix, GLfloat *vertices, GLfloat *colors, GLfloat alpha, int nbVertices);
	void cleanup();

private:
	void draw(unsigned int mode, const QMatrix4x4& transformMatrix, GLfloat *vertices, GLfloat *colors, GLfloat alpha, int nbVertices);

	QOpenGLShaderProgram *m_program;
    QOpenGLVertexArrayObject *m_vao;
    QOpenGLBuffer *m_verticesBuf;
    QOpenGLBuffer *m_colorBuf;
	int m_matrixLoc;
    int m_alphaLoc;
	static const QString m_vertexShaderSourceSimple2;
	static const QString m_vertexShaderSourceSimple;
	static const QString m_fragmentShaderSourceColored2;
	static const QString m_fragmentShaderSourceColored;
};

#endif /* INCLUDE_GUI_GLSHADERCOLORS_H_ */
