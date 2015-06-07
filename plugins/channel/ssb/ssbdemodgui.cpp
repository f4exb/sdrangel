#include <QDockWidget>
#include <QMainWindow>
#include "ssbdemodgui.h"
#include "ui_ssbdemodgui.h"
#include "ssbdemodgui.h"
#include "ui_ssbdemodgui.h"
#include "dsp/threadedsamplesink.h"
#include "dsp/channelizer.h"
#include "ssbdemod.h"
#include "dsp/spectrumvis.h"
#include "gui/glspectrum.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "gui/basicchannelsettingswidget.h"

#include <iostream>

SSBDemodGUI* SSBDemodGUI::create(PluginAPI* pluginAPI)
{
	SSBDemodGUI* gui = new SSBDemodGUI(pluginAPI);
	return gui;
}

void SSBDemodGUI::destroy()
{
	delete this;
}

void SSBDemodGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString SSBDemodGUI::getName() const
{
	return objectName();
}

void SSBDemodGUI::resetToDefaults()
{
	ui->BW->setValue(30);
	ui->volume->setValue(40);
	ui->deltaFrequency->setValue(0);
	applySettings();
}

QByteArray SSBDemodGUI::serialize() const
{
	SimpleSerializer s(1);
	s.writeS32(1, m_channelMarker->getCenterFrequency());
	s.writeS32(2, ui->BW->value());
	s.writeS32(6, ui->lowCut->value());
	s.writeS32(3, ui->volume->value());
	s.writeBlob(4, ui->spectrumGUI->serialize());
	s.writeU32(5, m_channelMarker->getColor().rgb());
	return s.final();
}

bool SSBDemodGUI::deserialize(const QByteArray& data)
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
		d.readS32(1, &tmp, 0);
		m_channelMarker->setCenterFrequency(tmp);
		d.readS32(2, &tmp, 30);
		ui->BW->setValue(tmp);
		d.readS32(6, &tmp, 3);
		ui->lowCut->setValue(tmp);
		d.readS32(3, &tmp, 20);
		ui->volume->setValue(tmp);
		d.readBlob(4, &bytetmp);
		ui->spectrumGUI->deserialize(bytetmp);
		if(d.readU32(5, &u32tmp))
			m_channelMarker->setColor(u32tmp);
		applySettings();
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

bool SSBDemodGUI::handleMessage(Message* message)
{
	return false;
}

void SSBDemodGUI::viewChanged()
{
	applySettings();
}

void SSBDemodGUI::on_deltaMinus_clicked(bool minus)
{
	int deltaFrequency = m_channelMarker->getCenterFrequency();
	bool minusDelta = (deltaFrequency < 0);

	if (minus ^ minusDelta) // sign change
	{
		m_channelMarker->setCenterFrequency(-deltaFrequency);
	}
}

void SSBDemodGUI::on_deltaFrequency_changed(quint64 value)
{
	if (ui->deltaMinus->isChecked()) {
		m_channelMarker->setCenterFrequency(-value);
	} else {
		m_channelMarker->setCenterFrequency(value);
	}
}

void SSBDemodGUI::on_BW_valueChanged(int value)
{
	QString s = QString::number(value/10.0, 'f', 1);
	ui->BWText->setText(s);
	m_channelMarker->setBandwidth(value * 100 * 2);

	if (value < 0) {
		m_channelMarker->setSidebands(ChannelMarker::lsb);
	} else {
		m_channelMarker->setSidebands(ChannelMarker::usb);
	}

	on_lowCut_valueChanged(m_channelMarker->getLowCutoff()/100);
}

int SSBDemodGUI::getEffectiveLowCutoff(int lowCutoff)
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

void SSBDemodGUI::on_lowCut_valueChanged(int value)
{
	int lowCutoff = getEffectiveLowCutoff(value * 100);
	m_channelMarker->setLowCutoff(lowCutoff);
	QString s = QString::number(lowCutoff/1000.0, 'f', 1);
	ui->lowCutText->setText(s);
	ui->lowCut->setValue(lowCutoff/100);
	applySettings();
}

void SSBDemodGUI::on_volume_valueChanged(int value)
{
	ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
	applySettings();
}

void SSBDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
	/*
	if((widget == ui->spectrumContainer) && (m_ssbDemod != NULL))
		m_ssbDemod->setSpectrum(m_threadedSampleSink->getMessageQueue(), rollDown);
	*/
}

void SSBDemodGUI::onMenuDoubleClicked()
{
	if(!m_basicSettingsShown) {
		m_basicSettingsShown = true;
		BasicChannelSettingsWidget* bcsw = new BasicChannelSettingsWidget(m_channelMarker, this);
		bcsw->show();
	}
}

SSBDemodGUI::SSBDemodGUI(PluginAPI* pluginAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::SSBDemodGUI),
	m_pluginAPI(pluginAPI),
	m_basicSettingsShown(false)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));

	m_audioFifo = new AudioFifo(4, 48000 / 4);
	m_spectrumVis = new SpectrumVis(ui->glSpectrum);
	m_ssbDemod = new SSBDemod(m_audioFifo, m_spectrumVis);
	m_channelizer = new Channelizer(m_ssbDemod);
	m_threadedSampleSink = new ThreadedSampleSink(m_channelizer);
	m_pluginAPI->addAudioSource(m_audioFifo);
	m_pluginAPI->addSampleSink(m_threadedSampleSink);

	ui->glSpectrum->setCenterFrequency(3000);
	ui->glSpectrum->setSampleRate(6000);
	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);

	m_channelMarker = new ChannelMarker(this);
	m_channelMarker->setColor(Qt::green);
	m_channelMarker->setBandwidth(6000);
	m_channelMarker->setSidebands(ChannelMarker::usb);
	m_channelMarker->setCenterFrequency(0);
	m_channelMarker->setVisible(true);
	connect(m_channelMarker, SIGNAL(changed()), this, SLOT(viewChanged()));
	m_pluginAPI->addChannelMarker(m_channelMarker);

	ui->spectrumGUI->setBuddies(m_threadedSampleSink->getMessageQueue(), m_spectrumVis, ui->glSpectrum);

	applySettings();
}

SSBDemodGUI::~SSBDemodGUI()
{
	m_pluginAPI->removeChannelInstance(this);
	m_pluginAPI->removeAudioSource(m_audioFifo);
	m_pluginAPI->removeSampleSink(m_threadedSampleSink);
	delete m_threadedSampleSink;
	delete m_channelizer;
	delete m_ssbDemod;
	delete m_spectrumVis;
	delete m_audioFifo;
	delete m_channelMarker;
	delete ui;
}

void SSBDemodGUI::applySettings()
{
	setTitleColor(m_channelMarker->getColor());
	ui->deltaFrequency->setValue(abs(m_channelMarker->getCenterFrequency()));
	ui->deltaMinus->setChecked(m_channelMarker->getCenterFrequency() < 0);
	m_channelizer->configure(m_threadedSampleSink->getMessageQueue(),
		48000,
		m_channelMarker->getCenterFrequency());
	m_ssbDemod->configure(m_threadedSampleSink->getMessageQueue(),
		ui->BW->value() * 100.0,
		ui->lowCut->value() * 100.0,
		ui->volume->value() / 10.0 );
}

void SSBDemodGUI::leaveEvent(QEvent*)
{
	m_channelMarker->setHighlighted(false);
}

void SSBDemodGUI::enterEvent(QEvent*)
{
	m_channelMarker->setHighlighted(true);
}

