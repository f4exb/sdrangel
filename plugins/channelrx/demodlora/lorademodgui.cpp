
#include <device/devicesourceapi.h>
#include <dsp/downchannelizer.h>
#include <QDockWidget>
#include <QMainWindow>

#include "ui_lorademodgui.h"
#include "dsp/spectrumvis.h"
#include "gui/glspectrum.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "gui/basicchannelsettingswidget.h"
#include "dsp/dspengine.h"

#include "lorademod.h"
#include "lorademodgui.h"

const QString LoRaDemodGUI::m_channelID = "de.maintech.sdrangelove.channel.lora";

LoRaDemodGUI* LoRaDemodGUI::create(PluginAPI* pluginAPI, DeviceSourceAPI *deviceAPI)
{
	LoRaDemodGUI* gui = new LoRaDemodGUI(pluginAPI, deviceAPI);
	return gui;
}

void LoRaDemodGUI::destroy()
{
	delete this;
}

void LoRaDemodGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString LoRaDemodGUI::getName() const
{
	return objectName();
}

qint64 LoRaDemodGUI::getCenterFrequency() const {
	return m_channelMarker.getCenterFrequency();
}

void LoRaDemodGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
}

void LoRaDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray LoRaDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool LoRaDemodGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data)) {
        displaySettings();
        applySettings(true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

bool LoRaDemodGUI::handleMessage(const Message& message __attribute__((unused)))
{
	return false;
}

void LoRaDemodGUI::viewChanged()
{
	applySettings();
}

void LoRaDemodGUI::on_BW_valueChanged(int value)
{
    if (value < 0) {
        m_settings.m_bandwidthIndex = 0;
    } else if (value < LoRaDemodSettings::nb_bandwidths) {
        m_settings.m_bandwidthIndex = value;
    } else {
        m_settings.m_bandwidthIndex = LoRaDemodSettings::nb_bandwidths - 1;
    }

	int thisBW = LoRaDemodSettings::bandwidths[value];
	ui->BWText->setText(QString("%1 Hz").arg(thisBW));
	m_channelMarker.setBandwidth(thisBW);

	applySettings();
}

void LoRaDemodGUI::on_Spread_valueChanged(int value __attribute__((unused)))
{
}

void LoRaDemodGUI::onWidgetRolled(QWidget* widget __attribute__((unused)), bool rollDown __attribute__((unused)))
{
	/*
	if((widget == ui->spectrumContainer) && (m_LoRaDemod != NULL))
		m_LoRaDemod->setSpectrum(m_threadedSampleSink->getMessageQueue(), rollDown);
	*/
}

void LoRaDemodGUI::onMenuDoubleClicked()
{
	if(!m_basicSettingsShown) {
		m_basicSettingsShown = true;
		BasicChannelSettingsWidget* bcsw = new BasicChannelSettingsWidget(&m_channelMarker, this);
		bcsw->show();
	}
}

LoRaDemodGUI::LoRaDemodGUI(PluginAPI* pluginAPI, DeviceSourceAPI *deviceAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::LoRaDemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceAPI(deviceAPI),
	m_channelMarker(this),
	m_basicSettingsShown(false),
	m_doApplySettings(true)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));

	m_spectrumVis = new SpectrumVis(ui->glSpectrum);
	m_LoRaDemod = new LoRaDemod(m_deviceAPI);
	m_LoRaDemod->setSpectrumSink(m_spectrumVis);

	ui->glSpectrum->setCenterFrequency(16000);
	ui->glSpectrum->setSampleRate(32000);
	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);

//	setTitleColor(Qt::magenta);
//
//	m_channelMarker.setColor(Qt::magenta);
//	m_channelMarker.setBandwidth(LoRaDemodSettings::bandwidths[0]);
//	m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setVisible(true);

	connect(&m_channelMarker, SIGNAL(changed()), this, SLOT(viewChanged()));

	m_deviceAPI->registerChannelInstance(m_channelID, this);
	m_deviceAPI->addChannelMarker(&m_channelMarker);
	m_deviceAPI->addRollupWidget(this);

	ui->spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, ui->glSpectrum);

	m_settings.setChannelMarker(&m_channelMarker);
	m_settings.setSpectrumGUI(ui->spectrumGUI);

	displaySettings();
	applySettings(true);
}

LoRaDemodGUI::~LoRaDemodGUI()
{
    m_deviceAPI->removeChannelInstance(this);
	delete m_LoRaDemod;
	delete m_spectrumVis;
	delete ui;
}

void LoRaDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void LoRaDemodGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
        setTitleColor(m_channelMarker.getColor());

        LoRaDemod::MsgConfigureChannelizer* channelConfigMsg = LoRaDemod::MsgConfigureChannelizer::create(
                LoRaDemodSettings::bandwidths[m_settings.m_bandwidthIndex],
                m_channelMarker.getCenterFrequency());
        m_LoRaDemod->getInputMessageQueue()->push(channelConfigMsg);

        LoRaDemod::MsgConfigureLoRaDemod* message = LoRaDemod::MsgConfigureLoRaDemod::create( m_settings, force);
        m_LoRaDemod->getInputMessageQueue()->push(message);
	}
}

void LoRaDemodGUI::displaySettings()
{
    blockApplySettings(true);
    int thisBW = LoRaDemodSettings::bandwidths[m_settings.m_bandwidthIndex];
    ui->BWText->setText(QString("%1 Hz").arg(thisBW));
    ui->BW->setValue(m_settings.m_bandwidthIndex);
    m_channelMarker.setBandwidth(thisBW);
    blockApplySettings(false);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setBandwidth(thisBW);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    setTitleColor(m_settings.m_rgbColor);
    m_channelMarker.blockSignals(false);
}
