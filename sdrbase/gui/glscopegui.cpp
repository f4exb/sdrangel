#include "gui/glscopegui.h"
#include "dsp/scopevis.h"
#include "dsp/dspcommands.h"
#include "gui/glscope.h"
#include "util/simpleserializer.h"
#include "ui_glscopegui.h"

#include <QDebug>

const qreal GLScopeGUI::amps[11] = { 0.2, 0.1, 0.05, 0.02, 0.01, 0.005, 0.002, 0.001, 0.0005, 0.0002, 0.0001 };

GLScopeGUI::GLScopeGUI(QWidget* parent) :
	QWidget(parent),
	ui(new Ui::GLScopeGUI),
	m_messageQueue(0),
	m_scopeVis(0),
	m_glScope(0),
	m_sampleRate(1),
	m_displayData(GLScope::ModeIQ),
	m_displayOrientation(Qt::Horizontal),
	m_displays(GLScope::DisplayBoth),
	m_timeBase(1),
	m_timeOffset(0),
	m_amplification1(0),
	m_amp1OffsetCoarse(0),
	m_amp1OffsetFine(0),
	m_amplification2(0),
	m_amp2OffsetCoarse(0),
	m_amp2OffsetFine(0),
	m_displayGridIntensity(1),
	m_displayTraceIntensity(50),
	m_triggerIndex(0),
    m_triggerPre(0),
	m_traceLenMult(20)
{
	for (unsigned int i = 0; i < ScopeVis::m_nbTriggers; i++)
	{
		m_triggerChannel[i] = ScopeVis::TriggerFreeRun;
		m_triggerLevelCoarse[i] = 0;
		m_triggerLevelFine[i] = 0;
		m_triggerPositiveEdge[i] = true;
		m_triggerBothEdges[i] = false;
		m_triggerDelay[i] = 0;
		m_triggerCounts[i] = 0;
	}

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
	ui->memIndex->setMaximum((1<<GLScope::m_memHistorySizeLog2)-1);
	connect(m_glScope, SIGNAL(traceSizeChanged(int)), this, SLOT(on_scope_traceSizeChanged(int)));
	connect(m_glScope, SIGNAL(sampleRateChanged(int)), this, SLOT(on_scope_sampleRateChanged(int)));
	applySettings();
}

void GLScopeGUI::setSampleRate(int sampleRate)
{
	m_sampleRate = sampleRate;
}

void GLScopeGUI::resetToDefaults()
{
	m_displayData = GLScope::ModeIQ;
	m_displayOrientation = Qt::Horizontal;
	m_timeBase = 1;
	m_timeOffset = 0;
	m_amplification1 = 0;
	m_amp1OffsetCoarse = 0;
	m_amp1OffsetFine = 0;
	m_amplification2 = 0;
	m_amp2OffsetCoarse = 0;
	m_amp2OffsetFine = 0;
	m_displayGridIntensity = 5;
    m_triggerPre = 0;
    m_traceLenMult = 20;

	for (unsigned int i = 0; i < ScopeVis::m_nbTriggers; i++)
	{
		m_triggerChannel[i] = ScopeVis::TriggerFreeRun;
		m_triggerLevelCoarse[i] = 0;
		m_triggerLevelFine[i] = 0;
		m_triggerPositiveEdge[i] = true;
		m_triggerBothEdges[i] = false;
		m_triggerDelay[i] = 0;
		m_triggerCounts[i] = 0;
	}

	applySettings();
}

QByteArray GLScopeGUI::serialize() const
{
	SimpleSerializer s(1);

	s.writeS32(1, m_displayData);
	s.writeS32(2, m_displayOrientation);
	s.writeS32(3, m_timeBase);
	s.writeS32(4, m_timeOffset);
	s.writeS32(5, m_amplification1);
	s.writeS32(6, m_displayGridIntensity);
	s.writeS32(7, m_amp1OffsetCoarse);
	s.writeS32(8, m_displays);
	s.writeS32(12, m_displayTraceIntensity);
	s.writeS32(13, m_triggerPre);
	s.writeS32(14, m_traceLenMult);
	s.writeS32(18, m_amp1OffsetFine);
	s.writeS32(19, m_amplification2);
	s.writeS32(20, m_amp2OffsetCoarse);
	s.writeS32(21, m_amp2OffsetFine);

	for (unsigned int i = 0; i < ScopeVis::m_nbTriggers; i++)
	{
		s.writeS32(50 + 10*i, m_triggerChannel[i]);
		s.writeS32(51 + 10*i, m_triggerLevelCoarse[i]);
		s.writeS32(52 + 10*i, m_triggerLevelFine[i]);
		s.writeBool(53 + 10*i, m_triggerPositiveEdge[i]);
		s.writeBool(54 + 10*i, m_triggerBothEdges[i]);
		s.writeS32(55 + 10*i, m_triggerDelay[i]);
		s.writeS32(56 + 10*i, m_triggerCounts[i]);
	}

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
		d.readS32(5, &m_amplification1, 0);
		d.readS32(6, &m_displayGridIntensity, 5);
		if(m_timeBase < 0)
			m_timeBase = 1;
		d.readS32(7, &m_amp1OffsetCoarse, 0);
		d.readS32(8, &m_displays, GLScope::DisplayBoth);
		d.readS32(12, &m_displayTraceIntensity, 50);
		d.readS32(13, &m_triggerPre, 0);
		ui->trigPre->setValue(m_triggerPre);
		setTrigPreDisplay();
		d.readS32(14, &m_traceLenMult, 20);
		ui->traceLen->setValue(m_traceLenMult);
		setTraceLenDisplay();
		setTrigDelayDisplay();
		d.readS32(18, &m_amp1OffsetFine, 0);
		d.readS32(19, &m_amplification2, 0);
		d.readS32(20, &m_amp2OffsetCoarse, 0);
		d.readS32(21, &m_amp2OffsetFine, 0);

		for (unsigned int i = 0; i < ScopeVis::m_nbTriggers; i++)
		{
			d.readS32(50 + 10*i, &m_triggerChannel[i], ScopeVis::TriggerFreeRun);
			d.readS32(51 + 10*i, &m_triggerLevelCoarse[i], 0);
			d.readS32(52 + 10*i, &m_triggerLevelFine[i], 0);
			d.readBool(53 + 10*i, &m_triggerPositiveEdge[i], true);
			d.readBool(54 + 10*i, &m_triggerBothEdges[i], false);
			d.readS32(55 + 10*i, &m_triggerDelay[i], 0);
			d.readS32(56 + 10*i, &m_triggerCounts[i], 0);
			m_triggerIndex = i;
			applyTriggerSettings();
		}

		m_triggerIndex = 0;

		setTrigUI(m_triggerIndex);
		setTrigLevelDisplay();
		applySettings();
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

void GLScopeGUI::applyAllTriggerSettings()
{
	quint32 currentTriggerIndex = m_triggerIndex;

	for (unsigned int i = 0; i < ScopeVis::m_nbTriggers; i++)
	{
		m_triggerIndex = i;
		applyTriggerSettings();
	}

	m_triggerIndex = currentTriggerIndex;
}

void GLScopeGUI::setTrigUI(uint index)
{
	index %= ScopeVis::m_nbTriggers;

	ui->trigMode->setCurrentIndex(m_triggerChannel[index]);
	ui->trigLevelCoarse->setValue(m_triggerLevelCoarse[index]);
	ui->trigLevelFine->setValue(m_triggerLevelFine[index]);
	ui->trigDelay->setValue(m_triggerDelay[index]);
	ui->trigCount->setValue(m_triggerCounts[index]);

	if (m_triggerBothEdges[index]) {
		ui->slopePos->setChecked(false);
		ui->slopeNeg->setChecked(false);
		ui->slopeBoth->setChecked(true);
	} else {
		ui->slopeBoth->setChecked(false);
		ui->slopePos->setChecked(m_triggerPositiveEdge[index]);
		ui->slopeNeg->setChecked(!m_triggerPositiveEdge[index]);
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
	ui->amp1->setValue(m_amplification1);
	ui->amp1OfsCoarse->setValue(m_amp1OffsetCoarse);
	ui->amp1OfsFine->setValue(m_amp1OffsetFine);
	ui->amp2->setValue(m_amplification2);
	ui->amp2OfsCoarse->setValue(m_amp2OffsetCoarse);
	ui->amp2OfsFine->setValue(m_amp2OffsetFine);
	ui->gridIntensity->setSliderPosition(m_displayGridIntensity);
	ui->traceIntensity->setSliderPosition(m_displayTraceIntensity);
}

void GLScopeGUI::applyTriggerSettings()
{
	qreal t = (m_triggerLevelCoarse[m_triggerIndex] / 100.0) + (m_triggerLevelFine[m_triggerIndex] / 20000.0); // [-1.0, 1.0]
	qreal triggerLevel;
	quint32 preTriggerSamples = (m_glScope->getTraceSize() * m_triggerPre) / 100;

	if (m_triggerChannel[m_triggerIndex] == ScopeVis::TriggerMagDb) {
		triggerLevel = 100.0 * (t - 1.0); // [-200.0, 0.0]
	}
	else if (m_triggerChannel[m_triggerIndex] == ScopeVis::TriggerMagLin) {
		triggerLevel = 1.0 + t; // [0.0, 2.0]
	}
	else {
		triggerLevel = t; // [-1.0, 1.0]
	}

	m_glScope->setTriggerChannel((ScopeVis::TriggerChannel) m_triggerChannel[m_triggerIndex]);
	m_glScope->setTriggerLevel(t);  // [-1.0, 1.0]
	m_glScope->setTriggerPre(m_triggerPre/100.0); // [0.0, 1.0]

	m_scopeVis->configure(m_messageQueue,
			m_triggerIndex,
			(ScopeVis::TriggerChannel) m_triggerChannel[m_triggerIndex],
			triggerLevel,
			m_triggerPositiveEdge[m_triggerIndex],
			m_triggerBothEdges[m_triggerIndex],
			preTriggerSamples,
            m_triggerDelay[m_triggerIndex],
			m_triggerCounts[m_triggerIndex],
			m_traceLenMult * ScopeVis::m_traceChunkSize);
}

void GLScopeGUI::setTrigLevelDisplay()
{
	qreal t = (m_triggerLevelCoarse[m_triggerIndex] / 100.0) + (m_triggerLevelFine[m_triggerIndex] / 20000.0);

	ui->trigLevelCoarse->setToolTip(QString("Trigger level coarse: %1 %").arg(m_triggerLevelCoarse[m_triggerIndex]));
	ui->trigLevelFine->setToolTip(QString("Trigger level fine: %1 ppm").arg(m_triggerLevelFine[m_triggerIndex] * 50));

	if (m_triggerChannel[m_triggerIndex] == ScopeVis::TriggerMagDb) {
		ui->trigText->setText(tr("%1\ndB").arg(100.0 * (t - 1.0), 0, 'f', 1));
	}
	else
	{
		qreal a;

		if (m_triggerChannel[m_triggerIndex] == ScopeVis::TriggerMagLin) {
			a = 1.0 + t;
		} else {
			a = t;
		}

		if(fabs(a) < 0.000001)
			ui->trigText->setText(tr("%1\nn").arg(a * 1000000000.0));
		else if(fabs(a) < 0.001)
			ui->trigText->setText(tr("%1\nµ").arg(a * 1000000.0));
		else if(fabs(a) < 1.0)
			ui->trigText->setText(tr("%1\nm").arg(a * 1000.0));
		else
			ui->trigText->setText(tr("%1").arg(a * 1.0));
	}
}

void GLScopeGUI::setAmp1ScaleDisplay()
{
	if ((m_glScope->getDataMode() == GLScope::ModeMagdBPha) || (m_glScope->getDataMode() == GLScope::ModeMagdBDPha))
	{
		ui->amp1Text->setText(tr("%1\ndB").arg(amps[m_amplification1]*500.0, 0, 'f', 1));
	}
	else
	{
		qreal a = amps[m_amplification1]*10.0;

		if(a < 0.000001)
			ui->amp1Text->setText(tr("%1\nn").arg(a * 1000000000.0));
		else if(a < 0.001)
			ui->amp1Text->setText(tr("%1\nµ").arg(a * 1000000.0));
		else if(a < 1.0)
			ui->amp1Text->setText(tr("%1\nm").arg(a * 1000.0));
		else
			ui->amp1Text->setText(tr("%1").arg(a * 1.0));
	}
}

void GLScopeGUI::setAmp2ScaleDisplay()
{
	if ((m_glScope->getDataMode() == GLScope::ModeMagdBPha)
			|| (m_glScope->getDataMode() == GLScope::ModeMagdBDPha)
			|| (m_glScope->getDataMode() == GLScope::ModeMagLinPha)
			|| (m_glScope->getDataMode() == GLScope::ModeMagLinDPha))
	{
		ui->amp2Text->setText(tr("%1").arg(amps[m_amplification2]*5.0, 0, 'f', 3));
	}
	else
	{
		qreal a = amps[m_amplification2]*10.0;

		if(a < 0.000001)
			ui->amp2Text->setText(tr("%1\nn").arg(a * 1000000000.0));
		else if(a < 0.001)
			ui->amp2Text->setText(tr("%1\nµ").arg(a * 1000000.0));
		else if(a < 1.0)
			ui->amp2Text->setText(tr("%1\nm").arg(a * 1000.0));
		else
			ui->amp2Text->setText(tr("%1").arg(a * 1.0));
	}
}

void GLScopeGUI::setAmp1OfsDisplay()
{
	qreal o = (m_amp1OffsetCoarse * 10.0) + (m_amp1OffsetFine / 20.0);

	if ((m_glScope->getDataMode() == GLScope::ModeMagdBPha) || (m_glScope->getDataMode() == GLScope::ModeMagdBDPha))
	{
		ui->amp1OfsText->setText(tr("%1\ndB").arg(o/10.0 - 100.0, 0, 'f', 1));
	}
	else
	{
		qreal a;

		if ((m_glScope->getDataMode() == GLScope::ModeMagLinPha) || (m_glScope->getDataMode() == GLScope::ModeMagLinDPha))
		{
			a = o/2000.0;
		}
		else
		{
			a = o/1000.0;
		}

		if(fabs(a) < 0.000001)
			ui->amp1OfsText->setText(tr("%1\nn").arg(a * 1000000000.0));
		else if(fabs(a) < 0.001)
			ui->amp1OfsText->setText(tr("%1\nµ").arg(a * 1000000.0));
		else if(fabs(a) < 1.0)
			ui->amp1OfsText->setText(tr("%1\nm").arg(a * 1000.0));
		else
			ui->amp1OfsText->setText(tr("%1").arg(a * 1.0));
	}
}

void GLScopeGUI::setAmp2OfsDisplay()
{
	qreal o = (m_amp2OffsetCoarse * 10.0) + (m_amp2OffsetFine / 20.0);

	if ((m_glScope->getDataMode() == GLScope::ModeMagdBPha)
			|| (m_glScope->getDataMode() == GLScope::ModeMagdBDPha)
			|| (m_glScope->getDataMode() == GLScope::ModeMagLinPha)
			|| (m_glScope->getDataMode() == GLScope::ModeMagLinDPha))
	{
		ui->amp2OfsText->setText(tr("%1").arg(o/1000.0, 0, 'f', 4));
	}
	else
	{
		qreal a = o/1000.0;

		if(fabs(a) < 0.000001)
			ui->amp2OfsText->setText(tr("%1\nn").arg(a * 1000000000.0));
		else if(fabs(a) < 0.001)
			ui->amp2OfsText->setText(tr("%1\nµ").arg(a * 1000000.0));
		else if(fabs(a) < 1.0)
			ui->amp2OfsText->setText(tr("%1\nm").arg(a * 1000.0));
		else
			ui->amp2OfsText->setText(tr("%1").arg(a * 1.0));
	}
}

void GLScopeGUI::on_amp1_valueChanged(int value)
{
	m_amplification1 = value;
	setAmp1ScaleDisplay();
	m_glScope->setAmp1(0.2 / amps[m_amplification1]);
}

void GLScopeGUI::on_amp1OfsCoarse_valueChanged(int value)
{
	m_amp1OffsetCoarse = value;
	setAmp1OfsDisplay();
	qreal o = (m_amp1OffsetCoarse * 10.0) + (m_amp1OffsetFine / 20.0);
	m_glScope->setAmp1Ofs(o/1000.0); // scale to [-1.0,1.0]
}

void GLScopeGUI::on_amp1OfsFine_valueChanged(int value)
{
	m_amp1OffsetFine = value;
	setAmp1OfsDisplay();
	qreal o = (m_amp1OffsetCoarse * 10.0) + (m_amp1OffsetFine / 20.0);
	m_glScope->setAmp1Ofs(o/1000.0); // scale to [-1.0,1.0]
}

void GLScopeGUI::on_amp2_valueChanged(int value)
{
	m_amplification2 = value;
	setAmp2ScaleDisplay();
	m_glScope->setAmp2(0.2 / amps[m_amplification2]);
}

void GLScopeGUI::on_amp2OfsCoarse_valueChanged(int value)
{
	m_amp2OffsetCoarse = value;
	setAmp2OfsDisplay();
	qreal o = (m_amp2OffsetCoarse * 10.0) + (m_amp2OffsetFine / 20.0);
	m_glScope->setAmp2Ofs(o/1000.0); // scale to [-1.0,1.0]
}

void GLScopeGUI::on_amp2OfsFine_valueChanged(int value)
{
	m_amp2OffsetFine = value;
	setAmp2OfsDisplay();
	qreal o = (m_amp2OffsetCoarse * 10.0) + (m_amp2OffsetFine / 20.0);
	m_glScope->setAmp2Ofs(o/1000.0); // scale to [-1.0,1.0]
}

void GLScopeGUI::on_scope_traceSizeChanged(int)
{
	setTimeScaleDisplay();
	setTraceLenDisplay();
	setTimeOfsDisplay();
	setTrigPreDisplay();
    setTrigDelayDisplay();
	applySettings();

	if (m_triggerPre > 0) {
		applyAllTriggerSettings(); // change of trace size changes number of pre-trigger samples
	}
}

void GLScopeGUI::on_scope_sampleRateChanged(int)
{
	m_sampleRate = m_glScope->getSampleRate();
	ui->sampleRateText->setText(tr("%1\nkS/s").arg(m_sampleRate / 1000.0f, 0, 'f', 2));
	setTimeScaleDisplay();
	setTraceLenDisplay();
	setTimeOfsDisplay();
	setTrigPreDisplay();
    setTrigDelayDisplay();
	applySettings();
}

void GLScopeGUI::setTimeScaleDisplay()
{
	m_sampleRate = m_glScope->getSampleRate();
	double t = (m_glScope->getTraceSize() * 1.0 / m_sampleRate) / (qreal)m_timeBase;

    if(t < 0.000001)
	{
        t = round(t * 100000000000.0) / 100.0;
        ui->timeText->setText(tr("%1\nns").arg(t));
	}
	else if(t < 0.001)
	{
	    t = round(t * 100000000.0) / 100.0;
        ui->timeText->setText(tr("%1\nµs").arg(t));
	}
	else if(t < 1.0)
	{
	    t = round(t * 100000.0) / 100.0;
        ui->timeText->setText(tr("%1\nms").arg(t));
	}
	else
	{
	    t = round(t * 100.0) / 100.0;
        ui->timeText->setText(tr("%1\ns").arg(t));
	}
}

void GLScopeGUI::setTraceLenDisplay()
{
	uint n_samples = m_traceLenMult * ScopeVis::m_traceChunkSize;

	if (n_samples < 1000) {
		ui->traceLenText->setToolTip(tr("%1S").arg(n_samples));
	} else if (n_samples < 1000000) {
		ui->traceLenText->setToolTip(tr("%1kS").arg(n_samples/1000.0));
	} else {
		ui->traceLenText->setToolTip(tr("%1MS").arg(n_samples/1000000.0));
	}

	m_sampleRate = m_glScope->getSampleRate();
	qreal t = (m_glScope->getTraceSize() * 1.0 / m_sampleRate);

	if(t < 0.000001)
		ui->traceLenText->setText(tr("%1\nns").arg(t * 1000000000.0));
	else if(t < 0.001)
		ui->traceLenText->setText(tr("%1\nµs").arg(t * 1000000.0));
	else if(t < 1.0)
		ui->traceLenText->setText(tr("%1\nms").arg(t * 1000.0));
	else
		ui->traceLenText->setText(tr("%1\ns").arg(t * 1.0));
}

void GLScopeGUI::setTrigDelayDisplay()
{
	uint n_samples_delay = m_traceLenMult * ScopeVis::m_traceChunkSize * m_triggerDelay[m_triggerIndex];

	if (n_samples_delay < 1000) {
		ui->trigDelayText->setToolTip(tr("%1S").arg(n_samples_delay));
	} else if (n_samples_delay < 1000000) {
		ui->trigDelayText->setToolTip(tr("%1kS").arg(n_samples_delay/1000.0));
	} else if (n_samples_delay < 1000000000) {
		ui->trigDelayText->setToolTip(tr("%1MS").arg(n_samples_delay/1000000.0));
	} else {
		ui->trigDelayText->setToolTip(tr("%1GS").arg(n_samples_delay/1000000000.0));
	}

	m_sampleRate = m_glScope->getSampleRate();
	qreal t = (n_samples_delay * 1.0 / m_sampleRate);

	if(t < 0.000001)
		ui->trigDelayText->setText(tr("%1\nns").arg(t * 1000000000.0));
	else if(t < 0.001)
		ui->trigDelayText->setText(tr("%1\nµs").arg(t * 1000000.0));
	else if(t < 1.0)
		ui->trigDelayText->setText(tr("%1\nms").arg(t * 1000.0));
	else
		ui->trigDelayText->setText(tr("%1\ns").arg(t * 1.0));
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
	else
		ui->timeOfsText->setText(tr("%1\ns").arg(dt * 1.0));

	//ui->timeOfsText->setText(tr("%1").arg(value/100.0, 0, 'f', 2));
}

void GLScopeGUI::setTrigPreDisplay()
{
	qreal dt = m_glScope->getTraceSize() * (m_triggerPre/100.0) / m_sampleRate;

	if(dt < 0.000001)
		ui->trigPreText->setText(tr("%1\nns").arg(dt * 1000000000.0));
	else if(dt < 0.001)
		ui->trigPreText->setText(tr("%1\nµs").arg(dt * 1000000.0));
	else if(dt < 1.0)
		ui->trigPreText->setText(tr("%1\nms").arg(dt * 1000.0));
	else
		ui->trigPreText->setText(tr("%1\ns").arg(dt * 1.0));
}

void GLScopeGUI::on_time_valueChanged(int value)
{
	m_timeBase = value;
	setTimeScaleDisplay();
	m_glScope->setTimeBase(m_timeBase);
}

void GLScopeGUI::on_traceLen_valueChanged(int value)
{
	if ((value < 1) || (value > 100)) {
		return;
	}
	m_traceLenMult = value;
	setTraceLenDisplay();
	applyTriggerSettings();
}

void GLScopeGUI::on_timeOfs_valueChanged(int value)
{
	if ((value < 0) || (value > 100)) {
		return;
	}
	m_timeOffset = value;
	setTimeOfsDisplay();
	m_glScope->setTimeOfsProMill(value*10);
}

void GLScopeGUI::on_trigPre_valueChanged(int value)
{
	if ((value < 0) || (value > 100)) {
		return;
	}
	m_triggerPre = value;
	setTrigPreDisplay();
	applyTriggerSettings();
}

void GLScopeGUI::on_trigDelay_valueChanged(int value)
{
	if ((value < 0) || (value > 100)) {
		return;
	}
	m_triggerDelay[m_triggerIndex] = value;
	setTrigDelayDisplay();
	applyTriggerSettings();
}

void GLScopeGUI::on_dataMode_currentIndexChanged(int index)
{
	m_displayData = index;

	switch(index) {
		case 0: // i+q
			m_glScope->setMode(GLScope::ModeIQ);
			break;
		case 1: // clostationary
			m_glScope->setMode(GLScope::ModeIQPolar);
			break;
		case 2: // mag(lin)+pha
			m_glScope->setMode(GLScope::ModeMagLinPha);
			break;
		case 3: // mag(dB)+pha
			m_glScope->setMode(GLScope::ModeMagdBPha);
			break;
		case 4: // mag(lin)+dPha
			m_glScope->setMode(GLScope::ModeMagLinDPha);
			break;
		case 5: // mag(dB)+dPha
			m_glScope->setMode(GLScope::ModeMagdBDPha);
			break;
		case 6: // derived1+derived2
			m_glScope->setMode(GLScope::ModeDerived12);
			break;
		case 7: // clostationary
			m_glScope->setMode(GLScope::ModeCyclostationary);
			break;

		default:
			break;
	}

	setAmp1ScaleDisplay();
	setAmp1OfsDisplay();
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
	ui->gridIntensity->setToolTip(QString("Grid intensity: %1").arg(m_displayGridIntensity));
	if(m_glScope != 0)
		m_glScope->setDisplayGridIntensity(m_displayGridIntensity);
}

void GLScopeGUI::on_traceIntensity_valueChanged(int index)
{
	m_displayTraceIntensity = index;
	ui->traceIntensity->setToolTip(QString("Trace intensity: %1").arg(m_displayTraceIntensity));
	if(m_glScope != 0)
		m_glScope->setDisplayTraceIntensity(m_displayTraceIntensity);
}

void GLScopeGUI::on_trigMode_currentIndexChanged(int index)
{
	m_triggerChannel[m_triggerIndex] = index;
	setTrigLevelDisplay();
	applyTriggerSettings();
}

void GLScopeGUI::on_trigLevelCoarse_valueChanged(int value)
{
	m_triggerLevelCoarse[m_triggerIndex] = value;
	setTrigLevelDisplay();
	applyTriggerSettings();
}

void GLScopeGUI::on_trigLevelFine_valueChanged(int value)
{
	m_triggerLevelFine[m_triggerIndex] = value;
	setTrigLevelDisplay();
	applyTriggerSettings();
}

void GLScopeGUI::on_memIndex_valueChanged(int value)
{
	QString text;
	text.sprintf("%02d", value % (1<<GLScope::m_memHistorySizeLog2));
	ui->memIndexText->setText(text);

	if(m_glScope != 0)
	{
		// If entering memory history block new triggers and all trigger controls
		m_scopeVis->blockTrigger(value != 0);
		ui->traceLen->setDisabled(value != 0);
		ui->trigIndex->setDisabled(value != 0);
		ui->trigCount->setDisabled(value != 0);
		ui->trigMode->setDisabled(value != 0);
		ui->slopePos->setDisabled(value != 0);
		ui->slopeNeg->setDisabled(value != 0);
		ui->slopeBoth->setDisabled(value != 0);
		ui->oneShot->setDisabled(value != 0);
		ui->trigLevelCoarse->setDisabled(value != 0);
		ui->trigLevelFine->setDisabled(value != 0);
		ui->trigDelay->setDisabled(value != 0);
		ui->trigPre->setDisabled(value != 0);
		// Set value
		m_glScope->setMemHistoryShift(value);
		ui->sampleRateText->setText(tr("%1\nkS/s").arg(m_glScope->getSampleRate() / 1000.0f, 0, 'f', 2));
	}
}

void GLScopeGUI::on_trigCount_valueChanged(int value)
{
	m_triggerCounts[m_triggerIndex] = value;

	QString text;
	text.sprintf("%02d", value);
	ui->trigCountText->setText(text);

	applyTriggerSettings();
}

void GLScopeGUI::on_trigIndex_valueChanged(int value)
{
	m_triggerIndex = value;
	QString text;
	text.sprintf("%d", value);
	ui->trigIndexText->setText(text);
	setTrigLevelDisplay();
	setTrigUI(m_triggerIndex);
}

void GLScopeGUI::on_slopePos_clicked()
{
	m_triggerPositiveEdge[m_triggerIndex] = true;
	m_triggerBothEdges[m_triggerIndex] = false;

	ui->slopeBoth->setChecked(false);

	if(ui->slopePos->isChecked()) {
		ui->slopeNeg->setChecked(false);
	} else {
		ui->slopePos->setChecked(true);
	}

	applyTriggerSettings();
}

void GLScopeGUI::on_slopeNeg_clicked()
{
	m_triggerPositiveEdge[m_triggerIndex] = false;
	m_triggerBothEdges[m_triggerIndex] = false;

	ui->slopeBoth->setChecked(false);

	if(ui->slopeNeg->isChecked()) {
		ui->slopePos->setChecked(false);
	} else {
		ui->slopeNeg->setChecked(true);
	}

	applyTriggerSettings();
}

void GLScopeGUI::on_slopeBoth_clicked()
{
	qDebug() << "GLScopeGUI::on_slopeBoth_clicked";
	ui->slopePos->setChecked(false);
	ui->slopeNeg->setChecked(false);
	ui->slopeBoth->setChecked(true);
	m_triggerBothEdges[m_triggerIndex] = true;

	applyTriggerSettings();
}

void GLScopeGUI::on_oneShot_clicked()
{
	m_scopeVis->setOneShot(ui->oneShot->isChecked());
}

bool GLScopeGUI::handleMessage(Message* cmd __attribute__((unused)))
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
