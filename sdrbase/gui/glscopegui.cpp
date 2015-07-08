#include "gui/glscopegui.h"
#include "dsp/scopevis.h"
#include "dsp/dspcommands.h"
#include "gui/glscope.h"
#include "util/simpleserializer.h"
#include "ui_glscopegui.h"

#include <iostream>
#include <cstdio>

GLScopeGUI::GLScopeGUI(QWidget* parent) :
	QWidget(parent),
	ui(new Ui::GLScopeGUI),
	m_messageQueue(NULL),
	m_scopeVis(NULL),
	m_glScope(NULL),
	m_sampleRate(1),
	m_displayData(GLScope::ModeIQ),
	m_displayOrientation(Qt::Horizontal),
	m_timeBase(1),
	m_timeOffset(0),
	m_amplification(0),
	m_ampOffset(0),
	m_displayGridIntensity(1)
{
	ui->setupUi(this);
}

GLScopeGUI::~GLScopeGUI()
{
	delete ui;
}

void GLScopeGUI::setBuddies(MessageQueue* messageQueue, ScopeVis* scopeVis, GLScope* glScope)
{
	m_messageQueue = messageQueue;
	m_scopeVis = scopeVis;
	m_glScope = glScope;
	connect(m_glScope, SIGNAL(traceSizeChanged(int)), this, SLOT(on_scope_traceSizeChanged(int)));
	connect(m_glScope, SIGNAL(sampleRateChanged(int)), this, SLOT(on_scope_sampleRateChanged(int)));
	applySettings();
}

void GLScopeGUI::setSampleRate(int sampleRate)
{
	m_sampleRate = sampleRate;
	setTimeScaleDisplay();
}

void GLScopeGUI::resetToDefaults()
{
	m_displayData = GLScope::ModeIQ;
	m_displayOrientation = Qt::Horizontal;
	m_timeBase = 1;
	m_timeOffset = 0;
	m_amplification = 0;
	m_displayGridIntensity = 5;
	applySettings();
}

QByteArray GLScopeGUI::serialize() const
{
	SimpleSerializer s(1);

	s.writeS32(1, m_displayData);
	s.writeS32(2, m_displayOrientation);
	s.writeS32(3, m_timeBase);
	s.writeS32(4, m_timeOffset);
	s.writeS32(5, m_amplification);
	s.writeS32(6, m_displayGridIntensity);
	s.writeS32(7, m_ampOffset);

	return s.final();
}

bool GLScopeGUI::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if(!d.isValid()) {
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1) {
		d.readS32(1, &m_displayData, GLScope::ModeIQ);
		d.readS32(2, &m_displayOrientation, Qt::Horizontal);
		d.readS32(3, &m_timeBase, 1);
		d.readS32(4, &m_timeOffset, 0);
		d.readS32(5, &m_amplification, 0);
		d.readS32(6, &m_displayGridIntensity, 5);
		if(m_timeBase < 0)
			m_timeBase = 1;
		d.readS32(7, &m_ampOffset, 0);
		applySettings();
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

void GLScopeGUI::applySettings()
{
	ui->dataMode->setCurrentIndex(m_displayData);
	if(m_displayOrientation == Qt::Horizontal) {
		m_glScope->setOrientation(Qt::Horizontal);
		ui->horizView->setChecked(true);
		ui->vertView->setChecked(false);
	} else {
		m_glScope->setOrientation(Qt::Vertical);
		ui->horizView->setChecked(false);
		ui->vertView->setChecked(true);
	}
	ui->time->setValue(m_timeBase);
	ui->timeOfs->setValue(m_timeOffset);
	ui->amp->setValue(m_amplification);
	ui->ampOfs->setValue(m_ampOffset);
	ui->gridIntensity->setSliderPosition(m_displayGridIntensity);
}

void GLScopeGUI::on_amp_valueChanged(int value)
{
	static qreal amps[11] = { 0.2, 0.1, 0.05, 0.02, 0.01, 0.005, 0.002, 0.001, 0.0005, 0.0002, 0.0001 };
	ui->ampText->setText(tr("%1\n/div").arg(amps[value], 0, 'f', 4));
	m_glScope->setAmp(0.2 / amps[value]);
	m_amplification = value;
}

void GLScopeGUI::on_ampOfs_valueChanged(int value)
{
	m_ampOffset = value;
	ui->ampOfsText->setText(tr("%1").arg(value/100.0, 0, 'f', 2));
	m_glScope->setAmpOfs(value/100.0); // scale to [-1.0,1.0]
}

void GLScopeGUI::on_scope_traceSizeChanged(int)
{
	setTimeScaleDisplay();
}

void GLScopeGUI::on_scope_sampleRateChanged(int)
{
	setTimeScaleDisplay();
}

void GLScopeGUI::setTimeScaleDisplay()
{
	m_sampleRate = m_glScope->getSampleRate();
	qreal t = (m_glScope->getTraceSize() * 0.1 / m_sampleRate) / (qreal)m_timeBase;
	/*
	std::cerr << "GLScopeGUI::setTimeScaleDisplay: sample rate: "
			<< m_glScope->getSampleRate()
			<< " traceSize: " << m_glScope->getTraceSize()
			<< " timeBase: " << m_timeBase
			<< " glScope @" << m_glScope << std::endl;
			*/
	if(t < 0.000001)
		ui->timeText->setText(tr("%1\nns/div").arg(t * 1000000000.0));
	else if(t < 0.001)
		ui->timeText->setText(tr("%1\nµs/div").arg(t * 1000000.0));
	else if(t < 1.0)
		ui->timeText->setText(tr("%1\nms/div").arg(t * 1000.0));
	else ui->timeText->setText(tr("%1\ns/div").arg(t * 1.0));
}

void GLScopeGUI::on_time_valueChanged(int value)
{
	m_timeBase = value;
	setTimeScaleDisplay();
	m_glScope->setTimeBase(m_timeBase);
}

void GLScopeGUI::on_timeOfs_valueChanged(int value)
{
	m_timeOffset = value;
	ui->timeOfsText->setText(tr("%1").arg(value/100.0, 0, 'f', 2));
	m_glScope->setTimeOfsProMill(value*10);
}

void GLScopeGUI::on_dataMode_currentIndexChanged(int index)
{
	m_displayData = index;
	switch(index) {
		case 0: // i+q
			m_glScope->setMode(GLScope::ModeIQ);
			break;
		case 1: // mag(lin)+pha
			m_glScope->setMode(GLScope::ModeMagLinPha);
			break;
		case 2: // mag(dB)+pha
			m_glScope->setMode(GLScope::ModeMagdBPha);
			break;
		case 3: // derived1+derived2
			m_glScope->setMode(GLScope::ModeDerived12);
			break;
		case 4: // clostationary
			m_glScope->setMode(GLScope::ModeCyclostationary);
			break;

		default:
			break;
	}
}

void GLScopeGUI::on_horizView_clicked()
{
	m_displayOrientation = Qt::Horizontal;
	if(ui->horizView->isChecked()) {
		ui->vertView->setChecked(false);
		m_glScope->setOrientation(Qt::Horizontal);
	} else {
		ui->horizView->setChecked(true);
	}
}

void GLScopeGUI::on_vertView_clicked()
{
	m_displayOrientation = Qt::Vertical;
	if(ui->vertView->isChecked()) {
		ui->horizView->setChecked(false);
		m_glScope->setOrientation(Qt::Vertical);
	} else {
		ui->vertView->setChecked(true);
		m_glScope->setOrientation(Qt::Vertical);
	}
}

void GLScopeGUI::on_gridIntensity_valueChanged(int index)
{
	m_displayGridIntensity = index;
	if(m_glScope != NULL)
		m_glScope->setDisplayGridIntensity(m_displayGridIntensity);
}

bool GLScopeGUI::handleMessage(Message* cmd)
{
	return false;
	/*
	if(DSPSignalNotification::match(cmd))
	{
		DSPSignalNotification* signal = (DSPSignalNotification*)cmd;
		//fprintf(stderr, "GLScopeGUI::handleMessage: %d samples/sec, %lld Hz offset", signal->getSampleRate(), signal->getFrequencyOffset());
		m_sampleRate = signal->getSampleRate();
		cmd->completed();
		return true;
	}
	else
	{
		return false;
	}
	*/
}
