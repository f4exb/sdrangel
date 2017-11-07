///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
//                                                                               //
// OpenGL interface modernization.                                               //
// See: http://doc.qt.io/qt-5/qopenglshaderprogram.html                          //
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

#ifndef INCLUDE_ATVSCREEN_H
#define INCLUDE_ATVSCREEN_H

#include <QGLWidget>
#include <QPen>
#include <QTimer>
#include <QMutex>
#include <QFont>
#include <QMatrix4x4>
#include "dsp/dsptypes.h"
#include "glshaderarray.h"
#include "gui/glshadertextured.h"
#include "util/export.h"
#include "util/bitfieldindex.h"

#include "atvscreeninterface.h"

class QPainter;



class SDRANGEL_API ATVScreen: public QGLWidget, public ATVScreenInterface
{
	Q_OBJECT

public:

	ATVScreen(QWidget* parent = NULL);
	virtual ~ATVScreen();

	virtual void resizeATVScreen(int intCols, int intRows);
	virtual void renderImage(unsigned char * objData);
    QRgb* getRowBuffer(int intRow);
    void resetImage();

    virtual bool selectRow(int intLine);
    virtual bool setDataColor(int intCol,int intRed, int intGreen, int intBlue);
    //void setRenderImmediate(bool blnRenderImmediate) { m_blnRenderImmediate = blnRenderImmediate; }

    void connectTimer(const QTimer& timer);

    //Valeurs par défaut
    static const int ATV_COLS=192;
    static const int ATV_ROWS=625;

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
    //bool m_blnRenderImmediate;
    bool m_blnConfigChanged;

    GLShaderArray m_objGLShaderArray;

    void initializeGL();
	void resizeGL(int width, int height);
	void paintGL();

	void mousePressEvent(QMouseEvent*);

    unsigned char *m_chrLastData;

protected slots:
	void cleanup();
	void tick();
};

#endif // INCLUDE_ATVSCREEN_H
