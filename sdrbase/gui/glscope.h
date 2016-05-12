///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef INCLUDE_GLSCOPE_H
#define INCLUDE_GLSCOPE_H

#include <QGLWidget>
#include <QPen>
#include <QTimer>
#include <QMutex>
#include <QFont>
#include <QMatrix4x4>
#include "dsp/dsptypes.h"
#include "dsp/scopevis.h"
#include "gui/scaleengine.h"
#include "gui/glshadersimple.h"
#include "gui/glshadertextured.h"
#include "util/export.h"
#include "util/bitfieldindex.h"

class ScopeVis;
class QPainter;

class SDRANGEL_API GLScope: public QGLWidget {
	Q_OBJECT

public:
	enum Mode {
		ModeIQ,
		ModeMagLinPha,
		ModeMagdBPha,
		ModeMagLinDPha,
		ModeMagdBDPha,
		ModeDerived12,
		ModeCyclostationary,
		ModeIQPolar
	};

	enum Displays {
		DisplayBoth,
		DisplayFirstOnly,
		DisplaySecondOnly
	};

	GLScope(QWidget* parent = NULL);
	~GLScope();

//	void setDSPEngine(DSPEngine* dspEngine);
	void setAmp1(Real amp);
	void setAmp1Ofs(Real ampOfs);
	void setAmp2(Real amp);
	void setAmp2Ofs(Real ampOfs);
	void setTimeBase(int timeBase);
	void setTimeOfsProMill(int timeOfsProMill);
	void setMode(Mode mode);
	void setDisplays(Displays displays);
	void setOrientation(Qt::Orientation orientation);
	void setDisplayGridIntensity(int intensity);
	void setDisplayTraceIntensity(int intensity);
	void setTriggerChannel(ScopeVis::TriggerChannel triggerChannel);
	void setTriggerLevel(Real triggerLevel);
	void setTriggerPre(Real triggerPre);
	void setMemHistoryShift(int value);

	void newTrace(const std::vector<Complex>& trace, int sampleRate);
	int getTraceSize() const { return m_rawTrace[m_memTraceIndex - m_memTraceHistory].size(); }

	void setSampleRate(int sampleRate);
	int getSampleRate() const {	return m_sampleRates[m_memTraceIndex - m_memTraceHistory]; }
	Mode getDataMode() const { return m_mode; }
	void connectTimer(const QTimer& timer);

	static const int m_memHistorySizeLog2 = 5;

signals:
	void traceSizeChanged(int);
	void sampleRateChanged(int);

private:
	// state
	QTimer m_timer;
	QMutex m_mutex;
	bool m_dataChanged;
	bool m_configChanged;
	Mode m_mode;
	Displays m_displays;
	Qt::Orientation m_orientation;

	// traces
	std::vector<Complex> m_rawTrace[1<<m_memHistorySizeLog2];
	int m_sampleRates[1<<m_memHistorySizeLog2];
	BitfieldIndex<m_memHistorySizeLog2> m_memTraceIndex;   //!< current index of trace being written
	BitfieldIndex<m_memHistorySizeLog2> m_memTraceHistory; //!< trace index shift into history
	int m_memTraceIndexMax;
	bool m_memTraceRecall;
	std::vector<Complex> m_mathTrace;
	std::vector<Complex>* m_displayTrace;
	std::vector<Real> m_powTrace;
	Real m_maxPow;
	Real m_sumPow;
	int m_oldTraceSize;
	int m_sampleRate;
	Real m_amp1;
	Real m_amp2;
	Real m_ofs1;
	Real m_ofs2;

	// config
	int m_timeBase;
	int m_timeOfsProMill;
	ScopeVis::TriggerChannel m_triggerChannel;
	Real m_triggerLevel;
	Real m_triggerPre;
	Real m_triggerLevelDis1;
	Real m_triggerLevelDis2;
	int m_nbPow;
	Real m_prevArg;

	// graphics stuff
	QRectF m_glScopeRect1;
	QRectF m_glScopeRect2;
	QMatrix4x4 m_glScopeMatrix1;
	QMatrix4x4 m_glScopeMatrix2;
	QMatrix4x4 m_glLeft1ScaleMatrix;
	QMatrix4x4 m_glRight1ScaleMatrix;
	QMatrix4x4 m_glLeft2ScaleMatrix;
	QMatrix4x4 m_glBot1ScaleMatrix;
	QMatrix4x4 m_glBot2ScaleMatrix;

	QPixmap m_left1ScalePixmap;
	QPixmap m_left2ScalePixmap;
	QPixmap m_bot1ScalePixmap;
	QPixmap m_bot2ScalePixmap;
	QPixmap m_powerOverlayPixmap1;

	int m_displayGridIntensity;
	int m_displayTraceIntensity;

	ScaleEngine m_x1Scale;
	ScaleEngine m_x2Scale;
	ScaleEngine m_y1Scale;
	ScaleEngine m_y2Scale;

	QFont m_powerOverlayFont;

	GLShaderSimple m_glShaderSimple;
	GLShaderTextured m_glShaderLeft1Scale;
	GLShaderTextured m_glShaderBottom1Scale;
	GLShaderTextured m_glShaderLeft2Scale;
	GLShaderTextured m_glShaderBottom2Scale;
	GLShaderTextured m_glShaderPowerOverlay;

	void initializeGL();
	void resizeGL(int width, int height);
	void paintGL();

	void mousePressEvent(QMouseEvent*);

	void handleMode();
	void applyConfig();
	void drawPowerOverlay();

protected slots:
	void cleanup();
	void tick();
};

#endif // INCLUDE_GLSCOPE_H
