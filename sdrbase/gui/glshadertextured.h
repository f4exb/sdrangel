///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// See: http://glslstudio.com/primer/#gl2frag                                    //
//      https://gitlab.com/pteam/korvins-qtbase/blob/5.4/examples/opengl/cube/mainwidget.cpp //
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

#ifndef INCLUDE_GUI_GLSHADERTEXTURED_H_
#define INCLUDE_GUI_GLSHADERTEXTURED_H_

#include <QString>
#include <QOpenGLTexture>
#include <QOpenGLFunctions>

class QOpenGLShaderProgram;
class QMatrix4x4;
class QImage;

class GLShaderTextured
{
public:
	GLShaderTextured();
	~GLShaderTextured();

	void initializeGL();
	void initTexture(const QImage& image, QOpenGLTexture::WrapMode wrapMode = QOpenGLTexture::Repeat);
	void subTexture(int xOffset, int yOffset, int width, int height, const void *pixels);
	void drawSurface(const QMatrix4x4& transformMatrix, GLfloat* textureCoords, GLfloat *vertices, int nbVertices);
	void cleanup();

private:
	void draw(unsigned int mode, const QMatrix4x4& transformMatrix, GLfloat *textureCoords, GLfloat *vertices, int nbVertices);

	QOpenGLShaderProgram *m_program;
	QOpenGLTexture *m_texture;
	int m_matrixLoc;
	int m_textureLoc;
	static const QString m_vertexShaderSourceTextured;
	static const QString m_fragmentShaderSourceTextured;
};

#endif /* INCLUDE_GUI_GLSHADERTEXTURED_H_ */
