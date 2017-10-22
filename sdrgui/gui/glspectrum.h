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

#ifndef INCLUDE_GLSPECTRUM_H
#define INCLUDE_GLSPECTRUM_H

#include <QTimer>
#include <QMutex>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QGLWidget>
#include "dsp/dsptypes.h"
#include "gui/scaleengine.h"
#include "gui/glshadersimple.h"
#include "gui/glshadertextured.h"
#include "dsp/channelmarker.h"
#include "util/export.h"

class QOpenGLShaderProgram;

class SDRANGEL_API GLSpectrum : public QGLWidget {
	Q_OBJECT

public:
	GLSpectrum(QWidget* parent = NULL);
	~GLSpectrum();

	void setCenterFrequency(qint64 frequency);
	void setSampleRate(qint32 sampleRate);
	void setReferenceLevel(Real referenceLevel);
	void setPowerRange(Real powerRange);
	void setDecay(int decay);
	void setHistoLateHoldoff(int lateHoldoff);
	void setHistoStroke(int stroke);
	void setDisplayWaterfall(bool display);
	void setSsbSpectrum(bool ssbSpectrum);
	void setLsbDisplay(bool lsbDisplay);
	void setInvertedWaterfall(bool inv);
	void setDisplayMaxHold(bool display);
	void setDisplayCurrent(bool display);
	void setDisplayHistogram(bool display);
	void setDisplayGrid(bool display);
	void setDisplayGridIntensity(int intensity);
	void setDisplayTraceIntensity(int intensity);

	void addChannelMarker(ChannelMarker* channelMarker);
	void removeChannelMarker(ChannelMarker* channelMarker);

	void newSpectrum(const std::vector<Real>& spectrum, int fftSize);
	void clearSpectrumHistogram();

	Real getWaterfallShare() const { return m_waterfallShare; }
	void setWaterfallShare(Real waterfallShare);
	void connectTimer(const QTimer& timer);

private:
	struct ChannelMarkerState {
		ChannelMarker* m_channelMarker;
		QMatrix4x4 m_glMatrixWaterfall;
		QMatrix4x4 m_glMatrixDsbWaterfall;
		QMatrix4x4 m_glMatrixFreqScale;
		QMatrix4x4 m_glMatrixDsbFreqScale;
		QMatrix4x4 m_glMatrixHistogram;
		QMatrix4x4 m_glMatrixDsbHistogram;
		QRect m_rect;

		ChannelMarkerState(ChannelMarker* channelMarker) :
			m_channelMarker(channelMarker)
		{ }
	};
	QList<ChannelMarkerState*> m_channelMarkerStates;

	enum CursorState {
		CSNormal,
		CSSplitter,
		CSSplitterMoving,
		CSChannel,
		CSChannelMoving
	};

	CursorState m_cursorState;
	int m_cursorChannel;

	QTimer m_timer;
	QMutex m_mutex;
	bool m_mouseInside;
	bool m_changesPending;

	qint64 m_centerFrequency;
	Real m_referenceLevel;
	Real m_powerRange;
	int m_decay;
	quint32 m_sampleRate;

	int m_fftSize;

	bool m_displayGrid;
	int m_displayGridIntensity;
	int m_displayTraceIntensity;
	bool m_invertedWaterfall;

	std::vector<Real> m_maxHold;
	bool m_displayMaxHold;
	const std::vector<Real> *m_currentSpectrum;
	bool m_displayCurrent;

	Real m_waterfallShare;

	QPixmap m_leftMarginPixmap;
	QPixmap m_frequencyPixmap;
	ScaleEngine m_timeScale;
	ScaleEngine m_powerScale;
	ScaleEngine m_frequencyScale;
	QRect m_frequencyScaleRect;
	QMatrix4x4 m_glFrequencyScaleBoxMatrix;
	QMatrix4x4 m_glLeftScaleBoxMatrix;

	QRgb m_waterfallPalette[240];
	QImage* m_waterfallBuffer;
	int m_waterfallBufferPos;
	int m_waterfallTextureHeight;
	int m_waterfallTexturePos;
	QMatrix4x4 m_glWaterfallBoxMatrix;
	bool m_displayWaterfall;
	bool m_ssbSpectrum;
	bool m_lsbDisplay;

	QRgb m_histogramPalette[240];
	QImage* m_histogramBuffer;
	quint8* m_histogram;
	quint8* m_histogramHoldoff;
	int m_histogramHoldoffBase;
	int m_histogramHoldoffCount;
	int m_histogramLateHoldoff;
	int m_histogramStroke;
	QMatrix4x4 m_glHistogramSpectrumMatrix;
	QMatrix4x4 m_glHistogramBoxMatrix;
	bool m_displayHistogram;

	bool m_displayChanged;

	GLShaderSimple m_glShaderSimple;
	GLShaderTextured m_glShaderLeftScale;
	GLShaderTextured m_glShaderFrequencyScale;
	GLShaderTextured m_glShaderWaterfall;
	GLShaderTextured m_glShaderHistogram;
	int m_matrixLoc;
	int m_colorLoc;

	static const int m_waterfallBufferHeight = 256;

	void updateWaterfall(const std::vector<Real>& spectrum);
	void updateHistogram(const std::vector<Real>& spectrum);

	void initializeGL();
	void resizeGL(int width, int height);
	void paintGL();

	void stopDrag();
	void applyChanges();

	void mouseMoveEvent(QMouseEvent* event);
	void mousePressEvent(QMouseEvent* event);
	void mouseReleaseEvent(QMouseEvent* event);

	void enterEvent(QEvent* event);
	void leaveEvent(QEvent* event);

private slots:
	void cleanup();
	void tick();
	void channelMarkerChanged();
	void channelMarkerDestroyed(QObject* object);
};

#endif // INCLUDE_GLSPECTRUM_H
