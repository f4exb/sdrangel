#include "gui/glscopegui.h"
#include "dsp/scopevis.h"
#include "dsp/dspcommands.h"
#include "gui/glscope.h"
#include "util/simpleserializer.h"
#include "ui_glscopegui.h"

#include <iostream>
#include <cstdio>

const qreal GLScopeGUI::amps[11] = { 0.2, 0.1, 0.05, 0.02, 0.01, 0.005, 0.002, 0.001, 0.0005, 0.0002, 0.0001 };

GLScopeGUI::GLScopeGUI(QWidget* parent) :
	QWidget(parent),
	ui(new Ui::GLScopeGUI),
	m_messageQueue(NULL),
	m_scopeVis(NULL),
	m_glScope(NULL),
	m_sampleRate(1),
	m_displayData(GLScope::ModeIQ),
	m_displayOrientation(Qt::Horizontal),
	m_displays(GLScope::DisplayBoth),
	m_timeBase(1),
	m_timeOffset(0),
	m_amplification(0),
	m_ampOffset(0),
	m_displayGridIntensity(1),
	m_triggerChannel(ScopeVis::TriggerFreeRun),
	m_triggerLevel(0.0),
	m_triggerPositiveEdge(true)
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
	setTimeOfsDisplay();
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
	s.writeS32(8, m_displays);
	s.writeS32(9, m_triggerChannel);
	s.writeS32(10, m_triggerLevel);
	s.writeBool(11, m_triggerPositiveEdge);

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
		d.readS32(8, &m_displays, GLScope::DisplayBoth);
		d.readS32(9, &m_triggerChannel, ScopeVis::TriggerFreeRun);
		ui->trigMode->setCurrentIndex(m_triggerChannel);
		d.readS32(10, &m_triggerLevel, 0);
		ui->trigLevel->setValue(m_triggerLevel);
		setTrigLevelDisplay();
		d.readBool(11, &m_triggerPositiveEdge, true);
		ui->slopePos->setChecked(m_triggerPositiveEdge);
		ui->slopeNeg->setChecked(!m_triggerPositiveEdge);
		applySettings();
		applyTriggerSettings();
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

void GLScopeGUI::applySettings()
{
	ui->dataMode->setCurrentIndex(m_displayData);
	if (m_displays == GLScope::DisplayBoth)
	{
		if(m_displayOrientation == Qt::Horizontal) {
			m_glScope->setOrientation(Qt::Horizontal);
			ui->horizView->setChecked(true);
			ui->vertView->setChecked(false);
			ui->onlyPrimeView->setChecked(false);
			ui->onlySecondView->setChecked(false);
		} else {
			m_glScope->setOrientation(Qt::Vertical);
			ui->horizView->setChecked(false);
			ui->vertView->setChecked(true);
			ui->onlyPrimeView->setChecked(false);
			ui->onlySecondView->setChecked(false);
		}
	}
	else if (m_displays == GLScope::DisplayFirstOnly)
	{
		m_glScope->setDisplays(GLScope::DisplayFirstOnly);
		ui->onlyPrimeView->setChecked(true);
		ui->horizView->setChecked(false);
		ui->vertView->setChecked(false);
		ui->onlySecondView->setChecked(false);
	}
	else if (m_displays == GLScope::DisplaySecondOnly)
	{
		m_glScope->setDisplays(GLScope::DisplaySecondOnly);
		ui->onlySecondView->setChecked(true);
		ui->onlyPrimeView->setChecked(false);
		ui->horizView->setChecked(false);
		ui->vertView->setChecked(false);
	}
	ui->time->setValue(m_timeBase);
	ui->timeOfs->setValue(m_timeOffset);
	ui->amp->setValue(m_amplification);
	ui->ampOfs->setValue(m_ampOffset);
	ui->gridIntensity->setSliderPosition(m_displayGridIntensity);
}

void GLScopeGUI::applyTriggerSettings()
{
	Real triggerLevel;

	if (m_triggerChannel == ScopeVis::TriggerMagDb) {
		triggerLevel = m_triggerLevel - 100.0;
	}
	else {
		triggerLevel = m_triggerLevel / 100.0;
	}

	m_glScope->setTriggerChannel((ScopeVis::TriggerChannel) m_triggerChannel);
	m_glScope->setTriggerLevel(m_triggerLevel / 100.0);

	m_scopeVis->configure(m_messageQueue, (ScopeVis::TriggerChannel) m_triggerChannel, triggerLevel, m_triggerPositiveEdge);
}

void GLScopeGUI::setTrigLevelDisplay()
{
	if (m_triggerChannel == ScopeVis::TriggerMagDb) {
		ui->trigText->setText(tr("%1dB").arg(m_triggerLevel - 100.0, 0, 'f', 0));
	}
	else if (m_triggerChannel == ScopeVis::TriggerPhase) {
		ui->trigText->setText(tr("%1π").arg(m_triggerLevel / 100.0, 0, 'f', 2));
	}
	else {
		ui->trigText->setText(tr("%1").arg(m_triggerLevel / 100.0, 0, 'f', 2));
	}
}

void GLScopeGUI::setAmpScaleDisplay()
{
	if (m_glScope->getDataMode() == GLScope::ModeMagdBPha) {
		ui->ampText->setText(tr("%1\ndB").arg(amps[m_amplification]*500.0, 0, 'f', 1));
	} else {
		ui->ampText->setText(tr("%1").arg(amps[m_amplification]*10.0, 0, 'f', 3));
	}
}

void GLScopeGUI::setAmpOfsDisplay()
{
	if (m_glScope->getDataMode() == GLScope::ModeMagdBPha) {
		ui->ampOfsText->setText(tr("%1\ndB").arg(m_ampOffset - 100.0, 0, 'f', 0));
	} else 	if (m_glScope->getDataMode() == GLScope::ModeMagLinPha) {
		ui->ampOfsText->setText(tr("%1").arg(m_ampOffset/200.0, 0, 'f', 3));
	} else {
		ui->ampOfsText->setText(tr("%1").arg(m_ampOffset/100.0, 0, 'f', 2));
	}
}

void GLScopeGUI::on_amp_valueChanged(int value)
{
	m_amplification = value;
	setAmpScaleDisplay();
	m_glScope->setAmp(0.2 / amps[m_amplification]);
}

void GLScopeGUI::on_ampOfs_valueChanged(int value)
{
	m_ampOffset = value;
	setAmpOfsDisplay();
	m_glScope->setAmpOfs(value/100.0); // scale to [-1.0,1.0]
}

void GLScopeGUI::on_scope_traceSizeChanged(int)
{
	setTimeScaleDisplay();
	setTimeOfsDisplay();
}

void GLScopeGUI::on_scope_sampleRateChanged(int)
{
	setTimeScaleDisplay();
	setTimeOfsDisplay();
}

void GLScopeGUI::setTimeScaleDisplay()
{
	m_sampleRate = m_glScope->getSampleRate();
	qreal t = (m_glScope->getTraceSize() * 1.0 / m_sampleRate) / (qreal)m_timeBase;
	/*
	std::cerr << "GLScopeGUI::setTimeScaleDisplay: sample rate: "
			<< m_glScope->getSampleRate()
			<< " traceSize: " << m_glScope->getTraceSize()
			<< " timeBase: " << m_timeBase
			<< " glScope @" << m_glScope << std::endl;
			*/
	if(t < 0.000001)
		ui->timeText->setText(tr("%1\nns").arg(t * 1000000000.0));
	else if(t < 0.001)
		ui->timeText->setText(tr("%1\nµs").arg(t * 1000000.0));
	else if(t < 1.0)
		ui->timeText->setText(tr("%1\nms").arg(t * 1000.0));
	else ui->timeText->setText(tr("%1\ns").arg(t * 1.0));
}

void GLScopeGUI::setTimeOfsDisplay()
{
	qreal dt = m_glScope->getTraceSize() * (m_timeOffset/100.0) / m_sampleRate;

	if(dt < 0.000001)
		ui->timeOfsText->setText(tr("%1\nns").arg(dt * 1000000000.0));
	else if(dt < 0.001)
		ui->timeOfsText->setText(tr("%1\nµs").arg(dt * 1000000.0));
	else if(dt < 1.0)
		ui->timeOfsText->setText(tr("%1\nms").arg(dt * 1000.0));
	else ui->timeOfsText->setText(tr("%1\ns").arg(dt * 1.0));

	//ui->timeOfsText->setText(tr("%1").arg(value/100.0, 0, 'f', 2));
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
	setTimeOfsDisplay();
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

	setAmpScaleDisplay();
	setAmpOfsDisplay();
}

void GLScopeGUI::on_horizView_clicked()
{
	m_displayOrientation = Qt::Horizontal;
	m_displays = GLScope::DisplayBoth;
	if(ui->horizView->isChecked()) {
		ui->vertView->setChecked(false);
		ui->onlyPrimeView->setChecked(false);
		ui->onlySecondView->setChecked(false);
		m_glScope->setOrientation(Qt::Horizontal);
		m_glScope->setDisplays(GLScope::DisplayBoth);
	} else {
		ui->horizView->setChecked(true);
		m_glScope->setOrientation(Qt::Horizontal);
		m_glScope->setDisplays(GLScope::DisplayBoth);
	}
}

void GLScopeGUI::on_vertView_clicked()
{
	m_displayOrientation = Qt::Vertical;
	m_displays = GLScope::DisplayBoth;
	if(ui->vertView->isChecked()) {
		ui->horizView->setChecked(false);
		ui->onlyPrimeView->setChecked(false);
		ui->onlySecondView->setChecked(false);
		m_glScope->setOrientation(Qt::Vertical);
		m_glScope->setDisplays(GLScope::DisplayBoth);
	} else {
		ui->vertView->setChecked(true);
		m_glScope->setOrientation(Qt::Vertical);
		m_glScope->setDisplays(GLScope::DisplayBoth);
	}
}

void GLScopeGUI::on_onlyPrimeView_clicked()
{
	m_displays = GLScope::DisplayFirstOnly;
	if(ui->onlyPrimeView->isChecked()) {
		ui->horizView->setChecked(false);
		ui->vertView->setChecked(false);
		ui->onlySecondView->setChecked(false);
		m_glScope->setDisplays(GLScope::DisplayFirstOnly);
	} else {
		ui->onlyPrimeView->setChecked(true);
		m_glScope->setDisplays(GLScope::DisplayFirstOnly);
	}
}

void GLScopeGUI::on_onlySecondView_clicked()
{
	m_displays = GLScope::DisplaySecondOnly;
	if(ui->onlySecondView->isChecked()) {
		ui->horizView->setChecked(false);
		ui->vertView->setChecked(false);
		ui->onlyPrimeView->setChecked(false);
		m_glScope->setDisplays(GLScope::DisplaySecondOnly);
	} else {
		ui->onlySecondView->setChecked(true);
		m_glScope->setDisplays(GLScope::DisplaySecondOnly);
	}
}

void GLScopeGUI::on_gridIntensity_valueChanged(int index)
{
	m_displayGridIntensity = index;
	if(m_glScope != NULL)
		m_glScope->setDisplayGridIntensity(m_displayGridIntensity);
}

void GLScopeGUI::on_trigMode_currentIndexChanged(int index)
{
	m_triggerChannel = index;
	setTrigLevelDisplay();
	applyTriggerSettings();
}

void GLScopeGUI::on_trigLevel_valueChanged(int value)
{
	m_triggerLevel = value;
	setTrigLevelDisplay();
	applyTriggerSettings();
}

void GLScopeGUI::on_slopePos_clicked()
{
	m_triggerPositiveEdge = true;

	if(ui->slopePos->isChecked()) {
		ui->slopeNeg->setChecked(false);
	} else {
		ui->slopePos->setChecked(true);
	}

	applyTriggerSettings();
}

void GLScopeGUI::on_slopeNeg_clicked()
{
	m_triggerPositiveEdge = false;

	if(ui->slopeNeg->isChecked()) {
		ui->slopePos->setChecked(false);
	} else {
		ui->slopeNeg->setChecked(true);
	}

	applyTriggerSettings();
}

void GLScopeGUI::on_oneShot_clicked()
{
	m_scopeVis->setOneShot(ui->oneShot->isChecked());
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
