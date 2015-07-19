#include <QDockWidget>
#include <QMainWindow>
#include "ui_chanalyzergui.h"
#include "dsp/threadedsamplesink.h"
#include "dsp/channelizer.h"
#include "dsp/spectrumscopecombovis.h"
#include "dsp/spectrumvis.h"
#include "dsp/scopevis.h"
#include "gui/glspectrum.h"
#include "gui/glscope.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "gui/basicchannelsettingswidget.h"

#include <iostream>
#include "chanalyzer.h"
#include "chanalyzergui.h"

ChannelAnalyzerGUI* ChannelAnalyzerGUI::create(PluginAPI* pluginAPI)
{
	ChannelAnalyzerGUI* gui = new ChannelAnalyzerGUI(pluginAPI);
	return gui;
}

void ChannelAnalyzerGUI::destroy()
{
	delete this;
}

void ChannelAnalyzerGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString ChannelAnalyzerGUI::getName() const
{
	return objectName();
}

qint64 ChannelAnalyzerGUI::getCenterFrequency() const
{
	return m_channelMarker->getCenterFrequency();
}

void ChannelAnalyzerGUI::resetToDefaults()
{
	ui->BW->setValue(30);
	ui->deltaFrequency->setValue(0);
	ui->spanLog2->setValue(3);
	applySettings();
}

QByteArray ChannelAnalyzerGUI::serialize() const
{
	SimpleSerializer s(1);
	s.writeS32(1, m_channelMarker->getCenterFrequency());
	s.writeS32(2, ui->BW->value());
	s.writeBlob(3, ui->spectrumGUI->serialize());
	s.writeU32(4, m_channelMarker->getColor().rgb());
	s.writeS32(5, ui->lowCut->value());
	s.writeS32(6, ui->spanLog2->value());
	s.writeBool(7, ui->ssb->isChecked());
	s.writeBlob(8, ui->scopeGUI->serialize());
	return s.final();
}

bool ChannelAnalyzerGUI::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if(!d.isValid()) {
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1) {
		QByteArray bytetmp;
		quint32 u32tmp;
		qint32 tmp;
		bool tmpBool;
		d.readS32(1, &tmp, 0);
		m_channelMarker->setCenterFrequency(tmp);
		d.readS32(2, &tmp, 30);
		ui->BW->setValue(tmp);
		d.readBlob(3, &bytetmp);
		ui->spectrumGUI->deserialize(bytetmp);
		if(d.readU32(4, &u32tmp))
			m_channelMarker->setColor(u32tmp);
		d.readS32(5, &tmp, 3);
		ui->lowCut->setValue(tmp);
		d.readS32(6, &tmp, 20);
		ui->spanLog2->setValue(tmp);
		setNewRate(tmp);
		d.readBool(7, &tmpBool, false);
		ui->ssb->setChecked(tmpBool);
		d.readBlob(8, &bytetmp);
		ui->scopeGUI->deserialize(bytetmp);
		applySettings();
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

bool ChannelAnalyzerGUI::handleMessage(Message* message)
{
	return false;
}

void ChannelAnalyzerGUI::viewChanged()
{
	applySettings();
}

void ChannelAnalyzerGUI::channelSampleRateChanged()
{
	setNewRate(m_spanLog2);
}

void ChannelAnalyzerGUI::on_deltaMinus_clicked(bool minus)
{
	int deltaFrequency = m_channelMarker->getCenterFrequency();
	bool minusDelta = (deltaFrequency < 0);

	if (minus ^ minusDelta) // sign change
	{
		m_channelMarker->setCenterFrequency(-deltaFrequency);
	}
}

void ChannelAnalyzerGUI::on_deltaFrequency_changed(quint64 value)
{
	if (ui->deltaMinus->isChecked()) {
		m_channelMarker->setCenterFrequency(-value);
	} else {
		m_channelMarker->setCenterFrequency(value);
	}
}

void ChannelAnalyzerGUI::on_BW_valueChanged(int value)
{
	QString s = QString::number(value/10.0, 'f', 1);
	ui->BWText->setText(tr("%1k").arg(s));
	m_channelMarker->setBandwidth(value * 100 * 2);

	if (ui->ssb->isChecked())
	{
		if (value < 0) {
			m_channelMarker->setSidebands(ChannelMarker::lsb);
		} else {
			m_channelMarker->setSidebands(ChannelMarker::usb);
		}
	}
	else
	{
		m_channelMarker->setSidebands(ChannelMarker::dsb);
	}

	on_lowCut_valueChanged(m_channelMarker->getLowCutoff()/100);
}

int ChannelAnalyzerGUI::getEffectiveLowCutoff(int lowCutoff)
{
	int ssbBW = m_channelMarker->getBandwidth() / 2;
	int effectiveLowCutoff = lowCutoff;
	const int guard = 100;

	if (ssbBW < 0) {
		if (effectiveLowCutoff < ssbBW + guard) {
			effectiveLowCutoff = ssbBW + guard;
		}
		if (effectiveLowCutoff > 0)	{
			effectiveLowCutoff = 0;
		}
	} else {
		if (effectiveLowCutoff > ssbBW - guard)	{
			effectiveLowCutoff = ssbBW - guard;
		}
		if (effectiveLowCutoff < 0)	{
			effectiveLowCutoff = 0;
		}
	}

	return effectiveLowCutoff;
}

void ChannelAnalyzerGUI::on_lowCut_valueChanged(int value)
{
	int lowCutoff = getEffectiveLowCutoff(value * 100);
	m_channelMarker->setLowCutoff(lowCutoff);
	QString s = QString::number(lowCutoff/1000.0, 'f', 1);
	ui->lowCutText->setText(tr("%1k").arg(s));
	ui->lowCut->setValue(lowCutoff/100);
	applySettings();
}

void ChannelAnalyzerGUI::on_spanLog2_valueChanged(int value)
{
	if (setNewRate(value)) {
		applySettings();
	}

}

void ChannelAnalyzerGUI::on_ssb_toggled(bool checked)
{
	if (checked)
	{
		if (ui->BW->value() < 0) {
			m_channelMarker->setSidebands(ChannelMarker::lsb);
		} else {
			m_channelMarker->setSidebands(ChannelMarker::usb);
		}

		ui->glSpectrum->setCenterFrequency(m_rate/4);
		ui->glSpectrum->setSampleRate(m_rate/2);
		ui->glSpectrum->setSsbSpectrum(true);

		on_lowCut_valueChanged(m_channelMarker->getLowCutoff()/100);
	}
	else
	{
		m_channelMarker->setSidebands(ChannelMarker::dsb);

		ui->glSpectrum->setCenterFrequency(0);
		ui->glSpectrum->setSampleRate(m_rate);
		ui->glSpectrum->setSsbSpectrum(false);

		applySettings();
	}
}

void ChannelAnalyzerGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
	/*
	if((widget == ui->spectrumContainer) && (m_ssbDemod != NULL))
		m_ssbDemod->setSpectrum(m_threadedSampleSink->getMessageQueue(), rollDown);
	*/
}

void ChannelAnalyzerGUI::onMenuDoubleClicked()
{
	if(!m_basicSettingsShown) {
		m_basicSettingsShown = true;
		BasicChannelSettingsWidget* bcsw = new BasicChannelSettingsWidget(m_channelMarker, this);
		bcsw->show();
	}
}

ChannelAnalyzerGUI::ChannelAnalyzerGUI(PluginAPI* pluginAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::ChannelAnalyzerGUI),
	m_pluginAPI(pluginAPI),
	m_basicSettingsShown(false),
	m_rate(6000),
	m_spanLog2(3)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));

	m_spectrumVis = new SpectrumVis(ui->glSpectrum);
	m_scopeVis = new ScopeVis(ui->glScope);
	m_spectrumScopeComboVis = new SpectrumScopeComboVis(m_spectrumVis, m_scopeVis);
	m_channelAnalyzer = new ChannelAnalyzer(m_spectrumScopeComboVis);
	m_channelizer = new Channelizer(m_channelAnalyzer);
	connect(m_channelizer, SIGNAL(inputSampleRateChanged()), this, SLOT(channelSampleRateChanged()));
	m_threadedSampleSink = new ThreadedSampleSink(m_channelizer);
	m_pluginAPI->addSampleSink(m_threadedSampleSink);

	ui->glSpectrum->setCenterFrequency(m_rate/2);
	ui->glSpectrum->setSampleRate(m_rate);
	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);
	ui->glSpectrum->setSsbSpectrum(true);

	m_channelMarker = new ChannelMarker(this);
	m_channelMarker->setColor(Qt::gray);
	m_channelMarker->setBandwidth(m_rate);
	m_channelMarker->setSidebands(ChannelMarker::usb);
	m_channelMarker->setCenterFrequency(0);
	m_channelMarker->setVisible(true);
	connect(m_channelMarker, SIGNAL(changed()), this, SLOT(viewChanged()));
	m_pluginAPI->addChannelMarker(m_channelMarker);

	ui->spectrumGUI->setBuddies(m_threadedSampleSink->getMessageQueue(), m_spectrumVis, ui->glSpectrum);
	ui->scopeGUI->setBuddies(m_threadedSampleSink->getMessageQueue(), m_scopeVis, ui->glScope);

	applySettings();
}

ChannelAnalyzerGUI::~ChannelAnalyzerGUI()
{
	m_pluginAPI->removeChannelInstance(this);
	m_pluginAPI->removeSampleSink(m_threadedSampleSink);
	delete m_threadedSampleSink;
	delete m_channelizer;
	delete m_channelAnalyzer;
	delete m_spectrumVis;
	delete m_scopeVis;
	delete m_spectrumScopeComboVis;
	delete m_channelMarker;
	delete ui;
}

bool ChannelAnalyzerGUI::setNewRate(int spanLog2)
{
	if ((spanLog2 < 0) || (spanLog2 > 6)) {
		return false;
	}

	m_spanLog2 = spanLog2;
	//m_rate = 48000 / (1<<spanLog2);
	m_rate = m_channelAnalyzer->getSampleRate() / (1<<spanLog2);

	if (ui->BW->value() < -m_rate/100) {
		ui->BW->setValue(-m_rate/100);
		m_channelMarker->setBandwidth(-m_rate*2);
	} else if (ui->BW->value() > m_rate/100) {
		ui->BW->setValue(m_rate/100);
		m_channelMarker->setBandwidth(m_rate*2);
	}

	if (ui->lowCut->value() < -m_rate/100) {
		ui->lowCut->setValue(-m_rate/100);
		m_channelMarker->setLowCutoff(-m_rate);
	} else if (ui->lowCut->value() > m_rate/100) {
		ui->lowCut->setValue(m_rate/100);
		m_channelMarker->setLowCutoff(m_rate);
	}

	ui->BW->setMinimum(-m_rate/100);
	ui->lowCut->setMinimum(-m_rate/100);
	ui->BW->setMaximum(m_rate/100);
	ui->lowCut->setMaximum(m_rate/100);

	QString s = QString::number(m_rate/1000.0, 'f', 1);
	ui->spanText->setText(tr("%1k").arg(s));

	if (ui->ssb->isChecked())
	{
		if (ui->BW->value() < 0) {
			m_channelMarker->setSidebands(ChannelMarker::lsb);
		} else {
			m_channelMarker->setSidebands(ChannelMarker::usb);
		}

		ui->glSpectrum->setCenterFrequency(m_rate/4);
		ui->glSpectrum->setSampleRate(m_rate/2);
		ui->glSpectrum->setSsbSpectrum(true);
	}
	else
	{
		m_channelMarker->setSidebands(ChannelMarker::dsb);

		ui->glSpectrum->setCenterFrequency(0);
		ui->glSpectrum->setSampleRate(m_rate);
		ui->glSpectrum->setSsbSpectrum(false);
	}

	ui->glScope->setSampleRate(m_rate);
	m_scopeVis->setSampleRate(m_rate);

	return true;
}

void ChannelAnalyzerGUI::applySettings()
{
	setTitleColor(m_channelMarker->getColor());
	ui->deltaFrequency->setValue(abs(m_channelMarker->getCenterFrequency()));
	ui->deltaMinus->setChecked(m_channelMarker->getCenterFrequency() < 0);
	m_channelizer->configure(m_threadedSampleSink->getMessageQueue(),
		m_channelizer->getInputSampleRate(),
		m_channelMarker->getCenterFrequency());
	m_channelAnalyzer->configure(m_threadedSampleSink->getMessageQueue(),
		ui->BW->value() * 100.0,
		ui->lowCut->value() * 100.0,
		m_spanLog2,
		ui->ssb->isChecked());
}

void ChannelAnalyzerGUI::leaveEvent(QEvent*)
{
	m_channelMarker->setHighlighted(false);
}

void ChannelAnalyzerGUI::enterEvent(QEvent*)
{
	m_channelMarker->setHighlighted(true);
}

