///////////////////////////////////////////////////////////////////////////////////
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

#ifndef INCLUDE_TVSCREEN_H
#define INCLUDE_TVSCREEN_H

#include <QOpenGLWidget>
#include <QPen>
#include <QTimer>
#include <QMutex>
#include <QFont>
#include <QMatrix4x4>
#include "dsp/dsptypes.h"
#include "glshadertextured.h"
#include "glshadertvarray.h"
#include "export.h"
#include "util/bitfieldindex.h"

class QPainter;

class SDRGUI_API TVScreen: public QOpenGLWidget
{
	Q_OBJECT

public:

	TVScreen(bool color, QWidget* parent = nullptr);
    virtual ~TVScreen();

    void setColor(bool color);
    void resizeTVScreen(int cols, int rows);
    void getSize(int& cols, int& rows) const;
    void renderImage(unsigned char * data);
    QRgb* getRowBuffer(int row);
    void resetImage();
    void resetImage(int alpha);

    bool selectRow(int line);
    bool setDataColor(int col, int red, int green, int blue);
    bool setDataColor(int col, int red, int green, int blue, int alpha);
    void setAlphaBlend(bool alphaBlend) { m_glShaderArray.setAlphaBlend(alphaBlend); }
    void setAlphaReset() { m_glShaderArray.setAlphaReset(); }

    void connectTimer(const QTimer& timer);

    //Valeurs par défaut
    static const int TV_COLS = 256;
    static const int TV_ROWS = 256;

signals:
	void traceSizeChanged(int);
	void sampleRateChanged(int);

private:
    bool m_glContextInitialized;
    int m_askedCols;
    int m_askedRows;


	// state
    QTimer m_timer;
    QMutex m_mutex;
    bool m_dataChanged;

    GLShaderTVArray m_glShaderArray;

    int m_cols;
    int m_rows;

    void initializeGL();
	void resizeGL(int width, int height);
	void paintGL();

	void mousePressEvent(QMouseEvent*);

	unsigned char *m_lastData;

protected slots:
	void cleanup();
	void tick();
};

#endif // INCLUDE_TVSCREEN_H
