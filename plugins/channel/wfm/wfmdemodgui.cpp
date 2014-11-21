#include <QDockWidget>
#include <QMainWindow>
#include "wfmdemodgui.h"
#include "ui_wfmdemodgui.h"
#include "wfmdemodgui.h"
#include "ui_wfmdemodgui.h"
#include "dsp/threadedsamplesink.h"
#include "dsp/channelizer.h"
#include "wfmdemod.h"
#include "dsp/spectrumvis.h"
#include "gui/glspectrum.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "gui/basicchannelsettingswidget.h"

WFMDemodGUI* WFMDemodGUI::create(PluginAPI* pluginAPI)
{
	WFMDemodGUI* gui = new WFMDemodGUI(pluginAPI);
	return gui;
}

void WFMDemodGUI::destroy()
{
	delete this;
}

void WFMDemodGUI::setName(const QString& name)
{
	setObjectName(name);
}

void WFMDemodGUI::resetToDefaults()
{
	ui->afBW->setValue(6);
	ui->volume->setValue(20);
	ui->spectrumGUI->resetToDefaults();
	applySettings();
}

QByteArray WFMDemodGUI::serialize() const
{
	SimpleSerializer s(1);
	s.writeS32(1, m_channelMarker->getCenterFrequency());
	s.writeS32(2, ui->afBW->value());
	s.writeS32(3, ui->volume->value());
	s.writeBlob(4, ui->spectrumGUI->serialize());
	s.writeU32(5, m_channelMarker->getColor().rgb());
	return s.final();
}

bool WFMDemodGUI::deserialize(const QByteArray& data)
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
		d.readS32(2, &tmp, 6);
		ui->afBW->setValue(tmp);
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

bool WFMDemodGUI::handleMessage(Message* message)
{
	return false;
}

void WFMDemodGUI::viewChanged()
{
	applySettings();
}

void WFMDemodGUI::on_afBW_valueChanged(int value)
{
	ui->afBWText->setText(QString("%1 kHz").arg(value));
	applySettings();
}

void WFMDemodGUI::on_volume_valueChanged(int value)
{
	ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
	applySettings();
}

void WFMDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
	/*
	if((widget == ui->spectrumContainer) && (m_wfmDemod != NULL))
		m_wfmDemod->setSpectrum(m_threadedSampleSink->getMessageQueue(), rollDown);
	*/
}

void WFMDemodGUI::onMenuDoubleClicked()
{
	if(!m_basicSettingsShown) {
		m_basicSettingsShown = true;
		BasicChannelSettingsWidget* bcsw = new BasicChannelSettingsWidget(m_channelMarker, this);
		bcsw->show();
	}
}

WFMDemodGUI::WFMDemodGUI(PluginAPI* pluginAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::WFMDemodGUI),
	m_pluginAPI(pluginAPI),
	m_basicSettingsShown(false)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));

	m_audioFifo = new AudioFifo(4, 48000 / 4);
	m_spectrumVis = new SpectrumVis(ui->glSpectrum);
	m_wfmDemod = new WFMDemod(m_audioFifo, m_spectrumVis);
	m_channelizer = new Channelizer(m_wfmDemod);
	m_threadedSampleSink = new ThreadedSampleSink(m_channelizer);
	m_pluginAPI->addAudioSource(m_audioFifo);
	m_pluginAPI->addSampleSink(m_threadedSampleSink);

	ui->glSpectrum->setCenterFrequency(12000);
	ui->glSpectrum->setSampleRate(24000);
	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);
	m_spectrumVis->configure(m_threadedSampleSink->getMessageQueue(), 64, 10, FFTWindow::BlackmanHarris);

	m_channelMarker = new ChannelMarker(this);
	m_channelMarker->setColor(Qt::blue);
	m_channelMarker->setBandwidth(32000);
	m_channelMarker->setCenterFrequency(0);
	m_channelMarker->setVisible(true);
	connect(m_channelMarker, SIGNAL(changed()), this, SLOT(viewChanged()));
	m_pluginAPI->addChannelMarker(m_channelMarker);

	ui->spectrumGUI->setBuddies(m_threadedSampleSink->getMessageQueue(), m_spectrumVis, ui->glSpectrum);

	applySettings();
}

WFMDemodGUI::~WFMDemodGUI()
{
	m_pluginAPI->removeChannelInstance(this);
	m_pluginAPI->removeAudioSource(m_audioFifo);
	m_pluginAPI->removeSampleSink(m_threadedSampleSink);
	delete m_threadedSampleSink;
	delete m_channelizer;
	delete m_wfmDemod;
	delete m_spectrumVis;
	delete m_audioFifo;
	delete m_channelMarker;
	delete ui;
}

void WFMDemodGUI::applySettings()
{
	setTitleColor(m_channelMarker->getColor());
	m_channelizer->configure(m_threadedSampleSink->getMessageQueue(),
		48000,
		m_channelMarker->getCenterFrequency());
	m_wfmDemod->configure(m_threadedSampleSink->getMessageQueue(),
		ui->afBW->value() * 1000.0,
		ui->volume->value() * 0.1);
}
