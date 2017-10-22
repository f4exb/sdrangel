///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// OpenGL interface modernization.                                               //
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

#include <QPainter>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSurface>
#include "gui/glscope.h"

#include <algorithm>
#include <QDebug>

#undef M_PI
#define M_PI  3.14159265358979323846

/*
#ifdef _WIN32
static double log2f(double n)
{
	return log(n) / log(2.0);
}
#endif*/

GLScope::GLScope(QWidget* parent) :
	QGLWidget(parent),
	m_dataChanged(false),
	m_configChanged(true),
	m_mode(ModeIQ),
	m_displays(DisplayBoth),
	m_orientation(Qt::Horizontal),
	m_memTraceIndex(0),
	m_memTraceHistory(0),
	m_memTraceIndexMax(0),
	m_memTraceRecall(false),
	m_displayTrace(&m_rawTrace[0]),
	//m_amp(1.0),
	//m_ofs(0.0),
	m_maxPow(0.0f),
	m_sumPow(0.0f),
    m_oldTraceSize(-1),
    m_sampleRate(0),
    m_amp1(1.0),
    m_amp2(1.0),
    m_ofs1(0.0),
    m_ofs2(0.0),
    m_timeBase(1),
    m_timeOfsProMill(0),
    m_triggerChannel(ScopeVis::TriggerFreeRun),
    m_triggerLevel(0.0),
    m_triggerPre(0.0),
    m_triggerLevelDis1(0.0),
    m_triggerLevelDis2(0.0),
    m_nbPow(1),
    m_prevArg(0),
    m_displayGridIntensity(5),
    m_displayTraceIntensity(50),
    m_powerOverlayFont(font())
{
	setAttribute(Qt::WA_OpaquePaintEvent);
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
	m_timer.start(50);
	m_y1Scale.setFont(font());
	m_y1Scale.setOrientation(Qt::Vertical);
	m_y2Scale.setFont(font());
	m_y2Scale.setOrientation(Qt::Vertical);
	m_x1Scale.setFont(font());
	m_x1Scale.setOrientation(Qt::Horizontal);
	m_x2Scale.setFont(font());
	m_x2Scale.setOrientation(Qt::Horizontal);
	m_powerOverlayFont.setBold(true);
	m_powerOverlayFont.setPointSize(font().pointSize()+1);
}

GLScope::~GLScope()
{
	cleanup();
}

void GLScope::setSampleRate(int sampleRate) {
	m_sampleRates[m_memTraceIndex-m_memTraceHistory] = sampleRate;
	m_configChanged = true;
	update();
	emit sampleRateChanged(m_sampleRates[m_memTraceIndex-m_memTraceHistory]);
}

void GLScope::setAmp1(Real amp)
{
	qDebug("GLScope::setAmp1: %f", amp);
	m_amp1 = amp;
	m_configChanged = true;
	update();
}

void GLScope::setAmp1Ofs(Real ampOfs)
{
	qDebug("GLScope::setAmp1Ofs: %f", ampOfs);
	m_ofs1 = ampOfs;
	m_configChanged = true;
	update();
}

void GLScope::setAmp2(Real amp)
{
	qDebug("GLScope::setAmp2: %f", amp);
	m_amp2 = amp;
	m_configChanged = true;
	update();
}

void GLScope::setAmp2Ofs(Real ampOfs)
{
	qDebug("GLScope::setAmp2Ofs: %f", ampOfs);
	m_ofs2 = ampOfs;
	m_configChanged = true;
	update();
}

void GLScope::setTimeBase(int timeBase)
{
	m_timeBase = timeBase;
	m_configChanged = true;
	update();
}

void GLScope::setTimeOfsProMill(int timeOfsProMill)
{
	m_timeOfsProMill = timeOfsProMill;
	m_configChanged = true;
	update();
}

void GLScope::setMode(Mode mode)
{
	m_mode = mode;
	m_dataChanged = true;
	m_configChanged = true;
	update();
}

void GLScope::setDisplays(Displays displays)
{
	m_displays = displays;
	m_dataChanged = true;
	m_configChanged = true;
	update();
}

void GLScope::setOrientation(Qt::Orientation orientation)
{
	m_orientation = orientation;
	m_configChanged = true;
	update();
}

void GLScope::setDisplayGridIntensity(int intensity)
{
	m_displayGridIntensity = intensity;
	if (m_displayGridIntensity > 100) {
		m_displayGridIntensity = 100;
	} else if (m_displayGridIntensity < 0) {
		m_displayGridIntensity = 0;
	}
	update();
}

void GLScope::setDisplayTraceIntensity(int intensity)
{
	m_displayTraceIntensity = intensity;
	if (m_displayTraceIntensity > 100) {
		m_displayTraceIntensity = 100;
	} else if (m_displayTraceIntensity < 0) {
		m_displayTraceIntensity = 0;
	}
	update();
}

void GLScope::newTrace(const std::vector<Complex>& trace, int sampleRate)
{
	if (!m_memTraceRecall)
	{
		if(!m_mutex.tryLock(2))
			return;
		if(m_dataChanged) {
			m_mutex.unlock();
			return;
		}

		m_memTraceIndex++;
		m_rawTrace[m_memTraceIndex] = trace;
		m_sampleRates[m_memTraceIndex] = sampleRate;

		if(m_memTraceIndexMax < (1<<m_memHistorySizeLog2))
		{
			m_memTraceIndexMax++;
		}

		//m_sampleRate = sampleRate; // sampleRate comes from scopeVis
		m_dataChanged = true;

		m_mutex.unlock();
	}
}

void GLScope::initializeGL()
{
	QOpenGLContext *glCurrentContext =  QOpenGLContext::currentContext();
	//QOpenGLContext *glCurrentContext =  context();

	if (glCurrentContext) {
		if (QOpenGLContext::currentContext()->isValid()) {
			qDebug() << "GLScope::initializeGL: context:"
				<< " major: " << (QOpenGLContext::currentContext()->format()).majorVersion()
				<< " minor: " << (QOpenGLContext::currentContext()->format()).minorVersion()
				<< " ES: " << (QOpenGLContext::currentContext()->isOpenGLES() ? "yes" : "no");
		}
		else {
			qDebug() << "GLScope::initializeGL: current context is invalid";
		}
	} else {
		qCritical() << "GLScope::initializeGL: no current context";
		return;
	}

	QSurface *surface = glCurrentContext->surface();

	if (surface == 0)
	{
		qCritical() << "GLScope::initializeGL: no surface attached";
		return;
	}
	else
	{
		if (surface->surfaceType() != QSurface::OpenGLSurface)
		{
			qCritical() << "GLScope::initializeGL: surface is not an OpenGLSurface: " << surface->surfaceType()
				<< " cannot use an OpenGL context";
			return;
		}
		else
		{
			qDebug() << "GLScope::initializeGL: OpenGL surface:"
				<< " class: " << (surface->surfaceClass() == QSurface::Window ? "Window" : "Offscreen");
		}
	}

	connect(glCurrentContext, &QOpenGLContext::aboutToBeDestroyed, this, &GLScope::cleanup); // TODO: when migrating to QOpenGLWidget

	QOpenGLFunctions *glFunctions = QOpenGLContext::currentContext()->functions();
	glFunctions->initializeOpenGLFunctions();

	//glDisable(GL_DEPTH_TEST);
	m_glShaderSimple.initializeGL();
	m_glShaderLeft1Scale.initializeGL();
	m_glShaderBottom1Scale.initializeGL();
	m_glShaderLeft2Scale.initializeGL();
	m_glShaderBottom2Scale.initializeGL();
	m_glShaderPowerOverlay.initializeGL();
}

void GLScope::resizeGL(int width, int height)
{
	QOpenGLFunctions *glFunctions = QOpenGLContext::currentContext()->functions();
	glFunctions->glViewport(0, 0, width, height);
	m_configChanged = true;
}

void GLScope::paintGL()
{
	if(!m_mutex.tryLock(2))
		return;

	if(m_configChanged)
		applyConfig();

	handleMode();

	if(m_displayTrace->size() - m_oldTraceSize != 0) {
		m_oldTraceSize = m_displayTrace->size();
		emit traceSizeChanged((int) m_displayTrace->size());
	}

	QOpenGLFunctions *glFunctions = QOpenGLContext::currentContext()->functions();
	glFunctions->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glFunctions->glClear(GL_COLOR_BUFFER_BIT);

	// I - primary display

	if ((m_displays == DisplayBoth) || (m_displays == DisplayFirstOnly))
	{
		// draw rect around
		{
			GLfloat q3[] {
				1, 1,
				0, 1,
				0, 0,
				1, 0
			};

			QVector4D color(1.0f, 1.0f, 1.0f, 0.5f);
			m_glShaderSimple.drawContour(m_glScopeMatrix1, color, q3, 4);

		}

		// paint grid
		const ScaleEngine::TickList* tickList;
		const ScaleEngine::Tick* tick;

		// Horizontal Y1
		tickList = &m_y1Scale.getTickList();

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
						float y = 1 - (tick->pos / m_y1Scale.getSize());
						q3[4*effectiveTicks] = 0;
						q3[4*effectiveTicks+1] = y;
						q3[4*effectiveTicks+2] = 1;
						q3[4*effectiveTicks+3] = y;
						effectiveTicks++;
					}
				}
			}

			float blue = (m_mode == ModeIQPolar ? 0.25f : 1.0f);
			QVector4D color(1.0f, 1.0f, blue, (float) m_displayGridIntensity / 100.0f);
			m_glShaderSimple.drawSegments(m_glScopeMatrix1, color, q3, 2*effectiveTicks);
		}

		{
			// Vertical X1
			tickList = &m_x1Scale.getTickList();

			GLfloat q3[4*tickList->count()];
			int effectiveTicks = 0;
			for(int i= 0; i < tickList->count(); i++) {
				tick = &(*tickList)[i];
				if(tick->major) {
					if(tick->textSize > 0) {
						float x = tick->pos / m_x1Scale.getSize();
						q3[4*effectiveTicks] = x;
						q3[4*effectiveTicks+1] = 0;
						q3[4*effectiveTicks+2] = x;
						q3[4*effectiveTicks+3] = 1;
						effectiveTicks++;
					}
				}
			}

			QVector4D color(1.0f, 1.0f, 1.0f, (float) m_displayGridIntensity / 100.0f);
			m_glShaderSimple.drawSegments(m_glScopeMatrix1, color, q3, 2*effectiveTicks);
		}

		// paint left #1 scale
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

			m_glShaderLeft1Scale.drawSurface(m_glLeft1ScaleMatrix, tex1, vtx1, 4);
		}

		// paint bottom #1 scale
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

			m_glShaderBottom1Scale.drawSurface(m_glBot1ScaleMatrix, tex1, vtx1, 4);
		}

		// paint trigger level #1
		if ((m_triggerChannel == ScopeVis::TriggerChannelI)
				|| (m_triggerChannel == ScopeVis::TriggerMagLin)
				|| (m_triggerChannel == ScopeVis::TriggerMagDb)
				)
		{
			float posLimit = 1.0 / m_amp1;
			float negLimit = -1.0 / m_amp1;

			if ((m_triggerLevelDis1 > negLimit) && (m_triggerLevelDis1 < posLimit))
			{
				GLfloat q3[] {
					0, m_triggerLevelDis1,
					1, m_triggerLevelDis1
				};

				float rectX = m_glScopeRect1.x();
				float rectY = m_glScopeRect1.y() + m_glScopeRect1.height() / 2.0f;
				float rectW = m_glScopeRect1.width();
				float rectH = -(m_glScopeRect1.height() / 2.0f) * m_amp1;

				QVector4D color(0.0f, 1.0f, 0.0f, 0.4f);
				QMatrix4x4 mat;
				mat.setToIdentity();
				mat.translate(-1.0f + 2.0f * rectX, 1.0f - 2.0f * rectY);
				mat.scale(2.0f * rectW, -2.0f * rectH);
				m_glShaderSimple.drawSegments(mat, color, q3, 2);

//				glPushMatrix();
//				glTranslatef(m_glScopeRect1.x(), m_glScopeRect1.y() + m_glScopeRect1.height() / 2.0, 0);
//				glScalef(m_glScopeRect1.width(), -(m_glScopeRect1.height() / 2) * m_amp1, 1);
			}
		}

		// paint trace #1
		if(m_displayTrace->size() > 0)
		{
			{
				int start = (m_timeOfsProMill/1000.0) * m_displayTrace->size();
				int end = std::min(start + m_displayTrace->size()/m_timeBase, m_displayTrace->size());
				if(end - start < 2)
					start--;
				float posLimit = 1.0 / m_amp1;
				float negLimit = -1.0 / m_amp1;

				GLfloat q3[2*(end -start)];

				for (int i = start; i < end; i++)
				{
					float v = (*m_displayTrace)[i].real();
					if(v > posLimit)
						v = posLimit;
					else if(v < negLimit)
						v = negLimit;

					q3[2*(i-start)] = i - start;
					q3[2*(i-start) + 1] = v;

					if ((m_mode == ModeMagdBPha) || (m_mode == ModeMagdBDPha))
					{
						if (i == start)
						{
							m_maxPow = m_powTrace[i];
							m_sumPow = m_powTrace[i];
						}
						else
						{
							if (m_powTrace[i] > m_maxPow)
							{
								m_maxPow = m_powTrace[i];
							}

							m_sumPow += m_powTrace[i];
						}
					}
				}

				float rectX = m_glScopeRect1.x();
				float rectY = m_glScopeRect1.y() + m_glScopeRect1.height() / 2.0f;
				float rectW = m_glScopeRect1.width() * (float)m_timeBase / (float)(m_displayTrace->size() - 1);
				float rectH = -(m_glScopeRect1.height() / 2.0f) * m_amp1;

				QVector4D color(1.0f, 1.0f, 0.25f, m_displayTraceIntensity / 100.0f);
				QMatrix4x4 mat;
				mat.setToIdentity();
				mat.translate(-1.0f + 2.0f * rectX, 1.0f - 2.0f * rectY);
				mat.scale(2.0f * rectW, -2.0f * rectH);
				m_glShaderSimple.drawPolyline(mat, color, q3, end -start);

//				glPushMatrix();
//				glTranslatef(m_glScopeRect1.x(), m_glScopeRect1.y() + m_glScopeRect1.height() / 2.0, 0);
//				glScalef(m_glScopeRect1.width() * (float)m_timeBase / (float)(m_displayTrace->size() - 1), -(m_glScopeRect1.height() / 2) * m_amp1, 1);
				m_nbPow = end - start;
			}
		}

		// Paint powers overlays

		if ((m_mode == ModeMagdBPha) || (m_mode == ModeMagdBDPha))
		{
			if (m_nbPow > 0)
			{
				drawPowerOverlay();
			}
		}

		if (m_mode == ModeIQPolar)
		{
			// Paint trace 2 (Q) over
			if (m_displayTrace->size() > 0)
			{
				{
					int start = (m_timeOfsProMill/1000.0) * m_displayTrace->size();
					int end = std::min(start + m_displayTrace->size()/m_timeBase, m_displayTrace->size());

					if(end - start < 2) {
						start--;
					}

					float posLimit = 1.0 / m_amp2;
					float negLimit = -1.0 / m_amp2;

					GLfloat q3[2*(end - start)];

					for(int i = start; i < end; i++)
					{
						float v = (*m_displayTrace)[i].imag();
						if(v > posLimit)
							v = posLimit;
						else if(v < negLimit)
							v = negLimit;
						q3[2*(i-start)] = i - start;
						q3[2*(i-start) + 1] = v;
					}

					float rectX = m_glScopeRect1.x();
					float rectY = m_glScopeRect1.y() + m_glScopeRect1.height() / 2.0f;
					float rectW = m_glScopeRect1.width() * (float)m_timeBase / (float)(m_displayTrace->size() - 1);
					float rectH = -(m_glScopeRect1.height() / 2.0f) * m_amp2;

					QVector4D color(0.25f, 1.0f, 1.0f, m_displayTraceIntensity / 100.0);
					QMatrix4x4 mat;
					mat.setToIdentity();
					mat.translate(-1.0f + 2.0f * rectX, 1.0f - 2.0f * rectY);
					mat.scale(2.0f * rectW, -2.0f * rectH);
					m_glShaderSimple.drawPolyline(mat, color, q3, end -start);

//					glPushMatrix();
//					glTranslatef(m_glScopeRect1.x(), m_glScopeRect1.y() + m_glScopeRect1.height() / 2.0, 0);
//					glScalef(m_glScopeRect1.width() * (float)m_timeBase / (float)(m_displayTrace->size() - 1), -(m_glScopeRect1.height() / 2) * m_amp2, 1);
				}
			}

			// Paint secondary grid
			// draw rect around
			const ScaleEngine::TickList* tickList;
			const ScaleEngine::Tick* tick;
			// Horizontal Y2
			tickList = &m_y2Scale.getTickList();
			{
				GLfloat q3[4*tickList->count()];
				int effectiveTicks = 0;
				for(int i= 0; i < tickList->count(); i++) {
					tick = &(*tickList)[i];
					if(tick->major) {
						if(tick->textSize > 0) {
							float y = 1 - (tick->pos / m_y2Scale.getSize());
							q3[4*effectiveTicks] = 0;
							q3[4*effectiveTicks+1] = y;
							q3[4*effectiveTicks+2] = 1;
							q3[4*effectiveTicks+3] = y;
							effectiveTicks++;
						}
					}
				}

				QVector4D color(0.25f, 1.0f, 1.0f, (float) m_displayGridIntensity / 100.0f);
				m_glShaderSimple.drawSegments(m_glScopeMatrix1, color, q3, 2*effectiveTicks);
			}

			// Paint secondary scale
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

				m_glShaderLeft2Scale.drawSurface(m_glRight1ScaleMatrix, tex1, vtx1, 4);
			}
		}
	} // Both displays or primary only

	// Q - secondary display

	if ((m_displays == DisplayBoth) || (m_displays == DisplaySecondOnly))
	{
		// draw rect around
		{
			GLfloat q3[] {
				1, 1,
				0, 1,
				0, 0,
				1, 0
			};

			QVector4D color(1.0f, 1.0f, 1.0f, 0.5f);
			m_glShaderSimple.drawContour(m_glScopeMatrix2, color, q3, 4);
		}

		// paint grid
		const ScaleEngine::TickList* tickList;
		const ScaleEngine::Tick* tick;

		// Horizontal Y2
		tickList = &m_y2Scale.getTickList();
		{
			GLfloat q3[4*tickList->count()];
			int effectiveTicks = 0;
			for(int i= 0; i < tickList->count(); i++) {
				tick = &(*tickList)[i];
				if(tick->major) {
					if(tick->textSize > 0) {
						float y = 1 - (tick->pos / m_y2Scale.getSize());
						q3[4*effectiveTicks] = 0;
						q3[4*effectiveTicks+1] = y;
						q3[4*effectiveTicks+2] = 1;
						q3[4*effectiveTicks+3] = y;
						effectiveTicks++;
					}
				}
			}

			QVector4D color(1.0f, 1.0f, 1.0f, (float) m_displayGridIntensity / 100.0f);
			m_glShaderSimple.drawSegments(m_glScopeMatrix2, color, q3, 2*effectiveTicks);
		}

		// Vertical X2
		tickList = &m_x2Scale.getTickList();
		{
			GLfloat q3[4*tickList->count()];
			int effectiveTicks = 0;
			for(int i= 0; i < tickList->count(); i++) {
				tick = &(*tickList)[i];
				if(tick->major) {
					if(tick->textSize > 0) {
						float x = tick->pos / m_x2Scale.getSize();
						q3[4*effectiveTicks] = x;
						q3[4*effectiveTicks+1] = 0;
						q3[4*effectiveTicks+2] = x;
						q3[4*effectiveTicks+3] = 1;
						effectiveTicks++;
					}
				}
			}

			QVector4D color(1.0f, 1.0f, 1.0f, (float) m_displayGridIntensity / 100.0f);
			m_glShaderSimple.drawSegments(m_glScopeMatrix2, color, q3, 2*effectiveTicks);
		}

		// paint left #2 scale
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

			m_glShaderLeft2Scale.drawSurface(m_glLeft2ScaleMatrix, tex1, vtx1, 4);
		}

		// paint bottom #2 scale
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

			m_glShaderBottom2Scale.drawSurface(m_glBot2ScaleMatrix, tex1, vtx1, 4);
		}

		// paint trigger level #2
		if ((m_triggerChannel == ScopeVis::TriggerPhase)
				|| (m_triggerChannel == ScopeVis::TriggerDPhase)
				|| (m_triggerChannel == ScopeVis::TriggerChannelQ))
		{
			float posLimit = 1.0 / m_amp2;
			float negLimit = -1.0 / m_amp2;

			if ((m_triggerLevelDis2 > negLimit) && (m_triggerLevelDis2 < posLimit))
			{
				GLfloat q3[] {
					0, m_triggerLevelDis2,
					1, m_triggerLevelDis2
				};

				float rectX = m_glScopeRect2.x();
				float rectY = m_glScopeRect2.y() + m_glScopeRect2.height() / 2.0f;
				float rectW = m_glScopeRect2.width();
				float rectH = -(m_glScopeRect2.height() / 2.0f) * m_amp2;

				QVector4D color(0.0f, 1.0f, 0.0f, 0.4f);
				QMatrix4x4 mat;
				mat.setToIdentity();
				mat.translate(-1.0f + 2.0f * rectX, 1.0f - 2.0f * rectY);
				mat.scale(2.0f * rectW, -2.0f * rectH);
				m_glShaderSimple.drawSegments(mat, color, q3, 2);
			}
		}

		// paint trace #2
		if(m_displayTrace->size() > 0)
		{
			if (m_mode == ModeIQPolar)
			{
				int start = (m_timeOfsProMill/1000.0) * m_displayTrace->size();
				int end = std::min(start + m_displayTrace->size()/m_timeBase, m_displayTrace->size());

				if (end - start < 2) {
					start--;
				}
				{
					GLfloat q3[2*(end - start)];

					for(int i = start; i < end; i++)
					{
						float x = (*m_displayTrace)[i].real() * m_amp1;
						float y = (*m_displayTrace)[i].imag() * m_amp2;
						if(x > 1.0f)
							x = 1.0f;
						else if(x < -1.0f)
							x = -1.0f;
						if(y > 1.0f)
							y = 1.0f;
						else if(y < -1.0f)
							y = -1.0f;
						q3[2*(i-start)] = x;
						q3[2*(i-start)+1] = y;
					}

					float rectX = m_glScopeRect2.x() + m_glScopeRect2.width() / 2.0f;
					float rectY = m_glScopeRect2.y() + m_glScopeRect2.height() / 2.0f;
					float rectW = m_glScopeRect2.width() / 2.0f;
					float rectH = -(m_glScopeRect2.height() / 2.0f);

					QVector4D color(1.0f, 1.0f, 0.25f, m_displayTraceIntensity / 100.0f);
					QMatrix4x4 mat;
					mat.setToIdentity();
					mat.translate(-1.0f + 2.0f * rectX, 1.0f - 2.0f * rectY);
					mat.scale(2.0f * rectW, -2.0f * rectH);
					m_glShaderSimple.drawPolyline(mat, color, q3, end -start);

//					glPushMatrix();
//					glTranslatef(m_glScopeRect2.x() + m_glScopeRect2.width() / 2.0, m_glScopeRect2.y() + m_glScopeRect2.height() / 2.0, 0);
//					glScalef(m_glScopeRect2.width() / 2, -(m_glScopeRect2.height() / 2), 1);
				}
			}
			else
			{
				{
					int start = (m_timeOfsProMill/1000.0) * m_displayTrace->size();
					int end = std::min(start + m_displayTrace->size()/m_timeBase, m_displayTrace->size());

					if (end - start < 2) {
						start--;
					}

					float posLimit = 1.0 / m_amp2;
					float negLimit = -1.0 / m_amp2;

					GLfloat q3[2*(end - start)];

					for(int i = start; i < end; i++) {
						float v = (*m_displayTrace)[i].imag();
						if(v > posLimit)
							v = posLimit;
						else if(v < negLimit)
							v = negLimit;

						q3[2*(i-start)] = i - start;
						q3[2*(i-start)+1] = v;
					}

					float rectX = m_glScopeRect2.x();
					float rectY = m_glScopeRect2.y() + m_glScopeRect2.height() / 2.0f;
					float rectW = m_glScopeRect2.width() * (float)m_timeBase / (float)(m_displayTrace->size() - 1);
					float rectH = -(m_glScopeRect2.height() / 2.0f) * m_amp2;

					QVector4D color(1.0f, 1.0f, 0.25f, m_displayTraceIntensity / 100.0f);
					QMatrix4x4 mat;
					mat.setToIdentity();
					mat.translate(-1.0f + 2.0f * rectX, 1.0f - 2.0f * rectY);
					mat.scale(2.0f * rectW, -2.0f * rectH);
					m_glShaderSimple.drawPolyline(mat, color, q3, end -start);

//					glPushMatrix();
//					glTranslatef(m_glScopeRect2.x(), m_glScopeRect2.y() + m_glScopeRect2.height() / 2.0, 0);
//					glScalef(m_glScopeRect2.width() * (float)m_timeBase / (float)(m_displayTrace->size() - 1), -(m_glScopeRect2.height() / 2) * m_amp2, 1);
				}
			}
		}
	} // Both displays or secondary display only

//	glPopMatrix();
	m_dataChanged = false;
	m_mutex.unlock();
}

void GLScope::mousePressEvent(QMouseEvent* event __attribute__((unused)))
{
#if 0
	int x = event->x() - 10;
	int y = event->y() - 10;
	Real time;
	Real amplitude;
	ScopeVis::TriggerChannel channel;
	if((x < 0) || (x >= width() - 20))
		return;
	if((y < 0) || (y >= height() - 20))
		return;

	if((m_sampleRate != 0) && (m_timeBase != 0) && (width() > 20))
		time = ((Real)x * (Real)m_displayTrace->size()) / ((Real)m_sampleRate * (Real)m_timeBase * (Real)(width() - 20));
	else time = -1.0;

	if(y < (height() - 30) / 2) {
		channel = ScopeVis::TriggerChannelI;
		if((m_amp != 0) && (height() > 30))
			amplitude = 2.0 * ((height() - 30) * 0.25 - (Real)y) / (m_amp * (height() - 30) / 2.0);
		else amplitude = -1;
	} else if(y > (height() - 30) / 2 + 10) {
		y -= 10 + (height() - 30) / 2;
		channel = ScopeVis::TriggerChannelQ;
		if((m_amp != 0) && (height() > 30))
			amplitude = 2.0 * ((height() - 30) * 0.25 - (Real)y) / (m_amp * (height() - 30) / 2.0);
		else amplitude = -1;
	} else {
		channel = ScopeVis::TriggerFreeRun;
	}

	if(m_dspEngine != NULL) {
		qDebug("amp %f", amplitude);
		m_triggerLevel = amplitude + 0.01 / m_amp;
		m_triggerLevelLow = amplitude - 0.01 / m_amp;
		if(m_triggerLevel > 1.0)
			m_triggerLevel = 1.0;
		else if(m_triggerLevel < -1.0)
			m_triggerLevel = -1.0;
		if(m_triggerLevelLow > 1.0)
			m_triggerLevelLow = 1.0;
		else if(m_triggerLevelLow < -1.0)
			m_triggerLevelLow = -1.0;
		m_scopeVis->configure(m_dspEngine->getMessageQueue(), channel, m_triggerLevel, m_triggerLevelLow);
		m_triggerChannel = channel;
		m_changed = true;
		update();
	}
#endif
}

void GLScope::handleMode()
{
	BitfieldIndex<m_memHistorySizeLog2> memIndex = m_memTraceIndex - m_memTraceHistory;

	switch(m_mode) {
		case ModeIQ:
		case ModeIQPolar:
		{
			m_mathTrace.resize(m_rawTrace[memIndex].size());
			std::vector<Complex>::iterator dst = m_mathTrace.begin();
			m_displayTrace = &m_rawTrace[memIndex];

			for(std::vector<Complex>::const_iterator src = m_rawTrace[memIndex].begin(); src != m_rawTrace[memIndex].end(); ++src) {
				*dst++ = Complex(src->real() - m_ofs1, src->imag() - m_ofs2);
			}

			m_triggerLevelDis1 = m_triggerLevel - m_ofs1;
			m_triggerLevelDis2 = m_triggerLevel - m_ofs2;

			m_displayTrace = &m_mathTrace;

			break;
		}
		case ModeMagLinPha:
		{
			m_mathTrace.resize(m_rawTrace[memIndex].size());
			std::vector<Complex>::iterator dst = m_mathTrace.begin();

			for(std::vector<Complex>::const_iterator src = m_rawTrace[memIndex].begin(); src != m_rawTrace[memIndex].end(); ++src)
			{
				*dst++ = Complex(abs(*src) - m_ofs1/2.0 - 1.0/m_amp1, (arg(*src) / M_PI) - m_ofs2);
			}

			m_triggerLevelDis1 = (m_triggerLevel + 1) - m_ofs1/2.0 - 1.0/m_amp1;
			m_triggerLevelDis2 = m_triggerLevel - m_ofs2;

			m_displayTrace = &m_mathTrace;

			break;
		}
		case ModeMagdBPha:
		{
			m_mathTrace.resize(m_rawTrace[memIndex].size());
			m_powTrace.resize(m_rawTrace[memIndex].size());
			std::vector<Complex>::iterator dst = m_mathTrace.begin();
			std::vector<Real>::iterator powDst = m_powTrace.begin();

			for(std::vector<Complex>::const_iterator src = m_rawTrace[memIndex].begin(); src != m_rawTrace[memIndex].end(); ++src) {
				Real v = src->real() * src->real() + src->imag() * src->imag();
				*powDst++ = v;
				v = 1.0f + 2.0f*(((10.0f*log10f(v))/100.0f) - m_ofs1)  + 1.0f - 1.0f/m_amp1;
				*dst++ = Complex(v, (arg(*src) / M_PI) - m_ofs2);
			}

			Real tdB = (m_triggerLevel - 1) * 100.0f;
			m_triggerLevelDis1 = 1.0f + 2.0f*(((tdB)/100.0f) - m_ofs1)  + 1.0f - 1.0f/m_amp1;
			m_triggerLevelDis2 = m_triggerLevel - m_ofs2;

			m_displayTrace = &m_mathTrace;

			break;
		}
		case ModeMagLinDPha:
		{
			m_mathTrace.resize(m_rawTrace[memIndex].size());
			std::vector<Complex>::iterator dst = m_mathTrace.begin();
			Real curArg;

			for(std::vector<Complex>::const_iterator src = m_rawTrace[memIndex].begin(); src != m_rawTrace[memIndex].end(); ++src)
			{
				curArg = arg(*src) - m_prevArg;

				if (curArg < -M_PI) {
					curArg += 2.0 * M_PI;
				} else if (curArg > M_PI) {
					curArg -= 2.0 * M_PI;
				}

				*dst++ = Complex(abs(*src) - m_ofs1/2.0 - 1.0/m_amp1, (curArg / M_PI) - m_ofs2);
				m_prevArg = arg(*src);
			}

			m_triggerLevelDis1 = (m_triggerLevel + 1) - m_ofs1/2.0 - 1.0/m_amp1;
			m_triggerLevelDis2 = m_triggerLevel - m_ofs2;

			m_displayTrace = &m_mathTrace;

			break;
		}
		case ModeMagdBDPha:
		{
			m_mathTrace.resize(m_rawTrace[memIndex].size());
			m_powTrace.resize(m_rawTrace[memIndex].size());
			std::vector<Complex>::iterator dst = m_mathTrace.begin();
			std::vector<Real>::iterator powDst = m_powTrace.begin();
			Real curArg;

			for(std::vector<Complex>::const_iterator src = m_rawTrace[memIndex].begin(); src != m_rawTrace[memIndex].end(); ++src)
			{
				Real v = src->real() * src->real() + src->imag() * src->imag();
				*powDst++ = v;
				v = 1.0f + 2.0f*(((10.0f*log10f(v))/100.0f) - m_ofs1)  + 1.0f - 1.0f/m_amp1;
				curArg = arg(*src) - m_prevArg;

				if (curArg < -M_PI) {
					curArg += 2.0 * M_PI;
				} else if (curArg > M_PI) {
					curArg -= 2.0 * M_PI;
				}

				*dst++ = Complex(v, (curArg / M_PI) - m_ofs2);
				m_prevArg = arg(*src);
			}

			Real tdB = (m_triggerLevel - 1) * 100.0f;
			m_triggerLevelDis1 = 1.0f + 2.0f*(((tdB)/100.0f) - m_ofs1)  + 1.0f - 1.0f/m_amp1;
			m_triggerLevelDis2 = m_triggerLevel - m_ofs2;

			m_displayTrace = &m_mathTrace;

			break;
		}
		case ModeDerived12:
		{
			if(m_rawTrace[memIndex].size() > 3)
			{
				m_mathTrace.resize(m_rawTrace[memIndex].size() - 3);
				std::vector<Complex>::iterator dst = m_mathTrace.begin();

				for(uint i = 3; i < m_rawTrace[memIndex].size() ; i++)
				{
					*dst++ = Complex(
						abs(m_rawTrace[memIndex][i] - m_rawTrace[memIndex][i - 1]),
						abs(m_rawTrace[memIndex][i] - m_rawTrace[memIndex][i - 1]) - abs(m_rawTrace[memIndex][i - 2] - m_rawTrace[0][i - 3]));
				}

				m_displayTrace = &m_mathTrace;
			}

			break;
		}
		case ModeCyclostationary:
		{
			if(m_rawTrace[0].size() > 2)
			{
				m_mathTrace.resize(m_rawTrace[memIndex].size() - 2);
				std::vector<Complex>::iterator dst = m_mathTrace.begin();

				for(uint i = 2; i < m_rawTrace[memIndex].size() ; i++)
					*dst++ = Complex(abs(m_rawTrace[memIndex][i] - conj(m_rawTrace[memIndex][i - 1])), 0);

				m_displayTrace = &m_mathTrace;
			}

			break;
		}
	}
}

void GLScope::drawPowerOverlay()
{
	double maxPow = 10.0f * log10f(m_maxPow);
	double avgPow = 10.0f * log10f(m_sumPow / m_nbPow);
	double peakToAvgPow = maxPow - avgPow;

	QString text = QString("%1  %2  %3").arg(maxPow, 0, 'f', 1).arg(avgPow, 0, 'f', 1).arg(peakToAvgPow, 0, 'f', 1);

	QFontMetricsF metrics(m_powerOverlayFont);
	QRectF rect = metrics.boundingRect(text);
	m_powerOverlayPixmap1 = QPixmap(rect.width() + 4.0f, rect.height());
	m_powerOverlayPixmap1.fill(Qt::transparent);
	QPainter painter(&m_powerOverlayPixmap1);
	painter.setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing, false);
	painter.fillRect(rect, QColor(0, 0, 0, 0x80));
	painter.setPen(QColor(0xff, 0xff, 0xff, 0x80));
	painter.setFont(m_powerOverlayFont);
	painter.drawText(QPointF(0, rect.height() - 2.0f), text);
	painter.end();

	m_glShaderPowerOverlay.initTexture(m_powerOverlayPixmap1.toImage());

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

		float shiftX = m_glScopeRect1.width() - ((rect.width() + 4.0f) / width());
		float rectX = m_glScopeRect1.x() + shiftX;
		float rectY = 0;
		float rectW = rect.width() / (float) width();
		float rectH = rect.height() / (float) height();

		QMatrix4x4 mat;
		mat.setToIdentity();
		mat.translate(-1.0f + 2.0f * rectX, 1.0f - 2.0f * rectY);
		mat.scale(2.0f * rectW, -2.0f * rectH);
		m_glShaderPowerOverlay.drawSurface(mat, tex1, vtx1, 4);

//		glPushMatrix();
//		glTranslatef(m_glScopeRect1.x() + shiftX, m_glScopeRect1.y(), 0);
//		glScalef(rect.width() / (float) width(), rect.height() / (float) height(), 1);
	}
}

void GLScope::applyConfig()
{
	m_configChanged = false;

	QFontMetrics fm(font());
	int M = fm.width("-");

	int topMargin = 5;
	int botMargin = 20;
	int leftMargin = 35;
	int rightMargin = 5;

    float pow_floor = -100.0 + m_ofs1 * 100.0;
    float pow_range = 100.0 / m_amp1;
    float amp1_range = 2.0 / m_amp1;
    float amp1_ofs = m_ofs1;
    float amp2_range = 2.0 / m_amp2;
    float amp2_ofs = m_ofs2;
    float t_start = ((m_timeOfsProMill / 1000.0) - m_triggerPre) * ((float) m_displayTrace->size() / m_sampleRates[m_memTraceIndex-m_memTraceHistory]);
    float t_len = ((float) m_displayTrace->size() / m_sampleRates[m_memTraceIndex-m_memTraceHistory]) / (float) m_timeBase;

    m_x1Scale.setRange(Unit::Time, t_start, t_start + t_len);

    if (m_mode == ModeIQPolar)
    {
		if (amp1_range < 2.0) {
			m_x2Scale.setRange(Unit::None, - amp1_range * 500.0 + amp1_ofs * 1000.0, amp1_range * 500.0 + amp1_ofs * 1000.0);
		} else {
			m_x2Scale.setRange(Unit::None, - amp1_range * 0.5 + amp1_ofs, amp1_range * 0.5 + amp1_ofs);
		}
    }
    else
    {
    	m_x2Scale.setRange(Unit::Time, t_start, t_start + t_len);
    }

	switch(m_mode) {
		case ModeIQ:
		case ModeIQPolar:
		{
			if (amp1_range < 2.0) {
				m_y1Scale.setRange(Unit::None, - amp1_range * 500.0 + amp1_ofs * 1000.0, amp1_range * 500.0 + amp1_ofs * 1000.0);
			} else {
				m_y1Scale.setRange(Unit::None, - amp1_range * 0.5 + amp1_ofs, amp1_range * 0.5 + amp1_ofs);
			}
			if (amp2_range < 2.0) {
				m_y2Scale.setRange(Unit::None, - amp2_range * 500.0 + amp2_ofs * 1000.0, amp2_range * 500.0 + amp2_ofs * 1000.0);
			} else {
				m_y2Scale.setRange(Unit::None, - amp2_range * 0.5 + amp2_ofs, amp2_range * 0.5 + amp2_ofs);
			}

			break;
		}
		case ModeMagLinPha:
		case ModeMagLinDPha:
		{
			if (amp1_range < 2.0) {
				m_y1Scale.setRange(Unit::None, amp1_ofs * 500.0, amp1_range * 1000.0 + amp1_ofs * 500.0);
			} else {
				m_y1Scale.setRange(Unit::None, amp1_ofs/2.0, amp1_range + amp1_ofs/2.0);
			}

			m_y2Scale.setRange(Unit::None, -1.0/m_amp2 + amp2_ofs, 1.0/m_amp2 + amp2_ofs); // Scale to Pi*A2

			break;
		}
		case ModeMagdBPha:
		case ModeMagdBDPha:
		{
			m_y1Scale.setRange(Unit::Decibel, pow_floor, pow_floor + pow_range);
			m_y2Scale.setRange(Unit::None, -1.0/m_amp2 + amp2_ofs, 1.0/m_amp2 + amp2_ofs); // Scale to Pi*A2
			break;
		}
		case ModeDerived12: {
			if (amp1_range < 2.0) {
				m_y1Scale.setRange(Unit::None, 0.0, amp1_range * 1000.0);
			} else {
				m_y1Scale.setRange(Unit::None, 0.0, amp1_range);
			}
			if (amp2_range < 2.0) {
				m_y2Scale.setRange(Unit::None, - amp2_range * 500.0, amp2_range * 500.0);
			} else {
				m_y2Scale.setRange(Unit::None, - amp2_range * 0.5, amp2_range * 0.5);
			}
			break;
		}
		case ModeCyclostationary: {
			if (amp1_range < 2.0) {
				m_y1Scale.setRange(Unit::None, 0.0, amp1_range * 1000.0);
			} else {
				m_y1Scale.setRange(Unit::None, 0.0, amp1_range);
			}
			if (amp2_range < 2.0) {
				m_y2Scale.setRange(Unit::None, - amp2_range * 500.0, amp2_range * 500.0);
			} else {
				m_y2Scale.setRange(Unit::None, - amp2_range * 0.5, amp2_range * 0.5);
			}
			break;
		}
	}

    // QRectF(x, y, w, h); (x, y) = top left corner

	if (m_displays == DisplayBoth)
	{
		if(m_orientation == Qt::Vertical) {
			int scopeHeight = (height() - topMargin) / 2 - botMargin;
			int scopeWidth = width() - leftMargin - rightMargin;

			if (m_mode == ModeIQPolar)
			{
				m_glScopeRect1 = QRectF(
					(float) leftMargin / (float) width(),
					(float) topMargin / (float) height(),
					(float) (width() - 2*leftMargin - rightMargin) / (float) width(),
					(float) scopeHeight / (float) height()
				);
				m_glScopeMatrix1.setToIdentity();
				m_glScopeMatrix1.translate (
					-1.0f + ((float) 2*leftMargin / (float) width()),
					 1.0f - ((float) 2*topMargin / (float) height())
				);
				m_glScopeMatrix1.scale (
					(float) 2*(width() - 2*leftMargin - rightMargin) / (float) width(),
					(float) -2*scopeHeight / (float) height()
				);

				m_glBot1ScaleMatrix.setToIdentity();
				m_glBot1ScaleMatrix.translate (
					-1.0f + ((float) 2*leftMargin / (float) width()),
					 1.0f - ((float) 2*(scopeHeight + topMargin + 1) / (float) height())
				);
				m_glBot1ScaleMatrix.scale (
					(float) 2*(width() - 2*leftMargin - rightMargin) / (float) width(),
					(float) -2*(botMargin - 1) / (float) height()
				);

				m_glRight1ScaleMatrix.setToIdentity();
				m_glRight1ScaleMatrix.translate (
					-1.0f + ((float)(2*(width() - leftMargin)) / (float) width()),
					 1.0f - ((float) 2*topMargin / (float) height())
				);
				m_glRight1ScaleMatrix.scale (
					(float) 2*(leftMargin-1) / (float) width(),
					(float) -2*scopeHeight / (float) height()
				);
			}
			else
			{
				m_glScopeRect1 = QRectF(
					(float) leftMargin / (float) width(),
					(float) topMargin / (float) height(),
					(float) scopeWidth / (float) width(),
					(float) scopeHeight / (float) height()
				);
				m_glScopeMatrix1.setToIdentity();
				m_glScopeMatrix1.translate (
					-1.0f + ((float) 2*leftMargin / (float) width()),
					 1.0f - ((float) 2*topMargin / (float) height())
				);
				m_glScopeMatrix1.scale (
					(float) 2*scopeWidth / (float) width(),
					(float) -2*scopeHeight / (float) height()
				);

				m_glBot1ScaleMatrix.setToIdentity();
				m_glBot1ScaleMatrix.translate (
					-1.0f + ((float) 2*leftMargin / (float) width()),
					 1.0f - ((float) 2*(scopeHeight + topMargin + 1) / (float) height())
				);
				m_glBot1ScaleMatrix.scale (
					(float) 2*scopeWidth / (float) width(),
					(float) -2*(botMargin - 1) / (float) height()
				);
			}

			m_glLeft1ScaleMatrix.setToIdentity();
			m_glLeft1ScaleMatrix.translate (
				-1.0f,
				 1.0f - ((float) 2*topMargin / (float) height())
			);
			m_glLeft1ScaleMatrix.scale (
				(float) 2*(leftMargin-1) / (float) width(),
				(float) -2*scopeHeight / (float) height()
			);

			{ // Y1 scale
				m_y1Scale.setSize(scopeHeight);

				m_left1ScalePixmap = QPixmap(
					leftMargin - 1,
					scopeHeight
				);

				const ScaleEngine::TickList* tickList;
				const ScaleEngine::Tick* tick;

				m_left1ScalePixmap.fill(Qt::black);
				QPainter painter(&m_left1ScalePixmap);
				painter.setPen(QColor(0xf0, 0xf0, 0xff));
				painter.setFont(font());
				tickList = &m_y1Scale.getTickList();

				for(int i = 0; i < tickList->count(); i++) {
					tick = &(*tickList)[i];
					if(tick->major) {
						if(tick->textSize > 0) {
							painter.drawText(QPointF(leftMargin - M - tick->textSize, topMargin + scopeHeight - tick->textPos - fm.ascent()/2), tick->text);
						}
					}
				}

				m_glShaderLeft1Scale.initTexture(m_left1ScalePixmap.toImage());

			} // Y1 scale
			{ // X1 scale
				m_x1Scale.setSize(scopeWidth);

				m_bot1ScalePixmap = QPixmap(
					scopeWidth,
					botMargin - 1
				);

				const ScaleEngine::TickList* tickList;
				const ScaleEngine::Tick* tick;

				m_bot1ScalePixmap.fill(Qt::black);
				QPainter painter(&m_bot1ScalePixmap);
				painter.setPen(QColor(0xf0, 0xf0, 0xff));
				painter.setFont(font());
				tickList = &m_x1Scale.getTickList();

				for(int i = 0; i < tickList->count(); i++) {
					tick = &(*tickList)[i];
					if(tick->major) {
						if(tick->textSize > 0) {
							painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
						}
					}
				}

				m_glShaderBottom1Scale.initTexture(m_bot1ScalePixmap.toImage());

			} // X1 scale

			if (m_mode == ModeIQPolar)
			{
				int scopeDim = std::min(scopeWidth, scopeHeight);

				m_glScopeRect2 = QRectF(
					(float) leftMargin / (float)width(),
					(float) (botMargin + topMargin + scopeDim) / (float)height(),
					(float) scopeDim / (float)width(),
					(float) scopeDim / (float)height()
				);
				m_glScopeMatrix2.setToIdentity();
				m_glScopeMatrix2.translate (
					-1.0f + ((float) 2*leftMargin / (float) width()),
					 1.0f - ((float) 2*(botMargin + topMargin + scopeDim) / (float) height())
				);
				m_glScopeMatrix2.scale (
					(float) 2*scopeDim / (float) width(),
					(float) -2*scopeDim / (float) height()
				);

				m_glLeft2ScaleMatrix.setToIdentity();
				m_glLeft2ScaleMatrix.translate (
					-1.0f,
					 1.0f - ((float) 2*(topMargin + scopeDim + botMargin) / (float) height())
				);
				m_glLeft2ScaleMatrix.scale (
					(float) 2*(leftMargin-1) / (float) width(),
					(float) -2*scopeDim / (float) height()
				);

				m_glBot2ScaleMatrix.setToIdentity();
				m_glBot2ScaleMatrix.translate (
					-1.0f + ((float) 2*leftMargin / (float) width()),
					 1.0f - ((float) 2*(scopeDim + topMargin + scopeDim + botMargin + 1) / (float) height())
				);
				m_glBot2ScaleMatrix.scale (
					(float) 2*scopeDim / (float) width(),
					(float) -2*(botMargin - 1) / (float) height()
				);
			}
			else
			{
				m_glScopeRect2 = QRectF(
					(float) leftMargin / (float)width(),
					(float) (botMargin + topMargin + scopeHeight) / (float)height(),
					(float) scopeWidth / (float)width(),
					(float) scopeHeight / (float)height()
				);
				m_glScopeMatrix2.setToIdentity();
				m_glScopeMatrix2.translate (
					-1.0f + ((float) 2*leftMargin / (float) width()),
					 1.0f - ((float) 2*(botMargin + topMargin + scopeHeight) / (float) height())
				);
				m_glScopeMatrix2.scale (
					(float) 2*scopeWidth / (float) width(),
					(float) -2*scopeHeight / (float) height()
				);

				m_glLeft2ScaleMatrix.setToIdentity();
				m_glLeft2ScaleMatrix.translate (
					-1.0f,
					 1.0f - ((float) 2*(topMargin + scopeHeight + botMargin) / (float) height())
				);
				m_glLeft2ScaleMatrix.scale (
					(float) 2*(leftMargin-1) / (float) width(),
					(float) -2*scopeHeight / (float) height()
				);

				m_glBot2ScaleMatrix.setToIdentity();
				m_glBot2ScaleMatrix.translate (
					-1.0f + ((float) 2*leftMargin / (float) width()),
					 1.0f - ((float) 2*(scopeHeight + topMargin + scopeHeight + botMargin + 1) / (float) height())
				);
				m_glBot2ScaleMatrix.scale (
					(float) 2*scopeWidth / (float) width(),
					(float) -2*(botMargin - 1) / (float) height()
				);
			}
			{ // Y2 scale
				m_y2Scale.setSize(scopeHeight);

				m_left2ScalePixmap = QPixmap(
					leftMargin - 1,
					scopeHeight
				);

				const ScaleEngine::TickList* tickList;
				const ScaleEngine::Tick* tick;

				m_left2ScalePixmap.fill(Qt::black);
				QPainter painter(&m_left2ScalePixmap);
				painter.setPen(QColor(0xf0, 0xf0, 0xff));
				painter.setFont(font());
				tickList = &m_y2Scale.getTickList();

				for(int i = 0; i < tickList->count(); i++) {
					tick = &(*tickList)[i];
					if(tick->major) {
						if(tick->textSize > 0) {
							painter.drawText(QPointF(leftMargin - M - tick->textSize, topMargin + scopeHeight - tick->textPos - fm.ascent()/2), tick->text);
						}
					}
				}

				m_glShaderLeft2Scale.initTexture(m_left2ScalePixmap.toImage());

			} // Y2 scale
			{ // X2 scale
				if (m_mode == ModeIQPolar)
				{
					int scopeDim = std::min(scopeWidth, scopeHeight);

					m_x2Scale.setSize(scopeDim);
					m_bot2ScalePixmap = QPixmap(
							scopeDim,
						botMargin - 1
					);
				}
				else
				{
					m_x2Scale.setSize(scopeWidth);
					m_bot2ScalePixmap = QPixmap(
						scopeWidth,
						botMargin - 1
					);
				}

				const ScaleEngine::TickList* tickList;
				const ScaleEngine::Tick* tick;

				m_bot2ScalePixmap.fill(Qt::black);
				QPainter painter(&m_bot2ScalePixmap);
				painter.setPen(QColor(0xf0, 0xf0, 0xff));
				painter.setFont(font());
				tickList = &m_x2Scale.getTickList();

				for(int i = 0; i < tickList->count(); i++) {
					tick = &(*tickList)[i];
					if(tick->major) {
						if(tick->textSize > 0) {
							painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
						}
					}
				}

				m_glShaderBottom2Scale.initTexture(m_bot2ScalePixmap.toImage());

			} // X2 scale
		}
		else // Horizontal
		{
			int scopeHeight = height() - topMargin - botMargin;
			int scopeWidth = (width() - rightMargin)/2 - leftMargin;

			if (m_mode == ModeIQPolar)
			{
				m_glScopeRect1 = QRectF(
					(float) leftMargin / (float) width(),
					(float) topMargin / (float) height(),
					(float) (scopeWidth-leftMargin) / (float) width(),
					(float) scopeHeight / (float) height()
				);
				m_glScopeMatrix1.setToIdentity();
				m_glScopeMatrix1.translate (
					-1.0f + ((float) 2*leftMargin / (float) width()),
					 1.0f - ((float) 2*topMargin / (float) height())
				);
				m_glScopeMatrix1.scale (
					(float) 2*(scopeWidth-leftMargin) / (float) width(),
					(float) -2*scopeHeight / (float) height()
				);

				m_glBot1ScaleMatrix.setToIdentity();
				m_glBot1ScaleMatrix.translate (
					-1.0f + ((float) 2*leftMargin / (float) width()),
					 1.0f - ((float) 2*(scopeHeight + topMargin + 1) / (float) height())
				);
				m_glBot1ScaleMatrix.scale (
					(float) 2*(scopeWidth-leftMargin) / (float) width(),
					(float) -2*(botMargin - 1) / (float) height()
				);

				m_glRight1ScaleMatrix.setToIdentity();
				m_glRight1ScaleMatrix.translate (
					-1.0f + ((float) 2*scopeWidth / (float) width()),
					 1.0f - ((float) 2*topMargin / (float) height())
				);
				m_glRight1ScaleMatrix.scale (
					(float) 2*(leftMargin-1) / (float) width(),
					(float) -2*scopeHeight / (float) height()
				);
			}
			else
			{
				m_glScopeRect1 = QRectF(
					(float) leftMargin / (float) width(),
					(float) topMargin / (float) height(),
					(float) scopeWidth / (float) width(),
					(float) scopeHeight / (float) height()
				);
				m_glScopeMatrix1.setToIdentity();
				m_glScopeMatrix1.translate (
					-1.0f + ((float) 2*leftMargin / (float) width()),
					 1.0f - ((float) 2*topMargin / (float) height())
				);
				m_glScopeMatrix1.scale (
					(float) 2*scopeWidth / (float) width(),
					(float) -2*scopeHeight / (float) height()
				);

				m_glBot1ScaleMatrix.setToIdentity();
				m_glBot1ScaleMatrix.translate (
					-1.0f + ((float) 2*leftMargin / (float) width()),
					 1.0f - ((float) 2*(scopeHeight + topMargin + 1) / (float) height())
				);
				m_glBot1ScaleMatrix.scale (
					(float) 2*scopeWidth / (float) width(),
					(float) -2*(botMargin - 1) / (float) height()
				);
			}

			m_glLeft1ScaleMatrix.setToIdentity();
			m_glLeft1ScaleMatrix.translate (
				-1.0f,
				 1.0f - ((float) 2*topMargin / (float) height())
			);
			m_glLeft1ScaleMatrix.scale (
				(float) 2*(leftMargin-1) / (float) width(),
				(float) -2*scopeHeight / (float) height()
			);

			{ // Y1 scale
				m_y1Scale.setSize(scopeHeight);

				m_left1ScalePixmap = QPixmap(
					leftMargin - 1,
					scopeHeight
				);

				const ScaleEngine::TickList* tickList;
				const ScaleEngine::Tick* tick;

				m_left1ScalePixmap.fill(Qt::black);
				QPainter painter(&m_left1ScalePixmap);
				painter.setPen(QColor(0xf0, 0xf0, 0xff));
				painter.setFont(font());
				tickList = &m_y1Scale.getTickList();

				for(int i = 0; i < tickList->count(); i++) {
					tick = &(*tickList)[i];
					if(tick->major) {
						if(tick->textSize > 0) {
							painter.drawText(QPointF(leftMargin - M - tick->textSize, topMargin + scopeHeight - tick->textPos - fm.ascent()/2), tick->text);
						}
					}
				}

				m_glShaderLeft1Scale.initTexture(m_left1ScalePixmap.toImage());

			} // Y1 scale
			{ // X1 scale
				m_x1Scale.setSize(scopeWidth);

				m_bot1ScalePixmap = QPixmap(
					scopeWidth,
					botMargin - 1
				);

				const ScaleEngine::TickList* tickList;
				const ScaleEngine::Tick* tick;

				m_bot1ScalePixmap.fill(Qt::black);
				QPainter painter(&m_bot1ScalePixmap);
				painter.setPen(QColor(0xf0, 0xf0, 0xff));
				painter.setFont(font());
				tickList = &m_x1Scale.getTickList();

				for(int i = 0; i < tickList->count(); i++) {
					tick = &(*tickList)[i];
					if(tick->major) {
						if(tick->textSize > 0) {
							painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
						}
					}
				}

				m_glShaderBottom1Scale.initTexture(m_bot1ScalePixmap.toImage());

			} // X1 scale

			if (m_mode == ModeIQPolar)
			{
				int scopeDim = std::min(scopeWidth, scopeHeight);

				m_glScopeRect2 = QRectF(
					(float)(leftMargin + scopeWidth + leftMargin) / (float)width(),
					(float)topMargin / (float)height(),
					(float) scopeDim / (float)width(),
					(float)(height() - topMargin - botMargin) / (float)height()
				);
				m_glScopeMatrix2.setToIdentity();
				m_glScopeMatrix2.translate (
					-1.0f + ((float) 2*(leftMargin + scopeWidth + leftMargin) / (float) width()),
					 1.0f - ((float) 2*topMargin / (float) height())
				);
				m_glScopeMatrix2.scale (
					(float) 2*scopeDim / (float) width(),
					(float) -2*(height() - topMargin - botMargin) / (float) height()
				);

				m_glLeft2ScaleMatrix.setToIdentity();
				m_glLeft2ScaleMatrix.translate (
					-1.0f + (float) 2*(leftMargin + scopeWidth) / (float) width(),
					 1.0f - ((float) 2*topMargin / (float) height())
				);
				m_glLeft2ScaleMatrix.scale (
					(float) 2*(leftMargin-1) / (float) width(),
					(float) -2*scopeHeight / (float) height()
				);

				m_glBot2ScaleMatrix.setToIdentity();
				m_glBot2ScaleMatrix.translate (
					-1.0f + ((float) 2*(leftMargin + leftMargin + scopeWidth) / (float) width()),
					 1.0f - ((float) 2*(scopeHeight + topMargin + 1) / (float) height())
				);
				m_glBot2ScaleMatrix.scale (
					(float) 2*scopeDim / (float) width(),
					(float) -2*(botMargin - 1) / (float) height()
				);
			}
			else
			{
				m_glScopeRect2 = QRectF(
					(float)(leftMargin + leftMargin + ((width() - leftMargin - leftMargin - rightMargin) / 2)) / (float)width(),
					(float)topMargin / (float)height(),
					(float)((width() - leftMargin - leftMargin - rightMargin) / 2) / (float)width(),
					(float)(height() - topMargin - botMargin) / (float)height()
				);
				m_glScopeMatrix2.setToIdentity();
				m_glScopeMatrix2.translate (
					-1.0f + ((float) 2*(leftMargin + leftMargin + ((width() - leftMargin - leftMargin - rightMargin) / 2)) / (float) width()),
					 1.0f - ((float) 2*topMargin / (float) height())
				);
				m_glScopeMatrix2.scale (
					(float) 2*((width() - leftMargin - leftMargin - rightMargin) / 2) / (float) width(),
					(float) -2*(height() - topMargin - botMargin) / (float) height()
				);

				m_glLeft2ScaleMatrix.setToIdentity();
				m_glLeft2ScaleMatrix.translate (
					-1.0f + (float) 2*(leftMargin + scopeWidth) / (float) width(),
					 1.0f - ((float) 2*topMargin / (float) height())
				);
				m_glLeft2ScaleMatrix.scale (
					(float) 2*(leftMargin-1) / (float) width(),
					(float) -2*scopeHeight / (float) height()
				);

				m_glBot2ScaleMatrix.setToIdentity();
				m_glBot2ScaleMatrix.translate (
					-1.0f + ((float) 2*(leftMargin + leftMargin + scopeWidth) / (float) width()),
					 1.0f - ((float) 2*(scopeHeight + topMargin + 1) / (float) height())
				);
				m_glBot2ScaleMatrix.scale (
					(float) 2*scopeWidth / (float) width(),
					(float) -2*(botMargin - 1) / (float) height()
				);
			}
			{ // Y2 scale
				m_y2Scale.setSize(scopeHeight);

				m_left2ScalePixmap = QPixmap(
					leftMargin - 1,
					scopeHeight
				);

				const ScaleEngine::TickList* tickList;
				const ScaleEngine::Tick* tick;

				m_left2ScalePixmap.fill(Qt::black);
				QPainter painter(&m_left2ScalePixmap);
				painter.setPen(QColor(0xf0, 0xf0, 0xff));
				painter.setFont(font());
				tickList = &m_y2Scale.getTickList();

				for(int i = 0; i < tickList->count(); i++) {
					tick = &(*tickList)[i];
					if(tick->major) {
						if(tick->textSize > 0) {
							painter.drawText(QPointF(leftMargin - M - tick->textSize, topMargin + scopeHeight - tick->textPos - fm.ascent()/2), tick->text);
						}
					}
				}

				m_glShaderLeft2Scale.initTexture(m_left2ScalePixmap.toImage());

			} // Y2 scale
			{ // X2 scale
				if (m_mode == ModeIQPolar)
				{
					int scopeDim = std::min(scopeWidth, scopeHeight);

					m_x2Scale.setSize(scopeDim);
					m_bot2ScalePixmap = QPixmap(
						scopeDim,
						botMargin - 1
					);
				}
				else
				{
					m_x2Scale.setSize(scopeWidth);
					m_bot2ScalePixmap = QPixmap(
						scopeWidth,
						botMargin - 1
					);
				}

				const ScaleEngine::TickList* tickList;
				const ScaleEngine::Tick* tick;

				m_bot2ScalePixmap.fill(Qt::black);
				QPainter painter(&m_bot2ScalePixmap);
				painter.setPen(QColor(0xf0, 0xf0, 0xff));
				painter.setFont(font());
				tickList = &m_x2Scale.getTickList();

				for(int i = 0; i < tickList->count(); i++) {
					tick = &(*tickList)[i];
					if(tick->major) {
						if(tick->textSize > 0) {
							painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
						}
					}
				}

				m_glShaderBottom2Scale.initTexture(m_bot2ScalePixmap.toImage());

			} // X2 scale
		}
	} // Both displays
	else if (m_displays == DisplayFirstOnly)
	{
		int scopeHeight = height() - topMargin - botMargin;
		int scopeWidth = width() - leftMargin - rightMargin;

		if (m_mode == ModeIQPolar)
		{
			m_glScopeRect1 = QRectF(
				(float) leftMargin / (float) width(),
				(float) topMargin / (float) height(),
				(float) (scopeWidth-leftMargin) / (float) width(),
				(float) scopeHeight / (float) height()
			);
			m_glScopeMatrix1.setToIdentity();
			m_glScopeMatrix1.translate (
				-1.0f + ((float) 2*leftMargin / (float) width()),
				 1.0f - ((float) 2*topMargin / (float) height())
			);
			m_glScopeMatrix1.scale (
				(float) 2*(scopeWidth-leftMargin) / (float) width(),
				(float) -2*scopeHeight / (float) height()
			);

			m_glBot1ScaleMatrix.setToIdentity();
			m_glBot1ScaleMatrix.translate (
				-1.0f + ((float) 2*leftMargin / (float) width()),
				 1.0f - ((float) 2*(scopeHeight + topMargin + 1) / (float) height())
			);
			m_glBot1ScaleMatrix.scale (
				(float) 2*(scopeWidth-leftMargin) / (float) width(),
				(float) -2*(botMargin - 1) / (float) height()
			);

			m_glRight1ScaleMatrix.setToIdentity();
			m_glRight1ScaleMatrix.translate (
				-1.0f + ((float) 2*(width() - leftMargin) / (float) width()),
				 1.0f - ((float) 2*topMargin / (float) height())
			);
			m_glRight1ScaleMatrix.scale (
				(float) 2*(leftMargin-1) / (float) width(),
				(float) -2*scopeHeight / (float) height()
			);
		}
		else
		{
			m_glScopeRect1 = QRectF(
				(float) leftMargin / (float) width(),
				(float) topMargin / (float) height(),
				(float) scopeWidth / (float) width(),
				(float) scopeHeight / (float) height()
			);
			m_glScopeMatrix1.setToIdentity();
			m_glScopeMatrix1.translate (
				-1.0f + ((float) 2*leftMargin / (float) width()),
				 1.0f - ((float) 2*topMargin / (float) height())
			);
			m_glScopeMatrix1.scale (
				(float) 2*scopeWidth / (float) width(),
				(float) -2*scopeHeight / (float) height()
			);

			m_glBot1ScaleMatrix.setToIdentity();
			m_glBot1ScaleMatrix.translate (
				-1.0f + ((float) 2*leftMargin / (float) width()),
				 1.0f - ((float) 2*(scopeHeight + topMargin + 1) / (float) height())
			);
			m_glBot1ScaleMatrix.scale (
				(float) 2*scopeWidth / (float) width(),
				(float) -2*(botMargin - 1) / (float) height()
			);
		}

		m_glLeft1ScaleMatrix.setToIdentity();
		m_glLeft1ScaleMatrix.translate (
			-1.0f,
			 1.0f - ((float) 2*topMargin / (float) height())
		);
		m_glLeft1ScaleMatrix.scale (
			(float) 2*(leftMargin-1) / (float) width(),
			(float) -2*scopeHeight / (float) height()
		);

		{ // Y1 scale
			m_y1Scale.setSize(scopeHeight);

			m_left1ScalePixmap = QPixmap(
				leftMargin - 1,
				scopeHeight
			);

			const ScaleEngine::TickList* tickList;
			const ScaleEngine::Tick* tick;

			m_left1ScalePixmap.fill(Qt::black);
			QPainter painter(&m_left1ScalePixmap);
			if (m_mode == ModeIQPolar) {
				painter.setPen(QColor(0xff, 0xff, 0x80));
			} else {
				painter.setPen(QColor(0xf0, 0xf0, 0xff));
			}
			painter.setFont(font());
			tickList = &m_y1Scale.getTickList();

			for(int i = 0; i < tickList->count(); i++) {
				tick = &(*tickList)[i];
				if(tick->major) {
					if(tick->textSize > 0) {
						painter.drawText(QPointF(leftMargin - M - tick->textSize, topMargin + scopeHeight - tick->textPos - fm.ascent()/2), tick->text);
					}
				}
			}

			m_glShaderLeft1Scale.initTexture(m_left1ScalePixmap.toImage());

		} // Y1 scale
		if (m_mode == ModeIQPolar) { // Y2 scale
			m_y2Scale.setSize(scopeHeight);

			m_left2ScalePixmap = QPixmap(
				leftMargin - 1,
				scopeHeight
			);

			const ScaleEngine::TickList* tickList;
			const ScaleEngine::Tick* tick;

			m_left2ScalePixmap.fill(Qt::black);
			QPainter painter(&m_left2ScalePixmap);
			painter.setPen(QColor(0x80, 0xff, 0xff));
			painter.setFont(font());
			tickList = &m_y2Scale.getTickList();

			for(int i = 0; i < tickList->count(); i++) {
				tick = &(*tickList)[i];
				if(tick->major) {
					if(tick->textSize > 0) {
						painter.drawText(QPointF(leftMargin - M - tick->textSize, topMargin + scopeHeight - tick->textPos - fm.ascent()/2), tick->text);
					}
				}
			}

			m_glShaderLeft2Scale.initTexture(m_left2ScalePixmap.toImage());

		} // Y2 scale
		{ // X1 scale
			m_x1Scale.setSize(scopeWidth);

			m_bot1ScalePixmap = QPixmap(
				scopeWidth,
				botMargin - 1
			);

			const ScaleEngine::TickList* tickList;
			const ScaleEngine::Tick* tick;

			m_bot1ScalePixmap.fill(Qt::black);
			QPainter painter(&m_bot1ScalePixmap);
			painter.setPen(QColor(0xf0, 0xf0, 0xff));
			painter.setFont(font());
			tickList = &m_x1Scale.getTickList();

			for(int i = 0; i < tickList->count(); i++) {
				tick = &(*tickList)[i];
				if(tick->major) {
					if(tick->textSize > 0) {
						painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
					}
				}
			}

			m_glShaderBottom1Scale.initTexture(m_bot1ScalePixmap.toImage());

		} // X1 scale
	} // Primary display only
	else if (m_displays == DisplaySecondOnly)
	{
		int scopeHeight = height() - topMargin - botMargin;
		int scopeWidth = width() - leftMargin - rightMargin;

		if (m_mode == ModeIQPolar)
		{
			int scopeDim = std::min(scopeWidth, scopeHeight);

			m_glScopeRect2 = QRectF(
				(float) leftMargin / (float) width(),
				(float) topMargin / (float) height(),
				(float) scopeDim / (float) width(),
				(float) scopeDim / (float) height()
			);
			m_glScopeMatrix2.setToIdentity();
			m_glScopeMatrix2.translate (
				-1.0f + ((float) 2*leftMargin / (float) width()),
				 1.0f - ((float) 2*topMargin / (float) height())
			);
			m_glScopeMatrix2.scale (
				(float) 2*scopeDim / (float) width(),
				(float) -2*scopeDim / (float) height()
			);

			m_glLeft2ScaleMatrix.setToIdentity();
			m_glLeft2ScaleMatrix.translate (
				-1.0f,
				 1.0f - ((float) 2*topMargin / (float) height())
			);
			m_glLeft2ScaleMatrix.scale (
				(float) 2*(leftMargin-1) / (float) width(),
				(float) -2*scopeDim / (float) height()
			);

			m_glBot2ScaleMatrix.setToIdentity();
			m_glBot2ScaleMatrix.translate (
				-1.0f + ((float) 2*leftMargin / (float) width()),
				 1.0f - ((float) 2*(scopeDim + topMargin + 1) / (float) height())
			);
			m_glBot2ScaleMatrix.scale (
				(float) 2*scopeDim / (float) width(),
				(float) -2*(botMargin - 1) / (float) height()
			);
		}
		else
		{
			m_glScopeRect2 = QRectF(
				(float) leftMargin / (float) width(),
				(float) topMargin / (float) height(),
				(float) scopeWidth / (float) width(),
				(float) scopeHeight / (float) height()
			);
			m_glScopeMatrix2.setToIdentity();
			m_glScopeMatrix2.translate (
				-1.0f + ((float) 2*leftMargin / (float) width()),
				 1.0f - ((float) 2*topMargin / (float) height())
			);
			m_glScopeMatrix2.scale (
				(float) 2*scopeWidth / (float) width(),
				(float) -2*scopeHeight / (float) height()
			);

			m_glLeft2ScaleMatrix.setToIdentity();
			m_glLeft2ScaleMatrix.translate (
				-1.0f,
				 1.0f - ((float) 2*topMargin / (float) height())
			);
			m_glLeft2ScaleMatrix.scale (
				(float) 2*(leftMargin-1) / (float) width(),
				(float) -2*scopeHeight / (float) height()
			);

			m_glBot2ScaleMatrix.setToIdentity();
			m_glBot2ScaleMatrix.translate (
				-1.0f + ((float) 2*leftMargin / (float) width()),
				 1.0f - ((float) 2*(scopeHeight + topMargin + 1) / (float) height())
			);
			m_glBot2ScaleMatrix.scale (
				(float) 2*scopeWidth / (float) width(),
				(float) -2*(botMargin - 1) / (float) height()
			);
		}

		{ // Y2 scale
			m_y2Scale.setSize(scopeHeight);

			m_left2ScalePixmap = QPixmap(
				leftMargin - 1,
				scopeHeight
			);

			const ScaleEngine::TickList* tickList;
			const ScaleEngine::Tick* tick;

			m_left2ScalePixmap.fill(Qt::black);
			QPainter painter(&m_left2ScalePixmap);
			painter.setPen(QColor(0xf0, 0xf0, 0xff));
			painter.setFont(font());
			tickList = &m_y2Scale.getTickList();

			for(int i = 0; i < tickList->count(); i++) {
				tick = &(*tickList)[i];
				if(tick->major) {
					if(tick->textSize > 0) {
						painter.drawText(QPointF(leftMargin - M - tick->textSize, topMargin + scopeHeight - tick->textPos - fm.ascent()/2), tick->text);
					}
				}
			}

			m_glShaderLeft2Scale.initTexture(m_left2ScalePixmap.toImage());

		} // Y2 scale
		{ // X2 scale
			if (m_mode == ModeIQPolar)
			{
				int scopeDim = std::min(scopeWidth, scopeHeight);

				m_x2Scale.setSize(scopeDim);
				m_bot2ScalePixmap = QPixmap(
					scopeDim,
					botMargin - 1
				);
			}
			else
			{
				m_x2Scale.setSize(scopeWidth);
				m_bot2ScalePixmap = QPixmap(
					scopeWidth,
					botMargin - 1
				);
			}

			const ScaleEngine::TickList* tickList;
			const ScaleEngine::Tick* tick;

			m_bot2ScalePixmap.fill(Qt::black);
			QPainter painter(&m_bot2ScalePixmap);
			painter.setPen(QColor(0xf0, 0xf0, 0xff));
			painter.setFont(font());
			tickList = &m_x2Scale.getTickList();

			for(int i = 0; i < tickList->count(); i++) {
				tick = &(*tickList)[i];
				if(tick->major) {
					if(tick->textSize > 0) {
						painter.drawText(QPointF(tick->textPos, fm.height() - 1), tick->text);
					}
				}
			}

			m_glShaderBottom2Scale.initTexture(m_bot2ScalePixmap.toImage());

		} // X2 scale
	} // Secondary display only
}

void GLScope::tick()
{
	if(m_dataChanged)
		update();
}

void GLScope::setTriggerChannel(ScopeVis::TriggerChannel triggerChannel)
{
	m_triggerChannel = triggerChannel;
}

void GLScope::setTriggerLevel(Real triggerLevel)
{
	qDebug("GLScope::setTriggerLevel: %f", triggerLevel);
	m_triggerLevel = triggerLevel;
}

void GLScope::setTriggerPre(Real triggerPre)
{
	m_triggerPre = triggerPre;
	m_configChanged = true;
	update();
}

void GLScope::setMemHistoryShift(int value)
{
	if (value < m_memTraceIndexMax)
	{
		m_memTraceHistory = value;
		m_configChanged = true;
		update();
	}
}

void GLScope::connectTimer(const QTimer& timer)
{
	qDebug() << "GLScope::connectTimer";
	disconnect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
	connect(&timer, SIGNAL(timeout()), this, SLOT(tick()));
	m_timer.stop();
}

void GLScope::cleanup()
{
	//makeCurrent();
	m_glShaderSimple.cleanup();
	m_glShaderBottom1Scale.cleanup();
	m_glShaderBottom2Scale.cleanup();
	m_glShaderLeft1Scale.cleanup();
	m_glShaderPowerOverlay.cleanup();
    //doneCurrent();
}
