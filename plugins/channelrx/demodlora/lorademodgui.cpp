///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2020 Edouard Griffiths, F4EXB                              //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "device/deviceuiset.h"
#include <QDockWidget>
#include <QMainWindow>

#include "ui_lorademodgui.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "gui/glspectrum.h"
#include "gui/glspectrumgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"

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
    if (m_settings.deserialize(data))
    {
        displaySettings();
        applySettings(true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool LoRaDemodGUI::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        int basebandSampleRate = notif.getSampleRate();
        qDebug() << "LoRaDemodGUI::handleMessage: DSPSignalNotification: m_basebandSampleRate: " << basebandSampleRate;

        if (basebandSampleRate != m_basebandSampleRate)
        {
            m_basebandSampleRate = basebandSampleRate;
            setBandwidths();
        }

        return true;
    }
    else
    {
    	return false;
    }
}

void LoRaDemodGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void LoRaDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
	applySettings();
}

void LoRaDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void LoRaDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void LoRaDemodGUI::on_BW_valueChanged(int value)
{
    if (value < 0) {
        m_settings.m_bandwidthIndex = 0;
    } else if (value < LoRaDemodSettings::nbBandwidths) {
        m_settings.m_bandwidthIndex = value;
    } else {
        m_settings.m_bandwidthIndex = LoRaDemodSettings::nbBandwidths - 1;
    }

	int thisBW = LoRaDemodSettings::bandwidths[value];
	ui->BWText->setText(QString("%1 Hz").arg(thisBW));
	m_channelMarker.setBandwidth(thisBW);
	ui->glSpectrum->setSampleRate(thisBW);
	ui->glSpectrum->setCenterFrequency(thisBW/2);

	applySettings();
}

void LoRaDemodGUI::on_Spread_valueChanged(int value)
{
    m_settings.m_spreadFactor = value;
    ui->SpreadText->setText(tr("%1").arg(value));
    ui->spectrumGUI->setFFTSize(m_settings.m_spreadFactor);

    applySettings();
}

void LoRaDemodGUI::on_deBits_valueChanged(int value)
{
    m_settings.m_deBits = value;
    ui->deBitsText->setText(tr("%1").arg(m_settings.m_deBits));
    applySettings();
}

void LoRaDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
}

LoRaDemodGUI::LoRaDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::LoRaDemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
    m_basebandSampleRate(250000),
	m_doApplySettings(true)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

	m_spectrumVis = new SpectrumVis(SDR_RX_SCALEF, ui->glSpectrum);
	m_LoRaDemod = (LoRaDemod*) rxChannel; //new LoRaDemod(m_deviceUISet->m_deviceSourceAPI);
	m_LoRaDemod->setSpectrumSink(m_spectrumVis);
    m_LoRaDemod->setMessageQueueToGUI(getInputMessageQueue());

	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

	m_channelMarker.setMovable(true);
	m_channelMarker.setVisible(true);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));

	m_deviceUISet->registerRxChannelInstance(LoRaDemod::m_channelIdURI, this);
	m_deviceUISet->addChannelMarker(&m_channelMarker);
	m_deviceUISet->addRollupWidget(this);

	ui->spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, ui->glSpectrum);

	m_settings.setChannelMarker(&m_channelMarker);
	m_settings.setSpectrumGUI(ui->spectrumGUI);

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    setBandwidths();
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
        LoRaDemod::MsgConfigureLoRaDemod* message = LoRaDemod::MsgConfigureLoRaDemod::create( m_settings, force);
        m_LoRaDemod->getInputMessageQueue()->push(message);
	}
}

void LoRaDemodGUI::displaySettings()
{
    int thisBW = LoRaDemodSettings::bandwidths[m_settings.m_bandwidthIndex];

    m_channelMarker.blockSignals(true);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(thisBW);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    setTitleColor(m_settings.m_rgbColor);

	ui->glSpectrum->setSampleRate(thisBW);
	ui->glSpectrum->setCenterFrequency(thisBW/2);

    blockApplySettings(true);
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    ui->BWText->setText(QString("%1 Hz").arg(thisBW));
    ui->BW->setValue(m_settings.m_bandwidthIndex);
    ui->Spread->setValue(m_settings.m_spreadFactor);
    ui->SpreadText->setText(tr("%1").arg(m_settings.m_spreadFactor));
    ui->deBits->setValue(m_settings.m_deBits);
    ui->deBitsText->setText(tr("%1").arg(m_settings.m_deBits));
    ui->spectrumGUI->setFFTSize(m_settings.m_spreadFactor);
    blockApplySettings(false);
}

void LoRaDemodGUI::setBandwidths()
{
    int maxBandwidth = m_basebandSampleRate/LoRaDemodSettings::oversampling;
    int maxIndex = 0;

    for (; (maxIndex < LoRaDemodSettings::nbBandwidths) && (LoRaDemodSettings::bandwidths[maxIndex] <= maxBandwidth); maxIndex++)
    {}

    if (maxIndex != 0)
    {
        qDebug("LoRaDemodGUI::setBandwidths: avl: %d max: %d", maxBandwidth, LoRaDemodSettings::bandwidths[maxIndex-1]);
        ui->BW->setMaximum(maxIndex - 1);
        int index = ui->BW->value();
        ui->BWText->setText(QString("%1 Hz").arg(LoRaDemodSettings::bandwidths[index]));
    }
}