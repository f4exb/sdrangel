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

#if 0 //def USE_SSE2
#include <emmintrin.h>
#endif

#include <QMouseEvent>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions>
#include <QPainter>
#include <QFontDatabase>
#include "dsp/spectrumvis.h"
#include "gui/glspectrum.h"
#include "util/messagequeue.h"
#include "util/db.h"

#include <QDebug>

MESSAGE_CLASS_DEFINITION(GLSpectrum::MsgReportSampleRate, Message)
MESSAGE_CLASS_DEFINITION(GLSpectrum::MsgReportWaterfallShare, Message)
MESSAGE_CLASS_DEFINITION(GLSpectrum::MsgReportFFTOverlap, Message)
MESSAGE_CLASS_DEFINITION(GLSpectrum::MsgReportPowerScale, Message)

const float GLSpectrum::m_maxFrequencyZoom = 10.0f;

GLSpectrum::GLSpectrum(QWidget* parent) :
	QGLWidget(parent),
	m_markersDisplay(SpectrumSettings::MarkersDisplaySpectrum),
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
	m_invertedWaterfall(false),
	m_displayMaxHold(false),
	m_currentSpectrum(nullptr),
	m_displayCurrent(false),
    m_leftMargin(0),
    m_rightMargin(0),
    m_topMargin(0),
    m_frequencyScaleHeight(0),
    m_histogramHeight(20),
    m_waterfallHeight(0),
    m_bottomMargin(0),
	m_waterfallBuffer(nullptr),
	m_waterfallBufferPos(0),
    m_waterfallTextureHeight(-1),
    m_waterfallTexturePos(0),
    m_displayWaterfall(true),
    m_ssbSpectrum(false),
    m_lsbDisplay(false),
    m_histogramBuffer(nullptr),
    m_histogram(nullptr),
    m_displayHistogram(true),
    m_displayChanged(false),
    m_displaySourceOrSink(true),
    m_displayStreamIndex(0),
    m_matrixLoc(0),
    m_colorLoc(0),
    m_messageQueueToGUI(nullptr)
{
	setAutoFillBackground(false);
	setAttribute(Qt::WA_OpaquePaintEvent, true);
	setAttribute(Qt::WA_NoSystemBackground, true);
	setMouseTracking(true);

	setMinimumSize(200, 200);

	m_waterfallShare = 0.66;

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
}

GLSpectrum::~GLSpectrum()
{
	QMutexLocker mutexLocker(&m_mutex);

	if (m_waterfallBuffer)
    {
		delete m_waterfallBuffer;
		m_waterfallBuffer = nullptr;
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
}

void GLSpectrum::setCenterFrequency(qint64 frequency)
{
	m_mutex.lock();
	m_centerFrequency = frequency;
	m_changesPending = true;
	m_mutex.unlock();
	update();
}

void GLSpectrum::setReferenceLevel(Real referenceLevel)
{
	m_mutex.lock();
	m_referenceLevel = referenceLevel;
	m_changesPending = true;
	m_mutex.unlock();
	update();
}

void GLSpectrum::setPowerRange(Real powerRange)
{
	m_mutex.lock();
	m_powerRange = powerRange;
	m_changesPending = true;
	m_mutex.unlock();
	update();
}

void GLSpectrum::setDecay(int decay)
{
	m_decay = decay < 0 ? 0 : decay > 20 ? 20 : decay;
}

void GLSpectrum::setDecayDivisor(int decayDivisor)
{
	m_decayDivisor = decayDivisor < 1 ? 1 : decayDivisor > 20 ? 20 : decayDivisor;
}

void GLSpectrum::setHistoStroke(int stroke)
{
	m_histogramStroke = stroke < 1 ? 1 : stroke > 60 ? 60 : stroke;
}

void GLSpectrum::setSampleRate(qint32 sampleRate)
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

void GLSpectrum::setTimingRate(qint32 timingRate)
{
    m_mutex.lock();
    m_timingRate = timingRate;
    m_changesPending = true;
	m_mutex.unlock();
    update();
}

void GLSpectrum::setFFTOverlap(int overlap)
{
    m_mutex.lock();
    m_fftOverlap = overlap;
    m_changesPending = true;
	m_mutex.unlock();
    update();
}

void GLSpectrum::setDisplayWaterfall(bool display)
{
    m_mutex.lock();
	m_displayWaterfall = display;
    if (!display) {
        m_waterfallMarkers.clear();
    }
	m_changesPending = true;
	stopDrag();
	m_mutex.unlock();
	update();
}

void GLSpectrum::setSsbSpectrum(bool ssbSpectrum)
{
	m_ssbSpectrum = ssbSpectrum;
	update();
}

void GLSpectrum::setLsbDisplay(bool lsbDisplay)
{
    m_lsbDisplay = lsbDisplay;
    update();
}

void GLSpectrum::setInvertedWaterfall(bool inv)
{
    m_mutex.lock();
	m_invertedWaterfall = inv;
	m_changesPending = true;
	stopDrag();
	m_mutex.unlock();
	update();
}

void GLSpectrum::setDisplayMaxHold(bool display)
{
    m_mutex.lock();
	m_displayMaxHold = display;
    if (!m_displayMaxHold && !m_displayCurrent && !m_displayHistogram) {
        m_histogramMarkers.clear();
    }
	m_changesPending = true;
	stopDrag();
	m_mutex.unlock();
	update();
}

void GLSpectrum::setDisplayCurrent(bool display)
{
    m_mutex.lock();
	m_displayCurrent = display;
    if (!m_displayMaxHold && !m_displayCurrent && !m_displayHistogram) {
        m_histogramMarkers.clear();
    }
	m_changesPending = true;
	stopDrag();
	m_mutex.unlock();
	update();
}

void GLSpectrum::setDisplayHistogram(bool display)
{
    m_mutex.lock();
	m_displayHistogram = display;
    if (!m_displayMaxHold && !m_displayCurrent && !m_displayHistogram) {
        m_histogramMarkers.clear();
    }
	m_changesPending = true;
	stopDrag();
	m_mutex.unlock();
	update();
}

void GLSpectrum::setDisplayGrid(bool display)
{
	m_displayGrid = display;
	update();
}

void GLSpectrum::setDisplayGridIntensity(int intensity)
{
	m_displayGridIntensity = intensity;

	if (m_displayGridIntensity > 100) {
		m_displayGridIntensity = 100;
	} else if (m_displayGridIntensity < 0) {
		m_displayGridIntensity = 0;
	}

    update();
}

void GLSpectrum::setDisplayTraceIntensity(int intensity)
{
	m_displayTraceIntensity = intensity;

	if (m_displayTraceIntensity > 100) {
		m_displayTraceIntensity = 100;
	} else if (m_displayTraceIntensity < 0) {
		m_displayTraceIntensity = 0;
	}

    update();
}

void GLSpectrum::setLinear(bool linear)
{
	m_mutex.lock();
    m_linear = linear;
    m_changesPending = true;
	m_mutex.unlock();
    update();
}

void GLSpectrum::addChannelMarker(ChannelMarker* channelMarker)
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

void GLSpectrum::removeChannelMarker(ChannelMarker* channelMarker)
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

void GLSpectrum::setHistogramMarkers(const QList<SpectrumHistogramMarker>& histogramMarkers)
{
    m_mutex.lock();
    m_histogramMarkers = histogramMarkers;
	updateHistogramMarkers();
    m_changesPending = true;
    m_mutex.unlock();
	update();
}

void GLSpectrum::setWaterfallMarkers(const QList<SpectrumWaterfallMarker>& waterfallMarkers)
{
    m_mutex.lock();
    m_waterfallMarkers = waterfallMarkers;
	updateWaterfallMarkers();
    m_changesPending = true;
    m_mutex.unlock();
	update();
}

float GLSpectrum::getPowerMax() const
{
	return m_linear ? m_powerScale.getRangeMax() : CalcDb::powerFromdB(m_powerScale.getRangeMax());
}

float GLSpectrum::getTimeMax() const
{
	return m_timeScale.getRangeMax();
}

void GLSpectrum::newSpectrum(const Real *spectrum, int nbBins, int fftSize)
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
	updateHistogram(spectrum);
}

void GLSpectrum::updateWaterfall(const Real *spectrum)
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

void GLSpectrum::updateHistogram(const Real *spectrum)
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

void GLSpectrum::initializeGL()
{
	QOpenGLContext *glCurrentContext =  QOpenGLContext::currentContext();

	if (glCurrentContext)
    {
		if (QOpenGLContext::currentContext()->isValid()) {
			qDebug() << "GLSpectrum::initializeGL: context:"
				<< " major: " << (QOpenGLContext::currentContext()->format()).majorVersion()
				<< " minor: " << (QOpenGLContext::currentContext()->format()).minorVersion()
				<< " ES: " << (QOpenGLContext::currentContext()->isOpenGLES() ? "yes" : "no");
		}
		else {
			qDebug() << "GLSpectrum::initializeGL: current context is invalid";
		}
	}
    else
    {
		qCritical() << "GLSpectrum::initializeGL: no current context";
		return;
	}

	connect(glCurrentContext, &QOpenGLContext::aboutToBeDestroyed, this, &GLSpectrum::cleanup); // TODO: when migrating to QOpenGLWidget

	QOpenGLFunctions *glFunctions = QOpenGLContext::currentContext()->functions();
	glFunctions->initializeOpenGLFunctions();

	//glDisable(GL_DEPTH_TEST);
	m_glShaderSimple.initializeGL();
	m_glShaderLeftScale.initializeGL();
	m_glShaderFrequencyScale.initializeGL();
	m_glShaderWaterfall.initializeGL();
	m_glShaderHistogram.initializeGL();
    m_glShaderTextOverlay.initializeGL();
	m_glShaderInfo.initializeGL();
}

void GLSpectrum::resizeGL(int width, int height)
{
    QMutexLocker mutexLocker(&m_mutex);
	QOpenGLFunctions *glFunctions = QOpenGLContext::currentContext()->functions();
	glFunctions->glViewport(0, 0, width, height);
	m_changesPending = true;
}

void GLSpectrum::clearSpectrumHistogram()
{
	if (!m_mutex.tryLock(2)) {
		return;
	}

	memset(m_histogram, 0x00, 100 * m_nbBins);

	m_mutex.unlock();
	update();
}

void GLSpectrum::paintGL()
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
	glFunctions->glClear(GL_COLOR_BUFFER_BIT);

	// paint waterfall
	if (m_displayWaterfall)
	{
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

		// draw rect around
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

			QVector4D color(1.0f, 0.0f, 0.0f, (float) m_displayTraceIntensity / 100.0f);
			m_glShaderSimple.drawPolyline(m_glHistogramSpectrumMatrix, color, q3, m_nbBins);
		}
	}

	// paint current spectrum line on top of histogram
	if ((m_displayCurrent) && m_currentSpectrum)
	{
		{
			Real bottom = -m_powerRange;
			GLfloat *q3 = m_q3FFT.m_array;

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
			}

			QVector4D color(1.0f, 1.0f, 0.25f, (float) m_displayTraceIntensity / 100.0f);
			m_glShaderSimple.drawPolyline(m_glHistogramSpectrumMatrix, color, q3, m_nbBins);
		}
	}

	if (m_markersDisplay == SpectrumSettings::MarkersDisplaySpectrum) {
	    drawSpectrumMarkers();
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

	m_mutex.unlock();
}

void GLSpectrum::drawSpectrumMarkers()
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
				float power = m_currentSpectrum[m_histogramMarkers.at(i).m_fftBin];
				ypoint.ry() =
					(m_powerScale.getRangeMax() - power) / m_powerScale.getRange();
				ypoint.ry() = ypoint.ry() < 0 ?
					0 : ypoint.ry() > 1 ?
						1 : ypoint.ry();
				powerStr = displayScaledF(
					power,
					m_linear ? 'e' : 'f',
					m_linear ? 3 : 1,
					false
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

				ypoint.ry() =
					(m_powerScale.getRangeMax() - m_histogramMarkers[i].m_powerMax) / m_powerScale.getRange();
				ypoint.ry() = ypoint.ry() < 0 ?
					0 : ypoint.ry() > 1 ?
						1 : ypoint.ry();
				powerStr = displayScaledF(
					m_histogramMarkers[i].m_powerMax,
					m_linear ? 'e' : 'f',
					m_linear ? 3 : 1,
					false
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
					deltaPowerStr = displayScaledF(poweri - power0, 'e', 3, false);
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

void GLSpectrum::stopDrag()
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

void GLSpectrum::applyChanges()
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
	if (m_displayWaterfall && (m_displayHistogram | m_displayMaxHold | m_displayCurrent))
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

		m_powerScale.setSize(m_histogramHeight);

		if (m_linear) {
            m_powerScale.setRange(Unit::Scientific, m_referenceLevel - m_powerRange, m_referenceLevel);
		} else {
		    m_powerScale.setRange(Unit::Decibel, m_referenceLevel - m_powerRange, m_referenceLevel);
		}

		m_leftMargin = m_timeScale.getScaleWidth();

		if (m_powerScale.getScaleWidth() > m_leftMargin) {
			m_leftMargin = m_powerScale.getScaleWidth();
		}

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
			((float) 2 * (width() - m_leftMargin - m_rightMargin)) / ((float) width() * (float)(m_nbBins - 1)),
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
	// displays waterfall only
	else if (m_displayWaterfall)
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

		m_powerScale.setSize(m_histogramHeight);
		m_powerScale.setRange(Unit::Decibel, m_referenceLevel - m_powerRange, m_referenceLevel);
		m_leftMargin = m_powerScale.getScaleWidth();
		m_leftMargin += 2 * M;

		setFrequencyScale();

		m_glHistogramSpectrumMatrix.setToIdentity();
		m_glHistogramSpectrumMatrix.translate(
			-1.0f + ((float)(2*m_leftMargin)   / (float) width()),
			 1.0f - ((float)(2*histogramTop) / (float) height())
		);
		m_glHistogramSpectrumMatrix.scale(
			((float) 2 * (width() - m_leftMargin - m_rightMargin)) / ((float) width() * (float)(m_nbBins - 1)),
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
		m_leftMarginPixmap.fill(Qt::black);
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
	if (m_displayWaterfall || m_displayHistogram || m_displayMaxHold || m_displayCurrent){
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

	bool fftSizeChanged = true;

	if (m_waterfallBuffer) {
		fftSizeChanged = m_waterfallBuffer->width() != m_nbBins;
	}

	bool windowSizeChanged = m_waterfallTextureHeight != m_waterfallHeight;

	if (fftSizeChanged || windowSizeChanged)
	{
		if (m_waterfallBuffer) {
			delete m_waterfallBuffer;
		}

		m_waterfallBuffer = new QImage(m_nbBins, m_waterfallHeight, QImage::Format_ARGB32);

        m_waterfallBuffer->fill(qRgb(0x00, 0x00, 0x00));
        m_glShaderWaterfall.initTexture(*m_waterfallBuffer);
        m_waterfallBufferPos = 0;
	}

	if (fftSizeChanged)
	{
		if (m_histogramBuffer)
        {
			delete m_histogramBuffer;
			m_histogramBuffer = nullptr;
		}

		if (m_histogram) {
			delete[] m_histogram;
			m_histogram = nullptr;
		}

		m_histogramBuffer = new QImage(m_nbBins, 100, QImage::Format_RGB32);

        m_histogramBuffer->fill(qRgb(0x00, 0x00, 0x00));
        m_glShaderHistogram.initTexture(*m_histogramBuffer, QOpenGLTexture::ClampToEdge);

		m_histogram = new quint8[100 * m_nbBins];
		memset(m_histogram, 0x00, 100 * m_nbBins);

		m_q3FFT.allocate(2*m_nbBins);
	}

	if (fftSizeChanged || windowSizeChanged)
	{
		m_waterfallTextureHeight = m_waterfallHeight;
		m_waterfallTexturePos = 0;
	}

	m_q3TickTime.allocate(4*m_timeScale.getTickList().count());
    m_q3TickFrequency.allocate(4*m_frequencyScale.getTickList().count());
    m_q3TickPower.allocate(4*m_powerScale.getTickList().count());
	updateHistogramMarkers();
	updateWaterfallMarkers();
} // applyChanges

void GLSpectrum::updateHistogramMarkers()
{
	for (int i = 0; i < m_histogramMarkers.size(); i++)
	{
		float powerI = m_linear ? m_histogramMarkers[i].m_power : CalcDb::dbPower(m_histogramMarkers[i].m_power);
		m_histogramMarkers[i].m_point.rx() =
			(m_histogramMarkers[i].m_frequency - m_frequencyScale.getRangeMin()) / m_frequencyScale.getRange();
		m_histogramMarkers[i].m_point.ry() =
			(m_powerScale.getRangeMax() - powerI) / m_powerScale.getRange();
		m_histogramMarkers[i].m_fftBin =
			(((m_histogramMarkers[i].m_frequency - m_centerFrequency) / (float) m_sampleRate) * m_fftSize) + (m_fftSize / 2);
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
		m_histogramMarkers[i].m_powerStr = displayScaledF(
			powerI,
			m_linear ? 'e' : 'f',
			m_linear ? 3 : 1,
			false);

		if (i > 0)
		{
			int64_t deltaFrequency = m_histogramMarkers.at(i).m_frequency - m_histogramMarkers.at(0).m_frequency;
			m_histogramMarkers.back().m_deltaFrequencyStr = displayScaled(
				deltaFrequency,
				'f',
				getPrecision(deltaFrequency/m_sampleRate),
				true);
			float power0 = m_linear ?
				m_histogramMarkers.at(0).m_power :
				CalcDb::dbPower(m_histogramMarkers.at(0).m_power);
			float powerI = m_linear ?
				m_histogramMarkers.at(i).m_power :
				CalcDb::dbPower(m_histogramMarkers.at(i).m_power);
			m_histogramMarkers.back().m_deltaPowerStr = displayScaledF(
				powerI - power0,
				m_linear ? 'e' : 'f',
				m_linear ? 3 : 1,
				false);
		}
	}
}

void GLSpectrum::updateWaterfallMarkers()
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

void GLSpectrum::mouseMoveEvent(QMouseEvent* event)
{
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
        Real freq = m_frequencyScale.getValueFromPos(event->x() - m_leftMarginPixmap.width() - 1) - m_centerFrequency;

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
}

void GLSpectrum::mousePressEvent(QMouseEvent* event)
{
    const QPointF& ep = event->localPos();

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
            }
        }
        else
        {
            if ((m_histogramMarkers.size() > 0) && (pHis.x() >= 0) && (pHis.x() <= 1) && (pHis.y() >= 0) && (pHis.y() <= 1))
            {
                m_histogramMarkers.pop_back();
                doUpdate = true;
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
            }
        }
        else
        {
            if ((m_waterfallMarkers.size() > 0) && (pWat.x() >= 0) && (pWat.x() <= 1) && (pWat.y() >= 0) && (pWat.y() <= 1))
            {
                m_waterfallMarkers.pop_back();
                doUpdate = true;
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
                    m_histogramMarkers.back().m_powerStr = displayScaledF(
                        powerVal,
                        m_linear ? 'e' : 'f',
                        m_linear ? 3 : 1,
                        false);

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
                        m_histogramMarkers.back().m_deltaPowerStr = displayScaledF(
                            power - power0,
                            m_linear ? 'e' : 'f',
                            m_linear ? 3 : 1,
                            false);
                    }

                    doUpdate = true;
                }
            }

            QPointF pWat = ep;
            pWat.rx() = (ep.x()/width() - m_waterfallRect.left()) / m_waterfallRect.width();
            pWat.ry() = (ep.y()/height() - m_waterfallRect.top()) / m_waterfallRect.height();
            frequency = m_frequencyScale.getRangeMin() + pWat.x()*m_frequencyScale.getRange();
            float time = m_timeScale.getRangeMin() + pWat.y()*m_timeScale.getRange();

            if ((pWat.x() >= 0) && (pWat.x() <= 1) && (pWat.y() >= 0) && (pWat.y() <= 1))
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
			!(event->modifiers() & Qt::AltModifier))
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

void GLSpectrum::mouseReleaseEvent(QMouseEvent*)
{
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

void GLSpectrum::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ShiftModifier) {
		channelMarkerMove(event, 100);
    } else if (event->modifiers() & Qt::ControlModifier) {
		channelMarkerMove(event, 10);
    } else {
        channelMarkerMove(event, 1);
    }
}

void GLSpectrum::zoom(QWheelEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	const QPointF& p = event->position();
#else
	const QPointF& p = event->pos();
#endif

	float pwx = (p.x() - m_leftMargin) / (width() - m_leftMargin - m_rightMargin); // x position in window

	if ((pwx >= 0.0f) && (pwx <= 1.0f))
	{
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

		frequencyZoom(pwx);
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

		//qDebug("GLSpectrum::zoom: pwyh: %f pwyw: %f", pwyh, pwyw);

		if ((pwyw >= 0.0f) && (pwyw <= 1.0f)) {
			timeZoom(event->angleDelta().y() > 0);
		}

		if ((pwyh >= 0.0f) && (pwyh <= 1.0f) && !m_linear) {
			powerZoom(pwyh, event->angleDelta().y() > 0);
		}
	}
}

void GLSpectrum::frequencyZoom(float pw)
{
	m_frequencyZoomPos += (pw - 0.5f) * (1.0f / m_frequencyZoomFactor);
	float lim = 0.5f / m_frequencyZoomFactor;
	m_frequencyZoomPos = m_frequencyZoomPos < lim ? lim : m_frequencyZoomPos > 1 - lim ? 1 - lim : m_frequencyZoomPos;

	qDebug("GLSpectrum::frequencyZoom: pw: %f p: %f z: %f", pw, m_frequencyZoomPos, m_frequencyZoomFactor);
	updateFFTLimits();
}

void GLSpectrum::frequencyPan(QMouseEvent *event)
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

	qDebug("GLSpectrum::frequencyPan: pw: %f p: %f", pw, m_frequencyZoomPos);
	updateFFTLimits();
}

void GLSpectrum::timeZoom(bool zoomInElseOut)
{
	if ((m_fftOverlap  == 0) && !zoomInElseOut) {
		return;
	}

	if (zoomInElseOut && (m_fftOverlap == m_fftSize/2 - 1)) {
		return;
	}

	m_fftOverlap = m_fftOverlap + (zoomInElseOut ? 1 : -1);
	m_changesPending = true;

	if (m_messageQueueToGUI)
	{
		MsgReportFFTOverlap *msg = new MsgReportFFTOverlap(m_fftOverlap);
		m_messageQueueToGUI->push(msg);
	}
}

void GLSpectrum::powerZoom(float pw, bool zoomInElseOut)
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

	if (m_messageQueueToGUI)
	{
		MsgReportPowerScale *msg = new MsgReportPowerScale(m_referenceLevel, m_powerRange);
		m_messageQueueToGUI->push(msg);
	}
}

void GLSpectrum::resetFrequencyZoom()
{
	m_frequencyZoomFactor = 1.0f;
	m_frequencyZoomPos = 0.5f;

	updateFFTLimits();
}

void GLSpectrum::updateFFTLimits()
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

void GLSpectrum::setFrequencyScale()
{
	int frequencySpan;
	int64_t centerFrequency;

	getFrequencyZoom(centerFrequency, frequencySpan);
	m_frequencyScale.setSize(width() - m_leftMargin - m_rightMargin);
	m_frequencyScale.setRange(Unit::Frequency, centerFrequency - frequencySpan / 2.0, centerFrequency + frequencySpan / 2.0);
	m_frequencyScale.setMakeOpposite(m_lsbDisplay);
}

void GLSpectrum::getFrequencyZoom(int64_t& centerFrequency, int& frequencySpan)
{
	frequencySpan = (m_frequencyZoomFactor == 1) ?
		m_sampleRate : m_sampleRate * (1.0 / m_frequencyZoomFactor);
	centerFrequency = (m_frequencyZoomFactor == 1) ?
		m_centerFrequency : (m_frequencyZoomPos - 0.5) * m_sampleRate + m_centerFrequency;
}

// void GLSpectrum::updateFFTLimits()
// {
// 	m_fftMin = m_frequencyZoomFactor == 1 ? 0 : (m_frequencyZoomPos - (0.5f / m_frequencyZoomFactor)) * m_fftSize;
// 	m_fftMax = m_frequencyZoomFactor == 1 ? m_fftSize : (m_frequencyZoomPos - (0.5f / m_frequencyZoomFactor)) * m_fftSize;
// }

void GLSpectrum::channelMarkerMove(QWheelEvent *event, int mul)
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

void GLSpectrum::enterEvent(QEvent* event)
{
	m_mouseInside = true;
	update();
	QGLWidget::enterEvent(event);
}

void GLSpectrum::leaveEvent(QEvent* event)
{
	m_mouseInside = false;
	update();
	QGLWidget::enterEvent(event);
}

void GLSpectrum::tick()
{
	if (m_displayChanged)
    {
		m_displayChanged = false;
		update();
	}
}

void GLSpectrum::channelMarkerChanged()
{
    QMutexLocker mutexLocker(&m_mutex);
	m_changesPending = true;
	update();
}

void GLSpectrum::channelMarkerDestroyed(QObject* object)
{
	removeChannelMarker((ChannelMarker*)object);
}

void GLSpectrum::setWaterfallShare(Real waterfallShare)
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

void GLSpectrum::setFPSPeriodMs(int fpsPeriodMs)
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

void GLSpectrum::cleanup()
{
	//makeCurrent();
	m_glShaderSimple.cleanup();
	m_glShaderFrequencyScale.cleanup();
	m_glShaderHistogram.cleanup();
	m_glShaderLeftScale.cleanup();
	m_glShaderWaterfall.cleanup();
    m_glShaderTextOverlay.cleanup();
	m_glShaderInfo.cleanup();
    //doneCurrent();
}

QString GLSpectrum::displayScaled(int64_t value, char type, int precision, bool showMult)
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

QString GLSpectrum::displayScaledF(float value, char type, int precision, bool showMult)
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
}

int GLSpectrum::getPrecision(int value)
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

void GLSpectrum::drawTextOverlay(
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

void GLSpectrum::formatTextInfo(QString& info)
{
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
