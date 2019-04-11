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

#include <QGLWidget>
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

class SDRGUI_API TVScreen: public QGLWidget
{
	Q_OBJECT

public:

	TVScreen(bool blnColor, QWidget* parent = 0);
    virtual ~TVScreen();

    void setColor(bool blnColor);
    void resizeTVScreen(int intCols, int intRows);
    void getSize(int& intCols, int& intRows) const;
    void renderImage(unsigned char * objData);
    QRgb* getRowBuffer(int intRow);
    void resetImage();
    void resetImage(int alpha);

    bool selectRow(int intLine);
    bool setDataColor(int intCol, int intRed, int intGreen, int intBlue);
    bool setDataColor(int intCol, int intRed, int intGreen, int intBlue, int intAlpha);
    void setAlphaBlend(bool blnAlphaBlend) { m_objGLShaderArray.setAlphaBlend(blnAlphaBlend); }
    void setAlphaReset() { m_objGLShaderArray.setAlphaReset(); }

    void connectTimer(const QTimer& timer);

    //Valeurs par défaut
    static const int TV_COLS=256;
    static const int TV_ROWS=256;

signals:
	void traceSizeChanged(int);
	void sampleRateChanged(int);

private:
    bool m_blnGLContextInitialized;
    int m_intAskedCols;
    int m_intAskedRows;


	// state
    QTimer m_objTimer;
    QMutex m_objMutex;
    bool m_blnDataChanged;
    bool m_blnConfigChanged;

    GLShaderTVArray m_objGLShaderArray;

    int m_cols;
    int m_rows;

    void initializeGL();
	void resizeGL(int width, int height);
	void paintGL();

	void mousePressEvent(QMouseEvent*);

	unsigned char *m_chrLastData;

protected slots:
	void cleanup();
	void tick();
};

#endif // INCLUDE_TVSCREEN_H
