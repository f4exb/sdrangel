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

#ifdef USE_SIMD
#include <immintrin.h>
#endif

#include <QMouseEvent>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions>
#include "gui/glspectrum.h"

#include <QDebug>

#ifdef GL_ANDROID
#include "util/gleshelp.h"
#endif

GLSpectrum::GLSpectrum(QWidget* parent) :
	QGLWidget(parent),
	m_cursorState(CSNormal),
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
#ifdef GL_DEPRECATED
	m_leftMarginTextureAllocated(false),
	m_frequencyTextureAllocated(false),
#endif
	m_waterfallBuffer(NULL),
	m_waterfallTextureAllocated(false),
	m_waterfallTextureHeight(-1),
	m_displayWaterfall(true),
	m_ssbSpectrum(false),
	m_histogramBuffer(NULL),
	m_histogram(NULL),
	m_histogramHoldoff(NULL),
	m_histogramTextureAllocated(false),
	m_displayHistogram(true),
	m_displayChanged(false)
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
	QMutexLocker mutexLocker(&m_mutex);

	m_changesPending = true;

	if(m_waterfallBuffer != NULL) {
		delete m_waterfallBuffer;
		m_waterfallBuffer = NULL;
	}
	if(m_waterfallTextureAllocated) {
		makeCurrent();
		deleteTexture(m_waterfallTexture);
		m_waterfallTextureAllocated = false;
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
	if(m_histogramTextureAllocated) {
		makeCurrent();
		deleteTexture(m_histogramTexture);
		m_histogramTextureAllocated = false;
	}
#ifdef GL_DEPRECATED
	if(m_leftMarginTextureAllocated) {
		deleteTexture(m_leftMarginTexture);
		m_leftMarginTextureAllocated = false;
	}
	if(m_frequencyTextureAllocated) {
		deleteTexture(m_frequencyTexture);
		m_frequencyTextureAllocated = false;
	}
#endif
}

void GLSpectrum::setCenterFrequency(quint64 frequency)
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

	connect(channelMarker, SIGNAL(changed()), this, SLOT(channelMarkerChanged()));
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

#ifndef USE_SIMD
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
#else
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
	glDisable(GL_DEPTH_TEST);
	m_glShaderSimple.initializeGL();
	m_glShaderLeftScale.initializeGL();
	m_glShaderFrequencyScale.initializeGL();
}

void GLSpectrum::resizeGL(int width, int height)
{
	glViewport(0, 0, width, height);

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

	glPushMatrix();
	glScalef(2.0, -2.0, 1.0);
	glTranslatef(-0.50, -0.5, 0);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// paint waterfall
	if (m_displayWaterfall)
	{
		glPushMatrix();
		glTranslatef(m_glWaterfallRect.x(), m_glWaterfallRect.y(), 0);
		glScalef(m_glWaterfallRect.width(), m_glWaterfallRect.height(), 1);

		glBindTexture(GL_TEXTURE_2D, m_waterfallTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

		for(int i = 0; i < m_waterfallBufferPos; i++) {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, m_waterfallTexturePos, m_fftSize, 1, GL_RGBA, GL_UNSIGNED_BYTE, m_waterfallBuffer->scanLine(i));
			m_waterfallTexturePos = (m_waterfallTexturePos + 1) % m_waterfallTextureHeight;
		}

		m_waterfallBufferPos = 0;

		float prop_y = m_waterfallTexturePos / (m_waterfallTextureHeight - 1.0);
		float off = 1.0 / (m_waterfallTextureHeight - 1.0);
		glEnable(GL_TEXTURE_2D);

#ifdef GL_DEPRECATED
		glBegin(GL_QUADS);
		glTexCoord2f(0, prop_y + 1 - off);
		glVertex2f(0, m_invertedWaterfall ? 0 : 1);
		glTexCoord2f(1, prop_y + 1 - off);
		glVertex2f(1, m_invertedWaterfall ? 0 : 1);
		glTexCoord2f(1, prop_y);
		glVertex2f(1, m_invertedWaterfall ? 1 : 0);
		glTexCoord2f(0, prop_y);
		glVertex2f(0, m_invertedWaterfall ? 1 : 0);
		glEnd();
#else
		{
			GLfloat vtx1[] = {
					0, m_invertedWaterfall ? 0.0f : 1.0f,
		    		1, m_invertedWaterfall ? 0.0f : 1.0f,
		    		1, m_invertedWaterfall ? 1.0f : 0.0f,
		    		0, m_invertedWaterfall ? 1.0f : 0.0f
		    };
			GLfloat tex1[] = {
		    		0, prop_y + 1 - off,
					1, prop_y + 1 - off,
					1, prop_y,
					0, prop_y
		    };

#ifdef GL_ANDROID
			glEnableVertexAttribArray(GL_VERTEX_ARRAY);
			glEnableVertexAttribArray(GL_TEXTURE_COORD_ARRAY);
			glVertexAttribPointer(GL_VERTEX_ARRAY, 2, GL_FLOAT, GL_FALSE, 0, vtx1);
			glVertexAttribPointer(GL_TEXTURE_COORD_ARRAY, 2, GL_FLOAT, GL_FALSE, 0, tex1);
			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
			glDisableVertexAttribArray(GL_VERTEX_ARRAY);
			glDisableVertexAttribArray(GL_TEXTURE_COORD_ARRAY);
#else
			glEnableClientState(GL_VERTEX_ARRAY);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glVertexPointer(2, GL_FLOAT, 0, vtx1);
			glTexCoordPointer(2, GL_FLOAT, 0, tex1);
			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
#endif
		}
#endif
		glDisable(GL_TEXTURE_2D);
		glPopMatrix();

		// paint channels
		if (m_mouseInside)
		{
			for (int i = 0; i < m_channelMarkerStates.size(); ++i)
			{
				ChannelMarkerState* dv = m_channelMarkerStates[i];
				if (dv->m_channelMarker->getVisible())
				{
#ifdef GL_DEPRECATED
					glPushMatrix();
					glTranslatef(m_glWaterfallRect.x(), m_glWaterfallRect.y(), 0);
					glScalef(m_glWaterfallRect.width(), m_glWaterfallRect.height(), 1);

					glPushMatrix();
					glTranslatef(dv->m_glRect.x(), dv->m_glRect.y(), 0);
					glScalef(dv->m_glRect.width(), dv->m_glRect.height(), 1);
					glEnable(GL_BLEND);
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					glColor4f(dv->m_channelMarker->getColor().redF(), dv->m_channelMarker->getColor().greenF(), dv->m_channelMarker->getColor().blueF(), 0.3f);

					glBegin(GL_QUADS);
					glVertex2f(0, 0);
					glVertex2f(1, 0);
					glVertex2f(1, 1);
					glVertex2f(0, 1);
					glEnd();

					glDisable(GL_BLEND);
					glPopMatrix();
					glPopMatrix();
#else
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
#endif
				}
			}
		}

		// draw rect around
#ifdef GL_DEPRECATED
		glPushMatrix();
		glTranslatef(m_glWaterfallRect.x(), m_glWaterfallRect.y(), 0);
		glScalef(m_glWaterfallRect.width(), m_glWaterfallRect.height(), 1);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glLineWidth(1.0f);
		glColor4f(1, 1, 1, 0.5);

		glBegin(GL_LINE_LOOP);
		glVertex2f(1, 1);
		glVertex2f(0, 1);
		glVertex2f(0, 0);
		glVertex2f(1, 0);
		glEnd();
		glDisable(GL_BLEND);
		glPopMatrix();
#else
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
#endif
	}

	// paint histogram
	if(m_displayHistogram || m_displayMaxHold || m_displayCurrent)
	{
		glPushMatrix();
		glTranslatef(m_glHistogramRect.x(), m_glHistogramRect.y(), 0);
		glScalef(m_glHistogramRect.width(), m_glHistogramRect.height(), 1);

		if(m_displayHistogram)
		{
			// import new lines into the texture
			quint32* pix;
			quint8* bs = m_histogram;
			for(int y = 0; y < 100; y++) {
				quint8* b = bs;
				pix = (quint32*)m_histogramBuffer->scanLine(99 - y);
				for(int x = 0; x < m_fftSize; x++) {
					*pix = m_histogramPalette[*b];
					pix++;
					b += 100;
				}
				bs++;
			}

			// draw texture
			glBindTexture(GL_TEXTURE_2D, m_histogramTexture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_fftSize, 100, GL_RGBA, GL_UNSIGNED_BYTE, m_histogramBuffer->scanLine(0));
			glEnable(GL_TEXTURE_2D);
#ifdef GL_DEPRECATED
			glBegin(GL_QUADS);
			glTexCoord2f(0, 0);
			glVertex2f(0, 0);
			glTexCoord2f(1, 0);
			glVertex2f(1, 0);
			glTexCoord2f(1, 1);
			glVertex2f(1, 1);
			glTexCoord2f(0, 1);
			glVertex2f(0, 1);
			glEnd();
#else
			{
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
#ifdef GL_ANDROID
				glEnableVertexAttribArray(GL_VERTEX_ARRAY);
				glEnableVertexAttribArray(GL_TEXTURE_COORD_ARRAY);
				glVertexAttribPointer(GL_VERTEX_ARRAY, 2, GL_FLOAT, GL_FALSE, 0, vtx1);
				glVertexAttribPointer(GL_TEXTURE_COORD_ARRAY, 2, GL_FLOAT, GL_FALSE, 0, tex1);
				glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
				glDisableVertexAttribArray(GL_VERTEX_ARRAY);
				glDisableVertexAttribArray(GL_TEXTURE_COORD_ARRAY);
#else
				glEnableClientState(GL_VERTEX_ARRAY);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				glVertexPointer(2, GL_FLOAT, 0, vtx1);
				glTexCoordPointer(2, GL_FLOAT, 0, tex1);
				glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
				glDisableClientState(GL_VERTEX_ARRAY);
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
#endif
			}
#endif
			glDisable(GL_TEXTURE_2D);
		}

		glPopMatrix();

		// paint channels
		if(m_mouseInside)
		{
			// Effective BW overlays
			for(int i = 0; i < m_channelMarkerStates.size(); ++i)
			{
				ChannelMarkerState* dv = m_channelMarkerStates[i];
				if(dv->m_channelMarker->getVisible())
				{
#ifdef GL_DEPRECATED
					glPushMatrix();
					glTranslatef(m_glHistogramRect.x(), m_glHistogramRect.y(), 0);
					glScalef(m_glHistogramRect.width(), m_glHistogramRect.height(), 1);
					glPushMatrix();
					glTranslatef(dv->m_glRect.x(), dv->m_glRect.y(), 0);
					glScalef(dv->m_glRect.width(), dv->m_glRect.height(), 1);
					glEnable(GL_BLEND);
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					glColor4f(dv->m_channelMarker->getColor().redF(), dv->m_channelMarker->getColor().greenF(), dv->m_channelMarker->getColor().blueF(), 0.3f);
					glBegin(GL_QUADS);
					glVertex2f(0, 0);
					glVertex2f(1, 0);
					glVertex2f(1, 1);
					glVertex2f(0, 1);
					glEnd();
					glPopMatrix();
					glPopMatrix();
#else
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

						m_glShaderSimple.drawSegments(dv->m_glMatrixDsbHistogram, colorLine, &q3[4], 4);
					}
#endif
				}
			}
		}

		// draw rect around
#ifdef GL_DEPRECATED
		glPushMatrix();
		glTranslatef(m_glHistogramRect.x(), m_glHistogramRect.y(), 0);
		glScalef(m_glHistogramRect.width(), m_glHistogramRect.height(), 1);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glLineWidth(1.0f);
		glColor4f(1, 1, 1, 0.5);
		glBegin(GL_LINE_LOOP);
		glVertex2f(1, 1);
		glVertex2f(0, 1);
		glVertex2f(0, 0);
		glVertex2f(1, 0);
		glEnd();
		glDisable(GL_BLEND);
		glPopMatrix();
#else
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
#endif
	}

	// paint left scales (time and power)
	if (m_displayWaterfall || m_displayMaxHold || m_displayCurrent || m_displayHistogram )
	{
#ifdef GL_DEPRECATED
		glPushMatrix();
		glTranslatef(m_glLeftScaleRect.x(), m_glLeftScaleRect.y(), 0);
		glScalef(m_glLeftScaleRect.width(), m_glLeftScaleRect.height(), 1);

		glBindTexture(GL_TEXTURE_2D, m_leftMarginTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glEnable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
		glTexCoord2f(0, 1);
		glVertex2f(0, 1);
		glTexCoord2f(1, 1);
		glVertex2f(1, 1);
		glTexCoord2f(1, 0);
		glVertex2f(1, 0);
		glTexCoord2f(0, 0);
		glVertex2f(0, 0);
		glEnd();
		glDisable(GL_TEXTURE_2D);

		glPopMatrix();
#else
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
#endif
	}

	// paint frequency scale
	if (m_displayWaterfall || m_displayMaxHold || m_displayCurrent || m_displayHistogram )
	{
#ifdef GL_DEPRECATED
		glPushMatrix();
		glTranslatef(m_glFrequencyScaleRect.x(), m_glFrequencyScaleRect.y(), 0);
		glScalef(m_glFrequencyScaleRect.width(), m_glFrequencyScaleRect.height(), 1);

		glBindTexture(GL_TEXTURE_2D, m_frequencyTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_TEXTURE_2D);
		glBegin(GL_QUADS);
		glTexCoord2f(0, 1);
		glVertex2f(0, 1);
		glTexCoord2f(1, 1);
		glVertex2f(1, 1);
		glTexCoord2f(1, 0);
		glVertex2f(1, 0);
		glTexCoord2f(0, 0);
		glVertex2f(0, 0);
		glEnd();
		glDisable(GL_TEXTURE_2D);

		glPopMatrix();
#else
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
#endif

		// paint channels

		// Effective bandwidth overlays
		for(int i = 0; i < m_channelMarkerStates.size(); ++i)
		{
			ChannelMarkerState* dv = m_channelMarkerStates[i];

			// frequency scale channel overlay
			if(dv->m_channelMarker->getVisible())
			{
#ifdef GL_DEPRECATED
				glPushMatrix();
				glTranslatef(m_glWaterfallRect.x(), m_glFrequencyScaleRect.y(), 0);
				glScalef(m_glWaterfallRect.width(), m_glFrequencyScaleRect.height(), 1);
				glPushMatrix();
				glTranslatef(dv->m_glRect.x(), dv->m_glRect.y(), 0);
				glScalef(dv->m_glRect.width(), dv->m_glRect.height(), 1);
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glColor4f(dv->m_channelMarker->getColor().redF(), dv->m_channelMarker->getColor().greenF(), dv->m_channelMarker->getColor().blueF(), 0.5f);
				glBegin(GL_QUADS);
				glVertex2f(0, 0);
				glVertex2f(1, 0);
				glVertex2f(1, 0.5);
				glVertex2f(0, 0.5);
				glEnd();
				glDisable(GL_BLEND);
				glPopMatrix();
				glPopMatrix();
#else
				{
					GLfloat q3[] {
						1, 0.5,
						0, 0.5,
						0, 0,
						1, 0,
						0.5, 0,
						0.5, 1
					};

					QVector4D color(dv->m_channelMarker->getColor().redF(), dv->m_channelMarker->getColor().greenF(), dv->m_channelMarker->getColor().blueF(), 0.5f);
					m_glShaderSimple.drawSurface(dv->m_glMatrixFreqScale, color, q3, 4);

					if (dv->m_channelMarker->getHighlighted())
					{
						if (dv->m_channelMarker->getSidebands() != ChannelMarker::dsb) {
							q3[4] = 0.5;
						}
						QVector4D colorLine(0.8f, 0.8f, 0.6f, 1.0f);
						m_glShaderSimple.drawSegments(dv->m_glMatrixDsbFreqScale, colorLine, &q3[4], 4);
					}
				}
#endif
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

#ifdef GL_DEPRECATED
		glPushMatrix();
		glTranslatef(m_glHistogramRect.x(), m_glHistogramRect.y(), 0);
		glScalef(m_glHistogramRect.width() / (float)(m_fftSize - 1), -m_glHistogramRect.height() / m_powerRange, 1);

		Real bottom = -m_powerRange;
		glColor4f(1, 0, 0, m_displayTraceIntensity / 100.0);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glLineWidth(1.0f);

		//glEnable(GL_LINE_SMOOTH);
		glBegin(GL_LINE_STRIP);
		for(int i = 0; i < m_fftSize; i++) {
			Real v = m_maxHold[i] - m_referenceLevel;
			if(v > 0)
				v = 0;
			else if(v < bottom)
				v = bottom;
			glVertex2f(i, v);
		}
		glEnd();
		//glDisable(GL_LINE_SMOOTH);
		glPopMatrix();
#else
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
#endif
	}

	// paint current spectrum line on top of histogram
	if ((m_displayCurrent) && m_currentSpectrum) {
#ifdef GL_DEPRECATED
		glPushMatrix();
		glTranslatef(m_glHistogramRect.x(), m_glHistogramRect.y(), 0);
		glScalef(m_glHistogramRect.width() / (float)(m_fftSize - 1), -m_glHistogramRect.height() / m_powerRange, 1);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glLineWidth(1.0f);
		glColor4f(1.0f, 1.0f, 0.25f, m_displayTraceIntensity / 100.0); // intense yellow
		Real bottom = -m_powerRange;

		{
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

			glEnableClientState(GL_VERTEX_ARRAY);
			glVertexPointer(2, GL_FLOAT, 0, q3);
			glDrawArrays(GL_LINE_STRIP, 0, m_fftSize);
			glDisableClientState(GL_VERTEX_ARRAY);
		}
		/*
		glBegin(GL_LINE_STRIP);
		for(int i = 0; i < m_fftSize; i++) {
			Real v = (*m_currentSpectrum)[i] - m_referenceLevel;
			if(v > 0)
				v = 0;
			else if(v < bottom)
				v = bottom;
			glVertex2f(i, v);
		}
		glEnd();*/
		glPopMatrix();
#else
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
#endif
	}

	// paint waterfall grid
	if(m_displayWaterfall && m_displayGrid)
	{
		const ScaleEngine::TickList* tickList;
		const ScaleEngine::Tick* tick;
		tickList = &m_timeScale.getTickList();

#ifdef GL_DEPRECATED
		glPushMatrix();
		glTranslatef(m_glWaterfallRect.x(), m_glWaterfallRect.y(), 0);
		glScalef(m_glWaterfallRect.width(), m_glWaterfallRect.height(), 1);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glLineWidth(1.0f);
		glColor4f(1, 1, 1, m_displayGridIntensity / 100.0);

		for(int i= 0; i < tickList->count(); i++) {
			tick = &(*tickList)[i];
			if(tick->major) {
				if(tick->textSize > 0) {
					float y = tick->pos / m_timeScale.getSize();
					glBegin(GL_LINE_LOOP);
					glVertex2f(0, y);
					glVertex2f(1, y);
					glEnd();
				}
			}
		}

		glPopMatrix();
#else
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
#endif
		tickList = &m_frequencyScale.getTickList();

#ifdef GL_DEPRECATED
		glPushMatrix();
		glTranslatef(m_glWaterfallRect.x(), m_glWaterfallRect.y(), 0);
		glScalef(m_glWaterfallRect.width(), m_glWaterfallRect.height(), 1);

		for(int i= 0; i < tickList->count(); i++) {
			tick = &(*tickList)[i];
			if(tick->major) {
				if(tick->textSize > 0) {
					float x = tick->pos / m_frequencyScale.getSize();
					glBegin(GL_LINE_LOOP);
					glVertex2f(x, 0);
					glVertex2f(x, 1);
					glEnd();
				}
			}
		}

		glPopMatrix();
#else
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
#endif
	}

	// paint histogram grid
	if((m_displayHistogram || m_displayMaxHold || m_displayCurrent) && (m_displayGrid))
	{
		const ScaleEngine::TickList* tickList;
		const ScaleEngine::Tick* tick;
		tickList = &m_powerScale.getTickList();

#ifdef GL_DEPRECATED
		glPushMatrix();
		glTranslatef(m_glHistogramRect.x(), m_glHistogramRect.y(), 0);
		glScalef(m_glHistogramRect.width(), m_glHistogramRect.height(), 1);

		for (int i= 0; i < tickList->count(); i++)
		{
			tick = &(*tickList)[i];
			if(tick->major)
			{
				if(tick->textSize > 0)
				{
					float y = tick->pos / m_powerScale.getSize();
					glBegin(GL_LINE_LOOP);
					glVertex2f(0, 1-y);
					glVertex2f(1, 1-y);
					glEnd();
				}
			}
		}

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glLineWidth(1.0f);
		glColor4f(1, 1, 1, m_displayGridIntensity / 100.0);

		glPopMatrix();
#else
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
#endif
		tickList = &m_frequencyScale.getTickList();

#ifdef GL_DEPRECATED
		glPushMatrix();
		glTranslatef(m_glHistogramRect.x(), m_glHistogramRect.y(), 0);
		glScalef(m_glHistogramRect.width(), m_glHistogramRect.height(), 1);

		for(int i= 0; i < tickList->count(); i++) {
			tick = &(*tickList)[i];
			if(tick->major) {
				if(tick->textSize > 0) {
					float x = tick->pos / m_frequencyScale.getSize();
					glBegin(GL_LINE_LOOP);
					glVertex2f(x, 0);
					glVertex2f(x, 1);
					glEnd();
				}
			}
		}

		glPopMatrix();
#else
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
#endif
	}

	glPopMatrix();
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
	int frequencyScaleTop;
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

		m_glWaterfallRect = QRectF(
			(float)leftMargin / (float)width(),
			(float)waterfallTop / (float)height(),
			(float)(width() - leftMargin - rightMargin) / (float)width(),
			(float)waterfallHeight / (float)height()
		);

		m_glWaterfallBoxMatrix.setToIdentity();
		m_glWaterfallBoxMatrix.translate(
			-1.0f + ((float)(2*leftMargin)   / (float) width()),
			 1.0f - ((float)(2*waterfallTop) / (float) height())
		);
		m_glWaterfallBoxMatrix.scale(
			((float) 2 * (width() - leftMargin - rightMargin)) / (float) width(),
			(float) (-2*waterfallHeight) / (float) height()
		);

		m_glHistogramRect = QRectF(
			(float)leftMargin / (float)width(),
			(float)histogramTop / (float)height(),
			(float)(width() - leftMargin - rightMargin) / (float)width(),
			(float)histogramHeight / (float)height()
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
#ifdef GL_DEPRECATED
		m_glFrequencyScaleRect = QRectF(
			(float)0,
			(float)frequencyScaleTop / (float)height(),
			(float)1,
			(float)frequencyScaleHeight / (float)height()
		);
#endif
		m_glFrequencyScaleBoxMatrix.setToIdentity();
		m_glFrequencyScaleBoxMatrix.translate (
			-1.0f,
			 1.0f - ((float) 2*frequencyScaleTop / (float) height())
		);
		m_glFrequencyScaleBoxMatrix.scale (
			2.0f,
			(float) -2*frequencyScaleHeight / (float) height()
		);

#ifdef GL_DEPRECATED
		m_glLeftScaleRect = QRectF(
			(float)0,
			(float)0,
			(float)(leftMargin - 1) / (float)width(),
			(float)1
		);
#endif

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

		m_glWaterfallRect = QRectF(
			(float)leftMargin / (float)width(),
			(float)topMargin / (float)height(),
			(float)(width() - leftMargin - rightMargin) / (float)width(),
			(float)waterfallHeight / (float)height()
		);

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
#ifdef GL_DEPRECATED
		m_glFrequencyScaleRect = QRectF(
			(float)0,
			(float)frequencyScaleTop / (float)height(),
			(float)1,
			(float)frequencyScaleHeight / (float)height()
		);
#endif
		m_glFrequencyScaleBoxMatrix.setToIdentity();
		m_glFrequencyScaleBoxMatrix.translate (
			-1.0f,
			 1.0f - ((float) 2*frequencyScaleTop / (float) height())
		);
		m_glFrequencyScaleBoxMatrix.scale (
			2.0f,
			(float) -2*frequencyScaleHeight / (float) height()
		);

#ifdef GL_DEPRECATED
		m_glLeftScaleRect = QRectF(
			(float)0,
			(float)0,
			(float)(leftMargin - 1) / (float)width(),
			(float)1
		);
#endif

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

		m_glHistogramRect = QRectF(
			(float)leftMargin / (float)width(),
			(float)histogramTop / (float)height(),
			(float)(width() - leftMargin - rightMargin) / (float)width(),
			(float)(height() - topMargin - frequencyScaleHeight) / (float)height()
		);

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
#ifdef GL_DEPRECATED
		m_glFrequencyScaleRect = QRectF(
			(float)0,
			(float)frequencyScaleTop / (float)height(),
			(float)1,
			(float)frequencyScaleHeight / (float)height()
		);
#endif
		m_glFrequencyScaleBoxMatrix.setToIdentity();
		m_glFrequencyScaleBoxMatrix.translate (
			-1.0f,
			 1.0f - ((float) 2*frequencyScaleTop / (float) height())
		);
		m_glFrequencyScaleBoxMatrix.scale (
			2.0f,
			(float) -2*frequencyScaleHeight / (float) height()
		);

#ifdef GL_DEPRECATED
		m_glLeftScaleRect = QRectF(
			(float)0,
			(float)0,
			(float)(leftMargin - 1) / (float)width(),
			(float)1
		);
#endif
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
		} else {
			pw = dsbw / 2;
			nw = -pw;
		}


		// draw the DSB rectangle
#ifdef GL_DEPRECATED
		dv->m_glRectDsb.setRect(
			m_frequencyScale.getPosFromValue(xc - (dsbw/2)) / (float)(width() - leftMargin - rightMargin),
			0,
			dsbw / (float)m_sampleRate,
			1);
#endif
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
#ifdef GL_DEPRECATED
		dv->m_glRect.setRect(
			m_frequencyScale.getPosFromValue(xc + nw) / (float)(width() - leftMargin - rightMargin),
			0,
			(pw-nw) / (float)m_sampleRate,
			1);
#endif
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
#ifdef GL_DEPRECATED
		if(m_leftMarginTextureAllocated)
			deleteTexture(m_leftMarginTexture);
		m_leftMarginTexture = bindTexture(m_leftMarginPixmap,
			GL_TEXTURE_2D,
			GL_RGBA,
			QGLContext::LinearFilteringBindOption |
			QGLContext::MipmapBindOption);
		m_leftMarginTextureAllocated = true;
#endif
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
					QString ftext = QString::number((m_centerFrequency + dv->m_channelMarker->getCenterFrequency())/1e6, 'f', 6);
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
#ifdef GL_DEPRECATED
		if(m_frequencyTextureAllocated)
			deleteTexture(m_frequencyTexture);
		m_frequencyTexture = bindTexture(m_frequencyPixmap,
			GL_TEXTURE_2D,
			GL_RGBA,
			QGLContext::LinearFilteringBindOption |
			QGLContext::MipmapBindOption);
		m_frequencyTextureAllocated = true;
#endif
		m_glShaderFrequencyScale.initTexture(m_frequencyPixmap.toImage());
	}

	if(!m_waterfallTextureAllocated) {
		glGenTextures(1, &m_waterfallTexture);
		m_waterfallTextureAllocated = true;
	}
	if(!m_histogramTextureAllocated) {
		glGenTextures(1, &m_histogramTexture);
		m_histogramTextureAllocated = true;
	}

	bool fftSizeChanged = true;
	if(m_waterfallBuffer != NULL)
		fftSizeChanged = m_waterfallBuffer->width() != m_fftSize;
	bool windowSizeChanged = m_waterfallTextureHeight != waterfallHeight;

	if(fftSizeChanged) {
		if(m_waterfallBuffer != NULL) {
			delete m_waterfallBuffer;
			m_waterfallBuffer = NULL;
		}
		m_waterfallBuffer = new QImage(m_fftSize, 256, QImage::Format_ARGB32);
		if(m_waterfallBuffer != NULL) {
			m_waterfallBuffer->fill(qRgb(0x00, 0x00, 0x00));
			m_waterfallBufferPos = 0;
		} else {
			m_fftSize = 0;
			m_changesPending = true;
			return;
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

		m_histogramBuffer = new QImage(m_fftSize, 100, QImage::Format_RGB32);
		if(m_histogramBuffer != NULL) {
			m_histogramBuffer->fill(qRgb(0x00, 0x00, 0x00));
		} else {
			m_fftSize = 0;
			m_changesPending = true;
			return;
		}

		m_histogram = new quint8[100 * m_fftSize];
		memset(m_histogram, 0x00, 100 * m_fftSize);
		m_histogramHoldoff = new quint8[100 * m_fftSize];
		memset(m_histogramHoldoff, 0x07, 100 * m_fftSize);

		quint8* data = new quint8[m_fftSize * 100 * 4];
		memset(data, 0x00, m_fftSize * 100 * 4);
		glBindTexture(GL_TEXTURE_2D, m_histogramTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_fftSize, 100, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		delete[] data;
	}

	if(fftSizeChanged || windowSizeChanged) {
		m_waterfallTextureHeight = waterfallHeight;
		quint8* data = new quint8[m_fftSize * m_waterfallTextureHeight * 4];
		memset(data, 0x00, m_fftSize * m_waterfallTextureHeight * 4);
		glBindTexture(GL_TEXTURE_2D, m_waterfallTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_fftSize, m_waterfallTextureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		delete[] data;
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

	if(m_cursorState == CSSplitterMoving) {
		float newShare;
		if(!m_invertedWaterfall)
			newShare = (float)(event->y() - m_frequencyScaleRect.height()) / (float)height();
		else newShare = 1.0 - (float)(event->y() + m_frequencyScaleRect.height()) / (float)height();
		if(newShare < 0.1)
			newShare = 0.1f;
		else if(newShare > 0.8)
			newShare = 0.8f;
		m_waterfallShare = newShare;
		m_changesPending = true;
		update();
		return;
	} else if(m_cursorState == CSChannelMoving) {
		Real freq = m_frequencyScale.getValueFromPos(event->x() - m_leftMarginPixmap.width() - 1) - m_centerFrequency;
		if(m_channelMarkerStates[m_cursorChannel]->m_channelMarker->getColor()!=Qt::blue)
			m_channelMarkerStates[m_cursorChannel]->m_channelMarker->setCenterFrequency(freq);
	}

	if(m_displayWaterfall || m_displayHistogram || m_displayMaxHold || m_displayCurrent) {
		for(int i = 0; i < m_channelMarkerStates.size(); ++i) {
			if(m_channelMarkerStates[i]->m_rect.contains(event->pos())) {
				if(m_cursorState == CSNormal) {
					setCursor(Qt::SizeHorCursor);
					m_cursorState = CSChannel;
					m_cursorChannel = i;
					m_channelMarkerStates[i]->m_channelMarker->setHighlighted(true);
					return;
				} else if(m_cursorState == CSChannel) {
					return;
				}
			} else if (m_channelMarkerStates[i]->m_channelMarker->getHighlighted()) {
				m_channelMarkerStates[i]->m_channelMarker->setHighlighted(false);
			}
		}
	}
	if(m_cursorState == CSChannel) {
		setCursor(Qt::ArrowCursor);
		m_cursorState = CSNormal;
		return;
	}
}

void GLSpectrum::mousePressEvent(QMouseEvent* event)
{
	if(event->button() != 1)
		return;

	if(m_cursorState == CSSplitter) {
		grabMouse();
		m_cursorState = CSSplitterMoving;
		return;
	} else if(m_cursorState == CSChannel) {
		grabMouse();
		m_cursorState = CSChannelMoving;
		return;
	} else if((m_cursorState == CSNormal) && (m_channelMarkerStates.size() == 1)) {
		grabMouse();
		setCursor(Qt::SizeHorCursor);
		m_cursorState = CSChannelMoving;
		m_cursorChannel = 0;
		Real freq = m_frequencyScale.getValueFromPos(event->x() - m_leftMarginPixmap.width() - 1) - m_centerFrequency;
		if(m_channelMarkerStates[m_cursorChannel]->m_channelMarker->getColor()!=Qt::blue)
			m_channelMarkerStates[m_cursorChannel]->m_channelMarker->setCenterFrequency(freq);
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
	makeCurrent();
	m_glShaderSimple.cleanup();
    doneCurrent();
}
