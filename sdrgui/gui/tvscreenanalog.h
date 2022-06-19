///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Vort                                                       //
// Copyright (C) 2018 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
//                                                                               //
// OpenGL interface modernization.                                               //
// See: http://doc.qt.io/qt-5/qopenglshaderprogram.html                          //
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

#ifndef INCLUDE_TVSCREENANALOG_H
#define INCLUDE_TVSCREENANALOG_H

#include "export.h"

#include <QMutex>
#include <QTimer>
#include <QOpenGLWidget>
#include <QOpenGLTexture>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>

class TVScreenAnalogBuffer
{
public:
	TVScreenAnalogBuffer(int width, int height)
	{
		m_width = width;
		m_height = height;

		m_imageData = new int[width * height];
		m_lineShiftData = new int[height];
		m_outOfBoundsLine = new int[width];
		m_currentLine = m_outOfBoundsLine;

		std::fill(m_imageData, m_imageData + width * height, 0);
		std::fill(m_lineShiftData, m_lineShiftData + height, 127);
	}

	~TVScreenAnalogBuffer()
	{
		delete[] m_imageData;
		delete[] m_lineShiftData;
		delete[] m_outOfBoundsLine;
	}

	int getWidth()
	{
		return m_width;
	}

	int getHeight()
	{
		return m_height;
	}

	const int* getImageData()
	{
		return m_imageData;
	}

	const int* getLineShiftData()
	{
		return m_lineShiftData;
	}

	void selectRow(int line, float shift)
	{
		if ((line < m_height) && (line >= 0))
		{
			m_currentLine = m_imageData + line * m_width;
			m_lineShiftData[line] = (1.0f + shift) * 127.5f;
		}
		else
		{
			m_currentLine = m_outOfBoundsLine;
		}
	}

	void setSampleValue(int column, int value)
	{
		if ((column < m_width - 2) && (column >= -2))
		{
			m_currentLine[column + 2] = value;
		}
	}

private:
	int m_width;
	int m_height;

	int* m_imageData;
	int* m_lineShiftData;

	int* m_currentLine;
	int* m_outOfBoundsLine;
};

class SDRGUI_API TVScreenAnalog : public QOpenGLWidget, protected QOpenGLFunctions
{
	Q_OBJECT

	QTimer m_updateTimer;

	bool m_isDataChanged;

	int m_textureLoc1;
	int m_textureLoc2;
	int m_imageWidthLoc;
	int m_imageHeightLoc;
	int m_texelWidthLoc;
	int m_texelHeightLoc;
	int m_vertexAttribIndex;
	int m_texCoordAttribIndex;

	QMutex m_buffersMutex;
	TVScreenAnalogBuffer *m_frontBuffer;
	TVScreenAnalogBuffer *m_backBuffer;

	QOpenGLShaderProgram *m_shader;
    QOpenGLVertexArrayObject *m_vao;
    QOpenGLBuffer *m_verticesBuf;
    QOpenGLBuffer *m_textureCoordsBuf;
	QOpenGLTexture *m_imageTexture;
	QOpenGLTexture *m_lineShiftsTexture;

public:
	TVScreenAnalog(QWidget *parent);
	~TVScreenAnalog();

	TVScreenAnalogBuffer *getBackBuffer();
	TVScreenAnalogBuffer *swapBuffers();
	void resizeTVScreen(int intCols, int intRows);

private:
	void initializeTextures(TVScreenAnalogBuffer *buffer);
	void initializeGL() override;
	void paintGL() override;
	void resizeGL(int width, int height);

private slots:
	void cleanup();
	void tick();
};

#endif // INCLUDE_TVSCREENANALOG_H
