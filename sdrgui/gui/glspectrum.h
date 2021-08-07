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

#ifndef INCLUDE_GLSPECTRUM_H
#define INCLUDE_GLSPECTRUM_H

#include <QTimer>
#include <QMutex>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QGLWidget>
#include "gui/scaleengine.h"
#include "gui/glshadersimple.h"
#include "gui/glshadertextured.h"
#include "dsp/glspectruminterface.h"
#include "dsp/spectrummarkers.h"
#include "dsp/channelmarker.h"
#include "dsp/spectrumsettings.h"
#include "export.h"
#include "util/incrementalarray.h"
#include "util/message.h"

class QOpenGLShaderProgram;
class MessageQueue;
class SpectrumVis;

class SDRGUI_API GLSpectrum : public QGLWidget, public GLSpectrumInterface {
	Q_OBJECT

public:
    class MsgReportSampleRate : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        MsgReportSampleRate(quint32 sampleRate) :
            Message(),
            m_sampleRate(sampleRate)
        {}

        quint32 getSampleRate() const { return m_sampleRate; }

    private:
        quint32 m_sampleRate;
    };

    class MsgReportWaterfallShare : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        MsgReportWaterfallShare(Real waterfallShare) :
            Message(),
            m_waterfallShare(waterfallShare)
        {}

        Real getWaterfallShare() const { return m_waterfallShare; }

    private:
        Real m_waterfallShare;
    };

    class MsgReportFFTOverlap : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        MsgReportFFTOverlap(int overlap) :
            Message(),
            m_overlap(overlap)
        {}

        int getOverlap() const { return m_overlap; }

    private:
        int m_overlap;
    };

    class MsgReportPowerScale : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        MsgReportPowerScale(int refLevel, int range) :
            Message(),
            m_refLevel(refLevel),
            m_range(range)
        {}

        Real getRefLevel() const { return m_refLevel; }
        Real getRange() const { return m_range; }

    private:
        Real m_refLevel;
        Real m_range;
    };

	GLSpectrum(QWidget* parent = nullptr);
	virtual ~GLSpectrum();

	void setCenterFrequency(qint64 frequency);
    qint64 getCenterFrequency() const { return m_centerFrequency; }
    float getPowerMax() const;
    float getTimeMax() const;
	void setSampleRate(qint32 sampleRate);
	void setTimingRate(qint32 timingRate);
    void setFFTOverlap(int overlap);
	void setReferenceLevel(Real referenceLevel);
	void setPowerRange(Real powerRange);
	void setDecay(int decay);
	void setDecayDivisor(int decayDivisor);
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
	void setLinear(bool linear);
	qint32 getSampleRate() const { return m_sampleRate; }

	void addChannelMarker(ChannelMarker* channelMarker);
	void removeChannelMarker(ChannelMarker* channelMarker);
	void setMessageQueueToGUI(MessageQueue* messageQueue) { m_messageQueueToGUI = messageQueue; }

	virtual void newSpectrum(const Real* spectrum, int nbBins, int fftSize);
	void clearSpectrumHistogram();

	Real getWaterfallShare() const { return m_waterfallShare; }
	void setWaterfallShare(Real waterfallShare);
    void setFPSPeriodMs(int fpsPeriodMs);

    void setDisplayedStream(bool sourceOrSink, int streamIndex)
    {
        m_displaySourceOrSink = sourceOrSink;
        m_displayStreamIndex = streamIndex;
    }
    void setSpectrumVis(SpectrumVis *spectrumVis) { m_spectrumVis = spectrumVis; }
    const QList<SpectrumHistogramMarker>& getHistogramMarkers() const { return m_histogramMarkers; }
    QList<SpectrumHistogramMarker>& getHistogramMarkers() { return m_histogramMarkers; }
    void setHistogramMarkers(const QList<SpectrumHistogramMarker>& histogramMarkers);
    const QList<SpectrumWaterfallMarker>& getWaterfallMarkers() const { return m_waterfallMarkers; }
    QList<SpectrumWaterfallMarker>& getWaterfallMarkers() { return m_waterfallMarkers; }
    void setWaterfallMarkers(const QList<SpectrumWaterfallMarker>& waterfallMarkers);
    void updateHistogramMarkers();
    void updateWaterfallMarkers();
    SpectrumSettings::MarkersDisplay& getMarkersDisplay() {  return m_markersDisplay; }

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

    QList<SpectrumHistogramMarker> m_histogramMarkers;
    QList<SpectrumWaterfallMarker> m_waterfallMarkers;
    SpectrumSettings::MarkersDisplay m_markersDisplay;

	CursorState m_cursorState;
	int m_cursorChannel;

    SpectrumVis* m_spectrumVis;
	QTimer m_timer;
    int m_fpsPeriodMs;
	QMutex m_mutex;
	bool m_mouseInside;
	bool m_changesPending;

	qint64 m_centerFrequency;
	Real m_referenceLevel;
	Real m_powerRange;
	bool m_linear;
	int m_decay;
	quint32 m_sampleRate;
	quint32 m_timingRate;
    int m_fftOverlap;

	int m_fftSize; //!< FFT size in number of bins
    int m_nbBins;  //!< Number of visible FFT bins (zoom support)

	bool m_displayGrid;
	int m_displayGridIntensity;
	int m_displayTraceIntensity;
	bool m_invertedWaterfall;

	std::vector<Real> m_maxHold;
	bool m_displayMaxHold;
	const Real *m_currentSpectrum;
	bool m_displayCurrent;

	Real m_waterfallShare;

    int m_leftMargin;
    int m_rightMargin;
    int m_topMargin;
    int m_frequencyScaleHeight;
    int m_infoHeight;
    int m_histogramHeight;
    int m_waterfallHeight;
    int m_bottomMargin;
    QFont m_textOverlayFont;
	QPixmap m_leftMarginPixmap;
	QPixmap m_frequencyPixmap;
    QPixmap m_infoPixmap;
	ScaleEngine m_timeScale;
	ScaleEngine m_powerScale;
	ScaleEngine m_frequencyScale;
    QRectF m_histogramRect;
	QRect m_frequencyScaleRect;
    QRectF m_waterfallRect;
    QRect m_infoRect;
	QMatrix4x4 m_glFrequencyScaleBoxMatrix;
	QMatrix4x4 m_glLeftScaleBoxMatrix;
    QMatrix4x4 m_glInfoBoxMatrix;

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
	quint8* m_histogram; //!< Spectrum phosphor matrix of FFT width and PSD height scaled to 100. values [0..239]
	int m_decayDivisor;
	int m_decayDivisorCount;
	int m_histogramStroke;
	QMatrix4x4 m_glHistogramSpectrumMatrix;
	QMatrix4x4 m_glHistogramBoxMatrix;
	bool m_displayHistogram;
	bool m_displayChanged;
    bool m_displaySourceOrSink;
    int m_displayStreamIndex;
    float m_frequencyZoomFactor;
    float m_frequencyZoomPos;
    static const float m_maxFrequencyZoom;

	GLShaderSimple m_glShaderSimple;
	GLShaderTextured m_glShaderLeftScale;
	GLShaderTextured m_glShaderFrequencyScale;
	GLShaderTextured m_glShaderWaterfall;
	GLShaderTextured m_glShaderHistogram;
    GLShaderTextured m_glShaderTextOverlay;
    GLShaderTextured m_glShaderInfo;
	int m_matrixLoc;
	int m_colorLoc;
	IncrementalArray<GLfloat> m_q3TickTime;
	IncrementalArray<GLfloat> m_q3TickFrequency;
	IncrementalArray<GLfloat> m_q3TickPower;
	IncrementalArray<GLfloat> m_q3FFT;

	MessageQueue *m_messageQueueToGUI;

	void updateWaterfall(const Real *spectrum);
	void updateHistogram(const Real *spectrum);

	void initializeGL();
	void resizeGL(int width, int height);
	void paintGL();
    void drawSpectrumMarkers();

	void stopDrag();
	void applyChanges();

	void mouseMoveEvent(QMouseEvent* event);
	void mousePressEvent(QMouseEvent* event);
	void mouseReleaseEvent(QMouseEvent* event);
    void wheelEvent(QWheelEvent*);
    void channelMarkerMove(QWheelEvent*, int mul);
    void zoom(QWheelEvent*);
    void frequencyZoom(float pw);
    void frequencyPan(QMouseEvent*);
    void timeZoom(bool zoomInElseOut);
    void powerZoom(float pw, bool zoomInElseOut);
    void resetFrequencyZoom();
    void updateFFTLimits();
    void setFrequencyScale();
    void getFrequencyZoom(int64_t& centerFrequency, int& frequencySpan);

	void enterEvent(QEvent* event);
	void leaveEvent(QEvent* event);

    QString displayScaled(int64_t value, char type, int precision, bool showMult);
    QString displayScaledF(float value, char type, int precision, bool showMult);
    int getPrecision(int value);
    void drawTextOverlay(      //!< Draws a text overlay
            const QString& text,
            const QColor& color,
            const QFont& font,
            float shiftX,
            float shiftY,
            bool leftHalf,
            bool topHalf,
            const QRectF& glRect);
    void formatTextInfo(QString& info);

private slots:
	void cleanup();
	void tick();
	void channelMarkerChanged();
	void channelMarkerDestroyed(QObject* object);
};

#endif // INCLUDE_GLSPECTRUM_H
