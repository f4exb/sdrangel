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

#include <memory>

#include <QImage>
#include <QMutex>
#include <QTimer>
#include <QGLWidget>
#include <QOpenGLTexture>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>

class SDRGUI_API TVScreenAnalog : public QGLWidget, protected QOpenGLFunctions
{
	Q_OBJECT

	QTimer m_objTimer;
	QMutex m_objMutex;

	bool m_isDataChanged;

	int m_objTextureLoc1;
	int m_objTextureLoc2;
	int m_objImageWidthLoc;
	int m_objImageHeightLoc;
	int m_objTexelWidthLoc;
	int m_objTexelHeightLoc;
	int m_vertexAttribIndex;
	int m_texCoordAttribIndex;
	std::shared_ptr<QOpenGLShaderProgram> m_shader;

	float m_time;

	int m_cols;
	int m_rows;

	int* m_objCurrentRow;

	std::shared_ptr<QImage> m_image;
	std::shared_ptr<QImage> m_lineShifts;
	std::shared_ptr<QOpenGLTexture> m_imageTexture;
	std::shared_ptr<QOpenGLTexture> m_lineShiftsTexture;

public:
	TVScreenAnalog(QWidget *parent);

	void resizeTVScreen(int intCols, int intRows);
	void selectRow(int intLine, float shift);
	void setDataColor(int intCol, int objColor);
	void renderImage();

private:
	void initializeTextures();
	void initializeGL() override;
	void paintGL() override;
	void resizeGL(int width, int height);

private slots:
	void cleanup();
	void tick();
};

#endif // INCLUDE_TVSCREENANALOG_H
