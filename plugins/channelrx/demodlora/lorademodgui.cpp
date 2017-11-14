
#include <device/devicesourceapi.h>
#include "device/deviceuiset.h"
#include <dsp/downchannelizer.h>
#include <QDockWidget>
#include <QMainWindow>

#include "ui_lorademodgui.h"
#include "dsp/spectrumvis.h"
#include "gui/glspectrum.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "dsp/dspengine.h"

#include "lorademod.h"
#include "lorademodgui.h"

LoRaDemodGUI* LoRaDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	LoRaDemodGUI* gui = new LoRaDemodGUI(pluginAPI, deviceUISet, rxChannel);
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
}

LoRaDemodGUI::LoRaDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::LoRaDemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
	m_doApplySettings(true)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

	m_spectrumVis = new SpectrumVis(ui->glSpectrum);
	m_LoRaDemod = (LoRaDemod*) rxChannel; //new LoRaDemod(m_deviceUISet->m_deviceSourceAPI);
	m_LoRaDemod->setSpectrumSink(m_spectrumVis);

	ui->glSpectrum->setCenterFrequency(16000);
	ui->glSpectrum->setSampleRate(32000);
	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);

	m_channelMarker.setMovable(false);
	m_channelMarker.setVisible(true);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(viewChanged()));

	m_deviceUISet->registerRxChannelInstance(LoRaDemod::m_channelID, this);
	m_deviceUISet->addChannelMarker(&m_channelMarker);
	m_deviceUISet->addRollupWidget(this);

	ui->spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, ui->glSpectrum);

	m_settings.setChannelMarker(&m_channelMarker);
	m_settings.setSpectrumGUI(ui->spectrumGUI);

	displaySettings();
	applySettings(true);
}

LoRaDemodGUI::~LoRaDemodGUI()
{
    m_deviceUISet->removeRxChannelInstance(this);
	delete m_LoRaDemod; // TODO: check this: when the GUI closes it has to delete the demodulator
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
    int thisBW = LoRaDemodSettings::bandwidths[m_settings.m_bandwidthIndex];

    m_channelMarker.blockSignals(true);
    m_channelMarker.setBandwidth(thisBW);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    setTitleColor(m_settings.m_rgbColor);
    m_channelMarker.blockSignals(false);

    blockApplySettings(true);
    ui->BWText->setText(QString("%1 Hz").arg(thisBW));
    ui->BW->setValue(m_settings.m_bandwidthIndex);
    blockApplySettings(false);
}
