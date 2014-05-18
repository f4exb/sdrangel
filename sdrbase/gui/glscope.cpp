#include <QPainter>
#include <QMouseEvent>
#include "gui/glscope.h"
#include "dsp/dspengine.h"

#ifdef _WIN32
static double log2f(double n)
{
	return log(n) / log(2.0);
}
#endif

GLScope::GLScope(QWidget* parent) :
	QGLWidget(parent),
	m_dataChanged(false),
	m_configChanged(true),
	m_mode(ModeIQ),
	m_orientation(Qt::Horizontal),
	m_displayTrace(&m_rawTrace),
	m_oldTraceSize(-1),
	m_sampleRate(0),
	m_dspEngine(NULL),
	m_scopeVis(NULL),
	m_amp(1.0),
	m_timeBase(1),
	m_timeOfsProMill(0),
	m_triggerChannel(ScopeVis::TriggerFreeRun)
{
	setAttribute(Qt::WA_OpaquePaintEvent);
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
	m_timer.start(50);
}

GLScope::~GLScope()
{
	if(m_dspEngine != NULL) {
		m_dspEngine->removeSink(m_scopeVis);
		delete m_scopeVis;
	}
}

void GLScope::setDSPEngine(DSPEngine* dspEngine)
{
	if((m_dspEngine == NULL) && (dspEngine != NULL)) {
		m_dspEngine = dspEngine;
		m_scopeVis = new ScopeVis(this);
		m_dspEngine->addSink(m_scopeVis);
	}
}

void GLScope::setAmp(Real amp)
{
	m_amp = amp;
	update();
}

void GLScope::setTimeBase(int timeBase)
{
	m_timeBase = timeBase;
	update();
}

void GLScope::setTimeOfsProMill(int timeOfsProMill)
{
	m_timeOfsProMill = timeOfsProMill;
	update();
}

void GLScope::setMode(Mode mode)
{
	m_mode = mode;
	m_dataChanged = true;
	update();
}

void GLScope::setOrientation(Qt::Orientation orientation)
{
	m_orientation = orientation;
	m_configChanged = true;
	update();
}

void GLScope::newTrace(const std::vector<Complex>& trace, int sampleRate)
{
	if(!m_mutex.tryLock(2))
		return;
	if(m_dataChanged) {
		m_mutex.unlock();
		return;
	}

	m_rawTrace = trace;

	m_sampleRate = sampleRate;
	m_dataChanged = true;

	m_mutex.unlock();
}

void GLScope::initializeGL()
{
	glDisable(GL_DEPTH_TEST);
}

void GLScope::resizeGL(int width, int height)
{
	glViewport(0, 0, width, height);
	m_configChanged = true;
}

void GLScope::paintGL()
{
	if(!m_mutex.tryLock(2))
		return;

	if(m_configChanged)
		applyConfig();

	handleMode();

	if(m_displayTrace->size() != m_oldTraceSize) {
		m_oldTraceSize = m_displayTrace->size();
		emit traceSizeChanged(m_displayTrace->size());
	}

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glPushMatrix();
	glScalef(2.0, -2.0, 1.0);
	glTranslatef(-0.50, -0.5, 0);

	// I

	// draw rect around
	glPushMatrix();
	glTranslatef(m_glScopeRect1.x(), m_glScopeRect1.y(), 0);
	glScalef(m_glScopeRect1.width(), m_glScopeRect1.height(), 1);
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
	// paint grid
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glLineWidth(1.0f);
	glColor4f(1, 1, 1, 0.05f);
	for(int i = 1; i < 10; i++) {
		glBegin(GL_LINE_LOOP);
		glVertex2f(0, i * 0.1);
		glVertex2f(1, i * 0.1);
		glEnd();
	}
	for(int i = 1; i < 10; i++) {
		glBegin(GL_LINE_LOOP);
		glVertex2f(i * 0.1, 0);
		glVertex2f(i * 0.1, 1);
		glEnd();
	}
	glPopMatrix();

	if(m_triggerChannel == ScopeVis::TriggerChannelI) {
		glPushMatrix();
		glTranslatef(m_glScopeRect1.x(), m_glScopeRect1.y() + m_glScopeRect1.height() / 2.0, 0);
		glScalef(m_glScopeRect1.width(), -(m_glScopeRect1.height() / 2) * m_amp1, 1);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_LINE_SMOOTH);
		glLineWidth(1.0f);
		glColor4f(0, 1, 0, 0.3f);
		glBegin(GL_LINE_LOOP);
		glVertex2f(0, m_triggerLevelHigh);
		glVertex2f(1, m_triggerLevelHigh);
		glEnd();
		glColor4f(0, 0.8f, 0.0, 0.3f);
		glBegin(GL_LINE_LOOP);
		glVertex2f(0, m_triggerLevelLow);
		glVertex2f(1, m_triggerLevelLow);
		glEnd();
		glDisable(GL_LINE_SMOOTH);
		glPopMatrix();
	}

	if(m_displayTrace->size() > 0) {
		glPushMatrix();
		glTranslatef(m_glScopeRect1.x(), m_glScopeRect1.y() + m_glScopeRect1.height() / 2.0, 0);
		glScalef(m_glScopeRect1.width() * (float)m_timeBase / (float)(m_displayTrace->size() - 1), -(m_glScopeRect1.height() / 2) * m_amp1, 1);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_LINE_SMOOTH);
		glLineWidth(1.0f);
		glColor4f(1, 1, 0, 0.4f);
		int start = m_timeOfsProMill * (m_displayTrace->size() - (m_displayTrace->size() / m_timeBase)) / 1000;
		int end = start + m_displayTrace->size() / m_timeBase;
		if(end - start < 2)
			start--;
		float posLimit = 1.0 / m_amp1;
		float negLimit = -1.0 / m_amp1;
		glBegin(GL_LINE_STRIP);
		for(int i = start; i < end; i++) {
			float v = (*m_displayTrace)[i].real() + m_ofs1;
			if(v > posLimit)
				v = posLimit;
			else if(v < negLimit)
				v = negLimit;
			glVertex2f(i - start, v);
		}
		glEnd();
		glDisable(GL_LINE_SMOOTH);
		glPopMatrix();
	}

	// Q

	// draw rect around
	glPushMatrix();
	glTranslatef(m_glScopeRect2.x(), m_glScopeRect2.y(), 0);
	glScalef(m_glScopeRect2.width(), m_glScopeRect2.height(), 1);
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
	// paint grid
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glLineWidth(1.0f);
	glColor4f(1, 1, 1, 0.05f);
	for(int i = 1; i < 10; i++) {
		glBegin(GL_LINE_LOOP);
		glVertex2f(0, i * 0.1);
		glVertex2f(1, i * 0.1);
		glEnd();
	}
	for(int i = 1; i < 10; i++) {
		glBegin(GL_LINE_LOOP);
		glVertex2f(i * 0.1, 0);
		glVertex2f(i * 0.1, 1);
		glEnd();
	}
	glPopMatrix();

	if(m_triggerChannel == ScopeVis::TriggerChannelQ) {
		glPushMatrix();
		glTranslatef(m_glScopeRect2.x(), m_glScopeRect2.y() + m_glScopeRect2.height() / 2.0, 0);
		glScalef(m_glScopeRect2.width(), -(m_glScopeRect2.height() / 2) * m_amp2, 1);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_LINE_SMOOTH);
		glLineWidth(1.0f);
		glColor4f(0, 1, 0, 0.3f);
		glBegin(GL_LINE_LOOP);
		glVertex2f(0, m_triggerLevelHigh);
		glVertex2f(1, m_triggerLevelHigh);
		glEnd();
		glColor4f(0, 0.8f, 0.0, 0.3f);
		glBegin(GL_LINE_LOOP);
		glVertex2f(0, m_triggerLevelLow);
		glVertex2f(1, m_triggerLevelLow);
		glEnd();
		glDisable(GL_LINE_SMOOTH);
		glPopMatrix();
	}

	if(m_displayTrace->size() > 0) {
		glPushMatrix();
		glTranslatef(m_glScopeRect2.x(), m_glScopeRect2.y() + m_glScopeRect2.height() / 2.0, 0);
		glScalef(m_glScopeRect2.width() * (float)m_timeBase / (float)(m_displayTrace->size() - 1), -(m_glScopeRect2.height() / 2) * m_amp2, 1);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_LINE_SMOOTH);
		glLineWidth(1.0f);
		glColor4f(1, 1, 0, 0.4f);
		int start = m_timeOfsProMill * (m_displayTrace->size() - (m_displayTrace->size() / m_timeBase)) / 1000;
		int end = start + m_displayTrace->size() / m_timeBase;
		if(end - start < 2)
			start--;
		float posLimit = 1.0 / m_amp2;
		float negLimit = -1.0 / m_amp2;
		glBegin(GL_LINE_STRIP);
		for(int i = start; i < end; i++) {
			float v = (*m_displayTrace)[i].imag();
			if(v > posLimit)
				v = posLimit;
			else if(v < negLimit)
				v = negLimit;
			glVertex2f(i - start, v);
		}
		glEnd();
		glDisable(GL_LINE_SMOOTH);
		glPopMatrix();
	}

	glPopMatrix();
	m_dataChanged = false;
	m_mutex.unlock();
}

void GLScope::mousePressEvent(QMouseEvent* event)
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
		m_triggerLevelHigh = amplitude + 0.01 / m_amp;
		m_triggerLevelLow = amplitude - 0.01 / m_amp;
		if(m_triggerLevelHigh > 1.0)
			m_triggerLevelHigh = 1.0;
		else if(m_triggerLevelHigh < -1.0)
			m_triggerLevelHigh = -1.0;
		if(m_triggerLevelLow > 1.0)
			m_triggerLevelLow = 1.0;
		else if(m_triggerLevelLow < -1.0)
			m_triggerLevelLow = -1.0;
		m_scopeVis->configure(m_dspEngine->getMessageQueue(), channel, m_triggerLevelHigh, m_triggerLevelLow);
		m_triggerChannel = channel;
		m_changed = true;
		update();
	}
#endif
}

void GLScope::handleMode()
{
	switch(m_mode) {
		case ModeIQ:
			m_displayTrace = &m_rawTrace;
			m_amp1 = m_amp;
			m_amp2 = m_amp;
			m_ofs1 = 0.0;
			m_ofs2 = 0.0;
			break;

		case ModeMagLinPha: {
			m_mathTrace.resize(m_rawTrace.size());
			std::vector<Complex>::iterator dst = m_mathTrace.begin();
			for(std::vector<Complex>::const_iterator src = m_rawTrace.begin(); src != m_rawTrace.end(); ++src)
				*dst++ = Complex(abs(*src), arg(*src) / M_PI);
			m_displayTrace = &m_mathTrace;
			m_amp1 = m_amp;
			m_amp2 = 1.0;
			m_ofs1 = -1.0 / m_amp1;
			m_ofs2 = 0.0;
			break;
		}

		case ModeMagdBPha: {
			m_mathTrace.resize(m_rawTrace.size());
			std::vector<Complex>::iterator dst = m_mathTrace.begin();
			Real mult = (10.0f / log2f(10.0f));
			for(std::vector<Complex>::const_iterator src = m_rawTrace.begin(); src != m_rawTrace.end(); ++src) {
				Real v = src->real() * src->real() + src->imag() * src->imag();
				v = (96.0 + (mult * log2f(v))) / 96.0;
				*dst++ = Complex(v, arg(*src) / M_PI);
			}
			m_displayTrace = &m_mathTrace;
			m_amp1 = 2.0 * m_amp;
			m_amp2 = 1.0;
			m_ofs1 = -1.0 / m_amp1;
			m_ofs2 = 0.0;
			break;
		}

		case ModeDerived12: {
			if(m_rawTrace.size() > 3) {
				m_mathTrace.resize(m_rawTrace.size() - 3);
				std::vector<Complex>::iterator dst = m_mathTrace.begin();
				for(uint i = 3; i < m_rawTrace.size() ; i++) {
					*dst++ = Complex(
						abs(m_rawTrace[i] - m_rawTrace[i - 1]),
						abs(m_rawTrace[i] - m_rawTrace[i - 1]) - abs(m_rawTrace[i - 2] - m_rawTrace[i - 3]));
				}
				m_displayTrace = &m_mathTrace;
				m_amp1 = m_amp;
				m_amp2 = m_amp;
				m_ofs1 = -1.0 / m_amp1;
				m_ofs2 = 0.0;
			}
			break;
		}

		case ModeCyclostationary: {
			if(m_rawTrace.size() > 2) {
				m_mathTrace.resize(m_rawTrace.size() - 2);
				std::vector<Complex>::iterator dst = m_mathTrace.begin();
				for(uint i = 2; i < m_rawTrace.size() ; i++)
					*dst++ = Complex(abs(m_rawTrace[i] - conj(m_rawTrace[i - 1])), 0);
				m_displayTrace = &m_mathTrace;
				m_amp1 = m_amp;
				m_amp2 = m_amp;
				m_ofs1 = -1.0 / m_amp1;
				m_ofs2 = 0.0;
			}
			break;
		}

	}
}

void GLScope::applyConfig()
{
	m_configChanged = false;

	if(m_orientation == Qt::Vertical) {
		m_glScopeRect1 = QRectF(
			(float)10 / (float)width(),
			(float)10 / (float)height(),
			(float)(width() - 10 - 10) / (float)width(),
			(float)((height() - 10 - 10 - 10) / 2) / (float)height()
		);
		m_glScopeRect2 = QRectF(
			(float)10 / (float)width(),
			(float)(10 + 10 + (height() - 10 - 10 - 10) / 2) / (float)height(),
			(float)(width() - 10 - 10) / (float)width(),
			(float)((height() - 10 - 10 - 10) / 2) / (float)height()
		);
	} else {
		m_glScopeRect1 = QRectF(
			(float)10 / (float)width(),
			(float)10 / (float)height(),
			(float)((width() - 10 - 10 - 10) / 2) / (float)width(),
			(float)(height() - 10 - 10) / (float)height()
		);
		m_glScopeRect2 = QRectF(
			(float)(10 + 10 + ((width() - 10 - 10 - 10) / 2)) / (float)width(),
			(float)10 / (float)height(),
			(float)((width() - 10 - 10 - 10) / 2) / (float)width(),
			(float)(height() - 10 - 10) / (float)height()
		);
	}
}

void GLScope::tick()
{
	if(m_dataChanged)
		update();
}
