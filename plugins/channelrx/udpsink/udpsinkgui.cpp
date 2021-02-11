///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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
#include "plugin/pluginapi.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "ui_udpsinkgui.h"
#include "maincore.h"

#include "udpsink.h"
#include "udpsinkgui.h"

UDPSinkGUI* UDPSinkGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	UDPSinkGUI* gui = new UDPSinkGUI(pluginAPI, deviceUISet, rxChannel);
	return gui;
}

void UDPSinkGUI::destroy()
{
	delete this;
}

void UDPSinkGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettingsImmediate(true);
    applySettings(true);
}

QByteArray UDPSinkGUI::serialize() const
{
    return m_settings.serialize();
}

bool UDPSinkGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        displaySettings();
        applySettingsImmediate(true);
        applySettings(true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

bool UDPSinkGUI::handleMessage(const Message& message )
{
    if (UDPSink::MsgConfigureUDPSink::match(message))
    {
        const UDPSink::MsgConfigureUDPSink& cfg = (UDPSink::MsgConfigureUDPSink&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else
    {
        return false;
    }
}

void UDPSinkGUI::handleSourceMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void UDPSinkGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettingsImmediate();
}

void UDPSinkGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void UDPSinkGUI::tick()
{
    if (m_tickCount % 4 == 0)
    {
//        m_channelPowerAvg.feed(m_udpSrc->getMagSq());
//        double powDb = CalcDb::dbPower(m_channelPowerAvg.average());
        double powDb = CalcDb::dbPower(m_udpSink->getMagSq());
        ui->channelPower->setText(QString::number(powDb, 'f', 1));
        m_inPowerAvg.feed(m_udpSink->getInMagSq());
        double inPowDb = CalcDb::dbPower(m_inPowerAvg.average());
        ui->inputPower->setText(QString::number(inPowDb, 'f', 1));
    }

    if (m_udpSink->getSquelchOpen()) {
        ui->squelchLabel->setStyleSheet("QLabel { background-color : green; }");
    } else {
        ui->squelchLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    }

	m_tickCount++;
}

UDPSinkGUI::UDPSinkGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::UDPSinkGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_udpSink(0),
	m_channelMarker(this),
	m_channelPowerAvg(4, 1e-10),
    m_inPowerAvg(4, 1e-10),
	m_tickCount(0),
    m_doApplySettings(true),
    m_rfBandwidthChanged(false)
{
	ui->setupUi(this);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
	setAttribute(Qt::WA_DeleteOnClose, true);

	m_udpSink = (UDPSink*) rxChannel;
    m_spectrumVis = m_udpSink->getSpectrumVis();
	m_spectrumVis->setGLSpectrum(ui->glSpectrum);
	m_udpSink->setMessageQueueToGUI(getInputMessageQueue());

	ui->fmDeviation->setEnabled(false);

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 8, -99999999, 99999999);
    ui->squelchLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");

	ui->glSpectrum->setCenterFrequency(0);
	ui->glSpectrum->setSampleRate(ui->sampleRate->text().toInt());
	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

	m_channelMarker.blockSignals(true);
	m_channelMarker.setBandwidth(16000);
	m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setTitle("UDP Sample Source");
    m_channelMarker.setColor(m_settings.m_rgbColor);
    m_channelMarker.blockSignals(false);
	m_channelMarker.setVisible(true); // activate signal on the last setting only

	setTitleColor(m_channelMarker.getColor());

	m_settings.setChannelMarker(&m_channelMarker);
	m_settings.setSpectrumGUI(ui->spectrumGUI);

	m_deviceUISet->addChannelMarker(&m_channelMarker);
	m_deviceUISet->addRollupWidget(this);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

	ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);

	displaySettings();
	applySettingsImmediate(true);
	applySettings(true);
}

UDPSinkGUI::~UDPSinkGUI()
{
	delete ui;
}

void UDPSinkGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void UDPSinkGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->sampleRate->setText(QString("%1").arg(m_settings.m_outputSampleRate, 0));
    setSampleFormatIndex(m_settings.m_sampleFormat);

    ui->outputUDPAddress->setText(m_settings.m_udpAddress);
    ui->outputUDPPort->setText(tr("%1").arg(m_settings.m_udpPort));
    ui->inputUDPAudioPort->setText(tr("%1").arg(m_settings.m_audioPort));

    ui->squelch->setValue(m_settings.m_squelchdB);
    ui->squelchText->setText(tr("%1").arg(ui->squelch->value()*1.0, 0, 'f', 0));

    qDebug("UDPSinkGUI::deserialize: m_squelchGate: %d", m_settings.m_squelchGate);
    ui->squelchGate->setValue(m_settings.m_squelchGate);
    ui->squelchGateText->setText(tr("%1").arg(m_settings.m_squelchGate*10.0, 0, 'f', 0));

    ui->rfBandwidth->setText(QString("%1").arg(m_settings.m_rfBandwidth, 0));
    ui->fmDeviation->setText(QString("%1").arg(m_settings.m_fmDeviation, 0));

    ui->agc->setChecked(m_settings.m_agc);
    ui->audioActive->setChecked(m_settings.m_audioActive);
    ui->audioStereo->setChecked(m_settings.m_audioStereo);

    ui->volume->setValue(m_settings.m_volume);
    ui->volumeText->setText(QString("%1").arg(ui->volume->value()));

    ui->gain->setValue(m_settings.m_gain*10.0);
    ui->gainText->setText(tr("%1").arg(ui->gain->value()/10.0, 0, 'f', 1));

    ui->applyBtn->setEnabled(false);
    ui->applyBtn->setStyleSheet("QPushButton { background:rgb(79,79,79); }");

    displayStreamIndex();

    blockApplySettings(false);

    ui->glSpectrum->setSampleRate(m_settings.m_outputSampleRate);
}

void UDPSinkGUI::displayStreamIndex()
{
    if (m_deviceUISet->m_deviceMIMOEngine) {
        setStreamIndicator(tr("%1").arg(m_settings.m_streamIndex));
    } else {
        setStreamIndicator("S"); // single channel indicator
    }
}

void UDPSinkGUI::setSampleFormatIndex(const UDPSinkSettings::SampleFormat& sampleFormat)
{
    switch(sampleFormat)
    {
        case UDPSinkSettings::FormatIQ16:
            ui->sampleFormat->setCurrentIndex(0);
            break;
        case UDPSinkSettings::FormatIQ24:
            ui->sampleFormat->setCurrentIndex(1);
            break;
        case UDPSinkSettings::FormatNFM:
            ui->sampleFormat->setCurrentIndex(2);
            break;
        case UDPSinkSettings::FormatNFMMono:
            ui->sampleFormat->setCurrentIndex(3);
            break;
        case UDPSinkSettings::FormatLSB:
            ui->sampleFormat->setCurrentIndex(4);
            break;
        case UDPSinkSettings::FormatUSB:
            ui->sampleFormat->setCurrentIndex(5);
            break;
        case UDPSinkSettings::FormatLSBMono:
            ui->sampleFormat->setCurrentIndex(6);
            break;
        case UDPSinkSettings::FormatUSBMono:
            ui->sampleFormat->setCurrentIndex(7);
            break;
        case UDPSinkSettings::FormatAMMono:
            ui->sampleFormat->setCurrentIndex(8);
            break;
        case UDPSinkSettings::FormatAMNoDCMono:
            ui->sampleFormat->setCurrentIndex(9);
            break;
        case UDPSinkSettings::FormatAMBPFMono:
            ui->sampleFormat->setCurrentIndex(10);
            break;
        default:
            ui->sampleFormat->setCurrentIndex(0);
            break;
    }
}

void UDPSinkGUI::setSampleFormat(int index)
{
    switch(index)
    {
        case 0:
            m_settings.m_sampleFormat = UDPSinkSettings::FormatIQ16;
            ui->fmDeviation->setEnabled(false);
            break;
        case 1:
            m_settings.m_sampleFormat = UDPSinkSettings::FormatIQ24;
            ui->fmDeviation->setEnabled(false);
            break;
        case 2:
            m_settings.m_sampleFormat = UDPSinkSettings::FormatNFM;
            ui->fmDeviation->setEnabled(true);
            break;
        case 3:
            m_settings.m_sampleFormat = UDPSinkSettings::FormatNFMMono;
            ui->fmDeviation->setEnabled(true);
            break;
        case 4:
            m_settings.m_sampleFormat = UDPSinkSettings::FormatLSB;
            ui->fmDeviation->setEnabled(false);
            break;
        case 5:
            m_settings.m_sampleFormat = UDPSinkSettings::FormatUSB;
            ui->fmDeviation->setEnabled(false);
            break;
        case 6:
            m_settings.m_sampleFormat = UDPSinkSettings::FormatLSBMono;
            ui->fmDeviation->setEnabled(false);
            break;
        case 7:
            m_settings.m_sampleFormat = UDPSinkSettings::FormatUSBMono;
            ui->fmDeviation->setEnabled(false);
            break;
        case 8:
            m_settings.m_sampleFormat = UDPSinkSettings::FormatAMMono;
            ui->fmDeviation->setEnabled(false);
            break;
        case 9:
            m_settings.m_sampleFormat = UDPSinkSettings::FormatAMNoDCMono;
            ui->fmDeviation->setEnabled(false);
            break;
        case 10:
            m_settings.m_sampleFormat = UDPSinkSettings::FormatAMBPFMono;
            ui->fmDeviation->setEnabled(false);
            break;
        default:
            m_settings.m_sampleFormat = UDPSinkSettings::FormatIQ16;
            ui->fmDeviation->setEnabled(false);
            break;
    }
}

void UDPSinkGUI::applySettingsImmediate(bool force)
{
	if (m_doApplySettings)
	{
        UDPSink::MsgConfigureUDPSink* message = UDPSink::MsgConfigureUDPSink::create( m_settings, force);
        m_udpSink->getInputMessageQueue()->push(message);
	}
}

void UDPSinkGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
        UDPSink::MsgConfigureUDPSink* message = UDPSink::MsgConfigureUDPSink::create( m_settings, force);
        m_udpSink->getInputMessageQueue()->push(message);

		ui->applyBtn->setEnabled(false);
		ui->applyBtn->setStyleSheet("QPushButton { background:rgb(79,79,79); }");
	}
}

void UDPSinkGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void UDPSinkGUI::on_sampleFormat_currentIndexChanged(int index)
{
	if ((index == 1) || (index == 2)) {
		ui->fmDeviation->setEnabled(true);
	} else {
		ui->fmDeviation->setEnabled(false);
	}

	setSampleFormat(index);

	ui->applyBtn->setEnabled(true);
	ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSinkGUI::on_outputUDPAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->outputUDPAddress->text();
    ui->applyBtn->setEnabled(true);
    ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSinkGUI::on_outputUDPPort_editingFinished()
{
    bool ok;
    quint16 udpPort = ui->outputUDPPort->text().toInt(&ok);

    if((!ok) || (udpPort < 1024)) {
        udpPort = 9998;
    }

    m_settings.m_udpPort = udpPort;
    ui->outputUDPPort->setText(tr("%1").arg(m_settings.m_udpPort));

    ui->applyBtn->setEnabled(true);
    ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSinkGUI::on_inputUDPAudioPort_editingFinished()
{
    bool ok;
    quint16 udpPort = ui->inputUDPAudioPort->text().toInt(&ok);

    if((!ok) || (udpPort < 1024)) {
        udpPort = 9997;
    }

    m_settings.m_audioPort = udpPort;
    ui->inputUDPAudioPort->setText(tr("%1").arg(m_settings.m_audioPort));

    ui->applyBtn->setEnabled(true);
    ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSinkGUI::on_sampleRate_textEdited(const QString& arg1)
{
    (void) arg1;
    bool ok;
    Real outputSampleRate = ui->sampleRate->text().toDouble(&ok);

    if((!ok) || (outputSampleRate < 1000))
    {
        m_settings.m_outputSampleRate = 48000;
        ui->sampleRate->setText(QString("%1").arg(outputSampleRate, 0));
    }
    else
    {
        m_settings.m_outputSampleRate = outputSampleRate;
    }

	ui->applyBtn->setEnabled(true);
	ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSinkGUI::on_rfBandwidth_textEdited(const QString& arg1)
{
    (void) arg1;
    bool ok;
    Real rfBandwidth = ui->rfBandwidth->text().toDouble(&ok);

    if ((!ok) || (rfBandwidth > m_settings.m_outputSampleRate))
    {
        m_settings.m_rfBandwidth = m_settings.m_outputSampleRate;
        ui->rfBandwidth->setText(QString("%1").arg(m_settings.m_rfBandwidth, 0));
    }
    else
    {
        m_settings.m_rfBandwidth = rfBandwidth;
    }

    m_rfBandwidthChanged = true;

	ui->applyBtn->setEnabled(true);
	ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSinkGUI::on_fmDeviation_textEdited(const QString& arg1)
{
    (void) arg1;
    bool ok;
    int fmDeviation = ui->fmDeviation->text().toInt(&ok);

    if ((!ok) || (fmDeviation < 1))
    {
        m_settings.m_fmDeviation = 2500;
        ui->fmDeviation->setText(QString("%1").arg(m_settings.m_fmDeviation));
    }
    else
    {
        m_settings.m_fmDeviation = fmDeviation;
    }

	ui->applyBtn->setEnabled(true);
	ui->applyBtn->setStyleSheet("QPushButton { background-color : green; }");
}

void UDPSinkGUI::on_applyBtn_clicked()
{
    if (m_rfBandwidthChanged)
    {
        m_channelMarker.setBandwidth((int) m_settings.m_rfBandwidth);
        m_rfBandwidthChanged = false;
    }

    ui->glSpectrum->setSampleRate(m_settings.m_outputSampleRate);

	applySettings();
}

void UDPSinkGUI::on_audioActive_toggled(bool active)
{
    m_settings.m_audioActive = active;
	applySettingsImmediate();
}

void UDPSinkGUI::on_audioStereo_toggled(bool stereo)
{
    m_settings.m_audioStereo = stereo;
	applySettingsImmediate();
}

void UDPSinkGUI::on_agc_toggled(bool agc)
{
    m_settings.m_agc = agc;
    applySettingsImmediate();
}

void UDPSinkGUI::on_gain_valueChanged(int value)
{
    m_settings.m_gain = value / 10.0;
	ui->gainText->setText(tr("%1").arg(value/10.0, 0, 'f', 1));
	applySettingsImmediate();
}

void UDPSinkGUI::on_volume_valueChanged(int value)
{
    m_settings.m_volume = value;
	ui->volumeText->setText(QString("%1").arg(value));
	applySettingsImmediate();
}

void UDPSinkGUI::on_squelch_valueChanged(int value)
{
    m_settings.m_squelchdB = value;

    if (value == -100) // means disabled
    {
        ui->squelchText->setText("---");
        m_settings.m_squelchEnabled = false;
    }
    else
    {
        ui->squelchText->setText(tr("%1").arg(value*1.0, 0, 'f', 0));
        m_settings.m_squelchEnabled = true;
    }

    applySettingsImmediate();
}

void UDPSinkGUI::on_squelchGate_valueChanged(int value)
{
    m_settings.m_squelchGate = value;
    ui->squelchGateText->setText(tr("%1").arg(value*10.0, 0, 'f', 0));
    applySettingsImmediate();
}

void UDPSinkGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
	if ((widget == ui->spectrumBox) && (m_udpSink)) {
        m_udpSink->enableSpectrum(rollDown);
	}
}

void UDPSinkGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);

        dialog.move(p);
        dialog.exec();

        m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        applySettingsImmediate();
    }
    else if ((m_contextMenuType == ContextMenuStreamSettings) && (m_deviceUISet->m_deviceMIMOEngine))
    {
        DeviceStreamSelectionDialog dialog(this);
        dialog.setNumberOfStreams(m_udpSink->getNumberOfDeviceStreams());
        dialog.setStreamIndex(m_settings.m_streamIndex);
        dialog.move(p);
        dialog.exec();

        m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
        m_channelMarker.clearStreamIndexes();
        m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
        displayStreamIndex();
        applySettings();
    }

    resetContextMenuType();
}

void UDPSinkGUI::leaveEvent(QEvent*)
{
	m_channelMarker.setHighlighted(false);
}

void UDPSinkGUI::enterEvent(QEvent*)
{
	m_channelMarker.setHighlighted(true);
}


