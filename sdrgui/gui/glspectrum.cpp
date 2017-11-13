///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifdef USE_SSE2
#include <emmintrin.h>
#endif

#include <QMouseEvent>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions>
#include <QPainter>
#include "gui/glspectrum.h"

#include <QDebug>

GLSpectrum::GLSpectrum(QWidget* parent) :
	QGLWidget(parent),
	m_cursorState(CSNormal),
    m_cursorChannel(0),
	m_mouseInside(false),
	m_changesPending(true),
	m_centerFrequency(100000000),
	m_referenceLevel(0),
	m_powerRange(100),
	m_decay(0),
	m_sampleRate(500000),
	m_fftSize(512),
	m_displayGrid(true),
	m_displayGridIntensity(5),
	m_displayTraceIntensity(50),
	m_invertedWaterfall(false),
	m_displayMaxHold(false),
	m_currentSpectrum(0),
	m_displayCurrent(false),
	m_waterfallBuffer(NULL),
	m_waterfallBufferPos(0),
    m_waterfallTextureHeight(-1),
    m_waterfallTexturePos(0),
    m_displayWaterfall(true),
    m_ssbSpectrum(false),
    m_lsbDisplay(false),
    m_histogramBuffer(NULL),
    m_histogram(NULL),
    m_histogramHoldoff(NULL),
    m_displayHistogram(true),
    m_displayChanged(false),
    m_matrixLoc(0),
    m_colorLoc(0)
{
	setAutoFillBackground(false);
	setAttribute(Qt::WA_OpaquePaintEvent, true);
	setAttribute(Qt::WA_NoSystemBackground, true);
	setMouseTracking(true);

	setMinimumSize(200, 200);

	m_waterfallShare = 0.66;

	for(int i = 0; i <= 239; i++) {
		 QColor c;
		 c.setHsv(239 - i, 255, 15 + i);
		 ((quint8*)&m_waterfallPalette[i])[0] = c.red();
		 ((quint8*)&m_waterfallPalette[i])[1] = c.green();
		 ((quint8*)&m_waterfallPalette[i])[2] = c.blue();
		 ((quint8*)&m_waterfallPalette[i])[3] = c.alpha();
	}
	m_waterfallPalette[239] = 0xffffffff;

	m_histogramPalette[0] = m_waterfallPalette[0];
	for(int i = 1; i < 240; i++) {
		 QColor c;
		 c.setHsv(239 - i, 255 - ((i < 200) ? 0 : (i - 200) * 3), 150 + ((i < 100) ? i : 100));
		 ((quint8*)&m_histogramPalette[i])[0] = c.red();
		 ((quint8*)&m_histogramPalette[i])[1] = c.green();
		 ((quint8*)&m_histogramPalette[i])[2] = c.blue();
		 ((quint8*)&m_histogramPalette[i])[3] = c.alpha();
	}
	for(int i = 1; i < 16; i++) {
		QColor c;
		c.setHsv(270, 128, 48 + i * 4);
		((quint8*)&m_histogramPalette[i])[0] = c.red();
		((quint8*)&m_histogramPalette[i])[1] = c.green();
		((quint8*)&m_histogramPalette[i])[2] = c.blue();
		((quint8*)&m_histogramPalette[i])[3] = c.alpha();
	}

	m_histogramHoldoffBase = 2; // was 4
	m_histogramHoldoffCount = m_histogramHoldoffBase;
	m_histogramLateHoldoff = 1; // was 20
	m_histogramStroke = 40; // was 4

	m_timeScale.setFont(font());
	m_timeScale.setOrientation(Qt::Vertical);
	m_timeScale.setRange(Unit::Time, 0, 1);
	m_powerScale.setFont(font());
	m_powerScale.setOrientation(Qt::Vertical);
	m_frequencyScale.setFont(font());
	m_frequencyScale.setOrientation(Qt::Horizontal);

	connect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
	m_timer.start(50);
}

GLSpectrum::~GLSpectrum()
{
	cleanup();

	QMutexLocker mutexLocker(&m_mutex);

	m_changesPending = true;

	if(m_waterfallBuffer != NULL) {
		delete m_waterfallBuffer;
		m_waterfallBuffer = NULL;
	}
	if(m_histogramBuffer != NULL) {
		delete m_histogramBuffer;
		m_histogramBuffer = NULL;
	}
	if(m_histogram != NULL) {
		delete[] m_histogram;
		m_histogram = NULL;
	}
	if(m_histogramHoldoff != NULL) {
		delete[] m_histogramHoldoff;
		m_histogramHoldoff = NULL;
	}
}

void GLSpectrum::setCenterFrequency(qint64 frequency)
{
	m_centerFrequency = frequency;
	m_changesPending = true;
	update();
}

void GLSpectrum::setReferenceLevel(Real referenceLevel)
{
	m_referenceLevel = referenceLevel;
	m_changesPending = true;
	update();
}

void GLSpectrum::setPowerRange(Real powerRange)
{
	m_powerRange = powerRange;
	m_changesPending = true;
	update();
}

void GLSpectrum::setDecay(int decay)
{
	m_decay = decay;
	if(m_decay < 0)
		m_decay = 0;
	else if(m_decay > 10)
		m_decay = 10;
}

void GLSpectrum::setHistoLateHoldoff(int lateHoldoff)
{
	m_histogramLateHoldoff = lateHoldoff;
	if(m_histogramLateHoldoff < 0)
		m_histogramLateHoldoff = 0;
	else if(m_histogramLateHoldoff > 20)
		m_histogramLateHoldoff = 20;
}

void GLSpectrum::setHistoStroke(int stroke)
{
	m_histogramStroke = stroke;
	if(m_histogramStroke < 4)
		m_histogramStroke = 4;
	else if(m_histogramStroke > 240)
		m_histogramStroke = 240;
}

void GLSpectrum::setSampleRate(qint32 sampleRate)
{
	m_sampleRate = sampleRate;
	m_changesPending = true;
	update();
}

void GLSpectrum::setDisplayWaterfall(bool display)
{
	m_displayWaterfall = display;
	m_changesPending = true;
	stopDrag();
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
	m_invertedWaterfall = inv;
	m_changesPending = true;
	stopDrag();
	update();
}

void GLSpectrum::setDisplayMaxHold(bool display)
{
	m_displayMaxHold = display;
	m_changesPending = true;
	stopDrag();
	update();
}

void GLSpectrum::setDisplayCurrent(bool display)
{
	m_displayCurrent = display;
	m_changesPending = true;
	stopDrag();
	update();
}

void GLSpectrum::setDisplayHistogram(bool display)
{
	m_displayHistogram = display;
	m_changesPending = true;
	stopDrag();
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

void GLSpectrum::addChannelMarker(ChannelMarker* channelMarker)
{
	QMutexLocker mutexLocker(&m_mutex);

	connect(channelMarker, SIGNAL(changedByAPI()), this, SLOT(channelMarkerChanged()));
	connect(channelMarker, SIGNAL(destroyed(QObject*)), this, SLOT(channelMarkerDestroyed(QObject*)));
	m_channelMarkerStates.append(new ChannelMarkerState(channelMarker));
	m_changesPending = true;
	stopDrag();
	update();
}

void GLSpectrum::removeChannelMarker(ChannelMarker* channelMarker)
{
	QMutexLocker mutexLocker(&m_mutex);

	for(int i = 0; i < m_channelMarkerStates.size(); ++i) {
		if(m_channelMarkerStates[i]->m_channelMarker == channelMarker) {
			channelMarker->disconnect(this);
			delete m_channelMarkerStates.takeAt(i);
			m_changesPending = true;
			stopDrag();
			update();
			return;
		}
	}
}

void GLSpectrum::newSpectrum(const std::vector<Real>& spectrum, int fftSize)
{
	QMutexLocker mutexLocker(&m_mutex);

	m_displayChanged = true;

	if(m_changesPending) {
		m_fftSize = fftSize;
		return;
	}

	if(fftSize != m_fftSize) {
		m_fftSize = fftSize;
		m_changesPending = true;
		return;
	}

	updateWaterfall(spectrum);
	updateHistogram(spectrum);
}

void GLSpectrum::updateWaterfall(const std::vector<Real>& spectrum)
{
	if(m_waterfallBufferPos < m_waterfallBuffer->height()) {
		quint32* pix = (quint32*)m_waterfallBuffer->scanLine(m_waterfallBufferPos);

		for(int i = 0; i < m_fftSize; i++) {
			int v = (int)((spectrum[i] - m_referenceLevel) * 2.4 * 100.0 / m_powerRange + 240.0);
			if(v > 239)
				v = 239;
			else if(v < 0)
				v = 0;

			*pix++ = m_waterfallPalette[(int)v];
		}

		m_waterfallBufferPos++;
	}
}

void GLSpectrum::updateHistogram(const std::vector<Real>& spectrum)
{
	quint8* b = m_histogram;
	quint8* h = m_histogramHoldoff;
	int sub = 1;
	int fftMulSize = 100 * m_fftSize;

	if(m_decay > 0)
		sub += m_decay;

	if (m_displayHistogram || m_displayMaxHold)
	{
		m_histogramHoldoffCount--;

		if(m_histogramHoldoffCount <= 0)
		{
			for(int i = 0; i < fftMulSize; i++)
			{
				if((*b>>4) > 0) // *b > 16
				{
					*b = *b - sub;
				}
				else if(*b > 0)
				{
					if(*h >= sub)
					{
						*h = *h - sub;
					}
					else if(*h > 0)
					{
						*h = *h - 1;
					}
					else
					{
						*b = *b - 1;
						*h = m_histogramLateHoldoff;
					}
				}

				b++;
				h++;
			}

			m_histogramHoldoffCount = m_histogramHoldoffBase;
		}
	}

	m_currentSpectrum = &spectrum; // Store spectrum for current spectrum line display

#ifdef USE_SSE2
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
    for(int i = 0; i < m_fftSize; i++) {
        int v = (int)((spectrum[i] - m_referenceLevel) * 100.0 / m_powerRange + 100.0);

        if ((v >= 0) && (v <= 99)) {
            b = m_histogram + i * 100 + v;
            if(*b < 220)
                *b += m_histogramStroke; // was 4
            else if(*b < 239)
                *b += 1;
        }
    }
#endif
}

void GLSpectrum::initializeGL()
{
	QOpenGLContext *glCurrentContext =  QOpenGLContext::currentContext();

	if (glCurrentContext) {
		if (QOpenGLContext::currentContext()->isValid()) {
			qDebug() << "GLSpectrum::initializeGL: context:"
				<< " major: " << (QOpenGLContext::currentContext()->format()).majorVersion()
				<< " minor: " << (QOpenGLContext::currentContext()->format()).minorVersion()
				<< " ES: " << (QOpenGLContext::currentContext()->isOpenGLES() ? "yes" : "no");
		}
		else {
			qDebug() << "GLSpectrum::initializeGL: current context is invalid";
		}
	} else {
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
}

void GLSpectrum::resizeGL(int width, int height)
{
	QOpenGLFunctions *glFunctions = QOpenGLContext::currentContext()->functions();
	glFunctions->glViewport(0, 0, width, height);
	m_changesPending = true;
}

void GLSpectrum::clearSpectrumHistogram()
{
	if(!m_mutex.tryLock(2))
		return;

	memset(m_histogram, 0x00, 100 * m_fftSize);
	memset(m_histogramHoldoff, 0x07, 100 * m_fftSize);

	m_mutex.unlock();
	update();
}

void GLSpectrum::paintGL()
{
	if(!m_mutex.tryLock(2))
		return;

	if(m_changesPending)
		applyChanges();

	if(m_fftSize <= 0) {
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
				m_glShaderWaterfall.subTexture(0, m_waterfallTexturePos, m_fftSize, m_waterfallBufferPos,  m_waterfallBuffer->scanLine(0));
				m_waterfallTexturePos += m_waterfallBufferPos;
			}
			else
			{
				int breakLine = m_waterfallTextureHeight - m_waterfallTexturePos;
				int linesLeft = m_waterfallTexturePos + m_waterfallBufferPos - m_waterfallTextureHeight;
				m_glShaderWaterfall.subTexture(0, m_waterfallTexturePos, m_fftSize, breakLine,  m_waterfallBuffer->scanLine(0));
				m_glShaderWaterfall.subTexture(0, 0, m_fftSize, linesLeft,  m_waterfallBuffer->scanLine(breakLine));
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
				if (dv->m_channelMarker->getVisible())
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
	if(m_displayHistogram || m_displayMaxHold || m_displayCurrent)
	{
		if(m_displayHistogram)
		{
			{
				// import new lines into the texture
				quint32* pix;
				quint8* bs = m_histogram;

				for (int y = 0; y < 100; y++)
				{
					quint8* b = bs;
					pix = (quint32*)m_histogramBuffer->scanLine(99 - y);

					for (int x = 0; x < m_fftSize; x++)
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

				m_glShaderHistogram.subTexture(0, 0, m_fftSize, 100,  m_histogramBuffer->scanLine(0));
				m_glShaderHistogram.drawSurface(m_glHistogramBoxMatrix, tex1, vtx1, 4);
			}
		}


		// paint channels
		if(m_mouseInside)
		{
			// Effective BW overlays
			for(int i = 0; i < m_channelMarkerStates.size(); ++i)
			{
				ChannelMarkerState* dv = m_channelMarkerStates[i];
				if(dv->m_channelMarker->getVisible())
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
		for(int i = 0; i < m_channelMarkerStates.size(); ++i)
		{
			ChannelMarkerState* dv = m_channelMarkerStates[i];

			// frequency scale channel overlay
			if(dv->m_channelMarker->getVisible())
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
		if (m_maxHold.size() < (uint)m_fftSize)
			m_maxHold.resize(m_fftSize);

		for(int i = 0; i < m_fftSize; i++)
		{
			int j;
			quint8* bs = m_histogram + i * 100;
			for(j = 99; j > 1; j--) {
				if(bs[j] > 0)
					break;
			}
			// TODO: ((bs[j] * (float)j) + (bs[j + 1] * (float)(j + 1))) / (bs[j] +  bs[j + 1])
			j = j - 99;
			m_maxHold[i] = (j * m_powerRange) / 99.0 + m_referenceLevel;
		}
		{
			GLfloat q3[2*m_fftSize];
			Real bottom = -m_powerRange;

			for(int i = 0; i < m_fftSize; i++) {
				Real v = m_maxHold[i] - m_referenceLevel;
				if(v > 0)
					v = 0;
				else if(v < bottom)
					v = bottom;
				q3[2*i] = (Real) i;
				q3[2*i+1] = v;
			}

			QVector4D color(1.0f, 0.0f, 0.0f, (float) m_displayTraceIntensity / 100.0f);
			m_glShaderSimple.drawPolyline(m_glHistogramSpectrumMatrix, color, q3, m_fftSize);
		}
	}

	// paint current spectrum line on top of histogram
	if ((m_displayCurrent) && m_currentSpectrum)
	{
		{
			Real bottom = -m_powerRange;
			GLfloat q3[2*m_fftSize];

			for(int i = 0; i < m_fftSize; i++) {
				Real v = (*m_currentSpectrum)[i] - m_referenceLevel;
				if(v > 0)
					v = 0;
				else if(v < bottom)
					v = bottom;
				q3[2*i] = (Real) i;
				q3[2*i+1] = v;
			}

			QVector4D color(1.0f, 1.0f, 0.25f, (float) m_displayTraceIntensity / 100.0f);
			m_glShaderSimple.drawPolyline(m_glHistogramSpectrumMatrix, color, q3, m_fftSize);
		}
	}

	// paint waterfall grid
	if(m_displayWaterfall && m_displayGrid)
	{
		const ScaleEngine::TickList* tickList;
		const ScaleEngine::Tick* tick;
		tickList = &m_timeScale.getTickList();

		{
			GLfloat q3[4*tickList->count()];
			int effectiveTicks = 0;

			for (int i= 0; i < tickList->count(); i++)
			{
				tick = &(*tickList)[i];
				if (tick->major)
				{
					if(tick->textSize > 0)
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
			GLfloat q3[4*tickList->count()];
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
	if((m_displayHistogram || m_displayMaxHold || m_displayCurrent) && (m_displayGrid))
	{
		const ScaleEngine::TickList* tickList;
		const ScaleEngine::Tick* tick;
		tickList = &m_powerScale.getTickList();

		{
			GLfloat q3[4*tickList->count()];
			int effectiveTicks = 0;

			for(int i= 0; i < tickList->count(); i++)
			{
				tick = &(*tickList)[i];
				if(tick->major)
				{
					if(tick->textSize > 0)
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
			GLfloat q3[4*tickList->count()];
			int effectiveTicks = 0;

			for(int i= 0; i < tickList->count(); i++)
			{
				tick = &(*tickList)[i];

				if(tick->major)
				{
					if(tick->textSize > 0)
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

	m_mutex.unlock();
}

void GLSpectrum::stopDrag()
{
	if(m_cursorState != CSNormal) {
		if((m_cursorState == CSSplitterMoving) || (m_cursorState == CSChannelMoving))
			releaseMouse();
		setCursor(Qt::ArrowCursor);
		m_cursorState = CSNormal;
	}
}

void GLSpectrum::applyChanges()
{
	m_changesPending = false;

	if(m_fftSize <= 0)
		return;

	QFontMetrics fm(font());
	int M = fm.width("-");

	int topMargin = fm.ascent() * 1.5;
	int bottomMargin = fm.ascent() * 1.5;

	int waterfallHeight = 0;
	int waterfallTop = 0;
	int frequencyScaleHeight = fm.height() * 3; // +1 line for marker frequency scale
	int frequencyScaleTop = 0;
	int histogramTop = 0;
	int histogramHeight = 20;
	int leftMargin;
	int rightMargin = fm.width("000");

	// displays both histogram and waterfall
	if(m_displayWaterfall && (m_displayHistogram | m_displayMaxHold | m_displayCurrent))
	{
		waterfallHeight = height() * m_waterfallShare - 1;

		if(waterfallHeight < 0)
		{
			waterfallHeight = 0;
		}

		if(!m_invertedWaterfall)
		{
			waterfallTop = topMargin;
			frequencyScaleTop = waterfallTop + waterfallHeight + 1;
			histogramTop = waterfallTop + waterfallHeight + frequencyScaleHeight + 1;
			histogramHeight = height() - topMargin - waterfallHeight - frequencyScaleHeight - bottomMargin;
		}
		else
		{
			histogramTop = topMargin;
			histogramHeight = height() - topMargin - waterfallHeight - frequencyScaleHeight - bottomMargin;
			waterfallTop = histogramTop + histogramHeight + frequencyScaleHeight + 1;
			frequencyScaleTop = histogramTop + histogramHeight + 1;
		}

		m_timeScale.setSize(waterfallHeight);

		if(m_sampleRate > 0)
		{
			float scaleDiv = (float)m_sampleRate * (m_ssbSpectrum ? 2 : 1);

			if(!m_invertedWaterfall)
			{
				m_timeScale.setRange(Unit::Time, (waterfallHeight * m_fftSize) / scaleDiv, 0);
			}
			else
			{
				m_timeScale.setRange(Unit::Time, 0, (waterfallHeight * m_fftSize) / scaleDiv);
			}
		}
		else
		{
			m_timeScale.setRange(Unit::Time, 0, 1);
		}

		m_powerScale.setSize(histogramHeight);
		m_powerScale.setRange(Unit::Decibel, m_referenceLevel - m_powerRange, m_referenceLevel);
		leftMargin = m_timeScale.getScaleWidth();

		if(m_powerScale.getScaleWidth() > leftMargin)
		{
			leftMargin = m_powerScale.getScaleWidth();
		}

		leftMargin += 2 * M;

		m_frequencyScale.setSize(width() - leftMargin - rightMargin);
		m_frequencyScale.setRange(Unit::Frequency, m_centerFrequency - m_sampleRate / 2, m_centerFrequency + m_sampleRate / 2);
		m_frequencyScale.setMakeOpposite(m_lsbDisplay);

		m_glWaterfallBoxMatrix.setToIdentity();
		m_glWaterfallBoxMatrix.translate(
			-1.0f + ((float)(2*leftMargin)   / (float) width()),
			 1.0f - ((float)(2*waterfallTop) / (float) height())
		);
		m_glWaterfallBoxMatrix.scale(
			((float) 2 * (width() - leftMargin - rightMargin)) / (float) width(),
			(float) (-2*waterfallHeight) / (float) height()
		);

		m_glHistogramBoxMatrix.setToIdentity();
		m_glHistogramBoxMatrix.translate(
			-1.0f + ((float)(2*leftMargin)   / (float) width()),
			 1.0f - ((float)(2*histogramTop) / (float) height())
		);
		m_glHistogramBoxMatrix.scale(
			((float) 2 * (width() - leftMargin - rightMargin)) / (float) width(),
			(float) (-2*histogramHeight) / (float) height()
		);

		m_glHistogramSpectrumMatrix.setToIdentity();
		m_glHistogramSpectrumMatrix.translate(
			-1.0f + ((float)(2*leftMargin)   / (float) width()),
			 1.0f - ((float)(2*histogramTop) / (float) height())
		);
		m_glHistogramSpectrumMatrix.scale(
			((float) 2 * (width() - leftMargin - rightMargin)) / ((float) width() * (float)(m_fftSize - 1)),
			((float) 2*histogramHeight / height()) / m_powerRange
		);

		m_frequencyScaleRect = QRect(
			0,
			frequencyScaleTop,
			width(),
			frequencyScaleHeight
		);

		m_glFrequencyScaleBoxMatrix.setToIdentity();
		m_glFrequencyScaleBoxMatrix.translate (
			-1.0f,
			 1.0f - ((float) 2*frequencyScaleTop / (float) height())
		);
		m_glFrequencyScaleBoxMatrix.scale (
			2.0f,
			(float) -2*frequencyScaleHeight / (float) height()
		);

		m_glLeftScaleBoxMatrix.setToIdentity();
		m_glLeftScaleBoxMatrix.translate(-1.0f, 1.0f);
		m_glLeftScaleBoxMatrix.scale(
			(float)(2*(leftMargin - 1)) / (float) width(),
			-2.0f
		);
	}
	// displays waterfall only
	else if(m_displayWaterfall)
	{
		bottomMargin = frequencyScaleHeight;
		waterfallTop = topMargin;
		waterfallHeight = height() - topMargin - frequencyScaleHeight;
		frequencyScaleTop = topMargin + waterfallHeight + 1;
		histogramTop = 0;

		m_timeScale.setSize(waterfallHeight);

		if(m_sampleRate > 0)
		{
			float scaleDiv = (float)m_sampleRate * (m_ssbSpectrum ? 2 : 1);

			if(!m_invertedWaterfall)
			{
				m_timeScale.setRange(Unit::Time, (waterfallHeight * m_fftSize) / scaleDiv, 0);
			}
			else
			{
				m_timeScale.setRange(Unit::Time, 0, (waterfallHeight * m_fftSize) / scaleDiv);
			}
		}
		else
		{
			if(!m_invertedWaterfall)
			{
				m_timeScale.setRange(Unit::Time, 10, 0);
			}
			else
			{
				m_timeScale.setRange(Unit::Time, 0, 10);
			}
		}

		leftMargin = m_timeScale.getScaleWidth();
		leftMargin += 2 * M;

		m_frequencyScale.setSize(width() - leftMargin - rightMargin);
		m_frequencyScale.setRange(Unit::Frequency, m_centerFrequency - m_sampleRate / 2.0, m_centerFrequency + m_sampleRate / 2.0);
		m_frequencyScale.setMakeOpposite(m_lsbDisplay);

		m_glWaterfallBoxMatrix.setToIdentity();
		m_glWaterfallBoxMatrix.translate(
			-1.0f + ((float)(2*leftMargin)   / (float) width()),
			 1.0f - ((float)(2*topMargin) / (float) height())
		);
		m_glWaterfallBoxMatrix.scale(
			((float) 2 * (width() - leftMargin - rightMargin)) / (float) width(),
			(float) (-2*waterfallHeight) / (float) height()
		);

		m_frequencyScaleRect = QRect(
			0,
			frequencyScaleTop,
			width(),
			frequencyScaleHeight
		);

		m_glFrequencyScaleBoxMatrix.setToIdentity();
		m_glFrequencyScaleBoxMatrix.translate (
			-1.0f,
			 1.0f - ((float) 2*frequencyScaleTop / (float) height())
		);
		m_glFrequencyScaleBoxMatrix.scale (
			2.0f,
			(float) -2*frequencyScaleHeight / (float) height()
		);

		m_glLeftScaleBoxMatrix.setToIdentity();
		m_glLeftScaleBoxMatrix.translate(-1.0f, 1.0f);
		m_glLeftScaleBoxMatrix.scale(
			(float)(2*(leftMargin - 1)) / (float) width(),
			-2.0f
		);
	}
	// displays histogram only
	else if(m_displayHistogram || m_displayMaxHold || m_displayCurrent)
	{
		bottomMargin = frequencyScaleHeight;
		frequencyScaleTop = height() - bottomMargin;
		histogramTop = topMargin - 1;
		waterfallHeight = 0;
		histogramHeight = height() - topMargin - frequencyScaleHeight;

		m_powerScale.setSize(histogramHeight);
		m_powerScale.setRange(Unit::Decibel, m_referenceLevel - m_powerRange, m_referenceLevel);
		leftMargin = m_powerScale.getScaleWidth();
		leftMargin += 2 * M;

		m_frequencyScale.setSize(width() - leftMargin - rightMargin);
		m_frequencyScale.setRange(Unit::Frequency, m_centerFrequency - m_sampleRate / 2, m_centerFrequency + m_sampleRate / 2);
		m_frequencyScale.setMakeOpposite(m_lsbDisplay);

		m_glHistogramSpectrumMatrix.setToIdentity();
		m_glHistogramSpectrumMatrix.translate(
			-1.0f + ((float)(2*leftMargin)   / (float) width()),
			 1.0f - ((float)(2*histogramTop) / (float) height())
		);
		m_glHistogramSpectrumMatrix.scale(
			((float) 2 * (width() - leftMargin - rightMargin)) / ((float) width() * (float)(m_fftSize - 1)),
			((float) 2*(height() - topMargin - frequencyScaleHeight) / height()) / m_powerRange
		);

		m_glHistogramBoxMatrix.setToIdentity();
		m_glHistogramBoxMatrix.translate(
			-1.0f + ((float)(2*leftMargin)   / (float) width()),
			 1.0f - ((float)(2*histogramTop) / (float) height())
		);
		m_glHistogramBoxMatrix.scale(
			((float) 2 * (width() - leftMargin - rightMargin)) / (float) width(),
			(float) (-2*(height() - topMargin - frequencyScaleHeight)) / (float) height()
		);

		m_frequencyScaleRect = QRect(
			0,
			frequencyScaleTop,
			width(),
			frequencyScaleHeight
		);

		m_glFrequencyScaleBoxMatrix.setToIdentity();
		m_glFrequencyScaleBoxMatrix.translate (
			-1.0f,
			 1.0f - ((float) 2*frequencyScaleTop / (float) height())
		);
		m_glFrequencyScaleBoxMatrix.scale (
			2.0f,
			(float) -2*frequencyScaleHeight / (float) height()
		);

		m_glLeftScaleBoxMatrix.setToIdentity();
		m_glLeftScaleBoxMatrix.translate(-1.0f, 1.0f);
		m_glLeftScaleBoxMatrix.scale(
			(float)(2*(leftMargin - 1)) / (float) width(),
			-2.0f
		);
	}
	else
	{
		leftMargin = 2;
		waterfallHeight = 0;
	}

	// channel overlays
	for(int i = 0; i < m_channelMarkerStates.size(); ++i)
	{
		ChannelMarkerState* dv = m_channelMarkerStates[i];

		qreal xc, pw, nw, dsbw;
		ChannelMarker::sidebands_t sidebands = dv->m_channelMarker->getSidebands();
		xc = m_centerFrequency + dv->m_channelMarker->getCenterFrequency(); // marker center frequency
		dsbw = dv->m_channelMarker->getBandwidth();

		if (sidebands == ChannelMarker::usb) {
			nw = dv->m_channelMarker->getLowCutoff();     // negative bandwidth
			pw = dv->m_channelMarker->getBandwidth() / 2; // positive bandwidth
		} else if (sidebands == ChannelMarker::lsb) {
			pw = dv->m_channelMarker->getLowCutoff();
			nw = dv->m_channelMarker->getBandwidth() / 2;
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
			-1.0f + 2.0f * ((leftMargin + m_frequencyScale.getPosFromValue(xc - (dsbw/2))) / (float) width()),
			 1.0f
		);
		glMatrixDsb.scale(
			2.0f * (dsbw / (float)m_sampleRate),
			-2.0f
		);

		dv->m_glMatrixDsbWaterfall = glMatrixDsb;
		dv->m_glMatrixDsbWaterfall.translate(
			 0.0f,
			 (float) waterfallTop / (float) height()
		);
		dv->m_glMatrixDsbWaterfall.scale(
			(float) (width() - leftMargin - rightMargin) / (float) width(),
			(float) waterfallHeight / (float) height()
		);

		dv->m_glMatrixDsbHistogram = glMatrixDsb;
		dv->m_glMatrixDsbHistogram.translate(
			 0.0f,
			 (float) histogramTop / (float) height()
		);
		dv->m_glMatrixDsbHistogram.scale(
			(float) (width() - leftMargin - rightMargin) / (float) width(),
			(float) histogramHeight / (float) height()
		);

		dv->m_glMatrixDsbFreqScale = glMatrixDsb;
		dv->m_glMatrixDsbFreqScale.translate(
			 0.0f,
			 (float) frequencyScaleTop / (float) height()
		);
		dv->m_glMatrixDsbFreqScale.scale(
			(float) (width() - leftMargin - rightMargin) / (float) width(),
			(float) frequencyScaleHeight / (float) height()
		);

		// draw the effective BW rectangle

		QMatrix4x4 glMatrix;
		glMatrix.setToIdentity();
		glMatrix.translate(
			-1.0f + 2.0f * ((leftMargin + m_frequencyScale.getPosFromValue(xc + nw)) / (float) width()),
			 1.0f
		);
		glMatrix.scale(
			2.0f * ((pw-nw) / (float)m_sampleRate),
			-2.0f
		);

		dv->m_glMatrixWaterfall = glMatrix;
		dv->m_glMatrixWaterfall.translate(
			 0.0f,
			 (float) waterfallTop / (float) height()
		);
		dv->m_glMatrixWaterfall.scale(
			(float) (width() - leftMargin - rightMargin) / (float) width(),
			(float) waterfallHeight / (float) height()
		);

		dv->m_glMatrixHistogram = glMatrix;
		dv->m_glMatrixHistogram.translate(
			 0.0f,
			 (float) histogramTop / (float) height()
		);
		dv->m_glMatrixHistogram.scale(
			(float) (width() - leftMargin - rightMargin) / (float) width(),
			(float) histogramHeight / (float) height()
		);

		dv->m_glMatrixFreqScale = glMatrix;
		dv->m_glMatrixFreqScale.translate(
			 0.0f,
			 (float) frequencyScaleTop / (float) height()
		);
		dv->m_glMatrixFreqScale.scale(
			(float) (width() - leftMargin - rightMargin) / (float) width(),
			(float) frequencyScaleHeight / (float) height()
		);


		/*
		dv->m_glRect.setRect(
			m_frequencyScale.getPosFromValue(m_centerFrequency + dv->m_channelMarker->getCenterFrequency() - dv->m_channelMarker->getBandwidth() / 2) / (float)(width() - leftMargin - rightMargin),
			0,
			(dv->m_channelMarker->getBandwidth() / (float)m_sampleRate),
			1);
		*/

		if(m_displayHistogram || m_displayMaxHold || m_displayCurrent || m_displayWaterfall)
		{
			dv->m_rect.setRect(m_frequencyScale.getPosFromValue(xc) + leftMargin - 1,
			topMargin,
			5,
			height() - topMargin - bottomMargin);
		}

		/*
		if(m_displayHistogram || m_displayMaxHold || m_displayWaterfall) {
			dv->m_rect.setRect(m_frequencyScale.getPosFromValue(m_centerFrequency + dv->m_channelMarker->getCenterFrequency()) + leftMargin - 1,
			topMargin,
			5,
			height() - topMargin - bottomMargin);
		}
		*/
	}

	// prepare left scales (time and power)
	{
		m_leftMarginPixmap = QPixmap(leftMargin - 1, height());
		m_leftMarginPixmap.fill(Qt::black);
		{
			QPainter painter(&m_leftMarginPixmap);
			painter.setPen(QColor(0xf0, 0xf0, 0xff));
			painter.setFont(font());
			const ScaleEngine::TickList* tickList;
			const ScaleEngine::Tick* tick;
			if(m_displayWaterfall) {
				tickList = &m_timeScale.getTickList();
				for(int i = 0; i < tickList->count(); i++) {
					tick = &(*tickList)[i];
					if(tick->major) {
						if(tick->textSize > 0)
							painter.drawText(QPointF(leftMargin - M - tick->textSize, waterfallTop + fm.ascent() + tick->textPos), tick->text);
					}
				}
			}
			if(m_displayHistogram || m_displayMaxHold || m_displayCurrent) {
				tickList = &m_powerScale.getTickList();
				for(int i = 0; i < tickList->count(); i++) {
					tick = &(*tickList)[i];
					if(tick->major) {
						if(tick->textSize > 0)
							painter.drawText(QPointF(leftMargin - M - tick->textSize, histogramTop + histogramHeight - tick->textPos - 1), tick->text);
					}
				}
			}
		}

		m_glShaderLeftScale.initTexture(m_leftMarginPixmap.toImage());
	}
	// prepare frequency scale
	if(m_displayWaterfall || m_displayHistogram || m_displayMaxHold || m_displayCurrent){
		m_frequencyPixmap = QPixmap(width(), frequencyScaleHeight);
		m_frequencyPixmap.fill(Qt::transparent);
		{
			QPainter painter(&m_frequencyPixmap);
			painter.setPen(Qt::NoPen);
			painter.setBrush(Qt::black);
			painter.setBrush(Qt::transparent);
			painter.drawRect(leftMargin, 0, width() - leftMargin, frequencyScaleHeight);
			painter.setPen(QColor(0xf0, 0xf0, 0xff));
			painter.setFont(font());
			const ScaleEngine::TickList* tickList = &m_frequencyScale.getTickList();
			const ScaleEngine::Tick* tick;
			for(int i = 0; i < tickList->count(); i++) {
				tick = &(*tickList)[i];
				if(tick->major) {
					if(tick->textSize > 0)
						painter.drawText(QPointF(leftMargin + tick->textPos, fm.height() + fm.ascent() / 2 - 1), tick->text);
				}
			}

			// Frequency overlay on highlighted marker
			for(int i = 0; i < m_channelMarkerStates.size(); ++i) {
				ChannelMarkerState* dv = m_channelMarkerStates[i];
				if (dv->m_channelMarker->getHighlighted())
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
						shift = - fm.width(ftext);
					}
					painter.drawText(QPointF(leftMargin + m_frequencyScale.getPosFromValue(xc) + shift, 2*fm.height() + fm.ascent() / 2 - 1), ftext);
				}
			}

		}

		m_glShaderFrequencyScale.initTexture(m_frequencyPixmap.toImage());
	}

	bool fftSizeChanged = true;

	if(m_waterfallBuffer != NULL) {
		fftSizeChanged = m_waterfallBuffer->width() != m_fftSize;
	}

	bool windowSizeChanged = m_waterfallTextureHeight != waterfallHeight;

	if (fftSizeChanged || windowSizeChanged)
	{
		if(m_waterfallBuffer != 0) {
			delete m_waterfallBuffer;
		}

		m_waterfallBuffer = new QImage(m_fftSize, waterfallHeight, QImage::Format_ARGB32);

		if(m_waterfallBuffer != 0)
		{
			m_waterfallBuffer->fill(qRgb(0x00, 0x00, 0x00));
			m_glShaderWaterfall.initTexture(*m_waterfallBuffer);
			m_waterfallBufferPos = 0;
		}
		else
		{
			m_fftSize = 0;
			m_changesPending = true;
			return;
		}
	}

	if(fftSizeChanged)
	{
		if(m_histogramBuffer != NULL) {
			delete m_histogramBuffer;
			m_histogramBuffer = NULL;
		}
		if(m_histogram != NULL) {
			delete[] m_histogram;
			m_histogram = NULL;
		}
		if(m_histogramHoldoff != NULL) {
			delete[] m_histogramHoldoff;
			m_histogramHoldoff = NULL;
		}

		m_histogramBuffer = new QImage(m_fftSize, 100, QImage::Format_RGB32);

		if(m_histogramBuffer != NULL)
		{
			m_histogramBuffer->fill(qRgb(0x00, 0x00, 0x00));
			m_glShaderHistogram.initTexture(*m_histogramBuffer, QOpenGLTexture::ClampToEdge);
		}
		else
		{
			m_fftSize = 0;
			m_changesPending = true;
			return;
		}

		m_histogram = new quint8[100 * m_fftSize];
		memset(m_histogram, 0x00, 100 * m_fftSize);
		m_histogramHoldoff = new quint8[100 * m_fftSize];
		memset(m_histogramHoldoff, 0x07, 100 * m_fftSize);
	}

	if(fftSizeChanged || windowSizeChanged)
	{
		m_waterfallTextureHeight = waterfallHeight;
		m_waterfallTexturePos = 0;
	}
}

void GLSpectrum::mouseMoveEvent(QMouseEvent* event)
{
	if(m_displayWaterfall && (m_displayWaterfall || m_displayHistogram || m_displayMaxHold || m_displayCurrent)) {
		if(m_frequencyScaleRect.contains(event->pos())) {
			if(m_cursorState == CSNormal) {
				setCursor(Qt::SizeVerCursor);
				m_cursorState = CSSplitter;
				return;
			}
		} else {
			if(m_cursorState == CSSplitter) {
				setCursor(Qt::ArrowCursor);
				m_cursorState = CSNormal;
				return;
			}
		}
	}

    if (m_cursorState == CSSplitterMoving)
    {
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

        update();
        return;
    }
    else if (m_cursorState == CSChannelMoving)
    {
        Real freq = m_frequencyScale.getValueFromPos(event->x() - m_leftMarginPixmap.width() - 1) - m_centerFrequency;

        if (m_channelMarkerStates[m_cursorChannel]->m_channelMarker->getMovable())
        {
            m_channelMarkerStates[m_cursorChannel]->m_channelMarker->setCenterFrequencyByCursor(freq);
            channelMarkerChanged();
        }
    }

    if (m_displayWaterfall || m_displayHistogram || m_displayMaxHold || m_displayCurrent)
    {
        for (int i = 0; i < m_channelMarkerStates.size(); ++i)
        {
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

	if(m_cursorState == CSChannel)
	{
		setCursor(Qt::ArrowCursor);
		m_cursorState = CSNormal;

		return;
	}
}

void GLSpectrum::mousePressEvent(QMouseEvent* event)
{
	if(event->button() != 1)
		return;

	if(m_cursorState == CSSplitter)
	{
		grabMouse();
		m_cursorState = CSSplitterMoving;
		return;
	}
	else if(m_cursorState == CSChannel)
	{
		grabMouse();
		m_cursorState = CSChannelMoving;
		return;
	}
	else if((m_cursorState == CSNormal) && (m_channelMarkerStates.size() == 1))
	{
		grabMouse();
		setCursor(Qt::SizeHorCursor);
		m_cursorState = CSChannelMoving;
		m_cursorChannel = 0;
		Real freq = m_frequencyScale.getValueFromPos(event->x() - m_leftMarginPixmap.width() - 1) - m_centerFrequency;

		if(m_channelMarkerStates[m_cursorChannel]->m_channelMarker->getMovable())
		{
			m_channelMarkerStates[m_cursorChannel]->m_channelMarker->setCenterFrequencyByCursor(freq);
			channelMarkerChanged();
		}

		return;
	}
}

void GLSpectrum::mouseReleaseEvent(QMouseEvent*)
{
	if(m_cursorState == CSSplitterMoving) {
		releaseMouse();
		m_cursorState = CSSplitter;
	} else if(m_cursorState == CSChannelMoving) {
		releaseMouse();
		m_cursorState = CSChannel;
	}
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
	if(m_displayChanged) {
		m_displayChanged = false;
		update();
	}
}

void GLSpectrum::channelMarkerChanged()
{
	m_changesPending = true;
	update();
}

void GLSpectrum::channelMarkerDestroyed(QObject* object)
{
	removeChannelMarker((ChannelMarker*)object);
}

void GLSpectrum::setWaterfallShare(Real waterfallShare)
{
	if (waterfallShare < 0.1f) {
		m_waterfallShare = 0.1f;
	}
	else if (waterfallShare > 0.8f) {
		m_waterfallShare = 0.8f;
	} else {
		m_waterfallShare = waterfallShare;
	}
	m_changesPending = true;
}

void GLSpectrum::connectTimer(const QTimer& timer)
{
	qDebug() << "GLSpectrum::connectTimer";
	disconnect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
	connect(&timer, SIGNAL(timeout()), this, SLOT(tick()));
	m_timer.stop();
}

void GLSpectrum::cleanup()
{
	//makeCurrent();
	m_glShaderSimple.cleanup();
	m_glShaderFrequencyScale.cleanup();
	m_glShaderHistogram.cleanup();
	m_glShaderLeftScale.cleanup();
	m_glShaderWaterfall.cleanup();
    //doneCurrent();
}
