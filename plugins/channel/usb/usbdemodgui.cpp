#include <QDockWidget>
#include <QMainWindow>
#include "usbdemodgui.h"
#include "ui_usbdemodgui.h"
#include "dsp/threadedsamplesink.h"
#include "dsp/channelizer.h"
#include "usbdemod.h"
#include "dsp/spectrumvis.h"
#include "gui/glspectrum.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "gui/basicchannelsettingswidget.h"

USBDemodGUI* USBDemodGUI::create(PluginAPI* pluginAPI)
{
	USBDemodGUI* gui = new USBDemodGUI(pluginAPI);
	return gui;
}

void USBDemodGUI::destroy()
{
	delete this;
}

void USBDemodGUI::setName(const QString& name)
{
	setObjectName(name);
}

void USBDemodGUI::resetToDefaults()
{
	ui->BW->setValue(3);
	ui->volume->setValue(4);
	applySettings();
}

QByteArray USBDemodGUI::serialize() const
{
	SimpleSerializer s(1);
	s.writeS32(1, m_channelMarker->getCenterFrequency());
	s.writeS32(2, ui->BW->value());
	s.writeS32(3, ui->volume->value());
	s.writeBlob(4, ui->spectrumGUI->serialize());
	s.writeU32(5, m_channelMarker->getColor().rgb());
	return s.final();
}

bool USBDemodGUI::deserialize(const QByteArray& data)
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
		d.readS32(2, &tmp, 3);
		ui->BW->setValue(tmp);
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

bool USBDemodGUI::handleMessage(Message* message)
{
	return false;
}

void USBDemodGUI::viewChanged()
{
	applySettings();
}

void USBDemodGUI::on_BW_valueChanged(int value)
{
	ui->BWText->setText(QString("%1 kHz").arg(value));
	m_channelMarker->setBandwidth(value * 1000);
	m_channelMarker->setCenterFrequency(value * 500);
	applySettings();
}

void USBDemodGUI::on_volume_valueChanged(int value)
{
	ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
	applySettings();
}

void USBDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
	/*
	if((widget == ui->spectrumContainer) && (m_usbDemod != NULL))
		m_usbDemod->setSpectrum(m_threadedSampleSink->getMessageQueue(), rollDown);
	*/
}

void USBDemodGUI::onMenuDoubleClicked()
{
	if(!m_basicSettingsShown) {
		m_basicSettingsShown = true;
		BasicChannelSettingsWidget* bcsw = new BasicChannelSettingsWidget(m_channelMarker, this);
		bcsw->show();
	}
}

USBDemodGUI::USBDemodGUI(PluginAPI* pluginAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::USBDemodGUI),
	m_pluginAPI(pluginAPI),
	m_basicSettingsShown(false)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));

	m_audioFifo = new AudioFifo(4, 48000 / 4);
	m_spectrumVis = new SpectrumVis(ui->glSpectrum);
	m_usbDemod = new USBDemod(m_audioFifo, m_spectrumVis);
	m_channelizer = new Channelizer(m_usbDemod);
	m_threadedSampleSink = new ThreadedSampleSink(m_channelizer);
	m_pluginAPI->addAudioSource(m_audioFifo);
	m_pluginAPI->addSampleSink(m_threadedSampleSink);

	ui->glSpectrum->setCenterFrequency(3000);
	ui->glSpectrum->setSampleRate(6000);
	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);

	m_channelMarker = new ChannelMarker(this);
	m_channelMarker->setColor(Qt::blue);
	m_channelMarker->setBandwidth(5000);
	m_channelMarker->setCenterFrequency(2500);
	m_channelMarker->setVisible(false);
	connect(m_channelMarker, SIGNAL(changed()), this, SLOT(viewChanged()));
	m_pluginAPI->addChannelMarker(m_channelMarker);

	ui->spectrumGUI->setBuddies(m_threadedSampleSink->getMessageQueue(), m_spectrumVis, ui->glSpectrum);

	applySettings();
}

USBDemodGUI::~USBDemodGUI()
{
	m_pluginAPI->removeChannelInstance(this);
	m_pluginAPI->removeAudioSource(m_audioFifo);
	m_pluginAPI->removeSampleSink(m_threadedSampleSink);
	delete m_threadedSampleSink;
	delete m_channelizer;
	delete m_usbDemod;
	delete m_spectrumVis;
	delete m_audioFifo;
	delete m_channelMarker;
	delete ui;
}

void USBDemodGUI::applySettings()
{
	setTitleColor(m_channelMarker->getColor());
	m_channelizer->configure(m_threadedSampleSink->getMessageQueue(),
		48000,
		m_channelMarker->getCenterFrequency());
	m_usbDemod->configure(m_threadedSampleSink->getMessageQueue(),
		ui->BW->value() * 1000.0,
		ui->volume->value() / 10.0 );
}
