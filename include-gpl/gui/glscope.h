///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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
#include "dsp/dsptypes.h"
#include "dsp/scopevis.h"
#include "gui/scaleengine.h"
#include "util/export.h"

class DSPEngine;
class ScopeVis;

class SDRANGELOVE_API GLScope: public QGLWidget {
	Q_OBJECT

public:
	enum Mode {
		ModeIQ,
		ModeMagLinPha,
		ModeMagdBPha,
		ModeDerived12,
		ModeCyclostationary
	};

	enum Displays {
		DisplayBoth,
		DisplayFirstOnly,
		DisplaySecondOnly
	};

	GLScope(QWidget* parent = NULL);
	~GLScope();

	void setDSPEngine(DSPEngine* dspEngine);
	void setAmp(Real amp);
	void setAmpOfs(Real ampOfs);
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

	void newTrace(const std::vector<Complex>& trace, int sampleRate);
	int getTraceSize() const { return m_rawTrace.size(); }

	void setSampleRate(int sampleRate);
	int getSampleRate() const {	return m_sampleRate; }
	Mode getDataMode() const { return m_mode; }

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
	std::vector<Complex> m_rawTrace;
	std::vector<Complex> m_mathTrace;
	std::vector<Complex>* m_displayTrace;
	int m_oldTraceSize;
	int m_sampleRate;
	Real m_amp1;
	Real m_amp2;
	Real m_ofs1;
	Real m_ofs2;

	// sample sink
	DSPEngine* m_dspEngine;
	ScopeVis* m_scopeVis;

	// config
	Real m_amp;
	Real m_ofs;
	int m_timeBase;
	int m_timeOfsProMill;
	ScopeVis::TriggerChannel m_triggerChannel;
	Real m_triggerLevel;
	Real m_triggerPre;

	// graphics stuff
	QRectF m_glScopeRect1;
	QRectF m_glScopeRect2;
	int m_displayGridIntensity;
	int m_displayTraceIntensity;
	QRectF m_glLeft1ScaleRect;
	QRectF m_glLeft2ScaleRect;
	QRectF m_glBot1ScaleRect;
	QRectF m_glBot2ScaleRect;
	QPixmap m_left1ScalePixmap;
	QPixmap m_left2ScalePixmap;
	QPixmap m_bot1ScalePixmap;
	QPixmap m_bot2ScalePixmap;
	bool m_left1ScaleTextureAllocated;
	bool m_left2ScaleTextureAllocated;
	bool m_bot1ScaleTextureAllocated;
	bool m_bot2ScaleTextureAllocated;
	GLuint m_left1ScaleTexture;
	GLuint m_left2ScaleTexture;
	GLuint m_bot1ScaleTexture;
	GLuint m_bot2ScaleTexture;
	ScaleEngine m_x1Scale;
	ScaleEngine m_x2Scale;
	ScaleEngine m_y1Scale;
	ScaleEngine m_y2Scale;

	void initializeGL();
	void resizeGL(int width, int height);
	void paintGL();

	void mousePressEvent(QMouseEvent*);

	void handleMode();
	void applyConfig();

protected slots:
	void tick();
};

#endif // INCLUDE_GLSCOPE_H
