///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <algorithm>

#include <QMouseEvent>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions>
#include <QPainter>
#include <QFontDatabase>
#include <QWindow>
#include "maincore.h"
#include "dsp/spectrumvis.h"
#include "gui/glspectrumview.h"
#include "gui/spectrummeasurements.h"
#include "settings/mainsettings.h"
#include "util/messagequeue.h"
#include "util/db.h"

#include <QDebug>

MESSAGE_CLASS_DEFINITION(GLSpectrumView::MsgReportSampleRate, Message)
MESSAGE_CLASS_DEFINITION(GLSpectrumView::MsgReportWaterfallShare, Message)
MESSAGE_CLASS_DEFINITION(GLSpectrumView::MsgReportFFTOverlap, Message)
MESSAGE_CLASS_DEFINITION(GLSpectrumView::MsgReportPowerScale, Message)
MESSAGE_CLASS_DEFINITION(GLSpectrumView::MsgReportCalibrationShift, Message)
MESSAGE_CLASS_DEFINITION(GLSpectrumView::MsgReportHistogramMarkersChange, Message)
MESSAGE_CLASS_DEFINITION(GLSpectrumView::MsgReportWaterfallMarkersChange, Message)

const float GLSpectrumView::m_maxFrequencyZoom = 10.0f;
const float GLSpectrumView::m_annotationMarkerHeight = 20.0f;

GLSpectrumView::GLSpectrumView(QWidget* parent) :
    QOpenGLWidget(parent),
    m_markersDisplay(SpectrumSettings::MarkersDisplaySpectrum),
    m_histogramFindPeaks(false),
    m_cursorState(CSNormal),
    m_cursorChannel(0),
    m_spectrumVis(nullptr),
    m_fpsPeriodMs(50),
    m_mouseInside(false),
    m_changesPending(true),
    m_centerFrequency(100000000),
    m_referenceLevel(0),
    m_powerRange(100),
    m_linear(false),
    m_decay(1),
    m_sampleRate(500000),
    m_timingRate(1),
    m_fftOverlap(0),
    m_fftSize(512),
    m_nbBins(512),
    m_displayGrid(true),
    m_displayGridIntensity(5),
    m_displayTraceIntensity(50),
    m_invertedWaterfall(true),
    m_displayMaxHold(false),
    m_currentSpectrum(nullptr),
    m_displayCurrent(false),
    m_leftMargin(0),
    m_rightMargin(0),
    m_topMargin(0),
    m_frequencyScaleHeight(0),
    m_histogramHeight(80),
    m_waterfallHeight(0),
    m_bottomMargin(0),
    m_waterfallBuffer(nullptr),
    m_waterfallBufferPos(0),
    m_waterfallTextureHeight(-1),
    m_waterfallTexturePos(0),
    m_displayWaterfall(true),
    m_ssbSpectrum(false),
    m_lsbDisplay(false),
    m_3DSpectrogramBuffer(nullptr),
    m_3DSpectrogramBufferPos(0),
    m_3DSpectrogramTextureHeight(-1),
    m_3DSpectrogramTexturePos(0),
    m_display3DSpectrogram(false),
    m_rotate3DSpectrogram(false),
    m_pan3DSpectrogram(false),
    m_scaleZ3DSpectrogram(false),
    m_3DSpectrogramStyle(SpectrumSettings::Outline),
    m_colorMapName("Angel"),
    m_scrollFrequency(false),
    m_scrollStartCenterFreq(0),
    m_histogramBuffer(nullptr),
    m_histogram(nullptr),
    m_displayHistogram(true),
    m_displayChanged(false),
    m_displaySourceOrSink(true),
    m_displayStreamIndex(0),
    m_matrixLoc(0),
    m_colorLoc(0),
    m_useCalibration(false),
    m_calibrationGain(1.0),
    m_calibrationShiftdB(0.0),
    m_calibrationInterpMode(SpectrumSettings::CalibInterpLinear),
    m_messageQueueToGUI(nullptr),
    m_openGLLogger(nullptr),
    m_isDeviceSpectrum(false),
    m_measurements(nullptr),
    m_measurement(SpectrumSettings::MeasurementNone),
    m_measurementCenterFrequencyOffset(0),
    m_measurementBandwidth(10000),
    m_measurementChSpacing(10000),
    m_measurementAdjChBandwidth(10000),
    m_measurementHarmonics(5),
    m_measurementPeaks(5),
    m_measurementHighlight(true),
    m_measurementPrecision(1)
{
    // Enable multisampling anti-aliasing (MSAA)
    int multisamples = MainCore::instance()->getSettings().getMultisampling();
    if (multisamples > 0)
    {
        QSurfaceFormat format;
        format.setSamples(multisamples);
        setFormat(format);
    }

    setObjectName("GLSpectrum");
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setMouseTracking(true);

    setMinimumSize(360, 200);

    m_waterfallShare = 0.5;

    for (int i = 0; i <= 239; i++)
    {
        QColor c;
        c.setHsv(239 - i, 255, 15 + i);
        ((quint8*)&m_waterfallPalette[i])[0] = c.red();
        ((quint8*)&m_waterfallPalette[i])[1] = c.green();
        ((quint8*)&m_waterfallPalette[i])[2] = c.blue();
        ((quint8*)&m_waterfallPalette[i])[3] = c.alpha();
    }

    m_waterfallPalette[239] = 0xffffffff;
    m_histogramPalette[0] = 0;

    for (int i = 1; i < 240; i++)
    {
        QColor c;
        int light = i < 60 ? 128 + (60-i) : 128;
        int sat   = i < 60 ? 140 + i : i < 180 ? 200 : 200 - (i-180);
        c.setHsl(239 - i, sat, light);
        ((quint8*)&m_histogramPalette[i])[0] = c.red();
        ((quint8*)&m_histogramPalette[i])[1] = c.green();
        ((quint8*)&m_histogramPalette[i])[2] = c.blue();
        ((quint8*)&m_histogramPalette[i])[3] = c.alpha();
    }

    // 4.2.3 palette
//    for (int i = 1; i < 240; i++)
//    {
//        QColor c;
//        int val = i < 60 ? 255 : 200;
//        int sat = i < 60 ? 128 : i < 180 ? 255 : 180;
//        c.setHsv(239 - i, sat, val);
//        ((quint8*)&m_histogramPalette[i])[0] = c.red();
//        ((quint8*)&m_histogramPalette[i])[1] = c.green();
//        ((quint8*)&m_histogramPalette[i])[2] = c.blue();
//        ((quint8*)&m_histogramPalette[i])[3] = c.alpha();
//    }

    // Original palette:
//	for(int i = 16; i < 240; i++) {
//		 QColor c;
//		 c.setHsv(239 - i, 255 - ((i < 200) ? 0 : (i - 200) * 3), 150 + ((i < 100) ? i : 100));
//		 ((quint8*)&m_histogramPalette[i])[0] = c.red();
//		 ((quint8*)&m_histogramPalette[i])[1] = c.green();
//		 ((quint8*)&m_histogramPalette[i])[2] = c.blue();
//		 ((quint8*)&m_histogramPalette[i])[3] = c.alpha();
//	}
//	for(int i = 1; i < 16; i++) {
//		QColor c;
//		c.setHsv(255, 128, 48 + i * 4);
//		((quint8*)&m_histogramPalette[i])[0] = c.red();
//		((quint8*)&m_histogramPalette[i])[1] = c.green();
//		((quint8*)&m_histogramPalette[i])[2] = c.blue();
//		((quint8*)&m_histogramPalette[i])[3] = c.alpha();
//	}

    m_decayDivisor = 1;
    m_decayDivisorCount = m_decayDivisor;
    m_histogramStroke = 30;

    m_timeScale.setFont(font());
    m_timeScale.setOrientation(Qt::Vertical);
    m_timeScale.setRange(Unit::Time, 0, 1);
    m_powerScale.setFont(font());
    m_powerScale.setOrientation(Qt::Vertical);
    m_frequencyScale.setFont(font());
    m_frequencyScale.setOrientation(Qt::Horizontal);

    m_textOverlayFont = font(); // QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_textOverlayFont.setBold(true);
    // m_textOverlayFont.setPointSize(font().pointSize() - 1);
    resetFrequencyZoom();

    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
    m_timer.start(m_fpsPeriodMs);

    // Handle KeyEvents
    setFocusPolicy(Qt::StrongFocus);
    installEventFilter(this);
}

GLSpectrumView::~GLSpectrumView()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (m_waterfallBuffer)
    {
        delete m_waterfallBuffer;
        m_waterfallBuffer = nullptr;
    }

    if (m_3DSpectrogramBuffer)
    {
        delete m_3DSpectrogramBuffer;
        m_3DSpectrogramBuffer = nullptr;
    }

    if (m_histogramBuffer)
    {
        delete m_histogramBuffer;
        m_histogramBuffer = nullptr;
    }

    if (m_histogram)
    {
        delete[] m_histogram;
        m_histogram = nullptr;
    }

    if (m_openGLLogger)
    {
        delete m_openGLLogger;
        m_openGLLogger = nullptr;
    }
}

void GLSpectrumView::setCenterFrequency(qint64 frequency)
{
    m_mutex.lock();
    m_centerFrequency = frequency;

    if (m_useCalibration) {
        updateCalibrationPoints();
    }

    m_changesPending = true;
    m_mutex.unlock();
    update();
}

void GLSpectrumView::setReferenceLevel(Real referenceLevel)
{
    m_mutex.lock();
    m_referenceLevel = referenceLevel;
    m_changesPending = true;
    m_mutex.unlock();
    update();
}

void GLSpectrumView::setPowerRange(Real powerRange)
{
    m_mutex.lock();
    m_powerRange = powerRange;
    m_changesPending = true;
    m_mutex.unlock();
    update();
}

void GLSpectrumView::setDecay(int decay)
{
    m_decay = decay < 0 ? 0 : decay > 20 ? 20 : decay;
}

void GLSpectrumView::setDecayDivisor(int decayDivisor)
{
    m_decayDivisor = decayDivisor < 1 ? 1 : decayDivisor > 20 ? 20 : decayDivisor;
}

void GLSpectrumView::setHistoStroke(int stroke)
{
    m_histogramStroke = stroke < 1 ? 1 : stroke > 60 ? 60 : stroke;
}

void GLSpectrumView::setSampleRate(qint32 sampleRate)
{
    m_mutex.lock();
    m_sampleRate = sampleRate;

    if (m_messageQueueToGUI) {
        m_messageQueueToGUI->push(new MsgReportSampleRate(m_sampleRate));
    }

    m_changesPending = true;
    m_mutex.unlock();
    update();
}

void GLSpectrumView::setTimingRate(qint32 timingRate)
{
    m_mutex.lock();
    m_timingRate = timingRate;
    m_changesPending = true;
    m_mutex.unlock();
    update();
}

void GLSpectrumView::setFFTOverlap(int overlap)
{
    m_mutex.lock();
    m_fftOverlap = overlap;
    m_changesPending = true;
    m_mutex.unlock();
    update();
}

void GLSpectrumView::setDisplayWaterfall(bool display)
{
    m_mutex.lock();
    m_displayWaterfall = display;
    if (!display)
    {
        m_waterfallMarkers.clear();
        if (m_messageQueueToGUI) {
            m_messageQueueToGUI->push(new MsgReportWaterfallMarkersChange());
        }
    }
    m_changesPending = true;
    stopDrag();
    m_mutex.unlock();
    update();
}

void GLSpectrumView::setDisplay3DSpectrogram(bool display)
{
    m_mutex.lock();
    m_display3DSpectrogram = display;
    m_changesPending = true;
    stopDrag();
    m_mutex.unlock();
    update();
}

void GLSpectrumView::setSpectrumStyle(SpectrumSettings::SpectrumStyle style)
{
    m_spectrumStyle = style;
    update();
}

void GLSpectrumView::set3DSpectrogramStyle(SpectrumSettings::SpectrogramStyle style)
{
    m_3DSpectrogramStyle = style;
    update();
}

void GLSpectrumView::setColorMapName(const QString &colorMapName)
{
    m_mutex.lock();
    m_colorMapName = colorMapName;
    m_changesPending = true;
    m_mutex.unlock();
    update();
}

void GLSpectrumView::setSsbSpectrum(bool ssbSpectrum)
{
    m_ssbSpectrum = ssbSpectrum;
    update();
}

void GLSpectrumView::setLsbDisplay(bool lsbDisplay)
{
    m_lsbDisplay = lsbDisplay;
    update();
}

void GLSpectrumView::setInvertedWaterfall(bool inv)
{
    m_mutex.lock();
    m_invertedWaterfall = inv;
    m_changesPending = true;
    stopDrag();
    m_mutex.unlock();
    update();
}

void GLSpectrumView::setDisplayMaxHold(bool display)
{
    m_mutex.lock();
    m_displayMaxHold = display;
    if (!m_displayMaxHold && !m_displayCurrent && !m_displayHistogram)
    {
        m_histogramMarkers.clear();
        if (m_messageQueueToGUI) {
            m_messageQueueToGUI->push(new MsgReportHistogramMarkersChange());
        }
    }
    m_changesPending = true;
    stopDrag();
    m_mutex.unlock();
    update();
}

void GLSpectrumView::setDisplayCurrent(bool display)
{
    m_mutex.lock();
    m_displayCurrent = display;
    if (!m_displayMaxHold && !m_displayCurrent && !m_displayHistogram)
    {
        m_histogramMarkers.clear();
        if (m_messageQueueToGUI) {
            m_messageQueueToGUI->push(new MsgReportHistogramMarkersChange());
        }
    }
    m_changesPending = true;
    stopDrag();
    m_mutex.unlock();
    update();
}

void GLSpectrumView::setDisplayHistogram(bool display)
{
    m_mutex.lock();
    m_displayHistogram = display;
    if (!m_displayMaxHold && !m_displayCurrent && !m_displayHistogram)
    {
        m_histogramMarkers.clear();
        if (m_messageQueueToGUI) {
            m_messageQueueToGUI->push(new MsgReportHistogramMarkersChange());
        }
    }
    m_changesPending = true;
    stopDrag();
    m_mutex.unlock();
    update();
}

void GLSpectrumView::setDisplayGrid(bool display)
{
    m_displayGrid = display;
    update();
}

void GLSpectrumView::setDisplayGridIntensity(int intensity)
{
    m_displayGridIntensity = intensity;

    if (m_displayGridIntensity > 100) {
        m_displayGridIntensity = 100;
    } else if (m_displayGridIntensity < 0) {
        m_displayGridIntensity = 0;
    }

    update();
}

void GLSpectrumView::setDisplayTraceIntensity(int intensity)
{
    m_displayTraceIntensity = intensity;

    if (m_displayTraceIntensity > 100) {
        m_displayTraceIntensity = 100;
    } else if (m_displayTraceIntensity < 0) {
        m_displayTraceIntensity = 0;
    }

    update();
}

void GLSpectrumView::setFreqScaleTruncationMode(bool mode)
{
    m_frequencyScale.setTruncateMode(mode);
    update();
}

void GLSpectrumView::setLinear(bool linear)
{
    m_mutex.lock();
    m_linear = linear;
    m_changesPending = true;
    m_mutex.unlock();
    update();
}

void GLSpectrumView::setUseCalibration(bool useCalibration)
{
    m_mutex.lock();
    m_useCalibration = useCalibration;

    if (m_messageQueueToGUI) {
        m_messageQueueToGUI->push(new MsgReportCalibrationShift(m_useCalibration ? m_calibrationShiftdB : 0.0));
    }

    m_changesPending = true;
    m_mutex.unlock();
    update();
}

void GLSpectrumView::setMeasurementParams(SpectrumSettings::Measurement measurement,
                                      int centerFrequencyOffset, int bandwidth, int chSpacing, int adjChBandwidth,
                                      int harmonics, int peaks, bool highlight, int precision)
{
    m_mutex.lock();
    m_measurement = measurement;
    m_measurementCenterFrequencyOffset = centerFrequencyOffset;
    m_measurementBandwidth = bandwidth;
    m_measurementChSpacing = chSpacing;
    m_measurementAdjChBandwidth = adjChBandwidth;
    m_measurementHarmonics = harmonics;
    m_measurementPeaks = peaks;
    m_measurementHighlight = highlight;
    m_measurementPrecision = precision;
    m_changesPending = true;
    if (m_measurements) {
        m_measurements->setMeasurementParams(measurement, peaks, precision);
    }
    m_mutex.unlock();
    update();
}

void GLSpectrumView::addChannelMarker(ChannelMarker* channelMarker)
{
    m_mutex.lock();
    connect(channelMarker, SIGNAL(changedByAPI()), this, SLOT(channelMarkerChanged()));
    connect(channelMarker, SIGNAL(destroyed(QObject*)), this, SLOT(channelMarkerDestroyed(QObject*)));
    m_channelMarkerStates.append(new ChannelMarkerState(channelMarker));
    m_changesPending = true;
    stopDrag();
    m_mutex.unlock();
    update();
}

void GLSpectrumView::removeChannelMarker(ChannelMarker* channelMarker)
{
    m_mutex.lock();

    for (int i = 0; i < m_channelMarkerStates.size(); ++i)
    {
        if (m_channelMarkerStates[i]->m_channelMarker == channelMarker)
        {
            channelMarker->disconnect(this);
            delete m_channelMarkerStates.takeAt(i);
            m_changesPending = true;
            stopDrag();
            m_mutex.unlock();
            update();
            return;
        }
    }

    m_mutex.unlock();
}

void GLSpectrumView::setHistogramMarkers(const QList<SpectrumHistogramMarker>& histogramMarkers)
{
    m_mutex.lock();
    m_histogramMarkers = histogramMarkers;
    updateHistogramMarkers();
    m_changesPending = true;
    m_mutex.unlock();
    update();
}

void GLSpectrumView::setWaterfallMarkers(const QList<SpectrumWaterfallMarker>& waterfallMarkers)
{
    m_mutex.lock();
    m_waterfallMarkers = waterfallMarkers;
    updateWaterfallMarkers();
    m_changesPending = true;
    m_mutex.unlock();
    update();
}

void GLSpectrumView::setAnnotationMarkers(const QList<SpectrumAnnotationMarker>& annotationMarkers)
{
    m_mutex.lock();
    m_annotationMarkers = annotationMarkers;
    updateAnnotationMarkers();
    m_changesPending = true;
    m_mutex.unlock();
    update();
}

void GLSpectrumView::setMarkersDisplay(SpectrumSettings::MarkersDisplay markersDisplay)
{
	m_mutex.lock();
	m_markersDisplay = markersDisplay;
	updateMarkersDisplay();
    m_changesPending = true;
    m_mutex.unlock();
    update();
}

void GLSpectrumView::setCalibrationPoints(const QList<SpectrumCalibrationPoint>& calibrationPoints)
{
    m_mutex.lock();
    m_calibrationPoints = calibrationPoints;
    updateCalibrationPoints();
    m_changesPending = true;
    m_mutex.unlock();
    update();
}

void GLSpectrumView::setCalibrationInterpMode(SpectrumSettings::CalibrationInterpolationMode mode)
{
	m_mutex.lock();
    m_calibrationInterpMode = mode;
    updateCalibrationPoints();
    m_changesPending = true;
    m_mutex.unlock();
    update();
}

float GLSpectrumView::getPowerMax() const
{
    return m_linear ? m_powerScale.getRangeMax() : CalcDb::powerFromdB(m_powerScale.getRangeMax());
}

float GLSpectrumView::getTimeMax() const
{
    return m_timeScale.getRangeMax();
}

void GLSpectrumView::newSpectrum(const Real *spectrum, int nbBins, int fftSize)
{
    QMutexLocker mutexLocker(&m_mutex);

    m_displayChanged = true;
    if (m_changesPending)
    {
        m_fftSize = fftSize;
        m_nbBins = nbBins;
        return;
    }

    if ((fftSize != m_fftSize) || (m_nbBins != nbBins))
    {
        m_fftSize = fftSize;
        m_nbBins = nbBins;
        m_changesPending = true;
        return;
    }

    updateWaterfall(spectrum);
    update3DSpectrogram(spectrum);
    updateHistogram(spectrum);
}

void GLSpectrumView::updateWaterfall(const Real *spectrum)
{
    if (m_waterfallBufferPos < m_waterfallBuffer->height())
    {
        quint32* pix = (quint32*)m_waterfallBuffer->scanLine(m_waterfallBufferPos);

        for (int i = 0; i < m_nbBins; i++)
        {
            int v = (int)((spectrum[i] - m_referenceLevel) * 2.4 * 100.0 / m_powerRange + 240.0);

            if (v > 239) {
                v = 239;
            } else if (v < 0) {
                v = 0;
            }

            *pix++ = m_waterfallPalette[(int)v];
        }

        m_waterfallBufferPos++;
    }
}

void GLSpectrumView::update3DSpectrogram(const Real *spectrum)
{
    if (m_3DSpectrogramBufferPos < m_3DSpectrogramBuffer->height())
    {
        quint8* pix = (quint8*)m_3DSpectrogramBuffer->scanLine(m_3DSpectrogramBufferPos);

        for (int i = 0; i < m_nbBins; i++)
        {
            int v = (int)((spectrum[i] - m_referenceLevel) * 2.4 * 100.0 / m_powerRange + 240.0);

            if (v > 255) {
                v = 255;
            } else if (v < 0) {
                v = 0;
            }

            *pix++ = v;
        }

        m_3DSpectrogramBufferPos++;
    }
}

void GLSpectrumView::updateHistogram(const Real *spectrum)
{
    quint8* b = m_histogram;
    int fftMulSize = 100 * m_nbBins;

    if ((m_displayHistogram || m_displayMaxHold) && (m_decay != 0))
    {
        m_decayDivisorCount--;

        if ((m_decay > 1) || (m_decayDivisorCount <= 0))
        {
            for (int i = 0; i < fftMulSize; i++)
            {
                if (*b > m_decay) {
                    *b = *b - m_decay;
                } else {
                    *b = 0;
                }

                b++;
            }

            m_decayDivisorCount = m_decayDivisor;
        }
    }

    m_currentSpectrum = spectrum; // Store spectrum for current spectrum line display

#if 0 //def USE_SSE2
    if(m_decay >= 0) { // normal
        const __m128 refl = {m_referenceLevel, m_referenceLevel, m_referenceLevel, m_referenceLevel};
        const __m128 power = {m_powerRange, m_powerRange, m_powerRange, m_powerRange};
        const __m128 mul = {100.0f, 100.0f, 100.0f, 100.0f};

        for(int i = 0; i < m_fftSize; i += 4) {
            __m128 abc = _mm_loadu_ps (&spectrum[i]);
            abc = _mm_sub_ps(abc, refl);
            abc = _mm_mul_ps(abc, mul);
            abc = _mm_div_ps(abc, power);
            abc =  _mm_add_ps(abc, mul);
            __m128i result = _mm_cvtps_epi32(abc);

            for(int j = 0; j < 4; j++) {
                int v = ((int*)&result)[j];
                if((v >= 0) && (v <= 99)) {
                    b = m_histogram + (i + j) * 100 + v;
                    if(*b < 220)
                        *b += m_histogramStroke; // was 4
                    else if(*b < 239)
                        *b += 1;
                }
            }
        }
    } else { // draw double pixels
        int add = -m_decay * 4;
        const __m128 refl = {m_referenceLevel, m_referenceLevel, m_referenceLevel, m_referenceLevel};
        const __m128 power = {m_powerRange, m_powerRange, m_powerRange, m_powerRange};
        const __m128 mul = {100.0f, 100.0f, 100.0f, 100.0f};

        for(int i = 0; i < m_fftSize; i += 4) {
            __m128 abc = _mm_loadu_ps (&spectrum[i]);
            abc = _mm_sub_ps(abc, refl);
            abc = _mm_mul_ps(abc, mul);
            abc = _mm_div_ps(abc, power);
            abc =  _mm_add_ps(abc, mul);
            __m128i result = _mm_cvtps_epi32(abc);

            for(int j = 0; j < 4; j++) {
                int v = ((int*)&result)[j];
                if((v >= 1) && (v <= 98)) {
                    b = m_histogram + (i + j) * 100 + v;
                    if(b[-1] < 220)
                        b[-1] += add;
                    else if(b[-1] < 239)
                        b[-1] += 1;
                    if(b[0] < 220)
                        b[0] += add;
                    else if(b[0] < 239)
                        b[0] += 1;
                    if(b[1] < 220)
                        b[1] += add;
                    else if(b[1] < 239)
                        b[1] += 1;
                } else if((v >= 0) && (v <= 99)) {
                    b = m_histogram + (i + j) * 100 + v;
                    if(*b < 220)
                        *b += add;
                    else if(*b < 239)
                        *b += 1;
                }
            }
        }
    }
#else
    for (int i = 0; i < m_nbBins; i++)
    {
        int v = (int)((spectrum[i] - m_referenceLevel) * 100.0 / m_powerRange + 100.0);

        if ((v >= 0) && (v <= 99))
        {
            b = m_histogram + i * 100 + v;

            // capping to 239 as palette values are [0..239]
            if (*b + m_histogramStroke <= 239) {
                *b += m_histogramStroke; // was 4
            } else {
                *b = 239;
            }
        }
    }
#endif
}

void GLSpectrumView::initializeGL()
{
    QOpenGLContext *glCurrentContext =  QOpenGLContext::currentContext();
    int majorVersion = 0;
    int minorVersion = 0;

    if (glCurrentContext)
    {
        if (QOpenGLContext::currentContext()->isValid())
        {
            qDebug() << "GLSpectrumView::initializeGL: context:"
                << " major: " << (QOpenGLContext::currentContext()->format()).majorVersion()
                << " minor: " << (QOpenGLContext::currentContext()->format()).minorVersion()
                << " ES: " << (QOpenGLContext::currentContext()->isOpenGLES() ? "yes" : "no");
            majorVersion = (QOpenGLContext::currentContext()->format()).majorVersion();
            minorVersion = (QOpenGLContext::currentContext()->format()).minorVersion();
        }
        else {
            qDebug() << "GLSpectrumView::initializeGL: current context is invalid";
        }

        // Enable OpenGL debugging
        // Disable for release, as some OpenGL drivers are quite verbose and output
        // info on every frame
        if (false)
        {
            QSurfaceFormat format = glCurrentContext->format();
            format.setOption(QSurfaceFormat::DebugContext);
            glCurrentContext->setFormat(format);

            if (glCurrentContext->hasExtension(QByteArrayLiteral("GL_KHR_debug")))
            {
                m_openGLLogger = new QOpenGLDebugLogger(this);
                m_openGLLogger->initialize();
                connect(m_openGLLogger, &QOpenGLDebugLogger::messageLogged, this, &GLSpectrumView::openGLDebug);
                m_openGLLogger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
            }
            else
            {
                qDebug() << "GLSpectrumView::initializeGL: GL_KHR_debug not available";
            }
        }
    }
    else
    {
        qCritical() << "GLSpectrumView::initializeGL: no current context";
        return;
    }

    QOpenGLFunctions *glFunctions = QOpenGLContext::currentContext()->functions();
    glFunctions->initializeOpenGLFunctions();

    //glDisable(GL_DEPTH_TEST);
    m_glShaderSimple.initializeGL(majorVersion, minorVersion);
    m_glShaderLeftScale.initializeGL(majorVersion, minorVersion);
    m_glShaderFrequencyScale.initializeGL(majorVersion, minorVersion);
    m_glShaderWaterfall.initializeGL(majorVersion, minorVersion);
    m_glShaderHistogram.initializeGL(majorVersion, minorVersion);
    m_glShaderColorMap.initializeGL(majorVersion, minorVersion);
    m_glShaderTextOverlay.initializeGL(majorVersion, minorVersion);
    m_glShaderInfo.initializeGL(majorVersion, minorVersion);
    m_glShaderSpectrogram.initializeGL(majorVersion, minorVersion);
    m_glShaderSpectrogramTimeScale.initializeGL(majorVersion, minorVersion);
    m_glShaderSpectrogramPowerScale.initializeGL(majorVersion, minorVersion);
}

void GLSpectrumView::openGLDebug(const QOpenGLDebugMessage &debugMessage)
{
    qDebug() << "GLSpectrumView::openGLDebug: " << debugMessage;
}

void GLSpectrumView::resizeGL(int width, int height)
{
    QMutexLocker mutexLocker(&m_mutex);
    QOpenGLFunctions *glFunctions = QOpenGLContext::currentContext()->functions();
    glFunctions->glViewport(0, 0, width, height);
    m_changesPending = true;
}

void GLSpectrumView::clearSpectrumHistogram()
{
    if (!m_mutex.tryLock(2)) {
        return;
    }

    memset(m_histogram, 0x00, 100 * m_nbBins);

    m_mutex.unlock();
    update();
}

void GLSpectrumView::paintGL()
{
    if (!m_mutex.tryLock(2)) {
        return;
    }

    if (m_changesPending)
    {
        applyChanges();
        m_changesPending = false;
    }

    if (m_nbBins <= 0)
    {
        m_mutex.unlock();
        return;
    }

    QOpenGLFunctions *glFunctions = QOpenGLContext::currentContext()->functions();
    glFunctions->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glFunctions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QMatrix4x4 spectrogramGridMatrix;
    int devicePixelRatio;

    if (m_display3DSpectrogram)
    {
        m_glShaderSpectrogram.applyTransform(spectrogramGridMatrix);
        // paint 3D spectrogram
        if (m_3DSpectrogramTexturePos + m_3DSpectrogramBufferPos < m_3DSpectrogramTextureHeight)
        {
            m_glShaderSpectrogram.subTexture(0, m_3DSpectrogramTexturePos, m_nbBins, m_3DSpectrogramBufferPos,  m_3DSpectrogramBuffer->scanLine(0));
            m_3DSpectrogramTexturePos += m_3DSpectrogramBufferPos;
        }
        else
        {
            int breakLine = m_3DSpectrogramTextureHeight - m_3DSpectrogramTexturePos;
            int linesLeft = m_3DSpectrogramTexturePos + m_3DSpectrogramBufferPos - m_3DSpectrogramTextureHeight;
            m_glShaderSpectrogram.subTexture(0, m_3DSpectrogramTexturePos, m_nbBins, breakLine,  m_3DSpectrogramBuffer->scanLine(0));
            m_glShaderSpectrogram.subTexture(0, 0, m_nbBins, linesLeft,  m_3DSpectrogramBuffer->scanLine(breakLine));
            m_3DSpectrogramTexturePos = linesLeft;
        }

        m_3DSpectrogramBufferPos = 0;

        float prop_y = m_3DSpectrogramTexturePos / (m_3DSpectrogramTextureHeight - 1.0);

        // Temporarily reduce viewport to waterfall area so anything outside is clipped
        if (window()->windowHandle()) {
            devicePixelRatio = window()->windowHandle()->devicePixelRatio();
        } else {
            devicePixelRatio = 1;
        }
        glFunctions->glViewport(0, m_3DSpectrogramBottom*devicePixelRatio, width()*devicePixelRatio, m_waterfallHeight*devicePixelRatio);
        m_glShaderSpectrogram.drawSurface(m_3DSpectrogramStyle, spectrogramGridMatrix, prop_y, m_invertedWaterfall);
        glFunctions->glViewport(0, 0, width()*devicePixelRatio, height()*devicePixelRatio);
    }
    else if (m_displayWaterfall)
    {
        // paint 2D waterfall
        {
            GLfloat vtx1[] = {
                    0, m_invertedWaterfall ? 0.0f : 1.0f,
                    1, m_invertedWaterfall ? 0.0f : 1.0f,
                    1, m_invertedWaterfall ? 1.0f : 0.0f,
                    0, m_invertedWaterfall ? 1.0f : 0.0f
            };


            if (m_waterfallTexturePos + m_waterfallBufferPos < m_waterfallTextureHeight)
            {
                m_glShaderWaterfall.subTexture(0, m_waterfallTexturePos, m_nbBins, m_waterfallBufferPos,  m_waterfallBuffer->scanLine(0));
                m_waterfallTexturePos += m_waterfallBufferPos;
            }
            else
            {
                int breakLine = m_waterfallTextureHeight - m_waterfallTexturePos;
                int linesLeft = m_waterfallTexturePos + m_waterfallBufferPos - m_waterfallTextureHeight;
                m_glShaderWaterfall.subTexture(0, m_waterfallTexturePos, m_nbBins, breakLine,  m_waterfallBuffer->scanLine(0));
                m_glShaderWaterfall.subTexture(0, 0, m_nbBins, linesLeft,  m_waterfallBuffer->scanLine(breakLine));
                m_waterfallTexturePos = linesLeft;
            }

            m_waterfallBufferPos = 0;

            float prop_y = m_waterfallTexturePos / (m_waterfallTextureHeight - 1.0);
            float off = 1.0 / (m_waterfallTextureHeight - 1.0);

            GLfloat tex1[] = {
                    0, prop_y + 1 - off,
                    1, prop_y + 1 - off,
                    1, prop_y,
                    0, prop_y
            };

            m_glShaderWaterfall.drawSurface(m_glWaterfallBoxMatrix, tex1, vtx1, 4);
        }

        // paint channels
        if (m_mouseInside)
        {
            for (int i = 0; i < m_channelMarkerStates.size(); ++i)
            {
                ChannelMarkerState* dv = m_channelMarkerStates[i];

                if (dv->m_channelMarker->getVisible()
                    && (dv->m_channelMarker->getSourceOrSinkStream() == m_displaySourceOrSink)
                    && dv->m_channelMarker->streamIndexApplies(m_displayStreamIndex))
                {
                    {
                        GLfloat q3[] {
                            0, 0,
                            1, 0,
                            1, 1,
                            0, 1,
                            0.5, 0,
                            0.5, 1,
                        };

                        QVector4D color(dv->m_channelMarker->getColor().redF(), dv->m_channelMarker->getColor().greenF(), dv->m_channelMarker->getColor().blueF(), 0.3f);
                        m_glShaderSimple.drawSurface(dv->m_glMatrixWaterfall, color, q3, 4);

                        QVector4D colorLine(0.8f, 0.8f, 0.6f, 1.0f);
                        m_glShaderSimple.drawSegments(dv->m_glMatrixDsbWaterfall, colorLine, &q3[8], 2);

                    }
                }
            }
        }

        // draw rect around
        {
            GLfloat q3[] {
                1, 1,
                0, 1,
                0, 0,
                1, 0
            };

            QVector4D color(1.0f, 1.0f, 1.0f, 0.5f);
            m_glShaderSimple.drawContour(m_glWaterfallBoxMatrix, color, q3, 4);
        }
    }

    // paint histogram
    if (m_displayHistogram || m_displayMaxHold || m_displayCurrent)
    {
        if (m_displayHistogram)
        {
            {
                // import new lines into the texture
                quint32* pix;
                quint8* bs = m_histogram;

                for (int y = 0; y < 100; y++)
                {
                    quint8* b = bs;
                    pix = (quint32*)m_histogramBuffer->scanLine(99 - y);

                    for (int x = 0; x < m_nbBins; x++)
                    {
                        *pix = m_histogramPalette[*b];
                        pix++;
                        b += 100;
                    }

                    bs++;
                }

                GLfloat vtx1[] = {
                        0, 0,
                        1, 0,
                        1, 1,
                        0, 1
                };
                GLfloat tex1[] = {
                        0, 0,
                        1, 0,
                        1, 1,
                        0, 1
                };

                m_glShaderHistogram.subTexture(0, 0, m_nbBins, 100,  m_histogramBuffer->scanLine(0));
                m_glShaderHistogram.drawSurface(m_glHistogramBoxMatrix, tex1, vtx1, 4);
            }
        }


        // paint channels
        if (m_mouseInside)
        {
            // Effective BW overlays
            for (int i = 0; i < m_channelMarkerStates.size(); ++i)
            {
                ChannelMarkerState* dv = m_channelMarkerStates[i];

                if (dv->m_channelMarker->getVisible()
                    && (dv->m_channelMarker->getSourceOrSinkStream() == m_displaySourceOrSink)
                    && dv->m_channelMarker->streamIndexApplies(m_displayStreamIndex))
                {
                    {
                        GLfloat q3[] {
                            0, 0,
                            1, 0,
                            1, 1,
                            0, 1,
                            0.5, 0,
                            0.5, 1
                        };

                        QVector4D color(dv->m_channelMarker->getColor().redF(), dv->m_channelMarker->getColor().greenF(), dv->m_channelMarker->getColor().blueF(), 0.3f);
                        m_glShaderSimple.drawSurface(dv->m_glMatrixHistogram, color, q3, 4);

                        QVector4D colorLine(0.8f, 0.8f, 0.6f, 1.0f);

                        if (dv->m_channelMarker->getSidebands() != ChannelMarker::dsb) {
                            q3[6] = 0.5;
                        }

                        m_glShaderSimple.drawSegments(dv->m_glMatrixDsbHistogram, colorLine, &q3[8], 2);
                        m_glShaderSimple.drawSegments(dv->m_glMatrixFreqScale, colorLine, q3, 2);
                    }
                }
            }
        }
    }

    // paint left scales (time and power)
    if (m_displayWaterfall || m_displayMaxHold || m_displayCurrent || m_displayHistogram )
    {
        {
            GLfloat vtx1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };
            GLfloat tex1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };

            m_glShaderLeftScale.drawSurface(m_glLeftScaleBoxMatrix, tex1, vtx1, 4);
        }
    }

    // paint frequency scale
    if (m_displayWaterfall || m_displayMaxHold || m_displayCurrent || m_displayHistogram)
    {
        {
            GLfloat vtx1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };
            GLfloat tex1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };

            m_glShaderFrequencyScale.drawSurface(m_glFrequencyScaleBoxMatrix, tex1, vtx1, 4);
        }

        // paint channels

        // Effective bandwidth overlays
        for (int i = 0; i < m_channelMarkerStates.size(); ++i)
        {
            ChannelMarkerState* dv = m_channelMarkerStates[i];

            // frequency scale channel overlay
            if (dv->m_channelMarker->getVisible()
                && (dv->m_channelMarker->getSourceOrSinkStream() == m_displaySourceOrSink)
                && dv->m_channelMarker->streamIndexApplies(m_displayStreamIndex))
            {
                {
                    GLfloat q3[] {
                        1, 0.2,
                        0, 0.2,
                        0, 0,
                        1, 0,
                        0.5, 0,
                        0.5, 1
                    };

                    QVector4D color(dv->m_channelMarker->getColor().redF(), dv->m_channelMarker->getColor().greenF(), dv->m_channelMarker->getColor().blueF(), 0.5f);
                    m_glShaderSimple.drawSurface(dv->m_glMatrixFreqScale, color, q3, 4);

                    if (dv->m_channelMarker->getHighlighted())
                    {
                        QVector4D colorLine(0.8f, 0.8f, 0.6f, 1.0f);
                        m_glShaderSimple.drawSegments(dv->m_glMatrixDsbFreqScale, colorLine, &q3[8], 2);
                        m_glShaderSimple.drawSegments(dv->m_glMatrixFreqScale, colorLine, &q3[4], 2);
                    }
                }
            }
        }
    }

    // paint 3D spectrogram scales
    if (m_display3DSpectrogram && m_displayGrid)
    {
        glFunctions->glViewport(0, m_3DSpectrogramBottom*devicePixelRatio, width()*devicePixelRatio, m_waterfallHeight*devicePixelRatio);
        {
            GLfloat l = m_spectrogramTimePixmap.width() / (GLfloat) width();
            GLfloat r = m_rightMargin / (GLfloat) width();
            GLfloat h = m_frequencyPixmap.height() / (GLfloat) m_waterfallHeight;

            GLfloat vtx1[] = {
                       -l, -h,
                   1.0f+r, -h,
                   1.0f+r,  0.0f,
                       -l,  0.0f
            };
            GLfloat tex1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };

            m_glShaderFrequencyScale.drawSurface(spectrogramGridMatrix, tex1, vtx1, 4);
        }

        {
            GLfloat w = m_spectrogramTimePixmap.width() / (GLfloat) width();
            GLfloat h = (m_bottomMargin/2) / (GLfloat) m_waterfallHeight;      // m_bottomMargin is fm.ascent

            GLfloat vtx1[] = {
                    -w, 0.0f-h,
                  0.0f, 0.0f-h,
                  0.0f, 1.0f+h,
                    -w, 1.0f+h
            };
            GLfloat tex1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };

            m_glShaderSpectrogramTimeScale.drawSurface(spectrogramGridMatrix, tex1, vtx1, 4);
        }

        {
            GLfloat w = m_spectrogramPowerPixmap.width() / (GLfloat) width();
            GLfloat h = m_topMargin / (GLfloat) m_spectrogramPowerPixmap.height();

            GLfloat vtx1[] = {
                    -w, 1.0f, 0.0f,
                  0.0f, 1.0f, 0.0f,
                  0.0f, 1.0f, 1.0f+h,
                    -w, 1.0f, 1.0f+h,
            };
            GLfloat tex1[] = {
                    0, 1,
                    1, 1,
                    1, 0,
                    0, 0
            };

            m_glShaderSpectrogramPowerScale.drawSurface(spectrogramGridMatrix, tex1, vtx1, 4, 3);
        }

        glFunctions->glViewport(0, 0, width()*devicePixelRatio, height()*devicePixelRatio);
    }

    // paint max hold lines on top of histogram
    if (m_displayMaxHold)
    {
        if (m_maxHold.size() < (uint) m_nbBins) {
            m_maxHold.resize(m_nbBins);
        }

        for (int i = 0; i < m_nbBins; i++)
        {
            int j;
            quint8* bs = m_histogram + i * 100;

            for (j = 99; j >= 0; j--)
            {
                if (bs[j] > 0) {
                    break;
                }
            }

            // m_referenceLevel : top
            // m_referenceLevel - m_powerRange : bottom
            m_maxHold[i] = ((j - 99) * m_powerRange) / 99.0 + m_referenceLevel;
        }
        // Fill under max hold line
        if (m_spectrumStyle != SpectrumSettings::Line)
        {
            GLfloat *q3 = m_q3ColorMap.m_array;
            for (int i = 0; i < m_nbBins; i++)
            {
                Real v = m_maxHold[i] - m_referenceLevel;

                if (v > 0) {
                    v = 0;
                } else if (v < -m_powerRange) {
                    v = -m_powerRange;
                }

                q3[4*i] = (GLfloat)i;
                q3[4*i+1] = -m_powerRange;
                q3[4*i+2] = (GLfloat)i;
                q3[4*i+3] = v;
            }
            // Replicate Nyquist sample to end of positive side
            q3[4*m_nbBins] = (GLfloat) m_nbBins;
            q3[4*m_nbBins+1] = q3[1];
            q3[4*m_nbBins+2] = (GLfloat) m_nbBins;
            q3[4*m_nbBins+3] = q3[3];

            QVector4D color(0.5f, 0.0f, 0.0f, (float) m_displayTraceIntensity / 100.0f);
            m_glShaderSimple.drawSurfaceStrip(m_glHistogramSpectrumMatrix, color, q3, 2*(m_nbBins+1));
        }
        // Max hold line
        {
            GLfloat *q3 = m_q3FFT.m_array;

            for (int i = 0; i < m_nbBins; i++)
            {
                Real v = m_maxHold[i] - m_referenceLevel;

                if (v >= 0) {
                    v = 0;
                } else if (v < -m_powerRange) {
                    v = -m_powerRange;
                }

                q3[2*i] = (Real) i;
                q3[2*i+1] = v;
            }
            // Replicate Nyquist sample to end of positive side
            q3[2*m_nbBins] = (GLfloat) m_nbBins;
            q3[2*m_nbBins+1] = q3[1];

            QVector4D color(1.0f, 0.0f, 0.0f, (float) m_displayTraceIntensity / 100.0f);
            m_glShaderSimple.drawPolyline(m_glHistogramSpectrumMatrix, color, q3, m_nbBins+1);
        }
    }

    // paint current spectrum line on top of histogram
    if (m_displayCurrent && m_currentSpectrum)
    {
        Real bottom = -m_powerRange;
        GLfloat *q3;

        if (m_spectrumStyle != SpectrumSettings::Line)
        {
            q3 = m_q3ColorMap.m_array;
            // Fill under line
            for (int i = 0; i < m_nbBins; i++)
            {
                Real v = m_currentSpectrum[i] - m_referenceLevel;

                if (v > 0) {
                    v = 0;
                } else if (v < bottom) {
                    v = bottom;
                }

                q3[4*i] = (GLfloat)i;
                q3[4*i+1] = bottom;
                q3[4*i+2] = (GLfloat)i;
                q3[4*i+3] = v;
            }
            // Replicate Nyquist sample to end of positive side
            q3[4*m_nbBins] = (GLfloat) m_nbBins;
            q3[4*m_nbBins+1] = q3[1];
            q3[4*m_nbBins+2] = (GLfloat) m_nbBins;
            q3[4*m_nbBins+3] = q3[3];

            QVector4D color(1.0f, 1.0f, 0.25f, (float) m_displayTraceIntensity / 100.0f);
            if (m_spectrumStyle == SpectrumSettings::Gradient) {
                m_glShaderColorMap.drawSurfaceStrip(m_glHistogramSpectrumMatrix, q3, 2*(m_nbBins+1), bottom, 0.75f);
            } else {
                m_glShaderSimple.drawSurfaceStrip(m_glHistogramSpectrumMatrix, color, q3, 2*(m_nbBins+1));
            }
        }

        {
            if (m_histogramFindPeaks) {
                m_peakFinder.init(m_currentSpectrum[0]);
            }

            // Draw line
            q3 = m_q3FFT.m_array;
            for (int i = 0; i < m_nbBins; i++)
            {
                Real v = m_currentSpectrum[i] - m_referenceLevel;

                if (v > 0) {
                    v = 0;
                } else if (v < bottom) {
                    v = bottom;
                }

                q3[2*i] = (Real) i;
                q3[2*i+1] = v;

                if (m_histogramFindPeaks && (i > 0)) {
                    m_peakFinder.push(m_currentSpectrum[i], i == m_nbBins - 1);
                }
            }
            // Replicate Nyquist sample to end of positive side
            q3[2*m_nbBins] = (GLfloat) m_nbBins;
            q3[2*m_nbBins+1] = q3[1];

            QVector4D color;
            if (m_spectrumStyle == SpectrumSettings::Gradient) {
                color = QVector4D(m_colorMap[255*3], m_colorMap[255*3+1], m_colorMap[255*3+2], (float) m_displayTraceIntensity / 100.0f);
            } else {
                color = QVector4D(1.0f, 1.0f, 0.25f, (float) m_displayTraceIntensity / 100.0f);
            }
            m_glShaderSimple.drawPolyline(m_glHistogramSpectrumMatrix, color, q3, m_nbBins+1);

            if (m_histogramFindPeaks) {
                m_peakFinder.sortPeaks();
            }
        }
    }

    if (m_displayCurrent && m_currentSpectrum && (m_markersDisplay & SpectrumSettings::MarkersDisplaySpectrum))
    {
        if (m_histogramFindPeaks) {
            updateHistogramPeaks();
        }

        drawSpectrumMarkers();
    }

    if (m_markersDisplay & SpectrumSettings::MarkersDisplayAnnotations) {
        drawAnnotationMarkers();
    }

    // paint waterfall grid
    if (m_displayWaterfall && m_displayGrid)
    {
        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;
        tickList = &m_timeScale.getTickList();

        {
            GLfloat *q3 = m_q3TickTime.m_array;
            int effectiveTicks = 0;

            for (int i= 0; i < tickList->count(); i++)
            {
                tick = &(*tickList)[i];

                if (tick->major)
                {
                    if (tick->textSize > 0)
                    {
                        float y = tick->pos / m_timeScale.getSize();
                        q3[4*effectiveTicks] = 0;
                        q3[4*effectiveTicks+1] = y;
                        q3[4*effectiveTicks+2] = 1;
                        q3[4*effectiveTicks+3] = y;
                        effectiveTicks++;
                    }
                }
            }

            QVector4D color(1.0f, 1.0f, 1.0f, (float) m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(m_glWaterfallBoxMatrix, color, q3, 2*effectiveTicks);
        }

        tickList = &m_frequencyScale.getTickList();

        {
            GLfloat *q3 = m_q3TickFrequency.m_array;
            int effectiveTicks = 0;

            for (int i= 0; i < tickList->count(); i++)
            {
                tick = &(*tickList)[i];

                if (tick->major)
                {
                    if (tick->textSize > 0)
                    {
                        float x = tick->pos / m_frequencyScale.getSize();
                        q3[4*effectiveTicks] = x;
                        q3[4*effectiveTicks+1] = 0;
                        q3[4*effectiveTicks+2] = x;
                        q3[4*effectiveTicks+3] = 1;
                        effectiveTicks++;
                    }
                }
            }

            QVector4D color(1.0f, 1.0f, 1.0f, (float) m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(m_glWaterfallBoxMatrix, color, q3, 2*effectiveTicks);
        }
    }

    // paint 3D spectrogram grid - this is drawn on top of signal, so that appears slightly transparent
    // x-axis is freq, y time and z power
    if (m_displayGrid && m_display3DSpectrogram)
    {
        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;

        glFunctions->glViewport(0, m_3DSpectrogramBottom*devicePixelRatio, width()*devicePixelRatio, m_waterfallHeight*devicePixelRatio);

        tickList = &m_powerScale.getTickList();
        {
            GLfloat *q3 = m_q3TickPower.m_array;
            int effectiveTicks = 0;

            for (int i= 0; i < tickList->count(); i++)
            {
                tick = &(*tickList)[i];

                if (tick->major)
                {
                    if (tick->textSize > 0)
                    {
                        float y = tick->pos / m_powerScale.getSize();
                        q3[6*effectiveTicks] = 0.0;
                        q3[6*effectiveTicks+1] = 1.0;
                        q3[6*effectiveTicks+2] = y;
                        q3[6*effectiveTicks+3] = 1.0;
                        q3[6*effectiveTicks+4] = 1.0;
                        q3[6*effectiveTicks+5] = y;
                        effectiveTicks++;
                    }
                }
            }

            QVector4D color(1.0f, 1.0f, 1.0f, (float) m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(spectrogramGridMatrix, color, q3, 2*effectiveTicks, 3);
        }

        tickList = &m_timeScale.getTickList();
        {
            GLfloat *q3 = m_q3TickTime.m_array;
            int effectiveTicks = 0;

            for (int i= 0; i < tickList->count(); i++)
            {
                tick = &(*tickList)[i];

                if (tick->major)
                {
                    if (tick->textSize > 0)
                    {
                        float y = tick->pos / m_timeScale.getSize();
                        q3[4*effectiveTicks] = 0.0;
                        q3[4*effectiveTicks+1] = 1.0 - y;
                        q3[4*effectiveTicks+2] = 1.0;
                        q3[4*effectiveTicks+3] = 1.0 - y;
                        effectiveTicks++;
                    }
                }
            }

            QVector4D color(1.0f, 1.0f, 1.0f, (float) m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(spectrogramGridMatrix, color, q3, 2*effectiveTicks);
        }

        tickList = &m_frequencyScale.getTickList();
        {
            GLfloat *q3 = m_q3TickFrequency.m_array;
            int effectiveTicks = 0;

            for (int i= 0; i < tickList->count(); i++)
            {
                tick = &(*tickList)[i];

                if (tick->major)
                {
                    if (tick->textSize > 0)
                    {
                        float x = tick->pos / m_frequencyScale.getSize();
                        q3[4*effectiveTicks] = x;
                        q3[4*effectiveTicks+1] = -0.0;
                        q3[4*effectiveTicks+2] = x;
                        q3[4*effectiveTicks+3] = 1.0;
                        effectiveTicks++;
                    }
                }
            }

            QVector4D color(1.0f, 1.0f, 1.0f, (float) m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(spectrogramGridMatrix, color, q3, 2*effectiveTicks);
        }
        {
            GLfloat *q3 = m_q3TickFrequency.m_array;
            int effectiveTicks = 0;

            for (int i= 0; i < tickList->count(); i++)
            {
                tick = &(*tickList)[i];

                if (tick->major)
                {
                    if (tick->textSize > 0)
                    {
                        float x = tick->pos / m_frequencyScale.getSize();
                        q3[6*effectiveTicks] = x;
                        q3[6*effectiveTicks+1] = 1.0;
                        q3[6*effectiveTicks+2] = 0.0;
                        q3[6*effectiveTicks+3] = x;
                        q3[6*effectiveTicks+4] = 1.0;
                        q3[6*effectiveTicks+5] = 1.0;
                        effectiveTicks++;
                    }
                }
            }

            QVector4D color(1.0f, 1.0f, 1.0f, (float) m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(spectrogramGridMatrix, color, q3, 2*effectiveTicks, 3);
        }

        glFunctions->glViewport(0, 0, width()*devicePixelRatio, height()*devicePixelRatio);
    }

    // paint histogram grid
    if ((m_displayHistogram || m_displayMaxHold || m_displayCurrent) && (m_displayGrid))
    {
        const ScaleEngine::TickList* tickList;
        const ScaleEngine::Tick* tick;
        tickList = &m_powerScale.getTickList();

        {
            GLfloat *q3 = m_q3TickPower.m_array;
            int effectiveTicks = 0;

            for (int i= 0; i < tickList->count(); i++)
            {
                tick = &(*tickList)[i];

                if (tick->major)
                {
                    if (tick->textSize > 0)
                    {
                        float y = tick->pos / m_powerScale.getSize();
                        q3[4*effectiveTicks] = 0;
                        q3[4*effectiveTicks+1] = 1-y;
                        q3[4*effectiveTicks+2] = 1;
                        q3[4*effectiveTicks+3] = 1-y;
                        effectiveTicks++;
                    }
                }
            }

            QVector4D color(1.0f, 1.0f, 1.0f, (float) m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(m_glHistogramBoxMatrix, color, q3, 2*effectiveTicks);
        }

        tickList = &m_frequencyScale.getTickList();

        {
            GLfloat *q3 = m_q3TickFrequency.m_array;
            int effectiveTicks = 0;

            for (int i= 0; i < tickList->count(); i++)
            {
                tick = &(*tickList)[i];

                if (tick->major)
                {
                    if (tick->textSize > 0)
                    {
                        float x = tick->pos / m_frequencyScale.getSize();
                        q3[4*effectiveTicks] = x;
                        q3[4*effectiveTicks+1] = 0;
                        q3[4*effectiveTicks+2] = x;
                        q3[4*effectiveTicks+3] = 1;
                        effectiveTicks++;
                    }
                }
            }

            QVector4D color(1.0f, 1.0f, 1.0f, (float) m_displayGridIntensity / 100.0f);
            m_glShaderSimple.drawSegments(m_glHistogramBoxMatrix, color, q3, 2*effectiveTicks);
        }
    }

    // paint rect around histogram (do last, so on top of filled spectrum)
    if (m_displayHistogram || m_displayMaxHold || m_displayCurrent)
    {
        {
            GLfloat q3[] {
                1, 1,
                0, 1,
                0, 0,
                1, 0
            };

            QVector4D color(1.0f, 1.0f, 1.0f, 0.5f);
            m_glShaderSimple.drawContour(m_glHistogramBoxMatrix, color, q3, 4);
        }
    }

    // Paint info line
    {
        GLfloat vtx1[] = {
                0, 1,
                1, 1,
                1, 0,
                0, 0
        };
        GLfloat tex1[] = {
                0, 1,
                1, 1,
                1, 0,
                0, 0
        };

        m_glShaderInfo.drawSurface(m_glInfoBoxMatrix, tex1, vtx1, 4);
    }

    if (m_currentSpectrum)
    {
        switch (m_measurement)
        {
        case SpectrumSettings::MeasurementPeaks:
            measurePeaks();
            break;
        case SpectrumSettings::MeasurementChannelPower:
            measureChannelPower();
            break;
        case SpectrumSettings::MeasurementAdjacentChannelPower:
            measureAdjacentChannelPower();
            break;
        case SpectrumSettings::MeasurementOccupiedBandwidth:
            measureOccupiedBandwidth();
            break;
        case SpectrumSettings::Measurement3dBBandwidth:
            measure3dBBandwidth();
            break;
        case SpectrumSettings::MeasurementSNR:
            measureSNR();
            measureSFDR();
            break;
        default:
            break;
        }
    }

    m_mutex.unlock();
} // paintGL

// Hightlight power band for SFDR
void GLSpectrumView::drawPowerBandMarkers(float max, float min, const QVector4D &color)
{
    float p1 = (m_powerScale.getRangeMax() - min) / m_powerScale.getRange();
    float p2 = (m_powerScale.getRangeMax() - max) / m_powerScale.getRange();

    GLfloat q3[] {
        1, p2,
        0, p2,
        0, p1,
        1, p1,
        0, p1,
        0, p2
    };

    m_glShaderSimple.drawSurface(m_glHistogramBoxMatrix, color, q3, 4);
}

// Hightlight bandwidth being measured
void GLSpectrumView::drawBandwidthMarkers(int64_t centerFrequency, int bandwidth, const QVector4D &color)
{
    float f1 = (centerFrequency - bandwidth / 2);
    float f2 = (centerFrequency + bandwidth / 2);
    float x1 = (f1 - m_frequencyScale.getRangeMin()) / m_frequencyScale.getRange();
    float x2 = (f2 - m_frequencyScale.getRangeMin()) / m_frequencyScale.getRange();

    GLfloat q3[] {
        x2, 1,
        x1, 1,
        x1, 0,
        x2, 0,
        x1, 0,
        x1, 1
    };

    m_glShaderSimple.drawSurface(m_glHistogramBoxMatrix, color, q3, 4);
}

// Hightlight peak being measured. Note that the peak isn't always at the center
void GLSpectrumView::drawPeakMarkers(int64_t startFrequency, int64_t endFrequency, const QVector4D &color)
{
    float x1 = (startFrequency - m_frequencyScale.getRangeMin()) / m_frequencyScale.getRange();
    float x2 = (endFrequency - m_frequencyScale.getRangeMin()) / m_frequencyScale.getRange();

    GLfloat q3[] {
        x2, 1,
        x1, 1,
        x1, 0,
        x2, 0,
        x1, 0,
        x1, 1
    };

    m_glShaderSimple.drawSurface(m_glHistogramBoxMatrix, color, q3, 4);
}

void GLSpectrumView::drawSpectrumMarkers()
{
    if (!m_currentSpectrum) {
        return;
    }

    QVector4D lineColor(1.0f, 1.0f, 1.0f, 0.3f);

    // paint histogram markers
    if (m_histogramMarkers.size() > 0)
    {
        for (int i = 0; i < m_histogramMarkers.size(); i++)
        {
            if (!m_histogramMarkers.at(i).m_show) {
                continue;
            }

            QPointF ypoint = m_histogramMarkers.at(i).m_point;
            QString powerStr = m_histogramMarkers.at(i).m_powerStr;

            if (m_histogramMarkers.at(i).m_markerType == SpectrumHistogramMarker::SpectrumMarkerTypePower)
            {
                float power = m_linear ?
                    m_currentSpectrum[m_histogramMarkers.at(i).m_fftBin] * (m_useCalibration ? m_calibrationGain : 1.0f):
                    m_currentSpectrum[m_histogramMarkers.at(i).m_fftBin] + (m_useCalibration ? m_calibrationShiftdB : 0.0f);
                ypoint.ry() =
                    (m_powerScale.getRangeMax() - power) / m_powerScale.getRange();
                ypoint.ry() = ypoint.ry() < 0 ?
                    0 :
                    ypoint.ry() > 1 ? 1 : ypoint.ry();
                powerStr = displayPower(
                    power,
                    m_linear ? 'e' : 'f',
                    m_linear ? 3 : 1
                );
            }
            else if (m_histogramMarkers.at(i).m_markerType == SpectrumHistogramMarker::SpectrumMarkerTypePowerMax)
            {
                float power = m_currentSpectrum[m_histogramMarkers.at(i).m_fftBin];

                if ((m_histogramMarkers.at(i).m_holdReset) || (power > m_histogramMarkers[i].m_powerMax))
                {
                    m_histogramMarkers[i].m_powerMax = power;
                    m_histogramMarkers[i].m_holdReset = false;
                }

                float powerMax = m_linear ?
                    m_histogramMarkers[i].m_powerMax * (m_useCalibration ? m_calibrationGain : 1.0f) :
                    m_histogramMarkers[i].m_powerMax + (m_useCalibration ? m_calibrationShiftdB : 0.0f);

                ypoint.ry() =
                    (m_powerScale.getRangeMax() - powerMax) / m_powerScale.getRange();
                ypoint.ry() = ypoint.ry() < 0 ?
                    0 : ypoint.ry() > 1 ?
                        1 : ypoint.ry();
                powerStr = displayPower(
                    powerMax,
                    m_linear ? 'e' : 'f',
                    m_linear ? 3 : 1
                );
            }

            // crosshairs
            GLfloat h[] {
                (float) m_histogramMarkers.at(i).m_point.x(), 0,
                (float) m_histogramMarkers.at(i).m_point.x(), 1
            };
            m_glShaderSimple.drawSegments(m_glHistogramBoxMatrix, lineColor, h, 2);
            GLfloat v[] {
                0, (float) ypoint.y(),
                1, (float) ypoint.y()
            };
            m_glShaderSimple.drawSegments(m_glHistogramBoxMatrix, lineColor, v, 2);
            QColor textColor = m_histogramMarkers.at(i).m_markerColor;
            // text
            if (i == 0)
            {
                drawTextOverlay(
                    m_histogramMarkers.at(i).m_frequencyStr,
                    textColor,
                    m_textOverlayFont,
                    m_histogramMarkers.at(i).m_point.x() * m_histogramRect.width(),
                    (m_invertedWaterfall || (m_waterfallHeight == 0)) ? m_histogramRect.height() : 0,
                    m_histogramMarkers.at(i).m_point.x() < 0.5f,
                    !m_invertedWaterfall && (m_waterfallHeight != 0),
                    m_histogramRect);
                drawTextOverlay(
                    powerStr,
                    textColor,
                    m_textOverlayFont,
                    0,
                    ypoint.y() * m_histogramRect.height(),
                    true,
                    ypoint.y() < 0.5f,
                    m_histogramRect);
            }
            else
            {
                textColor.setAlpha(192);
                float power0, poweri;

                if (m_histogramMarkers.at(0).m_markerType == SpectrumHistogramMarker::SpectrumMarkerTypePower) {
                    power0 = m_currentSpectrum[m_histogramMarkers.at(0).m_fftBin];
                } else if (m_histogramMarkers.at(0).m_markerType == SpectrumHistogramMarker::SpectrumMarkerTypePowerMax) {
                    power0 = m_histogramMarkers.at(0).m_powerMax;
                } else {
                    power0 = m_linear ? m_histogramMarkers.at(0).m_power : CalcDb::dbPower(m_histogramMarkers.at(0).m_power);
                }

                if (m_histogramMarkers.at(i).m_markerType == SpectrumHistogramMarker::SpectrumMarkerTypePower) {
                    poweri = m_currentSpectrum[m_histogramMarkers.at(i).m_fftBin];
                } else if (m_histogramMarkers.at(i).m_markerType == SpectrumHistogramMarker::SpectrumMarkerTypePowerMax) {
                    poweri = m_histogramMarkers.at(i).m_powerMax;
                } else {
                    poweri = m_linear ? m_histogramMarkers.at(i).m_power : CalcDb::dbPower(m_histogramMarkers.at(i).m_power);
                }

                QString deltaPowerStr;

                if (m_linear) {
                    deltaPowerStr = QString::number(poweri - power0, 'e', 3);
                } else {
                    deltaPowerStr = QString::number(poweri - power0, 'f', 1);
                }

                drawTextOverlay(
                    m_histogramMarkers.at(i).m_deltaFrequencyStr,
                    textColor,
                    m_textOverlayFont,
                    m_histogramMarkers.at(i).m_point.x() * m_histogramRect.width(),
                    (m_invertedWaterfall || (m_waterfallHeight == 0)) ? 0 : m_histogramRect.height(),
                    m_histogramMarkers.at(i).m_point.x() < 0.5f,
                    (m_invertedWaterfall || (m_waterfallHeight == 0)),
                    m_histogramRect);
                drawTextOverlay(
                    deltaPowerStr,
                    textColor,
                    m_textOverlayFont,
                    m_histogramRect.width(),
                    ypoint.y() * m_histogramRect.height(),
                    false,
                    ypoint.y() < 0.5f,
                    m_histogramRect);
            }
        }
    }

    // paint waterfall markers
    if (m_waterfallMarkers.size() > 0)
    {
        // crosshairs
        for (int i = 0; i < m_waterfallMarkers.size(); i++)
        {
            if (!m_waterfallMarkers.at(i).m_show) {
                continue;
            }

            GLfloat h[] {
                (float) m_waterfallMarkers.at(i).m_point.x(), 0,
                (float) m_waterfallMarkers.at(i).m_point.x(), 1
            };
            m_glShaderSimple.drawSegments(m_glWaterfallBoxMatrix, lineColor, h, 2);
            GLfloat v[] {
                0, (float) m_waterfallMarkers.at(i).m_point.y(),
                1, (float) m_waterfallMarkers.at(i).m_point.y()
            };
            m_glShaderSimple.drawSegments(m_glWaterfallBoxMatrix, lineColor, v, 2);
        // }
        // text
        // for (int i = 0; i < m_waterfallMarkers.size(); i++)
        // {
            QColor textColor = m_waterfallMarkers.at(i).m_markerColor;
            textColor.setAlpha(192);

            if (i == 0)
            {
                drawTextOverlay(
                    m_waterfallMarkers.at(i).m_frequencyStr,
                    textColor,
                    m_textOverlayFont,
                    m_waterfallMarkers.at(i).m_point.x() * m_waterfallRect.width(),
                    (!m_invertedWaterfall || (m_histogramHeight == 0)) ? m_waterfallRect.height() : 0,
                    m_waterfallMarkers.at(i).m_point.x() < 0.5f,
                    m_invertedWaterfall && (m_histogramHeight != 0),
                    m_waterfallRect);
                drawTextOverlay(
                    m_waterfallMarkers.at(i).m_timeStr,
                    textColor,
                    m_textOverlayFont,
                    0,
                    m_waterfallMarkers.at(i).m_point.y() * m_waterfallRect.height(),
                    true,
                    m_waterfallMarkers.at(i).m_point.y() < 0.5f,
                    m_waterfallRect);
            }
            else
            {
                drawTextOverlay(
                    m_waterfallMarkers.at(i).m_deltaFrequencyStr,
                    textColor,
                    m_textOverlayFont,
                    m_waterfallMarkers.at(i).m_point.x() * m_waterfallRect.width(),
                    (!m_invertedWaterfall || (m_histogramHeight == 0)) ? 0 : m_waterfallRect.height(),
                    m_waterfallMarkers.at(i).m_point.x() < 0.5f,
                    !m_invertedWaterfall || (m_histogramHeight == 0),
                    m_waterfallRect);
                drawTextOverlay(
                    m_waterfallMarkers.at(i).m_deltaTimeStr,
                    textColor,
                    m_textOverlayFont,
                    m_waterfallRect.width(),
                    m_waterfallMarkers.at(i).m_point.y() * m_waterfallRect.height(),
                    false,
                    m_waterfallMarkers.at(i).m_point.y() < 0.5f,
                    m_waterfallRect);
            }
        }
    }
}

void GLSpectrumView::drawAnnotationMarkers()
{
    if ((!m_currentSpectrum) || (m_visibleAnnotationMarkers.size() == 0)) {
        return;
    }

    float h = m_annotationMarkerHeight / (float) m_histogramHeight;
	float htop = 1.0f / (float) m_histogramHeight;

    for (const auto &marker : m_visibleAnnotationMarkers)
    {
		if (marker->m_show == SpectrumAnnotationMarker::Hidden) {
			continue;
		}

        QVector4D color(marker->m_markerColor.redF(), marker->m_markerColor.greenF(), marker->m_markerColor.blueF(), 0.5f);

        if (marker->m_bandwidth == 0)
        {
            GLfloat d[] {
                marker->m_startPos, htop,
                marker->m_startPos, h
            };
            m_glShaderSimple.drawSegments(m_glHistogramBoxMatrix, color, d, 2);
        }
        else
        {
            GLfloat q3[] {
                marker->m_stopPos, h,
                marker->m_startPos, h,
                marker->m_startPos, htop,
                marker->m_stopPos, htop
            };
            m_glShaderSimple.drawSurface(m_glHistogramBoxMatrix, color, q3, 4);
        }

        // Always draw a line in the top area, so we can see where bands start/stop when contiguous
        // When show is ShowFull, we draw at full height of spectrum
        bool full = marker->m_show == SpectrumAnnotationMarker::ShowFull;

        GLfloat d1[] {
            marker->m_startPos, full ? 0 : htop,
            marker->m_startPos, full ? 1 : h,
        };
        m_glShaderSimple.drawSegments(m_glHistogramBoxMatrix, color, d1, 2);

        if (marker->m_bandwidth != 0)
        {
            GLfloat d2[] {
                marker->m_stopPos, full ? 0 : htop,
                marker->m_stopPos, full ? 1 : h,
            };
            m_glShaderSimple.drawSegments(m_glHistogramBoxMatrix, color, d2, 2);
        }

        if ((marker->m_show == SpectrumAnnotationMarker::ShowFull) || (marker->m_show == SpectrumAnnotationMarker::ShowText))
        {
            float txtpos = marker->m_startPos < 0.5f ?
                marker->m_startPos :
                marker->m_stopPos;

            drawTextOverlay(
                marker->m_text,
                QColor(255, 255, 255, 192),
                m_textOverlayFont,
                txtpos * m_histogramRect.width(),
                0,
                marker->m_startPos < 0.5f,
                true,
                m_histogramRect);
        }
    }
}

// Find and display peak in info line
void GLSpectrumView::measurePeak()
{
    float power, frequency;

    findPeak(power, frequency);

    drawTextsRight(
        {"Peak: ", ""},
        {
         displayPower(power, m_linear ? 'e' : 'f', m_linear ? 3 : 1),
         displayFull(frequency)
        },
        {m_peakPowerMaxStr, m_peakFrequencyMaxStr},
        {m_peakPowerUnits, "Hz"}
    );
    if (m_measurements) {
        m_measurements->setPeak(0, frequency, power);
    }
}

// Find and display peaks
void GLSpectrumView::measurePeaks()
{
    // Copy current spectrum so we can modify it
    Real *spectrum = new Real[m_nbBins];
    std::copy(m_currentSpectrum, m_currentSpectrum + m_nbBins, spectrum);

    for (int i = 0; i < m_measurementPeaks; i++)
    {
        // Find peak
        int peakBin = findPeakBin(spectrum);
        int left, right;
        peakWidth(spectrum, peakBin, left, right, 0, m_nbBins);
        left++;
        right--;

        float power = m_linear ?
                        spectrum[peakBin] * (m_useCalibration ? m_calibrationGain : 1.0f) :
                        spectrum[peakBin] + (m_useCalibration ? m_calibrationShiftdB : 0.0f);
        int64_t frequency = binToFrequency(peakBin);

        // Add to table
        if (m_measurements) {
            m_measurements->setPeak(i, frequency, power);
        }

        if (m_measurementHighlight)
        {
            float x = peakBin / (float)m_nbBins;
            float y = (m_powerScale.getRangeMax() - power) / m_powerScale.getRange();

            QString text = QString::number(i + 1);

            drawTextOverlayCentered(
                text,
                QColor(255, 255, 255),
                m_textOverlayFont,
                x * m_histogramRect.width(),
                y * m_histogramRect.height(),
                m_histogramRect);
        }

        // Remove peak from spectrum so not found on next pass
        for (int j = left; j <= right; j++) {
            spectrum[j] = -std::numeric_limits<float>::max();
        }
    }

    delete[] spectrum;
}

// Calculate and display channel power
void GLSpectrumView::measureChannelPower()
{
    float power;

    power = calcChannelPower(m_centerFrequency + m_measurementCenterFrequencyOffset, m_measurementBandwidth);
    if (m_measurements) {
        m_measurements->setChannelPower(power);
    }
    if (m_measurementHighlight) {
        drawBandwidthMarkers(m_centerFrequency + m_measurementCenterFrequencyOffset, m_measurementBandwidth, m_measurementLightMarkerColor);
    }
}

// Calculate and display channel power and adjacent channel power
void GLSpectrumView::measureAdjacentChannelPower()
{
    float power, powerLeft, powerRight;

    power = calcChannelPower(m_centerFrequency + m_measurementCenterFrequencyOffset, m_measurementBandwidth);
    powerLeft = calcChannelPower(m_centerFrequency + m_measurementCenterFrequencyOffset - m_measurementChSpacing, m_measurementAdjChBandwidth);
    powerRight = calcChannelPower(m_centerFrequency + m_measurementCenterFrequencyOffset + m_measurementChSpacing, m_measurementAdjChBandwidth);

    float leftDiff = powerLeft - power;
    float rightDiff = powerRight - power;

    if (m_measurements) {
        m_measurements->setAdjacentChannelPower(powerLeft, leftDiff, power, powerRight, rightDiff);
    }

    if (m_measurementHighlight)
    {
        drawBandwidthMarkers(m_centerFrequency + m_measurementCenterFrequencyOffset, m_measurementBandwidth, m_measurementLightMarkerColor);
        drawBandwidthMarkers(m_centerFrequency + m_measurementCenterFrequencyOffset - m_measurementChSpacing, m_measurementAdjChBandwidth, m_measurementDarkMarkerColor);
        drawBandwidthMarkers(m_centerFrequency + m_measurementCenterFrequencyOffset + m_measurementChSpacing, m_measurementAdjChBandwidth, m_measurementDarkMarkerColor);
    }
}

// Measure bandwidth that has 99% of power
void GLSpectrumView::measureOccupiedBandwidth()
{
    float hzPerBin = m_sampleRate / (float) m_fftSize;
    int start = frequencyToBin(m_centerFrequency + m_measurementCenterFrequencyOffset);
    float totalPower, power = 0.0f;
    int step = 0;
    int width = 0;
    int idx = start;
    float gain = m_useCalibration ? m_calibrationGain : 1.0f;
    float shift = m_useCalibration ? m_calibrationShiftdB : 0.0f;

    totalPower = CalcDb::powerFromdB(calcChannelPower(m_centerFrequency + m_measurementCenterFrequencyOffset, m_measurementBandwidth));
    do
    {
        if ((idx >= 0) && (idx < m_nbBins))
        {
            if (m_linear) {
                power += m_currentSpectrum[idx] * gain;
            } else {
                power += CalcDb::powerFromdB(m_currentSpectrum[idx]) + shift;
            }
            width++;
        }

        step++;
        if ((step & 1) == 1) {
            idx -= step;
        } else {
            idx += step;
        }
    }
    while (((power / totalPower) < 0.99f) && (step < m_nbBins));

    float occupiedBandwidth = width * hzPerBin;
    if (m_measurements) {
        m_measurements->setOccupiedBandwidth(occupiedBandwidth);
    }
    if (m_measurementHighlight)
    {
        drawBandwidthMarkers(m_centerFrequency + m_measurementCenterFrequencyOffset, m_measurementBandwidth, m_measurementDarkMarkerColor);
        drawBandwidthMarkers(m_centerFrequency + m_measurementCenterFrequencyOffset, occupiedBandwidth, m_measurementLightMarkerColor);
    }
}

// Measure bandwidth -3dB from peak
void GLSpectrumView::measure3dBBandwidth()
{
    // Find max peak and it's power in dB
    int peakBin = findPeakBin(m_currentSpectrum);
    float peakPower = m_linear ? CalcDb::dbPower(m_currentSpectrum[peakBin]) : m_currentSpectrum[peakBin];

    // Search right until 3dB from peak
    int rightBin = peakBin;
    for (int i = peakBin + 1; i < m_nbBins; i++)
    {
        float power = m_linear ? CalcDb::dbPower(m_currentSpectrum[i]) : m_currentSpectrum[i];
        if (peakPower - power > 3.0f)
        {
            rightBin = i - 1;
            break;
        }
    }

    // Search left until 3dB from peak
    int leftBin = peakBin;
    for (int i = peakBin - 1; i >= 0; i--)
    {
        float power = m_linear ? CalcDb::dbPower(m_currentSpectrum[i]) : m_currentSpectrum[i];
        if (peakPower - power > 3.0f)
        {
            leftBin = i + 1;
            break;
        }
    }

    // Calcualte bandwidth
    int bins = rightBin - leftBin - 1;
    bins = std::max(1, bins);
    float hzPerBin = m_sampleRate / (float) m_fftSize;
    float bandwidth = bins * hzPerBin;
    int centerBin = leftBin + (rightBin - leftBin) / 2;
    float centerFrequency = binToFrequency(centerBin);

    if (m_measurements) {
        m_measurements->set3dBBandwidth(bandwidth);
    }
    if (m_measurementHighlight) {
        drawBandwidthMarkers(centerFrequency, bandwidth, m_measurementLightMarkerColor);
    }
}

const QVector4D GLSpectrumView::m_measurementLightMarkerColor = QVector4D(0.6f, 0.6f, 0.6f, 0.2f);
const QVector4D GLSpectrumView::m_measurementDarkMarkerColor = QVector4D(0.6f, 0.6f, 0.6f, 0.15f);

// Find the width of a peak, by seaching in either direction until
// power is no longer falling
void GLSpectrumView::peakWidth(const Real *spectrum, int center, int &left, int &right, int maxLeft, int maxRight) const
{
    float prevLeft = spectrum[center];
    float prevRight = spectrum[center];
    left = center - 1;
    right = center + 1;
    while ((left > maxLeft) && (spectrum[left] < prevLeft) && (right < maxRight) && (spectrum[right] < prevRight))
    {
        prevLeft = spectrum[left];
        left--;
        prevRight = spectrum[right];
        right++;
    }
}

int GLSpectrumView::findPeakBin(const Real *spectrum) const
{
    int bin;
    float power;

    bin = 0;
    power = spectrum[0];
    for (int i = 1; i < m_nbBins; i++)
    {
        if (spectrum[i] > power)
        {
            power = spectrum[i];
            bin = i;
        }
    }
    return bin;
}

float GLSpectrumView::calPower(float power) const
{
    if (m_linear) {
        return power * (m_useCalibration ? m_calibrationGain : 1.0f);
    } else {
        return CalcDb::powerFromdB(power) + (m_useCalibration ? m_calibrationShiftdB : 0.0f);
    }
}

int GLSpectrumView::frequencyToBin(int64_t frequency) const
{
    float rbw = m_sampleRate / (float)m_fftSize;
    return (frequency - m_frequencyScale.getRangeMin()) / rbw;
}

int64_t GLSpectrumView::binToFrequency(int bin) const
{
    float rbw = m_sampleRate / (float)m_fftSize;
    return m_frequencyScale.getRangeMin() + bin * rbw;
}

// Find a peak and measure SNR / THD / SINAD
void GLSpectrumView::measureSNR()
{
    // Find bin with max peak - that will be our signal
    int sig = findPeakBin(m_currentSpectrum);
    int sigLeft, sigRight;
    peakWidth(m_currentSpectrum, sig, sigLeft, sigRight, 0, m_nbBins);
    int sigBins = sigRight - sigLeft - 1;
    int binsLeft = sig - sigLeft;
    int binsRight = sigRight - sig;

    // Highlight the signal
    float sigFreq = binToFrequency(sig);
    if (m_measurementHighlight) {
        drawPeakMarkers(binToFrequency(sigLeft+1), binToFrequency(sigRight-1), m_measurementLightMarkerColor);
    }

    // Find the harmonics and highlight them
    QList<int> hBinsLeft;
    QList<int> hBinsRight;
    QList<int> hBinsBins;
    for (int h = 2; h < m_measurementHarmonics + 2; h++)
    {
        float hFreq = sigFreq * h;
        if (hFreq < m_frequencyScale.getRangeMax())
        {
            int hBin = frequencyToBin(hFreq);
            // Check if peak is an adjacent bin
            if (m_currentSpectrum[hBin-1] > m_currentSpectrum[hBin]) {
                hBin--;
            } else if (m_currentSpectrum[hBin+1] > m_currentSpectrum[hBin]) {
                hBin++;
            }
            hFreq = binToFrequency(hBin);
            int hLeft, hRight;
            peakWidth(m_currentSpectrum, hBin, hLeft, hRight, hBin - binsLeft, hBin + binsRight);
            int hBins = hRight - hLeft - 1;
            if (m_measurementHighlight) {
                drawPeakMarkers(binToFrequency(hLeft+1), binToFrequency(hRight-1), m_measurementDarkMarkerColor);
            }
            hBinsLeft.append(hLeft);
            hBinsRight.append(hRight);
            hBinsBins.append(hBins);
        }
    }

    // Integrate signal, harmonic and noise power
    float sigPower = 0.0f;
    float noisePower = 0.0f;
    float harmonicPower = 0.0f;
    QList<float> noise;
    float gain = m_useCalibration ? m_calibrationGain : 1.0f;
    float shift = m_useCalibration ? m_calibrationShiftdB : 0.0f;

    for (int i = 0; i < m_nbBins; i++)
    {
        float power;
        if (m_linear) {
            power = m_currentSpectrum[i] * gain;
        } else {
            power = CalcDb::powerFromdB(m_currentSpectrum[i]) + shift;
        }

        // Signal power
        if ((i > sigLeft) && (i < sigRight))
        {
            sigPower += power;
            continue;
        }

        // Harmonics
        for (int h = 0; h < hBinsLeft.size(); h++)
        {
            if ((i > hBinsLeft[h]) && (i < hBinsRight[h]))
            {
                harmonicPower += power;
                continue;
            }
        }

        // Noise
        noisePower += power;
        noise.append(power);
    }

    // Calculate median of noise
    float noiseMedian = 0.0;
    if (noise.size() > 0)
    {
        auto m = noise.begin() + noise.size()/2;
        std::nth_element(noise.begin(), m, noise.end());
        noiseMedian = noise[noise.size()/2];
    }

    // Assume we have similar noise where the signal and harmonics are
    float inBandNoise = noiseMedian * sigBins;
    noisePower += inBandNoise;
    sigPower -= inBandNoise;
    for (auto hBins : hBinsBins)
    {
        float hNoise = noiseMedian * hBins;
        noisePower += hNoise;
        harmonicPower -= hNoise;
    }

    if (m_measurements)
    {
        // Calculate SNR in dB over full bandwidth
        float snr = CalcDb::dbPower(sigPower / noisePower);

        // Calculate SNR, where noise is median of noise summed over signal b/w
        float snfr = CalcDb::dbPower(sigPower / inBandNoise);

        // Calculate THD - Total harmonic distortion
        float thd = harmonicPower / sigPower;
        float thdDB = CalcDb::dbPower(thd);

        // Calculate THD+N - Total harmonic distortion plus noise
        float thdpn = CalcDb::dbPower((harmonicPower + noisePower) / sigPower);

        // Calculate SINAD - Signal to noise and distotion ratio (Should be -THD+N)
        float sinad = CalcDb::dbPower((sigPower + harmonicPower + noisePower) / (harmonicPower + noisePower));

        m_measurements->setSNR(snr, snfr, thdDB, thdpn, sinad);
    }
}

void GLSpectrumView::measureSFDR()
{
    // Find first peak which is our signal
    int peakBin = findPeakBin(m_currentSpectrum);
    int peakLeft, peakRight;
    peakWidth(m_currentSpectrum, peakBin, peakLeft, peakRight, 0, m_nbBins);

    // Find next largest peak, which is the spur
    int nextPeakBin = -1;
    float nextPeakPower = -std::numeric_limits<float>::max();
    for (int i = 0; i < m_nbBins; i++)
    {
        if ((i < peakLeft) || (i > peakRight))
        {
            if (m_currentSpectrum[i] > nextPeakPower)
            {
                nextPeakBin = i;
                nextPeakPower = m_currentSpectrum[i];
            }
        }
    }
    if (nextPeakBin != -1)
    {
        // Calculate SFDR in dB from difference between two peaks
        float peakPower = calPower(m_currentSpectrum[peakBin]);
        float nextPeakPower = calPower(m_currentSpectrum[nextPeakBin]);
        float peakPowerDB = CalcDb::dbPower(peakPower);
        float nextPeakPowerDB = CalcDb::dbPower(nextPeakPower);
        float sfdr = peakPowerDB - nextPeakPowerDB;

        // Display
        if (m_measurements) {
            m_measurements->setSFDR(sfdr);
        }
        if (m_measurementHighlight)
        {
            if (m_linear) {
                drawPowerBandMarkers(peakPower, nextPeakPower, m_measurementDarkMarkerColor);
            } else {
                drawPowerBandMarkers(peakPowerDB, nextPeakPowerDB, m_measurementDarkMarkerColor);
            }
        }
    }
}

// Find power and frequency of max peak in current spectrum
void GLSpectrumView::findPeak(float &power, float &frequency) const
{
    int bin;

    bin = 0;
    power = m_currentSpectrum[0];
    for (int i = 1; i < m_nbBins; i++)
    {
        if (m_currentSpectrum[i] > power)
        {
            power = m_currentSpectrum[i];
            bin = i;
        }
    }

    power = m_linear ?
                power * (m_useCalibration ? m_calibrationGain : 1.0f) :
                power + (m_useCalibration ? m_calibrationShiftdB : 0.0f);
    frequency = binToFrequency(bin);
}

// Calculate channel power in dB
float GLSpectrumView::calcChannelPower(int64_t centerFrequency, int channelBandwidth) const
{
    float hzPerBin = m_sampleRate / (float) m_fftSize;
    int bins = channelBandwidth / hzPerBin;
    int start = frequencyToBin(centerFrequency) - (bins / 2);
    int end = start + bins;
    float power = 0.0;

    start = std::max(start, 0);
    end = std::min(end, m_nbBins);

    if (m_linear)
    {
        float gain = m_useCalibration ? m_calibrationGain : 1.0f;
        for (int i = start; i < end; i++) {
            power += m_currentSpectrum[i] * gain;
        }
    }
    else
    {
        float shift = m_useCalibration ? m_calibrationShiftdB : 0.0f;
        for (int i = start; i < end; i++) {
            power += CalcDb::powerFromdB(m_currentSpectrum[i]) + shift;
        }
    }

    return CalcDb::dbPower(power);
}

void GLSpectrumView::stopDrag()
{
    if (m_cursorState != CSNormal)
    {
        if ((m_cursorState == CSSplitterMoving) || (m_cursorState == CSChannelMoving)) {
            releaseMouse();
        }

        setCursor(Qt::ArrowCursor);
        m_cursorState = CSNormal;
    }
}

void GLSpectrumView::applyChanges()
{
    if (m_nbBins <= 0) {
        return;
    }

    QFontMetrics fm(font());
    int M = fm.horizontalAdvance("-");

    m_topMargin = fm.ascent() * 2.0;
    m_bottomMargin = fm.ascent() * 1.0;
    m_infoHeight = fm.height() * 3;

    int waterfallTop = 0;
    m_frequencyScaleHeight = fm.height() * 3; // +1 line for marker frequency scale
    int frequencyScaleTop = 0;
    int histogramTop = 0;
    //int m_leftMargin;
    m_rightMargin = fm.horizontalAdvance("000");

    // displays both histogram and waterfall
    if ((m_displayWaterfall || m_display3DSpectrogram) && (m_displayHistogram | m_displayMaxHold | m_displayCurrent))
    {
        m_waterfallHeight = height() * m_waterfallShare - 1;

        if (m_waterfallHeight < 0) {
            m_waterfallHeight = 0;
        }

        if (m_invertedWaterfall)
        {
            histogramTop = m_topMargin;
            m_histogramHeight = height() - m_topMargin - m_waterfallHeight - m_frequencyScaleHeight - m_bottomMargin;
            waterfallTop = histogramTop + m_histogramHeight + m_frequencyScaleHeight + 1;
            frequencyScaleTop = histogramTop + m_histogramHeight + 1;
        }
        else
        {
            waterfallTop = m_topMargin;
            frequencyScaleTop = waterfallTop + m_waterfallHeight + 1;
            histogramTop = waterfallTop + m_waterfallHeight + m_frequencyScaleHeight + 1;
            m_histogramHeight = height() - m_topMargin - m_waterfallHeight - m_frequencyScaleHeight - m_bottomMargin;
        }

        m_timeScale.setSize(m_waterfallHeight);

        if (m_sampleRate > 0)
        {
            float scaleDiv = ((float)m_sampleRate / (float)m_timingRate) * (m_ssbSpectrum ? 2 : 1);
            float halfFFTSize = m_fftSize / 2;

            if (halfFFTSize > m_fftOverlap) {
                scaleDiv *= halfFFTSize / (halfFFTSize - m_fftOverlap);
            }

            if (!m_invertedWaterfall) {
                m_timeScale.setRange(m_timingRate > 1 ? Unit::TimeHMS : Unit::Time, (m_waterfallHeight * m_fftSize) / scaleDiv, 0);
            } else {
                m_timeScale.setRange(m_timingRate > 1 ? Unit::TimeHMS : Unit::Time, 0, (m_waterfallHeight * m_fftSize) / scaleDiv);
            }
        }
        else
        {
            m_timeScale.setRange(Unit::Time, 0, 1);
        }

        m_leftMargin = m_timeScale.getScaleWidth();

        setPowerScale(m_histogramHeight);

        m_leftMargin += 2 * M;

        setFrequencyScale();

        m_glWaterfallBoxMatrix.setToIdentity();
        m_glWaterfallBoxMatrix.translate(
            -1.0f + ((float)(2*m_leftMargin)   / (float) width()),
             1.0f - ((float)(2*waterfallTop) / (float) height())
        );
        m_glWaterfallBoxMatrix.scale(
            ((float) 2 * (width() - m_leftMargin - m_rightMargin)) / (float) width(),
            (float) (-2*m_waterfallHeight) / (float) height()
        );

        m_glHistogramBoxMatrix.setToIdentity();
        m_glHistogramBoxMatrix.translate(
            -1.0f + ((float)(2*m_leftMargin)   / (float) width()),
             1.0f - ((float)(2*histogramTop) / (float) height())
        );
        m_glHistogramBoxMatrix.scale(
            ((float) 2 * (width() - m_leftMargin - m_rightMargin)) / (float) width(),
            (float) (-2*m_histogramHeight) / (float) height()
        );

        m_glHistogramSpectrumMatrix.setToIdentity();
        m_glHistogramSpectrumMatrix.translate(
            -1.0f + ((float)(2*m_leftMargin)   / (float) width()),
             1.0f - ((float)(2*histogramTop) / (float) height())
        );
        m_glHistogramSpectrumMatrix.scale(
            ((float) 2 * (width() - m_leftMargin - m_rightMargin)) / ((float) width() * (float)(m_nbBins)),
            ((float) 2*m_histogramHeight / height()) / m_powerRange
        );

        // m_frequencyScaleRect = QRect(
        // 	0,
        // 	frequencyScaleTop,
        // 	width(),
        // 	m_frequencyScaleHeight
        // );

        m_glFrequencyScaleBoxMatrix.setToIdentity();
        m_glFrequencyScaleBoxMatrix.translate (
            -1.0f,
             1.0f - ((float) 2*frequencyScaleTop / (float) height())
        );
        m_glFrequencyScaleBoxMatrix.scale (
            2.0f,
            (float) -2*m_frequencyScaleHeight / (float) height()
        );

        m_glLeftScaleBoxMatrix.setToIdentity();
        m_glLeftScaleBoxMatrix.translate(-1.0f, 1.0f);
        m_glLeftScaleBoxMatrix.scale(
            (float)(2*(m_leftMargin - 1)) / (float) width(),
            -2.0f
        );
    }
    // displays waterfall/3D spectrogram only
    else if (m_displayWaterfall || m_display3DSpectrogram)
    {
        m_histogramHeight = 0;
        histogramTop = 0;
        m_bottomMargin = m_frequencyScaleHeight;
        m_waterfallHeight = height() - m_topMargin - m_frequencyScaleHeight;
        waterfallTop = m_topMargin;
        frequencyScaleTop = m_topMargin + m_waterfallHeight + 1;

        m_timeScale.setSize(m_waterfallHeight);

        if (m_sampleRate > 0)
        {
            float scaleDiv = ((float)m_sampleRate / (float)m_timingRate) * (m_ssbSpectrum ? 2 : 1);
            float halfFFTSize = m_fftSize / 2;

            if (halfFFTSize > m_fftOverlap) {
                scaleDiv *= halfFFTSize / (halfFFTSize - m_fftOverlap);
            }

            if (!m_invertedWaterfall) {
                m_timeScale.setRange(m_timingRate > 1 ? Unit::TimeHMS : Unit::Time, (m_waterfallHeight * m_fftSize) / scaleDiv, 0);
            } else {
                m_timeScale.setRange(m_timingRate > 1 ? Unit::TimeHMS : Unit::Time, 0, (m_waterfallHeight * m_fftSize) / scaleDiv);
            }
        }
        else
        {
            if (!m_invertedWaterfall) {
                m_timeScale.setRange(m_timingRate > 1 ? Unit::TimeHMS : Unit::Time, 10, 0);
            } else {
                m_timeScale.setRange(m_timingRate > 1 ? Unit::TimeHMS : Unit::Time, 0, 10);
            }
        }

        m_leftMargin = m_timeScale.getScaleWidth();

        setPowerScale((height() - m_topMargin - m_bottomMargin) / 2.0);

        m_leftMargin += 2 * M;

        setFrequencyScale();

        m_glWaterfallBoxMatrix.setToIdentity();
        m_glWaterfallBoxMatrix.translate(
            -1.0f + ((float)(2*m_leftMargin)   / (float) width()),
             1.0f - ((float)(2*m_topMargin) / (float) height())
        );
        m_glWaterfallBoxMatrix.scale(
            ((float) 2 * (width() - m_leftMargin - m_rightMargin)) / (float) width(),
            (float) (-2*m_waterfallHeight) / (float) height()
        );

        // m_frequencyScaleRect = QRect(
        // 	0,
        // 	frequencyScaleTop,
        // 	width(),
        // 	m_frequencyScaleHeight
        // );

        m_glFrequencyScaleBoxMatrix.setToIdentity();
        m_glFrequencyScaleBoxMatrix.translate (
            -1.0f,
             1.0f - ((float) 2*frequencyScaleTop / (float) height())
        );
        m_glFrequencyScaleBoxMatrix.scale (
            2.0f,
            (float) -2*m_frequencyScaleHeight / (float) height()
        );

        m_glLeftScaleBoxMatrix.setToIdentity();
        m_glLeftScaleBoxMatrix.translate(-1.0f, 1.0f);
        m_glLeftScaleBoxMatrix.scale(
            (float)(2*(m_leftMargin - 1)) / (float) width(),
            -2.0f
        );
    }
    // displays histogram only
    else if (m_displayHistogram || m_displayMaxHold || m_displayCurrent)
    {
        m_bottomMargin = m_frequencyScaleHeight;
        frequencyScaleTop = height() - m_bottomMargin;
        histogramTop = m_topMargin - 1;
        m_waterfallHeight = 0;
        m_histogramHeight = height() - m_topMargin - m_frequencyScaleHeight;

        m_leftMargin = 0;

        setPowerScale(m_histogramHeight);

        m_leftMargin += 2 * M;

        setFrequencyScale();

        m_glHistogramSpectrumMatrix.setToIdentity();
        m_glHistogramSpectrumMatrix.translate(
            -1.0f + ((float)(2*m_leftMargin)   / (float) width()),
             1.0f - ((float)(2*histogramTop) / (float) height())
        );
        m_glHistogramSpectrumMatrix.scale(
            ((float) 2 * (width() - m_leftMargin - m_rightMargin)) / ((float) width() * (float)(m_nbBins)),
            ((float) 2*(height() - m_topMargin - m_frequencyScaleHeight)) / (height()*m_powerRange)
        );

        m_glHistogramBoxMatrix.setToIdentity();
        m_glHistogramBoxMatrix.translate(
            -1.0f + ((float)(2*m_leftMargin)   / (float) width()),
             1.0f - ((float)(2*histogramTop) / (float) height())
        );
        m_glHistogramBoxMatrix.scale(
            ((float) 2 * (width() - m_leftMargin - m_rightMargin)) / (float) width(),
            (float) (-2*(height() - m_topMargin - m_frequencyScaleHeight)) / (float) height()
        );

        // m_frequencyScaleRect = QRect(
        // 	0,
        // 	frequencyScaleTop,
        // 	width(),
        // 	m_frequencyScaleHeight
        // );

        m_glFrequencyScaleBoxMatrix.setToIdentity();
        m_glFrequencyScaleBoxMatrix.translate (
            -1.0f,
             1.0f - ((float) 2*frequencyScaleTop / (float) height())
        );
        m_glFrequencyScaleBoxMatrix.scale (
            2.0f,
            (float) -2*m_frequencyScaleHeight / (float) height()
        );

        m_glLeftScaleBoxMatrix.setToIdentity();
        m_glLeftScaleBoxMatrix.translate(-1.0f, 1.0f);
        m_glLeftScaleBoxMatrix.scale(
            (float)(2*(m_leftMargin - 1)) / (float) width(),
            -2.0f
        );
    }
    else
    {
        m_leftMargin = 2;
        m_waterfallHeight = 0;
    }

    m_glShaderSpectrogram.setScaleX(((width() - m_leftMargin - m_rightMargin) / (float)m_waterfallHeight));
    m_glShaderSpectrogram.setScaleZ((m_histogramHeight != 0 ? m_histogramHeight : m_waterfallHeight / 4)  / (float)(width() - m_leftMargin - m_rightMargin));

    // bounding boxes
    m_frequencyScaleRect = QRect(
        0,
        frequencyScaleTop,
        width(),
        m_frequencyScaleHeight
    );

    if ((m_invertedWaterfall) || (m_waterfallHeight == 0))
    {
        m_histogramRect = QRectF(
            (float) m_leftMargin / (float) width(),
            (float) m_topMargin / (float) height(),
            (float) (width() - m_leftMargin - m_rightMargin) / (float) width(),
            (float) (m_histogramHeight) / (float) height()
        );
    }
    else
    {
        m_histogramRect = QRectF(
            (float) m_leftMargin / (float) width(),
            (float) (waterfallTop + m_waterfallHeight + m_frequencyScaleHeight) / (float) height(),
            (float) (width() - m_leftMargin - m_rightMargin) / (float) width(),
            (float) m_histogramHeight / (float) height()
        );
    }

    if (!m_invertedWaterfall || (m_histogramHeight == 0))
    {
        m_waterfallRect = QRectF(
            (float) m_leftMargin / (float) width(),
            (float) m_topMargin / (float) height(),
            (float) (width() - m_leftMargin - m_rightMargin) / (float) width(),
            (float) m_waterfallHeight / (float) height()
        );
    }
    else
    {
        m_waterfallRect = QRectF(
            (float) m_leftMargin / (float) width(),
            (float) (m_topMargin + m_histogramHeight + m_frequencyScaleHeight) / (float) height(),
            (float) (width() - m_leftMargin - m_rightMargin) / (float) width(),
            (float) (m_waterfallHeight) / (float) height()
        );
    }

    m_glShaderSpectrogram.setAspectRatio((width() - m_leftMargin - m_rightMargin) / (float)m_waterfallHeight);

    m_3DSpectrogramBottom = m_bottomMargin;
    if (!m_invertedWaterfall) {
        m_3DSpectrogramBottom += m_histogramHeight + m_frequencyScaleHeight + 1;
    }

    // channel overlays
    int64_t centerFrequency;
    int frequencySpan;

    if (m_frequencyZoomFactor == 1.0f)
    {
        centerFrequency = m_centerFrequency;
        frequencySpan = m_sampleRate;
    }
    else
    {
        getFrequencyZoom(centerFrequency, frequencySpan);
    }

    for (int i = 0; i < m_channelMarkerStates.size(); ++i)
    {
        ChannelMarkerState* dv = m_channelMarkerStates[i];

        qreal xc, pw, nw, dsbw;
        ChannelMarker::sidebands_t sidebands = dv->m_channelMarker->getSidebands();
        xc = m_centerFrequency + dv->m_channelMarker->getCenterFrequency(); // marker center frequency
        dsbw = dv->m_channelMarker->getBandwidth();

        if (sidebands == ChannelMarker::usb) {
            nw = dv->m_channelMarker->getLowCutoff();     // negative bandwidth
            int bw = dv->m_channelMarker->getBandwidth() / 2;
            pw = (qreal) bw; // positive bandwidth
        } else if (sidebands == ChannelMarker::lsb) {
            pw = dv->m_channelMarker->getLowCutoff();
            int bw = dv->m_channelMarker->getBandwidth() / 2;
            nw = (qreal) bw;
        } else if (sidebands == ChannelMarker::vusb) {
            nw = -dv->m_channelMarker->getOppositeBandwidth(); // negative bandwidth
            pw = dv->m_channelMarker->getBandwidth(); // positive bandwidth
        } else if (sidebands == ChannelMarker::vlsb) {
            pw = dv->m_channelMarker->getOppositeBandwidth(); // positive bandwidth
            nw = -dv->m_channelMarker->getBandwidth(); // negative bandwidth
        } else {
            pw = dsbw / 2;
            nw = -pw;
        }

        // draw the DSB rectangle

        QMatrix4x4 glMatrixDsb;
        glMatrixDsb.setToIdentity();
        glMatrixDsb.translate(
            -1.0f + 2.0f * ((m_leftMargin + m_frequencyScale.getPosFromValue(xc - (dsbw/2))) / (float) width()),
             1.0f
        );
        glMatrixDsb.scale(
            2.0f * (dsbw / (float) frequencySpan),
            -2.0f
        );

        dv->m_glMatrixDsbWaterfall = glMatrixDsb;
        dv->m_glMatrixDsbWaterfall.translate(
             0.0f,
             (float) waterfallTop / (float) height()
        );
        dv->m_glMatrixDsbWaterfall.scale(
            (float) (width() - m_leftMargin - m_rightMargin) / (float) width(),
            (float) m_waterfallHeight / (float) height()
        );

        dv->m_glMatrixDsbHistogram = glMatrixDsb;
        dv->m_glMatrixDsbHistogram.translate(
             0.0f,
             (float) histogramTop / (float) height()
        );
        dv->m_glMatrixDsbHistogram.scale(
            (float) (width() - m_leftMargin - m_rightMargin) / (float) width(),
            (float) m_histogramHeight / (float) height()
        );

        dv->m_glMatrixDsbFreqScale = glMatrixDsb;
        dv->m_glMatrixDsbFreqScale.translate(
             0.0f,
             (float) frequencyScaleTop / (float) height()
        );
        dv->m_glMatrixDsbFreqScale.scale(
            (float) (width() - m_leftMargin - m_rightMargin) / (float) width(),
            (float) m_frequencyScaleHeight / (float) height()
        );

        // draw the effective BW rectangle

        QMatrix4x4 glMatrix;
        glMatrix.setToIdentity();
        glMatrix.translate(
            -1.0f + 2.0f * ((m_leftMargin + m_frequencyScale.getPosFromValue(xc + nw)) / (float) width()),
             1.0f
        );
        glMatrix.scale(
            2.0f * ((pw-nw) / (float) frequencySpan),
            -2.0f
        );

        dv->m_glMatrixWaterfall = glMatrix;
        dv->m_glMatrixWaterfall.translate(
             0.0f,
             (float) waterfallTop / (float) height()
        );
        dv->m_glMatrixWaterfall.scale(
            (float) (width() - m_leftMargin - m_rightMargin) / (float) width(),
            (float) m_waterfallHeight / (float) height()
        );

        dv->m_glMatrixHistogram = glMatrix;
        dv->m_glMatrixHistogram.translate(
             0.0f,
             (float) histogramTop / (float) height()
        );
        dv->m_glMatrixHistogram.scale(
            (float) (width() - m_leftMargin - m_rightMargin) / (float) width(),
            (float) m_histogramHeight / (float) height()
        );

        dv->m_glMatrixFreqScale = glMatrix;
        dv->m_glMatrixFreqScale.translate(
             0.0f,
             (float) frequencyScaleTop / (float) height()
        );
        dv->m_glMatrixFreqScale.scale(
            (float) (width() - m_leftMargin - m_rightMargin) / (float) width(),
            (float) m_frequencyScaleHeight / (float) height()
        );


        /*
        dv->m_glRect.setRect(
            m_frequencyScale.getPosFromValue(m_centerFrequency + dv->m_channelMarker->getCenterFrequency() - dv->m_channelMarker->getBandwidth() / 2) / (float)(width() - m_leftMargin - m_rightMargin),
            0,
            (dv->m_channelMarker->getBandwidth() / (float)m_sampleRate),
            1);
        */

        if (m_displayHistogram || m_displayMaxHold || m_displayCurrent || m_displayWaterfall)
        {
            dv->m_rect.setRect(m_frequencyScale.getPosFromValue(xc) + m_leftMargin - 1,
            m_topMargin,
            5,
            height() - m_topMargin - m_bottomMargin);
        }

        /*
        if(m_displayHistogram || m_displayMaxHold || m_displayWaterfall) {
            dv->m_rect.setRect(m_frequencyScale.getPosFromValue(m_centerFrequency + dv->m_channelMarker->getCenterFrequency()) + m_leftMargin - 1,
            m_topMargin,
            5,
            height() - m_topMargin - m_bottomMargin);
        }
        */
    }

    // prepare left scales (time and power)
    {
        m_leftMarginPixmap = QPixmap(m_leftMargin - 1, height());
        m_leftMarginPixmap.fill(Qt::transparent);
        {
            QPainter painter(&m_leftMarginPixmap);
            painter.setPen(QColor(0xf0, 0xf0, 0xff));
            painter.setFont(font());
            const ScaleEngine::TickList* tickList;
            const ScaleEngine::Tick* tick;
            if (m_displayWaterfall) {
                tickList = &m_timeScale.getTickList();
                for (int i = 0; i < tickList->count(); i++) {
                    tick = &(*tickList)[i];
                    if (tick->major) {
                        if (tick->textSize > 0)
                            painter.drawText(QPointF(m_leftMargin - M - tick->textSize, waterfallTop + fm.ascent() + tick->textPos), tick->text);
                    }
                }
            }
            if (m_displayHistogram || m_displayMaxHold || m_displayCurrent) {
                tickList = &m_powerScale.getTickList();
                for (int i = 0; i < tickList->count(); i++) {
                    tick = &(*tickList)[i];
                    if (tick->major) {
                        if (tick->textSize > 0)
                            painter.drawText(QPointF(m_leftMargin - M - tick->textSize, histogramTop + m_histogramHeight - tick->textPos - 1), tick->text);
                    }
                }
            }
        }

        m_glShaderLeftScale.initTexture(m_leftMarginPixmap.toImage());
    }
    // prepare frequency scale
    if (m_displayWaterfall || m_display3DSpectrogram || m_displayHistogram || m_displayMaxHold || m_displayCurrent) {
        m_frequencyPixmap = QPixmap(width(), m_frequencyScaleHeight);
        m_frequencyPixmap.fill(Qt::transparent);
        {
            QPainter painter(&m_frequencyPixmap);
            painter.setPen(Qt::NoPen);
            painter.setBrush(Qt::black);
            painter.setBrush(Qt::transparent);
            painter.drawRect(m_leftMargin, 0, width() - m_leftMargin, m_frequencyScaleHeight);
            painter.setPen(QColor(0xf0, 0xf0, 0xff));
            painter.setFont(font());
            const ScaleEngine::TickList* tickList = &m_frequencyScale.getTickList();
            const ScaleEngine::Tick* tick;

            for (int i = 0; i < tickList->count(); i++) {
                tick = &(*tickList)[i];
                if (tick->major) {
                    if (tick->textSize > 0)
                        painter.drawText(QPointF(m_leftMargin + tick->textPos, fm.height() + fm.ascent() / 2 - 1), tick->text);
                }
            }

            // Frequency overlay on highlighted marker
            for (int i = 0; i < m_channelMarkerStates.size(); ++i)
            {
                ChannelMarkerState* dv = m_channelMarkerStates[i];

                if (dv->m_channelMarker->getHighlighted()
                    && (dv->m_channelMarker->getSourceOrSinkStream() == m_displaySourceOrSink)
                    && dv->m_channelMarker->streamIndexApplies(m_displayStreamIndex))
                {
                    qreal xc;
                    int shift;
                    //ChannelMarker::sidebands_t sidebands = dv->m_channelMarker->getSidebands();
                    xc = m_centerFrequency + dv->m_channelMarker->getCenterFrequency(); // marker center frequency
                    QString ftext;
                    switch (dv->m_channelMarker->getFrequencyScaleDisplayType())
                    {
                    case ChannelMarker::FScaleDisplay_freq:
                        ftext = QString::number((m_centerFrequency + dv->m_channelMarker->getCenterFrequency())/1e6, 'f', 6);
                        break;
                    case ChannelMarker::FScaleDisplay_title:
                        ftext = dv->m_channelMarker->getTitle();
                        break;
                    case ChannelMarker::FScaleDisplay_addressSend:
                        ftext = dv->m_channelMarker->getDisplayAddressSend();
                        break;
                    case ChannelMarker::FScaleDisplay_addressReceive:
                        ftext = dv->m_channelMarker->getDisplayAddressReceive();
                        break;
                    default:
                        ftext = QString::number((m_centerFrequency + dv->m_channelMarker->getCenterFrequency())/1e6, 'f', 6);
                        break;
                    }
                    if (dv->m_channelMarker->getCenterFrequency() < 0) { // left half of scale
                        ftext = " " + ftext;
                        shift = 0;
                    } else { // right half of scale
                        ftext = ftext + " ";
                        shift = - fm.horizontalAdvance(ftext);
                    }
                    painter.drawText(QPointF(m_leftMargin + m_frequencyScale.getPosFromValue(xc) + shift, 2*fm.height() + fm.ascent() / 2 - 1), ftext);
                }
            }

        }

        m_glShaderFrequencyScale.initTexture(m_frequencyPixmap.toImage());
    }
    // prepare left scale for spectrogram (time)
    {
        m_spectrogramTimePixmap = QPixmap(m_leftMargin - 1, fm.ascent() + m_waterfallHeight);
        m_spectrogramTimePixmap.fill(Qt::transparent);
        {
            QPainter painter(&m_spectrogramTimePixmap);
            painter.setPen(QColor(0xf0, 0xf0, 0xff));
            painter.setFont(font());
            const ScaleEngine::TickList* tickList;
            const ScaleEngine::Tick* tick;
            if (m_display3DSpectrogram) {
                tickList = &m_timeScale.getTickList();
                for (int i = 0; i < tickList->count(); i++) {
                    tick = &(*tickList)[i];
                    if (tick->major) {
                        if (tick->textSize > 0)
                            painter.drawText(QPointF(m_leftMargin - M - tick->textSize, fm.height() + tick->textPos), tick->text);
                    }
                }
            }
        }

        m_glShaderSpectrogramTimeScale.initTexture(m_spectrogramTimePixmap.toImage());
    }
    // prepare vertical scale for spectrogram (power)
    {
        int h = m_histogramHeight != 0 ? m_histogramHeight : m_waterfallHeight / 4;
        m_spectrogramPowerPixmap = QPixmap(m_leftMargin - 1, m_topMargin + h);
        m_spectrogramPowerPixmap.fill(Qt::transparent);
        {
            QPainter painter(&m_spectrogramPowerPixmap);
            painter.setPen(QColor(0xf0, 0xf0, 0xff));
            painter.setFont(font());
            const ScaleEngine::TickList* tickList;
            const ScaleEngine::Tick* tick;
            if (m_display3DSpectrogram) {
                tickList = &m_powerScale.getTickList();
                for (int i = 0; i < tickList->count(); i++) {
                    tick = &(*tickList)[i];
                    if (tick->major) {
                        if (tick->textSize > 0)
                            painter.drawText(QPointF(m_leftMargin - M - tick->textSize, m_topMargin + h - tick->textPos - 1), tick->text);
                    }
                }
            }
        }

        m_glShaderSpectrogramPowerScale.initTexture(m_spectrogramPowerPixmap.toImage());
    }

    // Top info line
    m_glInfoBoxMatrix.setToIdentity();
    m_glInfoBoxMatrix.translate (
        -1.0f,
        1.0f
    );
    m_glInfoBoxMatrix.scale (
        2.0f,
        (float) -2*m_infoHeight / (float) height()
    );
    m_infoRect = QRect(
        0,
        0,
        width(),
        m_infoHeight
    );
    QString infoText;
    formatTextInfo(infoText);
    m_infoPixmap = QPixmap(width(), m_infoHeight);
    m_infoPixmap.fill(Qt::transparent);
    {
        QPainter painter(&m_infoPixmap);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::black);
        painter.setBrush(Qt::transparent);
        painter.drawRect(m_leftMargin, 0, width() - m_leftMargin, m_infoHeight);
        painter.setPen(QColor(0xf0, 0xf0, 0xff));
        painter.setFont(font());
        painter.drawText(QPointF(m_leftMargin, fm.height() + fm.ascent() / 2 - 2), infoText);
    }

    m_glShaderInfo.initTexture(m_infoPixmap.toImage());

    // Peak details in top info line
    QString minFrequencyStr = displayFull(m_centerFrequency - m_sampleRate/2); // This can be wider if negative, while max is positive
    QString maxFrequencyStr = displayFull(m_centerFrequency + m_sampleRate/2);
    m_peakFrequencyMaxStr = minFrequencyStr.size() > maxFrequencyStr.size() ? minFrequencyStr : maxFrequencyStr;
    m_peakFrequencyMaxStr = m_peakFrequencyMaxStr.append("Hz");
    m_peakPowerMaxStr = m_linear ? "8.000e-10" : "-100.0";
    m_peakPowerUnits = m_linear ? "" : "dB";

    bool waterfallFFTSizeChanged = true;

    if (m_waterfallBuffer) {
        waterfallFFTSizeChanged = m_waterfallBuffer->width() != m_nbBins;
    }

    bool windowSizeChanged = m_waterfallTextureHeight != m_waterfallHeight;

    if (waterfallFFTSizeChanged || windowSizeChanged)
    {
        if (m_waterfallBuffer) {
            delete m_waterfallBuffer;
        }

        m_waterfallBuffer = new QImage(m_nbBins, m_waterfallHeight, QImage::Format_ARGB32);
        m_waterfallBuffer->fill(qRgb(0x00, 0x00, 0x00));

        if (m_waterfallHeight > 0) {
            m_glShaderWaterfall.initTexture(*m_waterfallBuffer);
        }

        m_waterfallBufferPos = 0;

        if (m_3DSpectrogramBuffer) {
            delete m_3DSpectrogramBuffer;
        }

        m_3DSpectrogramBuffer = new QImage(m_nbBins, m_waterfallHeight, QImage::Format_Grayscale8);
        m_3DSpectrogramBuffer->fill(qRgb(0x00, 0x00, 0x00));

        if (m_waterfallHeight > 0) {
            m_glShaderSpectrogram.initTexture(*m_3DSpectrogramBuffer);
        }

        m_3DSpectrogramBufferPos = 0;

        m_waterfallTextureHeight = m_waterfallHeight;
        m_waterfallTexturePos = 0;
        m_3DSpectrogramTextureHeight = m_waterfallHeight;
        m_3DSpectrogramTexturePos = 0;
    }

    m_glShaderSpectrogram.initColorMapTexture(m_colorMapName);
    m_glShaderColorMap.initColorMapTexture(m_colorMapName);
    m_colorMap = ColorMap::getColorMap(m_colorMapName);
    // Why only 240 entries in the palette?
    for (int i = 0; i <= 239; i++)
    {
        ((quint8*)&m_waterfallPalette[i])[0] = (quint8)(m_colorMap[i*3] * 255.0);
        ((quint8*)&m_waterfallPalette[i])[1] = (quint8)(m_colorMap[i*3+1] * 255.0);
        ((quint8*)&m_waterfallPalette[i])[2] = (quint8)(m_colorMap[i*3+2] * 255.0);
        ((quint8*)&m_waterfallPalette[i])[3] = 255;
    }

    bool histogramFFTSizeChanged = true;

    if (m_histogramBuffer) {
        histogramFFTSizeChanged = m_histogramBuffer->width() != m_nbBins;
    }

    if (histogramFFTSizeChanged)
    {
        if (m_histogramBuffer) {
            delete m_histogramBuffer;
        }

        m_histogramBuffer = new QImage(m_nbBins, 100, QImage::Format_RGB32);

        m_histogramBuffer->fill(qRgb(0x00, 0x00, 0x00));
        m_glShaderHistogram.initTexture(*m_histogramBuffer, QOpenGLTexture::ClampToEdge);

        if (m_histogram) {
            delete[] m_histogram;
        }

        m_histogram = new quint8[100 * m_nbBins];
        memset(m_histogram, 0x00, 100 * m_nbBins);

        m_q3FFT.allocate(2*(m_nbBins+1));

        m_q3ColorMap.allocate(4*(m_nbBins+1));
        std::fill(m_q3ColorMap.m_array, m_q3ColorMap.m_array+4*(m_nbBins+1), 0.0f);
    }

    m_q3TickTime.allocate(4*m_timeScale.getTickList().count());
    m_q3TickFrequency.allocate(4*m_frequencyScale.getTickList().count());
    m_q3TickPower.allocate(6*m_powerScale.getTickList().count());   // 6 as we need 3d points for 3D spectrogram
    updateHistogramMarkers();
    updateWaterfallMarkers();
    updateSortedAnnotationMarkers();
} // applyChanges

void GLSpectrumView::updateHistogramMarkers()
{
    int64_t centerFrequency;
    int frequencySpan;
    getFrequencyZoom(centerFrequency, frequencySpan);
    int effFftSize = m_fftSize * ((float) frequencySpan / (float) m_sampleRate);

    for (int i = 0; i < m_histogramMarkers.size(); i++)
    {
        float powerI = m_linear ?
            m_histogramMarkers.at(i).m_power * (m_useCalibration ? m_calibrationGain : 1.0f) :
            CalcDb::dbPower(m_histogramMarkers.at(i).m_power) + (m_useCalibration ? m_calibrationShiftdB : 0.0f);
        m_histogramMarkers[i].m_point.rx() =
            (m_histogramMarkers[i].m_frequency - m_frequencyScale.getRangeMin()) / m_frequencyScale.getRange();
        m_histogramMarkers[i].m_point.ry() =
            (m_powerScale.getRangeMax() - powerI) / m_powerScale.getRange();
        // m_histogramMarkers[i].m_fftBin =
        //     (((m_histogramMarkers[i].m_frequency - m_centerFrequency) / (float) m_sampleRate) + 0.5) * m_fftSize;
        m_histogramMarkers[i].m_fftBin =
            (((m_histogramMarkers[i].m_frequency - centerFrequency) / (float) frequencySpan) + 0.5) * effFftSize;
        m_histogramMarkers[i].m_point.rx() = m_histogramMarkers[i].m_point.rx() < 0 ?
            0 : m_histogramMarkers[i].m_point.rx() > 1 ?
                1 : m_histogramMarkers[i].m_point.rx();
        m_histogramMarkers[i].m_point.ry() = m_histogramMarkers[i].m_point.ry() < 0 ?
            0 : m_histogramMarkers[i].m_point.ry() > 1 ?
                1 : m_histogramMarkers[i].m_point.ry();
        m_histogramMarkers[i].m_fftBin = m_histogramMarkers[i].m_fftBin < 0 ?
            0 : m_histogramMarkers[i].m_fftBin > m_fftSize - 1 ?
                m_fftSize - 1 : m_histogramMarkers[i].m_fftBin;
        m_histogramMarkers[i].m_frequencyStr = displayScaled(
            m_histogramMarkers[i].m_frequency,
            'f',
            getPrecision((m_centerFrequency*1000)/m_sampleRate),
            false);
        m_histogramMarkers[i].m_powerStr = displayPower(
            powerI,
            m_linear ? 'e' : 'f',
            m_linear ? 3 : 1);

        if (i > 0)
        {
            int64_t deltaFrequency = m_histogramMarkers.at(i).m_frequency - m_histogramMarkers.at(0).m_frequency;
            m_histogramMarkers[i].m_deltaFrequencyStr = displayScaled(
                deltaFrequency,
                'f',
                getPrecision(deltaFrequency/m_sampleRate),
                true);
            float power0 = m_linear ?
                m_histogramMarkers.at(0).m_power * (m_useCalibration ? m_calibrationGain : 1.0f) :
                CalcDb::dbPower(m_histogramMarkers.at(0).m_power) + (m_useCalibration ? m_calibrationShiftdB : 0.0f);
            m_histogramMarkers[i].m_deltaPowerStr = displayPower(
                powerI - power0,
                m_linear ? 'e' : 'f',
                m_linear ? 3 : 1);
        }
    }
}

void GLSpectrumView::updateHistogramPeaks()
{
    int j = 0;
    for (int i = 0; i < m_histogramMarkers.size(); i++)
    {
        if (j >= (int) m_peakFinder.getPeaks().size()) {
            break;
        }

        int fftBin = m_peakFinder.getPeaks()[j].second;
        Real power = m_peakFinder.getPeaks()[j].first;
        // qDebug("GLSpectrumView::updateHistogramPeaks: %d %d %f", j, fftBin, power);

        if ((m_histogramMarkers.at(i).m_markerType == SpectrumHistogramMarker::SpectrumMarkerTypePower) ||
            ((m_histogramMarkers.at(i).m_markerType == SpectrumHistogramMarker::SpectrumMarkerTypePowerMax) &&
             (m_histogramMarkers.at(i).m_holdReset || (power > m_histogramMarkers.at(i).m_powerMax))))
        {
            float binSize = m_frequencyScale.getRange() / m_nbBins;
            m_histogramMarkers[i].m_fftBin = fftBin;
            m_histogramMarkers[i].m_frequency = m_frequencyScale.getRangeMin() + binSize*fftBin;
            m_histogramMarkers[i].m_point.rx() = binSize*fftBin / m_frequencyScale.getRange();

            if (i == 0)
            {
                m_histogramMarkers[i].m_frequencyStr = displayScaled(
                    m_histogramMarkers[i].m_frequency,
                    'f',
                    getPrecision((m_centerFrequency*1000)/m_sampleRate),
                    false
                );
            }
            else
            {
                int64_t deltaFrequency = m_histogramMarkers.at(i).m_frequency - m_histogramMarkers.at(0).m_frequency;
                m_histogramMarkers[i].m_deltaFrequencyStr = displayScaled(
                    deltaFrequency,
                    'f',
                    getPrecision(deltaFrequency/m_sampleRate),
                    true
                );
            }
        }
        else
        {
            continue;
        }

        j++;
    }
}

void GLSpectrumView::updateWaterfallMarkers()
{
    for (int i = 0; i < m_waterfallMarkers.size(); i++)
    {
        m_waterfallMarkers[i].m_point.rx() =
            (m_waterfallMarkers[i].m_frequency - m_frequencyScale.getRangeMin()) / m_frequencyScale.getRange();
        m_waterfallMarkers[i].m_point.ry() =
            (m_waterfallMarkers[i].m_time - m_timeScale.getRangeMin()) / m_timeScale.getRange();
        m_waterfallMarkers[i].m_point.rx() = m_waterfallMarkers[i].m_point.rx() < 0 ?
            0 : m_waterfallMarkers[i].m_point.rx() > 1 ?
                1 : m_waterfallMarkers[i].m_point.rx();
        m_waterfallMarkers[i].m_point.ry() = m_waterfallMarkers[i].m_point.ry() < 0 ?
            0 : m_waterfallMarkers[i].m_point.ry() > 1 ?
                1 : m_waterfallMarkers[i].m_point.ry();
        m_waterfallMarkers[i].m_frequencyStr = displayScaled(
            m_waterfallMarkers[i].m_frequency,
            'f',
            getPrecision((m_centerFrequency*1000)/m_sampleRate),
            false);
        m_waterfallMarkers[i].m_timeStr = displayScaledF(
            m_waterfallMarkers[i].m_time,
            'f',
            3,
            true);

        if (i > 0)
        {
            int64_t deltaFrequency = m_waterfallMarkers.at(i).m_frequency - m_waterfallMarkers.at(0).m_frequency;
            m_waterfallMarkers.back().m_deltaFrequencyStr = displayScaled(
                deltaFrequency,
                'f',
                getPrecision(deltaFrequency/m_sampleRate),
                true);
            m_waterfallMarkers.back().m_deltaTimeStr = displayScaledF(
                m_waterfallMarkers.at(i).m_time - m_waterfallMarkers.at(0).m_time,
                'f',
                3,
                true);
        }
    }
}

void GLSpectrumView::updateAnnotationMarkers()
{
    if (!(m_markersDisplay & SpectrumSettings::MarkersDisplayAnnotations)) {
        return;
    }

    m_sortedAnnotationMarkers.clear();

    for (auto &marker : m_annotationMarkers) {
        m_sortedAnnotationMarkers.push_back(&marker);
    }

    std::sort(m_sortedAnnotationMarkers.begin(), m_sortedAnnotationMarkers.end(), annotationDisplayLessThan);
    updateSortedAnnotationMarkers();
}

void GLSpectrumView::updateSortedAnnotationMarkers()
{
    if (!(m_markersDisplay & SpectrumSettings::MarkersDisplayAnnotations)) {
        return;
    }

    m_visibleAnnotationMarkers.clear();

    for (auto &marker : m_sortedAnnotationMarkers)
    {
        float startPos = (marker->m_startFrequency - m_frequencyScale.getRangeMin()) / m_frequencyScale.getRange();
        float stopPos = ((marker->m_startFrequency + marker->m_bandwidth) - m_frequencyScale.getRangeMin()) / m_frequencyScale.getRange();

        if ((startPos > 1.0f) || (stopPos < 0.0f)) // out of range
		{
            continue;
        }

        m_visibleAnnotationMarkers.push_back(marker);
        m_visibleAnnotationMarkers.back()->m_startPos = startPos < 0.0f ? 0.0f : startPos;
        m_visibleAnnotationMarkers.back()->m_stopPos = stopPos > 1.0f ? 1.0f : stopPos;
    }
}

void GLSpectrumView::updateMarkersDisplay()
{
    if (m_markersDisplay & SpectrumSettings::MarkersDisplayAnnotations) {
        updateAnnotationMarkers();
    }
}

void GLSpectrumView::updateCalibrationPoints()
{
    if (m_calibrationPoints.size() == 0)
    {
        m_calibrationGain = 1.0;
        m_calibrationShiftdB = 0.0;
    }
    else if (m_calibrationPoints.size() == 1)
    {
        m_calibrationGain = m_calibrationPoints.first().m_powerCalibratedReference /
             m_calibrationPoints.first().m_powerRelativeReference;
        m_calibrationShiftdB = CalcDb::dbPower(m_calibrationGain);
    }
    else
    {
        QList<SpectrumCalibrationPoint> sortedCalibrationPoints = m_calibrationPoints;
        std::sort(sortedCalibrationPoints.begin(), sortedCalibrationPoints.end(), calibrationPointsLessThan);

        if (m_centerFrequency <= sortedCalibrationPoints.first().m_frequency)
        {
            m_calibrationGain = m_calibrationPoints.first().m_powerCalibratedReference /
                m_calibrationPoints.first().m_powerRelativeReference;
            m_calibrationShiftdB = CalcDb::dbPower(m_calibrationGain);
        }
        else if (m_centerFrequency >= sortedCalibrationPoints.last().m_frequency)
        {
            m_calibrationGain = m_calibrationPoints.last().m_powerCalibratedReference /
                m_calibrationPoints.last().m_powerRelativeReference;
            m_calibrationShiftdB = CalcDb::dbPower(m_calibrationGain);
        }
        else
        {
            int lowIndex = 0;
            int highIndex = sortedCalibrationPoints.size() - 1;

            for (int index = 0; index < sortedCalibrationPoints.size(); index++)
            {
                if (m_centerFrequency < sortedCalibrationPoints[index].m_frequency)
                {
                    highIndex = index;
                    break;
                }
                else
                {
                    lowIndex = index;
                }
            }

            // frequency interpolation is always linear
            double deltaFrequency = sortedCalibrationPoints[highIndex].m_frequency -
                sortedCalibrationPoints[lowIndex].m_frequency;
            double shiftFrequency = m_centerFrequency - sortedCalibrationPoints[lowIndex].m_frequency;
            double interpolationRatio = shiftFrequency / deltaFrequency;

            // calculate low and high gains in linear mode
            double gainLow = sortedCalibrationPoints[lowIndex].m_powerCalibratedReference /
                sortedCalibrationPoints[lowIndex].m_powerRelativeReference;
            double gainHigh = sortedCalibrationPoints[highIndex].m_powerCalibratedReference /
                sortedCalibrationPoints[highIndex].m_powerRelativeReference;

            // power interpolation depends on interpolation options
            if (m_calibrationInterpMode == SpectrumSettings::CalibInterpLinear)
            {
                m_calibrationGain = gainLow + interpolationRatio*(gainHigh - gainLow); // linear driven
                m_calibrationShiftdB = CalcDb::dbPower(m_calibrationGain);
            }
            else if (m_calibrationInterpMode == SpectrumSettings::CalibInterpLog)
            {
                m_calibrationShiftdB = CalcDb::dbPower(gainLow)
                    + interpolationRatio*(CalcDb::dbPower(gainHigh) - CalcDb::dbPower(gainLow)); // log driven
                m_calibrationGain = CalcDb::powerFromdB(m_calibrationShiftdB);
            }
        }
    }

    updateHistogramMarkers();

    if (m_messageQueueToGUI && m_useCalibration) {
        m_messageQueueToGUI->push(new MsgReportCalibrationShift(m_calibrationShiftdB));
    }

    m_changesPending = true;
}

void GLSpectrumView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_rotate3DSpectrogram)
    {
        // Rotate 3D Spectrogram
        QPointF delta = m_mousePrevLocalPos - event->localPos();
        m_mousePrevLocalPos = event->localPos();
        m_glShaderSpectrogram.rotateZ(-delta.x()/2.0f);
        m_glShaderSpectrogram.rotateX(-delta.y()/2.0f);
        repaint(); // Force repaint in case acquisition is stopped
        return;
    }
    if (m_pan3DSpectrogram)
    {
        // Pan 3D Spectrogram
        QPointF delta = m_mousePrevLocalPos - event->localPos();
        m_mousePrevLocalPos = event->localPos();
        m_glShaderSpectrogram.translateX(-delta.x()/2.0f/500.0f);
        m_glShaderSpectrogram.translateY(delta.y()/2.0f/500.0f);
        repaint(); // Force repaint in case acquisition is stopped
        return;
    }

    if (m_scaleZ3DSpectrogram)
    {
        // Scale 3D Spectrogram in Z dimension
        QPointF delta = m_mousePrevLocalPos - event->localPos();
        m_mousePrevLocalPos = event->localPos();
        m_glShaderSpectrogram.userScaleZ(1.0+(float)delta.y()/20.0);
        repaint(); // Force repaint in case acquisition is stopped
        return;
    }

    if (m_scrollFrequency)
    {
        // Request containing widget to adjust center frequency
        // Not all containers will support this - mainly for MainSpectrumGUI
        // This can be a little slow on some SDRs, so we use delta from where
        // button was originally pressed rather than do it incrementally
        QPointF delta = m_mousePrevLocalPos - event->localPos();
        float histogramWidth = width() - m_leftMargin - m_rightMargin;
        qint64 frequency = (qint64)(m_scrollStartCenterFreq + delta.x()/histogramWidth * m_frequencyScale.getRange());
        emit requestCenterFrequency(frequency);
        return;
    }

    if (m_displayWaterfall || m_displayHistogram || m_displayMaxHold || m_displayCurrent)
    {
        if (m_frequencyScaleRect.contains(event->pos()))
        {
            if (m_cursorState == CSNormal)
            {
                setCursor(Qt::SizeVerCursor);
                m_cursorState = CSSplitter;
                return;
            }
        }
        else
        {
            if (m_cursorState == CSSplitter)
            {
                setCursor(Qt::ArrowCursor);
                m_cursorState = CSNormal;
                return;
            }
        }
    }

    if (m_cursorState == CSSplitterMoving)
    {
        QMutexLocker mutexLocker(&m_mutex);
        float newShare;

        if (!m_invertedWaterfall) {
            newShare = (float) (event->y() - m_frequencyScaleRect.height()) / (float) height();
        } else {
            newShare = 1.0 - (float) (event->y() + m_frequencyScaleRect.height()) / (float) height();
        }

        if (newShare < 0.1) {
            newShare = 0.1f;
        } else if (newShare > 0.8) {
            newShare = 0.8f;
        }

        m_waterfallShare = newShare;
        m_changesPending = true;

        if (m_messageQueueToGUI) {
            m_messageQueueToGUI->push(new MsgReportWaterfallShare(m_waterfallShare));
        }

        update();
        return;
    }
    else if (m_cursorState == CSChannelMoving)
    {
        // Determine if user is trying to move the channel outside of the current frequency range
        // and if so, request an adjustment to the center frequency
        Real freqAbs = m_frequencyScale.getValueFromPos(event->x() - m_leftMarginPixmap.width() - 1);
        Real freqMin = m_centerFrequency - m_sampleRate / 2.0f;
        Real freqMax = m_centerFrequency + m_sampleRate / 2.0f;
        if (freqAbs < freqMin) {
            emit requestCenterFrequency(m_centerFrequency - (freqMin - freqAbs));
        } else if (freqAbs > freqMax) {
            emit requestCenterFrequency(m_centerFrequency + (freqAbs - freqMax));
        }

        Real freq = freqAbs - m_centerFrequency;
        if (m_channelMarkerStates[m_cursorChannel]->m_channelMarker->getMovable()
            && (m_channelMarkerStates[m_cursorChannel]->m_channelMarker->getSourceOrSinkStream() == m_displaySourceOrSink)
            && m_channelMarkerStates[m_cursorChannel]->m_channelMarker->streamIndexApplies(m_displayStreamIndex))
        {
            m_channelMarkerStates[m_cursorChannel]->m_channelMarker->setCenterFrequencyByCursor(freq);
            channelMarkerChanged();
        }
    }

    if (m_displayWaterfall || m_displayHistogram || m_displayMaxHold || m_displayCurrent)
    {
        for (int i = 0; i < m_channelMarkerStates.size(); ++i)
        {
            if ((m_channelMarkerStates[i]->m_channelMarker->getSourceOrSinkStream() != m_displaySourceOrSink)
                || !m_channelMarkerStates[i]->m_channelMarker->streamIndexApplies(m_displayStreamIndex))
            {
                continue;
            }

            if (m_channelMarkerStates[i]->m_rect.contains(event->pos()))
            {
                if (m_cursorState == CSNormal)
                {
                    setCursor(Qt::SizeHorCursor);
                    m_cursorState = CSChannel;
                    m_cursorChannel = i;
                    m_channelMarkerStates[i]->m_channelMarker->setHighlightedByCursor(true);
                    channelMarkerChanged();

                    return;
                }
                else if (m_cursorState == CSChannel)
                {
                    return;
                }
            }
            else if (m_channelMarkerStates[i]->m_channelMarker->getHighlighted())
            {
                m_channelMarkerStates[i]->m_channelMarker->setHighlightedByCursor(false);
                channelMarkerChanged();
            }
        }
    }

    if (m_cursorState == CSChannel)
    {
        setCursor(Qt::ArrowCursor);
        m_cursorState = CSNormal;

        return;
    }

    event->setAccepted(false);
}

void GLSpectrumView::mousePressEvent(QMouseEvent* event)
{
    const QPointF& ep = event->localPos();

    if ((event->button() == Qt::MiddleButton) && (m_displayMaxHold || m_displayCurrent || m_displayHistogram) && pointInHistogram(ep))
    {
        m_scrollFrequency = true;
        m_scrollStartCenterFreq = m_centerFrequency;
        m_mousePrevLocalPos = ep;
        return;
    }

    if ((event->button() == Qt::MiddleButton) && m_display3DSpectrogram && pointInWaterfallOrSpectrogram(ep))
    {
        m_pan3DSpectrogram = true;
        m_mousePrevLocalPos = ep;
        return;
    }

    if ((event->button() == Qt::RightButton) && m_display3DSpectrogram && pointInWaterfallOrSpectrogram(ep))
    {
        m_scaleZ3DSpectrogram = true;
        m_mousePrevLocalPos = ep;
        return;
    }

    if (event->button() == Qt::RightButton)
    {
        QPointF pHis = ep;
        bool doUpdate = false;
        pHis.rx() = (ep.x()/width() - m_histogramRect.left()) / m_histogramRect.width();
        pHis.ry() = (ep.y()/height() - m_histogramRect.top()) / m_histogramRect.height();

        if (event->modifiers() & Qt::ShiftModifier)
        {
            if ((pHis.x() >= 0) && (pHis.x() <= 1) && (pHis.y() >= 0) && (pHis.y() <= 1))
            {
                m_histogramMarkers.clear();
                doUpdate = true;
                if (m_messageQueueToGUI) {
                    m_messageQueueToGUI->push(new MsgReportHistogramMarkersChange());
                }
            }
        }
        else
        {
            if ((m_histogramMarkers.size() > 0) && (pHis.x() >= 0) && (pHis.x() <= 1) && (pHis.y() >= 0) && (pHis.y() <= 1))
            {
                m_histogramMarkers.pop_back();
                doUpdate = true;
                if (m_messageQueueToGUI) {
                    m_messageQueueToGUI->push(new MsgReportHistogramMarkersChange());
                }
            }
        }

        QPointF pWat = ep;
        pWat.rx() = (ep.x()/width() - m_waterfallRect.left()) / m_waterfallRect.width();
        pWat.ry() = (ep.y()/height() - m_waterfallRect.top()) / m_waterfallRect.height();

        if (event->modifiers() & Qt::ShiftModifier)
        {
            if ((pWat.x() >= 0) && (pWat.x() <= 1) && (pWat.y() >= 0) && (pWat.y() <= 1))
            {
                m_waterfallMarkers.clear();
                doUpdate = true;
                if (m_messageQueueToGUI) {
                    m_messageQueueToGUI->push(new MsgReportWaterfallMarkersChange());
                }
            }
        }
        else
        {
            if ((m_waterfallMarkers.size() > 0) && (pWat.x() >= 0) && (pWat.x() <= 1) && (pWat.y() >= 0) && (pWat.y() <= 1))
            {
                m_waterfallMarkers.pop_back();
                doUpdate = true;
                if (m_messageQueueToGUI) {
                    m_messageQueueToGUI->push(new MsgReportWaterfallMarkersChange());
                }
            }
        }

        if (doUpdate) {
            update();
        }
    }
    else if (event->button() == Qt::LeftButton)
    {
        if (event->modifiers() & Qt::ShiftModifier)
        {
            QPointF pHis = ep;
            bool doUpdate = false;
            pHis.rx() = (ep.x()/width() - m_histogramRect.left()) / m_histogramRect.width();
            pHis.ry() = (ep.y()/height() - m_histogramRect.top()) / m_histogramRect.height();
            float frequency = m_frequencyScale.getRangeMin() + pHis.x()*m_frequencyScale.getRange();
            float powerVal = m_powerScale.getRangeMax() - pHis.y()*m_powerScale.getRange();
            float power = m_linear ? powerVal : CalcDb::powerFromdB(powerVal);
            int fftBin = (((frequency - m_centerFrequency) / (float) m_sampleRate) * m_fftSize) + (m_fftSize / 2);

            if ((pHis.x() >= 0) && (pHis.x() <= 1) && (pHis.y() >= 0) && (pHis.y() <= 1))
            {
                if (m_histogramMarkers.size() < SpectrumHistogramMarker::m_maxNbOfMarkers)
                {
                    m_histogramMarkers.push_back(SpectrumHistogramMarker());
                    m_histogramMarkers.back().m_point = pHis;
                    m_histogramMarkers.back().m_frequency = frequency;
                    m_histogramMarkers.back().m_fftBin = fftBin;
                    m_histogramMarkers.back().m_frequencyStr = displayScaled(
                        frequency,
                        'f',
                        getPrecision((m_centerFrequency*1000)/m_sampleRate),
                        false);
                    m_histogramMarkers.back().m_power = power;
                    m_histogramMarkers.back().m_powerStr = displayPower(
                        powerVal,
                        m_linear ? 'e' : 'f',
                        m_linear ? 3 : 1);

                    if (m_histogramMarkers.size() > 1)
                    {
                        int64_t deltaFrequency = frequency - m_histogramMarkers.at(0).m_frequency;
                        m_histogramMarkers.back().m_deltaFrequencyStr = displayScaled(
                            deltaFrequency,
                            'f',
                            getPrecision(deltaFrequency/m_sampleRate),
                            true);
                        float power0 = m_linear ?
                            m_histogramMarkers.at(0).m_power :
                            CalcDb::dbPower(m_histogramMarkers.at(0).m_power);
                        m_histogramMarkers.back().m_deltaPowerStr = displayPower(
                            power - power0,
                            m_linear ? 'e' : 'f',
                            m_linear ? 3 : 1);
                    }

                    if (m_messageQueueToGUI) {
                        m_messageQueueToGUI->push(new MsgReportHistogramMarkersChange());
                    }
                    doUpdate = true;
                }
            }

            QPointF pWat = ep;
            pWat.rx() = (ep.x()/width() - m_waterfallRect.left()) / m_waterfallRect.width();
            pWat.ry() = (ep.y()/height() - m_waterfallRect.top()) / m_waterfallRect.height();
            frequency = m_frequencyScale.getRangeMin() + pWat.x()*m_frequencyScale.getRange();
            float time = m_timeScale.getRangeMin() + pWat.y()*m_timeScale.getRange();

            if ((pWat.x() >= 0) && (pWat.x() <= 1) && (pWat.y() >= 0) && (pWat.y() <= 1) && !m_display3DSpectrogram)
            {
                if (m_waterfallMarkers.size() < SpectrumWaterfallMarker::m_maxNbOfMarkers)
                {
                    m_waterfallMarkers.push_back(SpectrumWaterfallMarker());
                    m_waterfallMarkers.back().m_point = pWat;
                    m_waterfallMarkers.back().m_frequency = frequency;
                    m_waterfallMarkers.back().m_frequencyStr = displayScaled(
                        frequency,
                        'f',
                        getPrecision((m_centerFrequency*1000)/m_sampleRate),
                        false);
                    m_waterfallMarkers.back().m_time = time;
                    m_waterfallMarkers.back().m_timeStr = displayScaledF(
                        time,
                        'f',
                        3,
                        true);

                    if (m_waterfallMarkers.size() > 1)
                    {
                        int64_t deltaFrequency = frequency - m_waterfallMarkers.at(0).m_frequency;
                        m_waterfallMarkers.back().m_deltaFrequencyStr = displayScaled(
                            deltaFrequency,
                            'f',
                            getPrecision(deltaFrequency/m_sampleRate),
                            true);
                        m_waterfallMarkers.back().m_deltaTimeStr = displayScaledF(
                            time - m_waterfallMarkers.at(0).m_time,
                            'f',
                            3,
                            true);
                    }

                    if (m_messageQueueToGUI) {
                        m_messageQueueToGUI->push(new MsgReportWaterfallMarkersChange());
                    }
                    doUpdate = true;
                }
            }

            if (doUpdate) {
                update();
            }
        }
        else if (event->modifiers() & Qt::AltModifier)
        {
            frequencyPan(event);
        }
        else if (m_display3DSpectrogram)
        {
            // Detect click and drag to rotate 3D spectrogram
            if (pointInWaterfallOrSpectrogram(ep))
            {
                m_rotate3DSpectrogram = true;
                m_mousePrevLocalPos = ep;
                return;
            }
        }

        if ((m_markersDisplay & SpectrumSettings::MarkersDisplayAnnotations) &&
            (ep.y() <= m_histogramRect.top()*height() + m_annotationMarkerHeight + 2.0f))
        {
            QPointF pHis;
            pHis.rx() = (ep.x()/width() - m_histogramRect.left()) / m_histogramRect.width();
            qint64 selectedFrequency = m_frequencyScale.getRangeMin() + pHis.x() * m_frequencyScale.getRange();
            bool selected = false;

            for (auto iMarker = m_visibleAnnotationMarkers.rbegin(); iMarker != m_visibleAnnotationMarkers.rend(); ++iMarker)
            {
				if ((*iMarker)->m_show == SpectrumAnnotationMarker::Hidden) {
					continue;
				}

                qint64 stopFrequency = (*iMarker)->m_startFrequency +
                    ((*iMarker)->m_bandwidth == 0 ? m_frequencyScale.getRange()*0.01f : (*iMarker)->m_bandwidth);

                if (((*iMarker)->m_startFrequency < selectedFrequency) && (selectedFrequency <= stopFrequency) && !selected)
                {
                    switch ((*iMarker)->m_show)
                    {
                    case SpectrumAnnotationMarker::ShowTop:
                        (*iMarker)->m_show = SpectrumAnnotationMarker::ShowText;
                        break;
                    case SpectrumAnnotationMarker::ShowText:
                        (*iMarker)->m_show = SpectrumAnnotationMarker::ShowFull;
                        break;
                    case SpectrumAnnotationMarker::ShowFull:
                        (*iMarker)->m_show = SpectrumAnnotationMarker::ShowTop;
                        break;
                    case SpectrumAnnotationMarker::Hidden:
                        break;
                    }
                    selected = true;
                }
            }
        }

        if  (m_cursorState == CSSplitter)
        {
            grabMouse();
            m_cursorState = CSSplitterMoving;
            return;
        }
        else if (m_cursorState == CSChannel)
        {
            grabMouse();
            m_cursorState = CSChannelMoving;
            return;
        }
        else if ((m_cursorState == CSNormal) &&
            (m_channelMarkerStates.size() == 1) &&
            !(event->modifiers() & Qt::ShiftModifier) &&
            !(event->modifiers() & Qt::AltModifier) &&
            !(event->modifiers() & Qt::ControlModifier) &&
            (ep.y() > m_histogramRect.top()*height() + m_annotationMarkerHeight + 2.0f)) // out of annotation selection zone
        {
            grabMouse();
            setCursor(Qt::SizeHorCursor);
            m_cursorState = CSChannelMoving;
            m_cursorChannel = 0;
            Real freq = m_frequencyScale.getValueFromPos(event->x() - m_leftMarginPixmap.width() - 1) - m_centerFrequency;

            if (m_channelMarkerStates[m_cursorChannel]->m_channelMarker->getMovable()
                && (m_channelMarkerStates[m_cursorChannel]->m_channelMarker->getSourceOrSinkStream() == m_displaySourceOrSink)
                && m_channelMarkerStates[m_cursorChannel]->m_channelMarker->streamIndexApplies(m_displayStreamIndex))
            {
                m_channelMarkerStates[m_cursorChannel]->m_channelMarker->setCenterFrequencyByCursor(freq);
                channelMarkerChanged();
            }

            return;
        }
    }
}

void GLSpectrumView::mouseReleaseEvent(QMouseEvent*)
{
    m_scrollFrequency = false;
    m_pan3DSpectrogram = false;
    m_rotate3DSpectrogram = false;
    m_scaleZ3DSpectrogram = false;
    if (m_cursorState == CSSplitterMoving)
    {
        releaseMouse();
        m_cursorState = CSSplitter;
    }
    else if (m_cursorState == CSChannelMoving)
    {
        releaseMouse();
        m_cursorState = CSChannel;
    }
}

void GLSpectrumView::wheelEvent(QWheelEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    const QPointF& ep = event->position();
#else
    const QPointF& ep = event->pos();
#endif
    if (m_display3DSpectrogram && pointInWaterfallOrSpectrogram(ep))
    {
        // Scale 3D spectrogram when mouse wheel moved
        // Some mice use delta in steps of 120 for 15 degrees
        // for one step of mouse wheel
        // Other mice/trackpads use smaller values
        int delta = event->angleDelta().y();
        if (delta != 0) {
             m_glShaderSpectrogram.verticalAngle(-5.0*delta/120.0);
        }
        repaint(); // Force repaint in case acquisition is stopped
    }
    else
    {
        if (event->modifiers() & Qt::ShiftModifier) {
            channelMarkerMove(event, 100);
        } else if (event->modifiers() & Qt::ControlModifier) {
            channelMarkerMove(event, 10);
        } else {
            channelMarkerMove(event, 1);
        }
    }
}

void GLSpectrumView::zoom(QWheelEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    const QPointF& p = event->position();
#else
    const QPointF& p = event->pos();
#endif

    float pwx = (p.x() - m_leftMargin) / (width() - m_leftMargin - m_rightMargin); // x position in window

    if ((pwx >= 0.0f) && (pwx <= 1.0f))
    {
        // When we zoom, we want the frequency under the cursor to remain the same

        // Determine frequency at cursor position
        float zoomFreq = m_frequencyScale.getRangeMin() + pwx*m_frequencyScale.getRange();

        // Calculate current centre frequency
        float currentCF = (m_frequencyZoomFactor == 1) ? m_centerFrequency : ((m_frequencyZoomPos - 0.5) * m_sampleRate + m_centerFrequency);

        // Calculate difference from frequency under cursor to centre frequency
        float freqDiff = (currentCF - zoomFreq);

        // Calculate what that difference would be if there was no zoom
        float freqDiffZoom1 = freqDiff * m_frequencyZoomFactor;

        if (event->angleDelta().y() > 0) // zoom in
        {
            if (m_frequencyZoomFactor < m_maxFrequencyZoom) {
                m_frequencyZoomFactor += 0.5f;
            } else {
                return;
            }
        }
        else
        {
            if (m_frequencyZoomFactor > 1.0f) {
                m_frequencyZoomFactor -= 0.5f;
            } else {
                return;
            }
        }

        // Calculate what frequency difference should be at new zoom
        float zoomedFreqDiff = freqDiffZoom1 / m_frequencyZoomFactor;
        // Then calculate what the center frequency should be
        float zoomedCF = zoomFreq + zoomedFreqDiff;

        // Calculate zoom position which will set the desired center frequency
        float zoomPos = (zoomedCF - m_centerFrequency) / m_sampleRate + 0.5;
        zoomPos = std::max(0.0f, zoomPos);
        zoomPos = std::min(1.0f, zoomPos);

        frequencyZoom(zoomPos);
    }
    else
    {
        float pwyh, pwyw;

        if (m_invertedWaterfall) // histo on top
        {
            pwyh = (p.y() - m_topMargin) / m_histogramHeight;
            pwyw = (p.y() - m_topMargin - m_histogramHeight - m_frequencyScaleHeight) / m_waterfallHeight;
        }
        else // waterfall on top
        {
            pwyw = (p.y() - m_topMargin) / m_waterfallHeight;
            pwyh = (p.y() - m_topMargin - m_waterfallHeight - m_frequencyScaleHeight) / m_histogramHeight;
        }

        //qDebug("GLSpectrumView::zoom: pwyh: %f pwyw: %f", pwyh, pwyw);

        if ((pwyw >= 0.0f) && (pwyw <= 1.0f)) {
            timeZoom(event->angleDelta().y() > 0);
        }

        if ((pwyh >= 0.0f) && (pwyh <= 1.0f) && !m_linear) {
            powerZoom(pwyh, event->angleDelta().y() > 0);
        }
    }
}

void GLSpectrumView::frequencyZoom(float zoomPos)
{
    m_frequencyZoomPos = zoomPos;
    updateFFTLimits();
}

void GLSpectrumView::frequencyPan(QMouseEvent *event)
{
    if (m_frequencyZoomFactor == 1.0f) {
        return;
    }

    const QPointF& p = event->pos();
    float pw = (p.x() - m_leftMargin) / (width() - m_leftMargin - m_rightMargin); // position in window
    pw = pw < 0.0f ? 0.0f : pw > 1.0f ? 1.0 : pw;
    float dw = pw - 0.5f;
    m_frequencyZoomPos += dw * (1.0f / m_frequencyZoomFactor);
    float lim = 0.5f / m_frequencyZoomFactor;
    m_frequencyZoomPos = m_frequencyZoomPos < lim ? lim : m_frequencyZoomPos > 1 - lim ? 1 - lim : m_frequencyZoomPos;

    qDebug("GLSpectrumView::frequencyPan: pw: %f p: %f", pw, m_frequencyZoomPos);
    updateFFTLimits();
}

void GLSpectrumView::timeZoom(bool zoomInElseOut)
{
    if ((m_fftOverlap  == 0) && !zoomInElseOut) {
        return;
    }

    if (zoomInElseOut && (m_fftOverlap == m_fftSize/2 - 1)) {
        return;
    }

    m_fftOverlap = m_fftOverlap + (zoomInElseOut ? 1 : -1);
    m_changesPending = true;

    if (m_messageQueueToGUI) {
        m_messageQueueToGUI->push(new MsgReportFFTOverlap(m_fftOverlap));
    }
}

void GLSpectrumView::powerZoom(float pw, bool zoomInElseOut)
{
    m_powerRange = m_powerRange + (zoomInElseOut ? -2 : 2);

    if (pw > 2.0/3.0) { // bottom
        m_referenceLevel = m_referenceLevel + (zoomInElseOut ? -2 : 2);
    } else if (pw > 1.0/3.0) { // middle
        m_referenceLevel = m_referenceLevel + (zoomInElseOut ? -1 : 1);
    } // top

    m_powerRange = m_powerRange < 1 ? 1 : m_powerRange > 100 ? 100 : m_powerRange;
    m_referenceLevel = m_referenceLevel < -110 ? -110 : m_referenceLevel > 0 ? 0 : m_referenceLevel;
    m_changesPending = true;

    if (m_messageQueueToGUI) {
        m_messageQueueToGUI->push(new MsgReportPowerScale(m_referenceLevel, m_powerRange));
    }
}

void GLSpectrumView::resetFrequencyZoom()
{
    m_frequencyZoomFactor = 1.0f;
    m_frequencyZoomPos = 0.5f;

    updateFFTLimits();
}

void GLSpectrumView::updateFFTLimits()
{
    if (!m_spectrumVis) {
        return;
    }

    SpectrumVis::MsgFrequencyZooming *msg = SpectrumVis::MsgFrequencyZooming::create(
        m_frequencyZoomFactor, m_frequencyZoomPos
    );

    m_spectrumVis->getInputMessageQueue()->push(msg);
    m_changesPending = true;
}

void GLSpectrumView::setFrequencyScale()
{
    int frequencySpan;
    int64_t centerFrequency;

    getFrequencyZoom(centerFrequency, frequencySpan);
    m_frequencyScale.setSize(width() - m_leftMargin - m_rightMargin);
    m_frequencyScale.setRange(Unit::Frequency, centerFrequency - frequencySpan / 2.0, centerFrequency + frequencySpan / 2.0);
    m_frequencyScale.setMakeOpposite(m_lsbDisplay);
}

void GLSpectrumView::setPowerScale(int height)
{
    m_powerScale.setSize(height);

    if (m_linear)
    {
        Real referenceLevel = m_useCalibration ? m_referenceLevel * m_calibrationGain : m_referenceLevel;
        m_powerScale.setRange(Unit::Scientific, 0.0f, referenceLevel);
    }
    else
    {
        Real referenceLevel = m_useCalibration ? m_referenceLevel + m_calibrationShiftdB : m_referenceLevel;
        m_powerScale.setRange(Unit::Decibel, referenceLevel - m_powerRange, referenceLevel);
    }

    if (m_powerScale.getScaleWidth() > m_leftMargin) {
        m_leftMargin = m_powerScale.getScaleWidth();
    }
}

void GLSpectrumView::getFrequencyZoom(int64_t& centerFrequency, int& frequencySpan)
{
    frequencySpan = (m_frequencyZoomFactor == 1) ?
        m_sampleRate : m_sampleRate * (1.0 / m_frequencyZoomFactor);
    centerFrequency = (m_frequencyZoomFactor == 1) ?
        m_centerFrequency : (m_frequencyZoomPos - 0.5) * m_sampleRate + m_centerFrequency;
}

// void GLSpectrumView::updateFFTLimits()
// {
// 	m_fftMin = m_frequencyZoomFactor == 1 ? 0 : (m_frequencyZoomPos - (0.5f / m_frequencyZoomFactor)) * m_fftSize;
// 	m_fftMax = m_frequencyZoomFactor == 1 ? m_fftSize : (m_frequencyZoomPos - (0.5f / m_frequencyZoomFactor)) * m_fftSize;
// }

void GLSpectrumView::channelMarkerMove(QWheelEvent *event, int mul)
{
    for (int i = 0; i < m_channelMarkerStates.size(); ++i)
    {
        if ((m_channelMarkerStates[i]->m_channelMarker->getSourceOrSinkStream() != m_displaySourceOrSink)
            || !m_channelMarkerStates[i]->m_channelMarker->streamIndexApplies(m_displayStreamIndex))
        {
            continue;
        }

        if (m_channelMarkerStates[i]->m_rect.contains(event->position()))
        {
            int freq = m_channelMarkerStates[i]->m_channelMarker->getCenterFrequency();

            if (event->angleDelta().y() > 0) {
                freq += 10 * mul;
            } else if (event->angleDelta().y() < 0) {
                freq -= 10 * mul;
            }

            // calculate scale relative cursor position for new frequency
            float x_pos = m_frequencyScale.getPosFromValue(m_centerFrequency + freq);

            if ((x_pos >= 0.0) && (x_pos < m_frequencyScale.getSize())) // cursor must be in scale
            {
                m_channelMarkerStates[i]->m_channelMarker->setCenterFrequencyByCursor(freq);
                m_channelMarkerStates[i]->m_channelMarker->setCenterFrequency(freq);

                // cursor follow-up
                int xd = x_pos + m_leftMargin;
                QCursor c = cursor();
                QPoint cp_a = c.pos();
                QPoint cp_w = mapFromGlobal(cp_a);
                cp_w.setX(xd);
                cp_a = mapToGlobal(cp_w);
                c.setPos(cp_a);
                setCursor(c);
            }

            return;
        }
    }

    zoom(event);
}

// Return if specified point is within the bounds of the waterfall / 3D spectrogram screen area
bool GLSpectrumView::pointInWaterfallOrSpectrogram(const QPointF &point) const
{
    // m_waterfallRect is normalised to [0,1]
    QPointF pWat = point;
    pWat.rx() = (point.x()/width() - m_waterfallRect.left()) / m_waterfallRect.width();
    pWat.ry() = (point.y()/height() - m_waterfallRect.top()) / m_waterfallRect.height();

    return (pWat.x() >= 0) && (pWat.x() <= 1) && (pWat.y() >= 0) && (pWat.y() <= 1);
}

// Return if specified point is within the bounds of the histogram screen area
bool GLSpectrumView::pointInHistogram(const QPointF &point) const
{
    // m_histogramRect is normalised to [0,1]
    QPointF p = point;
    p.rx() = (point.x()/width() - m_histogramRect.left()) / m_histogramRect.width();
    p.ry() = (point.y()/height() - m_histogramRect.top()) / m_histogramRect.height();

    return (p.x() >= 0) && (p.x() <= 1) && (p.y() >= 0) && (p.y() <= 1);
}

void GLSpectrumView::enterEvent(EnterEventType* event)
{
    m_mouseInside = true;
    update();
    QOpenGLWidget::enterEvent(event);
}

void GLSpectrumView::leaveEvent(QEvent* event)
{
    m_mouseInside = false;
    update();
    QOpenGLWidget::leaveEvent(event);
}

void GLSpectrumView::tick()
{
    if (m_displayChanged)
    {
        m_displayChanged = false;
        update();
    }
}

void GLSpectrumView::channelMarkerChanged()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_changesPending = true;
    update();
}

void GLSpectrumView::channelMarkerDestroyed(QObject* object)
{
    removeChannelMarker((ChannelMarker*)object);
}

void GLSpectrumView::setWaterfallShare(Real waterfallShare)
{
    QMutexLocker mutexLocker(&m_mutex);

    if (waterfallShare < 0.1f) {
        m_waterfallShare = 0.1f;
    } else if (waterfallShare > 0.8f) {
        m_waterfallShare = 0.8f;
    } else {
        m_waterfallShare = waterfallShare;
    }

    m_changesPending = true;
}

void GLSpectrumView::setFPSPeriodMs(int fpsPeriodMs)
{
    if (fpsPeriodMs == 0)
    {
        disconnect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
        m_timer.stop();
    }
    else
    {
        connect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
        m_timer.start(fpsPeriodMs);
    }

    m_fpsPeriodMs = fpsPeriodMs;
}

void GLSpectrumView::cleanup()
{
    //makeCurrent();
    m_glShaderSimple.cleanup();
    m_glShaderFrequencyScale.cleanup();
    m_glShaderHistogram.cleanup();
    m_glShaderLeftScale.cleanup();
    m_glShaderWaterfall.cleanup();
    m_glShaderTextOverlay.cleanup();
    m_glShaderInfo.cleanup();
    m_glShaderSpectrogram.cleanup();
    m_glShaderSpectrogramTimeScale.cleanup();
    m_glShaderSpectrogramPowerScale.cleanup();
    //doneCurrent();
}

// Display number with full precision, group separators and eng. unit suffixes
// E.g:
// -1.505,123,304G
//    456.034,123M
//        300.345k
//            789
QString GLSpectrumView::displayFull(int64_t value)
{
    if (value == 0) {
        return "0";
    }
    int64_t absValue = std::abs(value);

    QString digits = QString::number(absValue);
    int cnt = digits.size();

    QString point = QLocale::system().decimalPoint();
    QString group = QLocale::system().groupSeparator();
    int i;
    for (i = cnt - 3; i >= 4; i -= 3)
    {
        digits = digits.insert(i, group);
    }
    if (absValue >= 1000) {
        digits = digits.insert(i, point);
    }
    if (cnt > 9) {
        digits = digits.append("G");
    } else if (cnt > 6) {
        digits = digits.append("M");
    } else if (cnt > 3) {
        digits = digits.append("k");
    }
    if (value < 0) {
        digits = digits.insert(0, "-");
    }

    return digits;
}

QString GLSpectrumView::displayScaled(int64_t value, char type, int precision, bool showMult)
{
    int64_t posValue = (value < 0) ? -value : value;

    if (posValue < 1000) {
        return tr("%1").arg(QString::number(value, type, precision));
    } else if (posValue < 1000000) {
        return tr("%1%2").arg(QString::number(value / 1000.0, type, precision)).arg(showMult ? "k" : "");
    } else if (posValue < 1000000000) {
        return tr("%1%2").arg(QString::number(value / 1000000.0, type, precision)).arg(showMult ? "M" : "");
    } else if (posValue < 1000000000000) {
        return tr("%1%2").arg(QString::number(value / 1000000000.0, type, precision)).arg(showMult ? "G" : "");
    } else {
        return tr("%1").arg(QString::number(value, 'e', precision));
    }
}

QString GLSpectrumView::displayPower(float value, char type, int precision)
{
    return tr("%1").arg(QString::number(value, type, precision));
}

QString GLSpectrumView::displayScaledF(float value, char type, int precision, bool showMult)
{
    float posValue = (value < 0) ? -value : value;

    if (posValue == 0)
    {
        return tr("%1").arg(QString::number(value, 'f', precision));
    }
    else if (posValue < 1)
    {
        if (posValue > 0.001) {
            return tr("%1%2").arg(QString::number(value * 1000.0, type, precision)).arg(showMult ? "m" : "");
        } else if (posValue > 0.000001) {
            return tr("%1%2").arg(QString::number(value * 1000000.0, type, precision)).arg(showMult ? "u" : "");
        } else if (posValue > 1e-9) {
            return tr("%1%2").arg(QString::number(value * 1e9, type, precision)).arg(showMult ? "n" : "");
        } else if (posValue > 1e-12) {
            return tr("%1%2").arg(QString::number(value * 1e12, type, precision)).arg(showMult ? "p" : "");
        } else {
            return tr("%1").arg(QString::number(value, 'e', precision));
        }
    }
    else
    {
        if (posValue < 1e3) {
            return tr("%1").arg(QString::number(value, type, precision));
        } else if (posValue < 1e6) {
            return tr("%1%2").arg(QString::number(value / 1000.0, type, precision)).arg(showMult ? "k" : "");
        } else if (posValue < 1e9) {
            return tr("%1%2").arg(QString::number(value / 1000000.0, type, precision)).arg(showMult ? "M" : "");
        } else if (posValue < 1e12) {
            return tr("%1%2").arg(QString::number(value / 1000000000.0, type, precision)).arg(showMult ? "G" : "");
        } else {
            return tr("%1").arg(QString::number(value, 'e', precision));
        }
    }
}

int GLSpectrumView::getPrecision(int value)
{
    int posValue = (value < 0) ? -value : value;

    if (posValue < 1000) {
        return 3;
    } else if (posValue < 10000) {
        return 4;
    } else if (posValue < 100000) {
        return 5;
    } else {
        return 6;
    }
}

// Draw text right justified in top info bar - currently unused
void GLSpectrumView::drawTextRight(const QString &text, const QString &value, const QString &max, const QString &units)
{
    drawTextsRight({text}, {value}, {max}, {units});
}

void GLSpectrumView::drawTextsRight(const QStringList &text, const QStringList &value, const QStringList &max, const QStringList &units)
{
    QFontMetrics fm(font());

    m_infoPixmap.fill(Qt::transparent);

    QPainter painter(&m_infoPixmap);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    painter.setBrush(Qt::transparent);
    painter.drawRect(m_leftMargin, 0, width() - m_leftMargin, m_infoHeight);
    painter.setPen(QColor(0xf0, 0xf0, 0xff));
    painter.setFont(font());

    int x = width() - m_rightMargin;
    int y = fm.height() + fm.ascent() / 2 - 2;
    int textWidth, maxWidth;
    for (int i = text.length() - 1; i >= 0; i--)
    {
        textWidth = fm.horizontalAdvance(units[i]);
        painter.drawText(QPointF(x - textWidth, y), units[i]);
        x -= textWidth;

        textWidth = fm.horizontalAdvance(value[i]);
        maxWidth = fm.horizontalAdvance(max[i]);
        painter.drawText(QPointF(x - textWidth, y), value[i]);
        x -= maxWidth;

        textWidth = fm.horizontalAdvance(text[i]);
        painter.drawText(QPointF(x - textWidth, y), text[i]);
        x -= textWidth;
    }

    m_glShaderTextOverlay.initTexture(m_infoPixmap.toImage());

    GLfloat vtx1[] = {
            0, 1,
            1, 1,
            1, 0,
            0, 0
    };
    GLfloat tex1[] = {
            0, 1,
            1, 1,
            1, 0,
            0, 0
    };

    m_glShaderTextOverlay.drawSurface(m_glInfoBoxMatrix, tex1, vtx1, 4);
}

void GLSpectrumView::drawTextOverlayCentered (
    const QString &text,
    const QColor &color,
    const QFont& font,
    float shiftX,
    float shiftY,
    const QRectF &glRect)
{
    if (text.isEmpty()) {
        return;
    }

    QFontMetricsF metrics(font);
    QRectF textRect = metrics.boundingRect(text);
    QRectF overlayRect(0, 0, textRect.width() * 1.05f + 4.0f, textRect.height());
    QPixmap channelOverlayPixmap = QPixmap(overlayRect.width(), overlayRect.height());
    channelOverlayPixmap.fill(Qt::transparent);
    QPainter painter(&channelOverlayPixmap);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, false);
    painter.fillRect(overlayRect, QColor(0, 0, 0, 0x80));
    QColor textColor(color);
    textColor.setAlpha(0xC0);
    painter.setPen(textColor);
    painter.setFont(font);
    painter.drawText(QPointF(2.0f, overlayRect.height() - 4.0f), text);
    painter.end();

    m_glShaderTextOverlay.initTexture(channelOverlayPixmap.toImage());

    {
        GLfloat vtx1[] = {
            0, 1,
            1, 1,
            1, 0,
            0, 0};
        GLfloat tex1[] = {
            0, 1,
            1, 1,
            1, 0,
            0, 0};

        float rectX = glRect.x() + shiftX - ((overlayRect.width()/2)/width());
        float rectY = glRect.y() + shiftY + (4.0f / height()) - ((overlayRect.height()+5)/height());
        float rectW = overlayRect.width() / (float) width();
        float rectH = overlayRect.height() / (float) height();

        QMatrix4x4 mat;
        mat.setToIdentity();
        mat.translate(-1.0f + 2.0f * rectX, 1.0f - 2.0f * rectY);
        mat.scale(2.0f * rectW, -2.0f * rectH);
        m_glShaderTextOverlay.drawSurface(mat, tex1, vtx1, 4);
    }
}

void GLSpectrumView::drawTextOverlay(
    const QString &text,
    const QColor &color,
    const QFont& font,
    float shiftX,
    float shiftY,
    bool leftHalf,
    bool topHalf,
    const QRectF &glRect)
{
    if (text.isEmpty()) {
        return;
    }

    QFontMetricsF metrics(font);
    QRectF textRect = metrics.boundingRect(text);
    QRectF overlayRect(0, 0, textRect.width() * 1.05f + 4.0f, textRect.height());
    QPixmap channelOverlayPixmap = QPixmap(overlayRect.width(), overlayRect.height());
    channelOverlayPixmap.fill(Qt::transparent);
    QPainter painter(&channelOverlayPixmap);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, false);
    painter.fillRect(overlayRect, QColor(0, 0, 0, 0x80));
    QColor textColor(color);
    textColor.setAlpha(0xC0);
    painter.setPen(textColor);
    painter.setFont(font);
    painter.drawText(QPointF(2.0f, overlayRect.height() - 4.0f), text);
    painter.end();

    m_glShaderTextOverlay.initTexture(channelOverlayPixmap.toImage());

    {
        GLfloat vtx1[] = {
            0, 1,
            1, 1,
            1, 0,
            0, 0};
        GLfloat tex1[] = {
            0, 1,
            1, 1,
            1, 0,
            0, 0};

        // float shiftX = glRect.width() - ((overlayRect.width() + 4.0f) / width());
        // float shiftY = 4.0f / height();
        float rectX = glRect.x() + shiftX - (leftHalf ? 0 : (overlayRect.width()+1)/width());
        float rectY = glRect.y() + shiftY + (4.0f / height()) - (topHalf ? 0 : (overlayRect.height()+5)/height());
        float rectW = overlayRect.width() / (float) width();
        float rectH = overlayRect.height() / (float) height();

        QMatrix4x4 mat;
        mat.setToIdentity();
        mat.translate(-1.0f + 2.0f * rectX, 1.0f - 2.0f * rectY);
        mat.scale(2.0f * rectW, -2.0f * rectH);
        m_glShaderTextOverlay.drawSurface(mat, tex1, vtx1, 4);
    }
}

void GLSpectrumView::formatTextInfo(QString& info)
{
    if (m_useCalibration) {
        info.append(tr("CAL:%1dB ").arg(QString::number(m_calibrationShiftdB, 'f', 1)));
    }

    if (m_frequencyZoomFactor != 1.0f) {
        info.append(tr("%1x ").arg(QString::number(m_frequencyZoomFactor, 'f', 1)));
    }

    if (m_sampleRate == 0)
    {
        info.append(tr("CF:%1 SP:%2").arg(m_centerFrequency).arg(m_sampleRate));
    }
    else
    {
        int64_t centerFrequency;
        int frequencySpan;
        getFrequencyZoom(centerFrequency, frequencySpan);
        info.append(tr("CF:%1 ").arg(displayScaled(centerFrequency, 'f', getPrecision(centerFrequency/frequencySpan), true)));
        info.append(tr("SP:%1 ").arg(displayScaled(frequencySpan, 'f', 3, true)));
    }
}

bool GLSpectrumView::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_Up:
            if (keyEvent->modifiers() & Qt::ShiftModifier) {
                m_glShaderSpectrogram.lightRotateX(-5.0f);
            } else if (keyEvent->modifiers() & Qt::AltModifier) {
                m_glShaderSpectrogram.lightTranslateY(0.05);
            } else if (keyEvent->modifiers() & Qt::ControlModifier) {
                m_glShaderSpectrogram.translateY(0.05);
            } else {
                m_glShaderSpectrogram.rotateX(-5.0f);
            }
            break;
        case Qt::Key_Down:
            if (keyEvent->modifiers() & Qt::ShiftModifier) {
                m_glShaderSpectrogram.lightRotateX(5.0f);
            } else if (keyEvent->modifiers() & Qt::AltModifier) {
                m_glShaderSpectrogram.lightTranslateY(-0.05);
            } else if (keyEvent->modifiers() & Qt::ControlModifier) {
                m_glShaderSpectrogram.translateY(-0.05);
            } else {
                m_glShaderSpectrogram.rotateX(5.0f);
            }
            break;
        case Qt::Key_Left:
            if (keyEvent->modifiers() & Qt::ShiftModifier) {
                m_glShaderSpectrogram.lightRotateZ(5.0f);
            } else if (keyEvent->modifiers() & Qt::AltModifier) {
                m_glShaderSpectrogram.lightTranslateX(-0.05);
            } else if (keyEvent->modifiers() & Qt::ControlModifier) {
                m_glShaderSpectrogram.translateX(-0.05);
            } else {
                m_glShaderSpectrogram.rotateZ(5.0f);
            }
            break;
        case Qt::Key_Right:
            if (keyEvent->modifiers() & Qt::ShiftModifier) {
                m_glShaderSpectrogram.lightRotateZ(-5.0f);
            } else if (keyEvent->modifiers() & Qt::AltModifier) {
                m_glShaderSpectrogram.lightTranslateX(0.05);
            } else if (keyEvent->modifiers() & Qt::ControlModifier) {
                m_glShaderSpectrogram.translateX(0.05);
            } else {
                m_glShaderSpectrogram.rotateZ(-5.0f);
            }
            break;
        case Qt::Key_Equal: // So you don't need to press shift
        case Qt::Key_Plus:
            if (keyEvent->modifiers() & Qt::ControlModifier) {
                m_glShaderSpectrogram.userScaleZ(1.1f);
            } else {
                m_glShaderSpectrogram.verticalAngle(-1.0f);
            }
            break;
        case Qt::Key_Minus:
            if (keyEvent->modifiers() & Qt::ControlModifier) {
                m_glShaderSpectrogram.userScaleZ(0.9f);
            } else {
                m_glShaderSpectrogram.verticalAngle(1.0f);
            }
            break;
        case Qt::Key_R:
            m_glShaderSpectrogram.reset();
            break;
        case Qt::Key_F:
            // Project straight down and scale to view, so it's a bit like 2D
            m_glShaderSpectrogram.reset();
            m_glShaderSpectrogram.rotateX(45.0f);
            m_glShaderSpectrogram.verticalAngle(-9.0f);
            m_glShaderSpectrogram.userScaleZ(0.0f);
            break;
        }
        repaint(); // Force repaint in case acquisition is stopped
        return true;
    }
    else
    {
        return QOpenGLWidget::eventFilter(object, event);
    }
}
