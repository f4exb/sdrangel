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

#ifndef INCLUDE_GLSPECTRUM_H
#define INCLUDE_GLSPECTRUM_H

#include <QGLWidget>
#include <QTimer>
#include <QMutex>
#include "dsp/dsptypes.h"
#include "gui/scaleengine.h"
#include "dsp/channelmarker.h"
#include "util/export.h"

class SDRANGELOVE_API GLSpectrum : public QGLWidget {
	Q_OBJECT

public:
	GLSpectrum(QWidget* parent = NULL);
	~GLSpectrum();

	void setCenterFrequency(quint64 frequency);
	void setSampleRate(qint32 sampleRate);
	void setReferenceLevel(Real referenceLevel);
	void setPowerRange(Real powerRange);
	void setDecay(int decay);
	void setDisplayWaterfall(bool display);
	void setInvertedWaterfall(bool inv);
	void setDisplayMaxHold(bool display);
	void setDisplayHistogram(bool display);
	void setDisplayGrid(bool display);

	void addChannelMarker(ChannelMarker* channelMarker);
	void removeChannelMarker(ChannelMarker* channelMarker);

	void newSpectrum(const std::vector<Real>& spectrum, int fftSize);

private:
	struct ChannelMarkerState {
		ChannelMarker* m_channelMarker;
		QRectF m_glRect;
		QRect m_rect;

		ChannelMarkerState(ChannelMarker* channelMarker) :
			m_channelMarker(channelMarker),
			m_glRect()
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
	bool m_invertedWaterfall;

	std::vector<Real> m_maxHold;
	bool m_displayMaxHold;

	Real m_waterfallShare;

	QPixmap m_leftMarginPixmap;
	bool m_leftMarginTextureAllocated;
	GLuint m_leftMarginTexture;
	QPixmap m_frequencyPixmap;
	bool m_frequencyTextureAllocated;
	GLuint m_frequencyTexture;
	ScaleEngine m_timeScale;
	ScaleEngine m_powerScale;
	ScaleEngine m_frequencyScale;
	QRectF m_glLeftScaleRect;
	QRectF m_glFrequencyScaleRect;
	QRect m_frequencyScaleRect;

	QRgb m_waterfallPalette[240];
	QImage* m_waterfallBuffer;
	int m_waterfallBufferPos;
	bool m_waterfallTextureAllocated;
	GLuint m_waterfallTexture;
	int m_waterfallTextureHeight;
	int m_waterfallTexturePos;
	QRectF m_glWaterfallRect;
	bool m_displayWaterfall;

	QRgb m_histogramPalette[240];
	QImage* m_histogramBuffer;
	quint8* m_histogram;
	quint8* m_histogramHoldoff;
	bool m_histogramTextureAllocated;
	GLuint m_histogramTexture;
	int m_histogramHoldoffBase;
	int m_histogramHoldoffCount;
	int m_histogramLateHoldoff;
	QRectF m_glHistogramRect;
	bool m_displayHistogram;

	bool m_displayChanged;

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
	void tick();
	void channelMarkerChanged();
	void channelMarkerDestroyed(QObject* object);
};

#endif // INCLUDE_GLSPECTRUM_H
