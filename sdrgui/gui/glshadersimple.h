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

#ifndef INCLUDE_GUI_GLSHADERSIMPLE_H_
#define INCLUDE_GUI_GLSHADERSIMPLE_H_

#include <QString>
#include <QOpenGLFunctions>

class QOpenGLShaderProgram;
class QMatrix4x4;
class QVector4D;

class GLShaderSimple
{
public:
	GLShaderSimple();
	~GLShaderSimple();

	void initializeGL();
	void drawPolyline(const QMatrix4x4& transformMatrix, const QVector4D& color, GLfloat *vertices, int nbVertices);
	void drawSegments(const QMatrix4x4& transformMatrix, const QVector4D& color, GLfloat *vertices, int nbVertices);
	void drawContour(const QMatrix4x4& transformMatrix, const QVector4D& color, GLfloat *vertices, int nbVertices);
	void drawSurface(const QMatrix4x4& transformMatrix, const QVector4D& color, GLfloat *vertices, int nbVertices);
	void cleanup();

private:
	void draw(unsigned int mode, const QMatrix4x4& transformMatrix, const QVector4D& color, GLfloat *vertices, int nbVertices);

	QOpenGLShaderProgram *m_program;
	int m_matrixLoc;
	int m_colorLoc;
	static const QString m_vertexShaderSourceSimple;
	static const QString m_fragmentShaderSourceColored;
};

#endif /* INCLUDE_GUI_GLSHADERSIMPLE_H_ */
