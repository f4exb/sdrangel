///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include <QDebug>
#include <QMessageBox>
#include <QDateTime>
#include <QString>
#include <QFileDialog>

#include "ui_remotetcpinputgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "util/db.h"
#include "remotetcpinputgui.h"
#include "remotetcpinputtcphandler.h"
#include "maincore.h"

RemoteTCPInputGui::RemoteTCPInputGui(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::RemoteTCPInputGui),
    m_settings(),
    m_sampleSource(nullptr),
    m_sampleRate(0),
    m_centerFrequency(0),
    m_tickCount(0),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_deviceGains(nullptr),
    m_remoteDevice(RemoteTCPProtocol::RTLSDR_R820T),
    m_connectionError(false),
    m_spyServerGainRange("Gain", 0, 41, 1, ""),
    m_spyServerGains({m_spyServerGainRange}, false, false)
{
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);

    ui->setupUi(getContents());
    sizeToContents();
    getContents()->setStyleSheet("#RemoteTCPInputGui { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesource/remotetcpinput/readme.md";

    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(9, 0, 999999999); // frequency dial is in kHz

    ui->devSampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->devSampleRate->setValueRange(8, 0, 99999999);
    ui->rfBW->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->rfBW->setValueRange(5, 0, 99999);    // In kHz
    ui->channelSampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->channelSampleRate->setValueRange(8, 0, 99999999);

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

    displaySettings();

    connect(deviceUISet->m_deviceAPI, &DeviceAPI::stateChanged, this, &RemoteTCPInputGui::updateStatus);
    updateStatus();

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));

    m_sampleSource = (RemoteTCPInput*) m_deviceUISet->m_deviceAPI->getSampleSource();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleSource->setMessageQueueToGUI(&m_inputMessageQueue);

    m_forceSettings = true;
    sendSettings();
    makeUIConnections();
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));
}

RemoteTCPInputGui::~RemoteTCPInputGui()
{
    m_updateTimer.stop();
    delete ui;
}

void RemoteTCPInputGui::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void RemoteTCPInputGui::destroy()
{
    delete this;
}

void RemoteTCPInputGui::resetToDefaults()
{
    qDebug() << "RemoteTCPInputGui::resetToDefaults";
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

QByteArray RemoteTCPInputGui::serialize() const
{
    return m_settings.serialize();
}

bool RemoteTCPInputGui::deserialize(const QByteArray& data)
{
    qDebug("RemoteTCPInputGui::deserialize");

    if (m_settings.deserialize(data))
    {
        displaySettings();
        m_forceSettings = true;
        sendSettings();

        return true;
    }
    else
    {
        return false;
    }
}

bool RemoteTCPInputGui::handleMessage(const Message& message)
{
    if (RemoteTCPInput::MsgConfigureRemoteTCPInput::match(message))
    {
        const RemoteTCPInput::MsgConfigureRemoteTCPInput& cfg = (const RemoteTCPInput::MsgConfigureRemoteTCPInput&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (RemoteTCPInput::MsgStartStop::match(message))
    {
        const RemoteTCPInput::MsgStartStop& notif = (const RemoteTCPInput::MsgStartStop&) message;
        m_connectionError = false;
        blockApplySettings(true);
        ui->startStop->setChecked(notif.getStartStop());
        blockApplySettings(false);
        return true;
    }
    else if (RemoteTCPInput::MsgReportTCPBuffer::match(message))
    {
        const RemoteTCPInput::MsgReportTCPBuffer& report = (const RemoteTCPInput::MsgReportTCPBuffer&) message;
        ui->inGauge->setMaximum((int)report.getInSize());
        ui->inGauge->setValue((int)report.getInBytesAvailable());
        ui->inBufferLenSecsText->setText(QString("%1s").arg(report.getInSeconds(), 0, 'f', 2));
        ui->outGauge->setMaximum((int)report.getOutSize());
        ui->outGauge->setValue((int)report.getOutBytesAvailable());
        ui->outBufferLenSecsText->setText(QString("%1s").arg(report.getOutSeconds(), 0, 'f', 2));
        return true;
    }
    else if (RemoteTCPInputTCPHandler::MsgReportRemoteDevice::match(message))
    {
        const RemoteTCPInputTCPHandler::MsgReportRemoteDevice& report = (const RemoteTCPInputTCPHandler::MsgReportRemoteDevice&) message;
        QHash<RemoteTCPProtocol::Device, QString> devices = {
             {RemoteTCPProtocol::RTLSDR_E4000, "RTLSDR E4000"},
             {RemoteTCPProtocol::RTLSDR_FC0012, "RTLSDR FC0012"},
             {RemoteTCPProtocol::RTLSDR_FC0013, "RTLSDR FC0013"},
             {RemoteTCPProtocol::RTLSDR_FC2580, "RTLSDR FC2580"},
             {RemoteTCPProtocol::RTLSDR_R820T, "RTLSDR R820T"},
             {RemoteTCPProtocol::RTLSDR_R828D, "RTLSDR R828D"},
             {RemoteTCPProtocol::AIRSPY, "Airspy"},
             {RemoteTCPProtocol::AIRSPY_HF, "AirspyHF"},
             {RemoteTCPProtocol::AUDIO_INPUT, "AudioInput"},
             {RemoteTCPProtocol::BLADE_RF1, "BladeRF1"},
             {RemoteTCPProtocol::BLADE_RF2, "BladeRF2"},
             {RemoteTCPProtocol::FCD_PRO, "FCDPro"},
             {RemoteTCPProtocol::FCD_PRO_PLUS, "FCDProPlus"},
             {RemoteTCPProtocol::FILE_INPUT, "FileInput"},
             {RemoteTCPProtocol::HACK_RF, "HackRF"},
             {RemoteTCPProtocol::KIWI_SDR, "KiwiSDR"},
             {RemoteTCPProtocol::LIME_SDR, "LimeSDR"},
             {RemoteTCPProtocol::LOCAL_INPUT, "LocalInput"},
             {RemoteTCPProtocol::PERSEUS, "Perseus"},
             {RemoteTCPProtocol::PLUTO_SDR, "PlutoSDR"},
             {RemoteTCPProtocol::REMOTE_INPUT, "RemoteInput"},
             {RemoteTCPProtocol::REMOTE_TCP_INPUT, "RemoteTCPInput"},
             {RemoteTCPProtocol::SDRPLAY_1, "SDRplay1"},
             {RemoteTCPProtocol::SDRPLAY_V3_RSP1, "SDRplayV3 RSP1"},
             {RemoteTCPProtocol::SDRPLAY_V3_RSP1A, "SDRplayV3 RSP1A"},
             {RemoteTCPProtocol::SDRPLAY_V3_RSP1B, "SDRplayV3 RSP1B"},
             {RemoteTCPProtocol::SDRPLAY_V3_RSP2, "SDRplayV3 RSP2"},
             {RemoteTCPProtocol::SDRPLAY_V3_RSPDUO, "SDRplayV3 RSPduo"},
             {RemoteTCPProtocol::SDRPLAY_V3_RSPDX, "SDRplayV3 RSPdx"},
             {RemoteTCPProtocol::SIGMF_FILE_INPUT, "SigMFFileInput"},
             {RemoteTCPProtocol::SOAPY_SDR, "SoapySDR"},
             {RemoteTCPProtocol::TEST_SOURCE, "TestSource"},
             {RemoteTCPProtocol::USRP, "USRP"},
             {RemoteTCPProtocol::XTRX, "XTRX"},
        };
        QString device = "Unknown";
        m_remoteDevice = report.getDevice();
        if (devices.contains(m_remoteDevice)) {
            device = devices.value(m_remoteDevice);
        }
        ui->device->setText(QString("Device: %1").arg(device));
        ui->detectedProtocol->setText(QString("Protocol: %1").arg(report.getProtocol()));

        // Update GUI so we only show widgets available for the protocol in use
        m_sdra = report.getProtocol() == "SDRA";
        m_spyServer = report.getProtocol() == "Spy Server";
        m_remoteControl = report.getRemoteControl();
        m_iqOnly = report.getIQOnly();

        if (m_spyServer) {
            m_spyServerGains.m_gains[0].m_max = report.getMaxGain();
        }
        if ((m_sdra || m_spyServer) && (ui->sampleBits->count() < 4))
        {
            ui->sampleBits->addItem("16");
            ui->sampleBits->addItem("24");
            ui->sampleBits->addItem("32");
        }
        else if (!(m_sdra || m_spyServer) && (ui->sampleBits->count() != 1))
        {
            while (ui->sampleBits->count() > 1) {
                ui->sampleBits->removeItem(ui->sampleBits->count() - 1);
            }
        }
        if ((m_sdra || m_spyServer) && (ui->decim->count() != 7))
        {
            ui->decim->addItem("2");
            ui->decim->addItem("4");
            ui->decim->addItem("8");
            ui->decim->addItem("16");
            ui->decim->addItem("32");
            ui->decim->addItem("64");
        }
        else if (!(m_sdra || m_spyServer) && (ui->decim->count() != 1))
        {
            while (ui->decim->count() > 1) {
                ui->decim->removeItem(ui->decim->count() - 1);
            }
        }
        if (!m_sdra)
        {
            ui->deltaFrequency->setValue(0);
            ui->channelGain->setValue(0);
            ui->decimation->setChecked(true);
        }
        if (m_sdra) {
            ui->centerFrequency->setValueRange(9, 0, 999999999); // Should add transverter control to protocol in the future
        } else {
            ui->centerFrequency->setValueRange(7, 0, 9999999);
        }

        // Set sample rate range
        if (m_sampleRateRanges.contains(m_remoteDevice))
        {
            const SampleRateRange *range = m_sampleRateRanges.value(m_remoteDevice);
            ui->devSampleRate->setValueRange(8, range->m_min, range->m_max);
        }
        else if (m_sampleRateLists.contains(m_remoteDevice))
        {
            const QList<int> *list = m_sampleRateLists.value(m_remoteDevice);
            // FIXME: Should probably use a combobox for devices that have a list of sample rates
            ui->devSampleRate->setValueRange(8, list->front(), list->back());
        }
        else
        {
            ui->devSampleRate->setValueRange(8, 0, 99999999);
        }
        displayEnabled();
        displayGains();
        return true;
    }
    else if (RemoteTCPInputTCPHandler::MsgReportConnection::match(message))
    {
        const RemoteTCPInputTCPHandler::MsgReportConnection& report = (const RemoteTCPInputTCPHandler::MsgReportConnection&) message;
        qDebug() << "RemoteTCPInputGui::handleMessage: MsgReportConnection connected: " << report.getConnected();
        m_connectionError = !report.getConnected();
        updateStatus();
        return true;
    }
    else if (RemoteTCPInput::MsgSendMessage::match(message))
    {
        const RemoteTCPInput::MsgSendMessage& msg = (const RemoteTCPInput::MsgSendMessage&) message;

        ui->messages->addItem(QString("%1> %2").arg(msg.getCallsign()).arg(msg.getText()));
        ui->messages->scrollToBottom();

        return true;
    }
    else if (RemoteTCPInput::MsgReportPosition::match(message))
    {
        // Could display in future
        return true;
    }
    else if (RemoteTCPInput::MsgReportDirection::match(message))
    {
        // Could display in future
        return true;
    }
    else
    {
        return false;
    }
}

void RemoteTCPInputGui::displayEnabled()
{
    int state = m_deviceUISet->m_deviceAPI->state();
    bool remoteControl;
    bool enableMessages;
    bool enableSquelchEnable;
    bool enableSquelch;
    bool sdra;

    if (state == DeviceAPI::StRunning)
    {
        sdra = m_sdra;
        remoteControl = m_remoteControl;
        enableMessages = !m_iqOnly;
        enableSquelchEnable = !m_iqOnly;
        enableSquelch = !m_iqOnly && m_settings.m_squelchEnabled;
    }
    else
    {
        sdra = m_settings.m_protocol == "SDRangel";
        remoteControl = m_settings.m_overrideRemoteSettings;
        enableMessages = false;
        enableSquelchEnable = m_settings.m_overrideRemoteSettings;
        enableSquelch = m_settings.m_overrideRemoteSettings && m_settings.m_squelchEnabled;
    }

    ui->deltaFrequencyLabel->setEnabled(sdra && remoteControl);
    ui->deltaFrequency->setEnabled(sdra && remoteControl);
    ui->deltaUnits->setEnabled(sdra && remoteControl);
    ui->channelGainLabel->setEnabled(sdra && remoteControl);
    ui->channelGain->setEnabled(sdra && remoteControl);
    ui->channelGainText->setEnabled(sdra && remoteControl);
    ui->decimation->setEnabled(sdra && remoteControl);

    ui->channelSampleRate->setEnabled(m_settings.m_channelDecimation && sdra && remoteControl);
    ui->channelSampleRateLabel->setEnabled(m_settings.m_channelDecimation && sdra && remoteControl);
    ui->channelSampleRateUnit->setEnabled(m_settings.m_channelDecimation && sdra && remoteControl);

    ui->devSampleRateLabel->setEnabled(!m_spyServer && remoteControl);
    ui->devSampleRate->setEnabled(!m_spyServer && remoteControl);
    ui->devSampleRateUnits->setEnabled(!m_spyServer && remoteControl);
    ui->agc->setEnabled(!m_spyServer && remoteControl);
    ui->rfBWLabel->setEnabled(!m_spyServer && remoteControl);
    ui->rfBW->setEnabled(!m_spyServer && remoteControl);
    ui->rfBWUnits->setEnabled(!m_spyServer && remoteControl);
    ui->dcOffset->setEnabled(!m_spyServer && remoteControl);
    ui->iqImbalance->setEnabled(!m_spyServer && remoteControl);
    ui->ppm->setEnabled(!m_spyServer && remoteControl);
    ui->ppmLabel->setEnabled(!m_spyServer && remoteControl);
    ui->ppmText->setEnabled(!m_spyServer && remoteControl);

    ui->centerFrequency->setEnabled(remoteControl);
    ui->biasTee->setEnabled(remoteControl);
    ui->directSampling->setEnabled(remoteControl);
    ui->decimLabel->setEnabled(remoteControl);
    ui->decim->setEnabled(remoteControl);
    ui->gain1Label->setEnabled(remoteControl);
    ui->gain1->setEnabled(remoteControl);
    ui->gain1Text->setEnabled(remoteControl);
    ui->gain2Label->setEnabled(remoteControl);
    ui->gain2->setEnabled(remoteControl);
    ui->gain2Text->setEnabled(remoteControl);
    ui->gain3Label->setEnabled(remoteControl);
    ui->gain3->setEnabled(remoteControl);
    ui->gain3Text->setEnabled(remoteControl);
    ui->sampleBitsLabel->setEnabled(remoteControl);
    ui->sampleBits->setEnabled(remoteControl);
    ui->sampleBitsUnits->setEnabled(remoteControl);

    ui->squelchEnabled->setEnabled(enableSquelchEnable);
    ui->squelch->setEnabled(enableSquelch);
    ui->squelchText->setEnabled(enableSquelch);
    ui->squelchUnits->setEnabled(enableSquelch);
    ui->squelchGate->setEnabled(enableSquelch);

    ui->sendMessage->setEnabled(enableMessages);
    ui->txAddress->setEnabled(enableMessages);
    ui->txMessage->setEnabled(enableMessages);
    ui->messages->setEnabled(enableMessages);
}

void RemoteTCPInputGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_centerFrequency = notif->getCenterFrequency();
            qDebug("RemoteTCPInputGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();
            delete message;
        }
        else
        {
            if (handleMessage(*message))
            {
                delete message;
            }
        }
    }
}

void RemoteTCPInputGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_centerFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_sampleRate / 1000));
}

void RemoteTCPInputGui::displaySettings()
{
    blockApplySettings(true);

    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    ui->ppm->setValue(m_settings.m_loPpmCorrection);

    ui->dcOffset->setChecked(m_settings.m_dcBlock);
    ui->iqImbalance->setChecked(m_settings.m_iqCorrection);
    ui->biasTee->setChecked(m_settings.m_biasTee);
    ui->directSampling->setChecked(m_settings.m_directSampling);

    ui->devSampleRate->setValue(m_settings.m_devSampleRate);
    ui->decim->setCurrentIndex(m_settings.m_log2Decim);

    ui->agc->setChecked(m_settings.m_agc);

    ui->rfBW->setValue(m_settings.m_rfBW / 1000);

    ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);
    ui->channelGain->setValue(m_settings.m_channelGain);
    ui->channelGainText->setText(tr("%1dB").arg(m_settings.m_channelGain));
    ui->channelSampleRate->setValue(m_settings.m_channelSampleRate);
    ui->deviceRateText->setText(tr("%1k").arg(m_settings.m_channelSampleRate / 1000.0));
    ui->decimation->setChecked(!m_settings.m_channelDecimation);
    ui->sampleBits->setCurrentText(QString::number(m_settings.m_sampleBits));

    ui->squelchEnabled->setChecked(m_settings.m_squelchEnabled);
    ui->squelch->setValue(m_settings.m_squelch);
    ui->squelchText->setText(QString::number(m_settings.m_squelch));
    ui->squelchGate->setValue(m_settings.m_squelchGate);

    ui->dataPort->setValue(m_settings.m_dataPort);
    ui->dataAddress->blockSignals(true);
    ui->dataAddress->clear();
    for (const auto& address : m_settings.m_addressList) {
        ui->dataAddress->addItem(address);
    }
    if (ui->dataAddress->findText(m_settings.m_dataAddress) == -1) {
        ui->dataAddress->addItem(m_settings.m_dataAddress);
    }
    ui->dataAddress->setCurrentText(m_settings.m_dataAddress);
    ui->dataAddress->blockSignals(false);
    ui->overrideRemoteSettings->setChecked(m_settings.m_overrideRemoteSettings);

    ui->preFill->setValue((int)(m_settings.m_preFill * 10.0));
    ui->preFillText->setText(QString("%1s").arg(m_settings.m_preFill, 0, 'f', 2));
    int idx = ui->protocol->findText(m_settings.m_protocol);
    if (idx > 0) {
        ui->protocol->setCurrentIndex(idx);
    }

    displayGains();
    displayReplayLength();
    displayReplayOffset();
    displayReplayStep();
    ui->replayLoop->setChecked(m_settings.m_replayLoop);
    displayEnabled();
    blockApplySettings(false);
}

// Device sample rates

const RemoteTCPInputGui::SampleRateRange RemoteTCPInputGui::m_rtlSDRSampleRateRange{900001, 3200000};
const RemoteTCPInputGui::SampleRateRange RemoteTCPInputGui::m_sdrPlaySampleRateRange{2000000, 10660000};
const RemoteTCPInputGui::SampleRateRange RemoteTCPInputGui::m_bladeRF1SampleRateRange{330000, 40000000};
const RemoteTCPInputGui::SampleRateRange RemoteTCPInputGui::m_hackRFSampleRateRange{1000000, 20000000};
const RemoteTCPInputGui::SampleRateRange RemoteTCPInputGui::m_limeSampleRateRange{100000, 614400000}; // Mini is lower than this
const RemoteTCPInputGui::SampleRateRange RemoteTCPInputGui::m_plutoSampleRateRange{2000000, 20000000};
const RemoteTCPInputGui::SampleRateRange RemoteTCPInputGui::m_usrpSampleRateRange{100000, 614400000}; // For B210

const QHash<RemoteTCPProtocol::Device, const RemoteTCPInputGui::SampleRateRange *> RemoteTCPInputGui::m_sampleRateRanges =
{
    {RemoteTCPProtocol::RTLSDR_E4000, &m_rtlSDRSampleRateRange},
    {RemoteTCPProtocol::RTLSDR_R820T, &m_rtlSDRSampleRateRange},
    {RemoteTCPProtocol::BLADE_RF1, &m_bladeRF1SampleRateRange},
    {RemoteTCPProtocol::HACK_RF, &m_hackRFSampleRateRange},
    {RemoteTCPProtocol::LIME_SDR, &m_limeSampleRateRange},
    {RemoteTCPProtocol::SDRPLAY_V3_RSP1, &m_sdrPlaySampleRateRange},
    {RemoteTCPProtocol::SDRPLAY_V3_RSP1A, &m_sdrPlaySampleRateRange},
    {RemoteTCPProtocol::SDRPLAY_V3_RSP1B, &m_sdrPlaySampleRateRange},
    {RemoteTCPProtocol::SDRPLAY_V3_RSP2, &m_sdrPlaySampleRateRange},
    {RemoteTCPProtocol::SDRPLAY_V3_RSPDUO, &m_sdrPlaySampleRateRange},
    {RemoteTCPProtocol::SDRPLAY_V3_RSPDX, &m_sdrPlaySampleRateRange},
    {RemoteTCPProtocol::PLUTO_SDR, &m_plutoSampleRateRange},
    {RemoteTCPProtocol::USRP, &m_usrpSampleRateRange},
};

const QList<int> RemoteTCPInputGui::m_airspySampleRateList {2500000, 10000000};
const QList<int> RemoteTCPInputGui::m_airspyHFSampleRateList {192000, 256000, 384000, 456000, 768000, 912000};

const QHash<RemoteTCPProtocol::Device, const QList<int> *> RemoteTCPInputGui::m_sampleRateLists =
{
    {RemoteTCPProtocol::AIRSPY, &m_airspySampleRateList},
    {RemoteTCPProtocol::AIRSPY_HF, &m_airspyHFSampleRateList}
};

// Device gains

const RemoteTCPInputGui::DeviceGains::GainRange RemoteTCPInputGui::m_rtlSDR34kGainRange(
    "Gain",
    {
         -10, 15, 40, 65, 90, 115, 140, 165, 190, 215,
        240, 290, 340, 420
    }
);
const RemoteTCPInputGui::DeviceGains RemoteTCPInputGui::m_rtlSDRe4kGains({RemoteTCPInputGui::m_rtlSDR34kGainRange}, true, false);

const RemoteTCPInputGui::DeviceGains::GainRange RemoteTCPInputGui::m_rtlSDRR820GainRange(
    "Gain",
    {
        0, 9, 14, 27, 37, 77, 87, 125, 144, 157,
        166, 197, 207, 229, 254, 280, 297, 328,
        338, 364, 372, 386, 402, 421, 434, 439,
        445, 480, 496
    }
);
const RemoteTCPInputGui::DeviceGains RemoteTCPInputGui::m_rtlSDRR820Gains({RemoteTCPInputGui::m_rtlSDRR820GainRange}, true, true);

const RemoteTCPInputGui::DeviceGains::GainRange RemoteTCPInputGui::m_airspyLNAGainRange("LNA", 0, 14, 1, "");   // Not sure what the units are for these
const RemoteTCPInputGui::DeviceGains::GainRange RemoteTCPInputGui::m_airspyMixerGainRange("Mixer", 0, 15, 1, "");
const RemoteTCPInputGui::DeviceGains::GainRange RemoteTCPInputGui::m_airspyVGAGainRange("VGA", 0, 15, 1, "");
const RemoteTCPInputGui::DeviceGains RemoteTCPInputGui::m_airspyGains({m_airspyLNAGainRange, m_airspyMixerGainRange, m_airspyVGAGainRange}, true, true);

const RemoteTCPInputGui::DeviceGains::GainRange RemoteTCPInputGui::m_airspyHFAttRange("Att", 0, 48, 6);
const RemoteTCPInputGui::DeviceGains RemoteTCPInputGui::m_airspyHFGains({m_airspyHFAttRange}, true, false);

const RemoteTCPInputGui::DeviceGains::GainRange RemoteTCPInputGui::m_bladeRF1LNARange("LNA", 0, 6, 3);
const RemoteTCPInputGui::DeviceGains::GainRange RemoteTCPInputGui::m_bladeRF1VGA1Range("VGA1", 5, 30, 1, "");
const RemoteTCPInputGui::DeviceGains::GainRange RemoteTCPInputGui::m_bladeRF1VGA2Range("VGA2", 0, 30, 3, "");
const RemoteTCPInputGui::DeviceGains RemoteTCPInputGui::m_baldeRF1Gains({m_bladeRF1LNARange, m_bladeRF1VGA1Range, m_bladeRF1VGA2Range}, false, true);

const RemoteTCPInputGui::DeviceGains::GainRange RemoteTCPInputGui::m_funCubeProPlusRange("Gain", 0, 59, 1);
const RemoteTCPInputGui::DeviceGains RemoteTCPInputGui::m_funCubeProPlusGains({m_funCubeProPlusRange}, false, true);

const RemoteTCPInputGui::DeviceGains::GainRange RemoteTCPInputGui::m_hackRFLNAGainRange("LNA", 0, 40, 8);
const RemoteTCPInputGui::DeviceGains::GainRange RemoteTCPInputGui::m_hackRFVGAGainRange("VGA", 0, 62, 2);
const RemoteTCPInputGui::DeviceGains RemoteTCPInputGui::m_hackRFGains({m_hackRFLNAGainRange, m_hackRFVGAGainRange}, false, true);

const RemoteTCPInputGui::DeviceGains::GainRange RemoteTCPInputGui::m_kiwiGainRange("Gain", 0, 120, 1);
const RemoteTCPInputGui::DeviceGains RemoteTCPInputGui::m_kiwiGains({m_kiwiGainRange}, true, false);

const RemoteTCPInputGui::DeviceGains::GainRange RemoteTCPInputGui::m_limeRange("Gain", 0, 70, 1);   // Assuming auto setting
const RemoteTCPInputGui::DeviceGains RemoteTCPInputGui::m_limeGains({m_limeRange}, true, false);

// SDRplay LNA gain is device & frequency dependent (See sdrplayv3input.h SDRPlayV3LNA), server should choose closest value
const RemoteTCPInputGui::DeviceGains::GainRange RemoteTCPInputGui::m_sdrplayV3LNAGainRange("LNA", -80, 0, 1);
const RemoteTCPInputGui::DeviceGains::GainRange RemoteTCPInputGui::m_sdrplayV3IFGainRange("IF", -59, 0, 1);
const RemoteTCPInputGui::DeviceGains RemoteTCPInputGui::m_sdrplayV3Gains({m_sdrplayV3LNAGainRange, m_sdrplayV3IFGainRange}, true, true);

const RemoteTCPInputGui::DeviceGains::GainRange RemoteTCPInputGui::m_plutoGainRange("Gain", 1, 77, 1);
const RemoteTCPInputGui::DeviceGains RemoteTCPInputGui::m_plutoGains({m_plutoGainRange}, true, false);

const RemoteTCPInputGui::DeviceGains::GainRange RemoteTCPInputGui::m_usrpGainRange("Gain", 0, 70, 1);
const RemoteTCPInputGui::DeviceGains RemoteTCPInputGui::m_usrpGains({m_usrpGainRange}, true, false);

const RemoteTCPInputGui::DeviceGains::GainRange RemoteTCPInputGui::m_xtrxGainRange("Gain", 0, 77, 1);
const RemoteTCPInputGui::DeviceGains RemoteTCPInputGui::m_xtrxGains({m_xtrxGainRange}, true, false);

const QHash<RemoteTCPProtocol::Device, const RemoteTCPInputGui::DeviceGains *> RemoteTCPInputGui::m_gains =
{
    {RemoteTCPProtocol::RTLSDR_E4000, &m_rtlSDRe4kGains},
    {RemoteTCPProtocol::RTLSDR_R820T, &m_rtlSDRR820Gains},
    {RemoteTCPProtocol::RTLSDR_R828D, &m_rtlSDRR820Gains},
    {RemoteTCPProtocol::AIRSPY, &m_airspyGains},
    {RemoteTCPProtocol::AIRSPY_HF, &m_airspyHFGains},
    {RemoteTCPProtocol::BLADE_RF1, &m_baldeRF1Gains},
    {RemoteTCPProtocol::FCD_PRO_PLUS, &m_funCubeProPlusGains},
    {RemoteTCPProtocol::HACK_RF, &m_hackRFGains},
    {RemoteTCPProtocol::KIWI_SDR, &m_kiwiGains},
    {RemoteTCPProtocol::LIME_SDR, &m_limeGains},
    {RemoteTCPProtocol::SDRPLAY_V3_RSP1, &m_sdrplayV3Gains},
    {RemoteTCPProtocol::SDRPLAY_V3_RSP1A, &m_sdrplayV3Gains},
    {RemoteTCPProtocol::SDRPLAY_V3_RSP1B, &m_sdrplayV3Gains},
    {RemoteTCPProtocol::SDRPLAY_V3_RSP2, &m_sdrplayV3Gains},
    {RemoteTCPProtocol::SDRPLAY_V3_RSPDUO, &m_sdrplayV3Gains},
    {RemoteTCPProtocol::SDRPLAY_V3_RSPDX, &m_sdrplayV3Gains},
    {RemoteTCPProtocol::PLUTO_SDR, &m_plutoGains},
    {RemoteTCPProtocol::USRP, &m_usrpGains},
    {RemoteTCPProtocol::XTRX, &m_xtrxGains}
};

QString RemoteTCPInputGui::gainText(int stage)
{
    if (m_deviceGains) {
        return QString("%1.%2%3").arg(m_settings.m_gain[stage] / 10).arg(abs(m_settings.m_gain[stage] % 10)).arg(m_deviceGains->m_gains[stage].m_units);
    } else {
        return "";
    }
}

void RemoteTCPInputGui::displayGains()
{
    QLabel *gainLabels[3] = {ui->gain1Label, ui->gain2Label, ui->gain3Label};
    QDial *gain[3] = {ui->gain1, ui->gain2, ui->gain3};
    QLabel *gainTexts[3] = {ui->gain1Text, ui->gain2Text, ui->gain3Text};
    QWidget *gainLine[2] = {ui->gainLine1, ui->gainLine2};

    if (m_settings.m_protocol == "Spy Server") {
        m_deviceGains = &m_spyServerGains;
    } else {
        m_deviceGains = m_gains.value(m_remoteDevice);
    }
    if (m_deviceGains)
    {
        ui->agc->setVisible(m_deviceGains->m_agc);
        ui->biasTee->setVisible(m_deviceGains->m_biasTee);
        ui->directSampling->setVisible(m_remoteDevice <= RemoteTCPProtocol::RTLSDR_R828D);
        for (int i = 0; i < 3; i++)
        {
            bool visible = i < m_deviceGains->m_gains.size();
            gainLabels[i]->setVisible(visible);
            gain[i]->setVisible(visible);
            gainTexts[i]->setVisible(visible);
            if (i > 0) {
                gainLine[i-1]->setVisible(visible);
            }
            if (visible)
            {
                gainLabels[i]->setText(m_deviceGains->m_gains[i].m_name);
                if (m_deviceGains->m_gains[i].m_gains.size() > 0)
                {
                    gain[i]->setMinimum(0);
                    gain[i]->setMaximum(m_deviceGains->m_gains[i].m_gains.size() - 1);
                    gain[i]->setSingleStep(1);
                    gain[i]->setPageStep(1);
                }
                else
                {
                    gain[i]->setMinimum(m_deviceGains->m_gains[i].m_min);
                    gain[i]->setMaximum(m_deviceGains->m_gains[i].m_max);
                    gain[i]->setSingleStep(m_deviceGains->m_gains[i].m_step);
                    gain[i]->setPageStep(m_deviceGains->m_gains[i].m_step);
                }
                if (m_deviceGains->m_gains[i].m_gains.size() > 0) {
                    gain[i]->setValue(m_deviceGains->m_gains[i].m_gains.indexOf(m_settings.m_gain[i]));
                } else {
                    gain[i]->setValue(m_settings.m_gain[i] / 10);
                }
                gainTexts[i]->setText(gainText(i));
            }
        }
    }
}

void RemoteTCPInputGui::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void RemoteTCPInputGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        if (m_connectionError)
        {
            // Clear previous error
            m_connectionError = false;
            updateStatus();
        }
        RemoteTCPInput::MsgStartStop *message = RemoteTCPInput::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void RemoteTCPInputGui::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void RemoteTCPInputGui::on_devSampleRate_changed(quint64 value)
{
    m_settings.m_devSampleRate = value;
    m_settingsKeys.append("devSampleRate");

    if (!m_settings.m_channelDecimation)
    {
        m_settings.m_channelSampleRate = m_settings.m_devSampleRate >> m_settings.m_log2Decim;
        m_settingsKeys.append("channelSampleRate");
        ui->channelSampleRate->setValue(m_settings.m_channelSampleRate);
    }

    sendSettings();
}

void RemoteTCPInputGui::on_ppm_valueChanged(int value)
{
    m_settings.m_loPpmCorrection = value;
    ui->ppmText->setText(tr("%1").arg(value));
    m_settingsKeys.append("loPpmCorrection");
    sendSettings();
}

void RemoteTCPInputGui::on_dcOffset_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    m_settingsKeys.append("dcBlock");
    sendSettings();
}

void RemoteTCPInputGui::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    m_settingsKeys.append("iqCorrection");
    sendSettings();
}

void RemoteTCPInputGui::on_biasTee_toggled(bool checked)
{
    m_settings.m_biasTee = checked;
    m_settingsKeys.append("biasTee");
    sendSettings();
}

void RemoteTCPInputGui::on_directSampling_toggled(bool checked)
{
    m_settings.m_directSampling = checked;
    m_settingsKeys.append("directSampling");
    sendSettings();
}

void RemoteTCPInputGui::on_agc_toggled(bool checked)
{
    m_settings.m_agc = checked;
    m_settingsKeys.append("agc");
    sendSettings();
}

void RemoteTCPInputGui::on_decim_currentIndexChanged(int index)
{
    m_settings.m_log2Decim = index;
    m_settingsKeys.append("log2Decim");

    if (!m_settings.m_channelDecimation)
    {
        m_settings.m_channelSampleRate = m_settings.m_devSampleRate >> m_settings.m_log2Decim;
        m_settingsKeys.append("channelSampleRate");
        ui->channelSampleRate->setValue(m_settings.m_channelSampleRate);
    }

    sendSettings();
}

void RemoteTCPInputGui::on_gain1_valueChanged(int value)
{
    if (m_deviceGains && (m_deviceGains->m_gains.size() >= 1) && (m_deviceGains->m_gains[0].m_gains.size() > 0)) {
        m_settings.m_gain[0] = m_deviceGains->m_gains[0].m_gains[value];
    } else {
        m_settings.m_gain[0] = value * 10;
    }

    ui->gain1Text->setText(gainText(0));
    m_settingsKeys.append("gain[0]");
    sendSettings();
}

void RemoteTCPInputGui::on_gain2_valueChanged(int value)
{
    if (m_deviceGains && (m_deviceGains->m_gains.size() >= 2) && (m_deviceGains->m_gains[1].m_gains.size() > 0)) {
        m_settings.m_gain[1] = m_deviceGains->m_gains[1].m_gains[value];
    } else {
        m_settings.m_gain[1] = value * 10;
    }

    ui->gain2Text->setText(gainText(1));
    m_settingsKeys.append("gain[1]");
    sendSettings();
}

void RemoteTCPInputGui::on_gain3_valueChanged(int value)
{
    if (m_deviceGains && (m_deviceGains->m_gains.size() >= 3) && (m_deviceGains->m_gains[2].m_gains.size() > 0)) {
        m_settings.m_gain[2] = m_deviceGains->m_gains[2].m_gains[value];
    } else {
        m_settings.m_gain[2] = value * 10;
    }

    ui->gain3Text->setText(gainText(2));
    m_settingsKeys.append("gain[2]");
    sendSettings();
}

void RemoteTCPInputGui::on_rfBW_changed(int value)
{
    m_settings.m_rfBW = value * 1000;
    m_settingsKeys.append("rfBW");
    sendSettings();
}

void RemoteTCPInputGui::on_deltaFrequency_changed(int value)
{
    m_settings.m_inputFrequencyOffset = value;
    m_settingsKeys.append("inputFrequencyOffset");
    sendSettings();
}

void RemoteTCPInputGui::on_channelGain_valueChanged(int value)
{
    m_settings.m_channelGain = value;
    ui->channelGainText->setText(tr("%1dB").arg(m_settings.m_channelGain));
    m_settingsKeys.append("channelGain");
    sendSettings();
}

void RemoteTCPInputGui::on_channelSampleRate_changed(quint64 value)
{
    m_settings.m_channelSampleRate = value;
    m_settingsKeys.append("channelSampleRate");
    sendSettings();
}

void RemoteTCPInputGui::on_decimation_toggled(bool checked)
{
    m_settings.m_channelDecimation = !checked;
    m_settingsKeys.append("channelDecimation");

    if (!m_settings.m_channelDecimation)
    {
        m_settings.m_channelSampleRate = m_settings.m_devSampleRate >> m_settings.m_log2Decim;
        m_settingsKeys.append("channelSampleRate");
        ui->channelSampleRate->setValue(m_settings.m_channelSampleRate);
    }
    ui->channelSampleRate->setEnabled(!checked);
    ui->channelSampleRateLabel->setEnabled(!checked);
    ui->channelSampleRateUnit->setEnabled(!checked);
    sendSettings();
}

void RemoteTCPInputGui::on_sampleBits_currentIndexChanged(int index)
{
    (void) index;

    m_settings.m_sampleBits = ui->sampleBits->currentText().toInt();
    m_settingsKeys.append("sampleBits");
    sendSettings();
}

void RemoteTCPInputGui::on_squelchEnabled_toggled(bool checked)
{
    m_settings.m_squelchEnabled = checked;
    m_settingsKeys.append("squelchEnabled");
    displayEnabled();
    sendSettings();
}

void RemoteTCPInputGui::on_squelch_valueChanged(int value)
{
    m_settings.m_squelch = value;
    ui->squelchText->setText(QString::number(m_settings.m_squelch));
    m_settingsKeys.append("squelch");
    sendSettings();
}

void RemoteTCPInputGui::on_squelchGate_valueChanged(double value)
{
    m_settings.m_squelchGate = value;
    m_settingsKeys.append("squelchGate");
    sendSettings();
}

void RemoteTCPInputGui::on_dataAddress_editingFinished()
{
    QString text = ui->dataAddress->currentText();
    if (text != m_settings.m_dataAddress)
    {
        m_settings.m_dataAddress = text;
        m_settingsKeys.append("dataAddress");
        m_settings.m_addressList.clear();
        for (int i = 0; i < ui->dataAddress->count(); i++) {
            m_settings.m_addressList.append(ui->dataAddress->itemText(i));
        }
        m_settingsKeys.append("addressList");
        sendSettings();
    }
}

void RemoteTCPInputGui::on_dataAddress_currentIndexChanged(int index)
{
    (void) index;

    m_settings.m_dataAddress = ui->dataAddress->currentText();
    m_settingsKeys.append("dataAddress");
    sendSettings();
}

void RemoteTCPInputGui::on_dataPort_valueChanged(int value)
{
    m_settings.m_dataPort = value;
    m_settingsKeys.append("dataPort");

    sendSettings();
}

void RemoteTCPInputGui::on_overrideRemoteSettings_toggled(bool checked)
{
    m_settings.m_overrideRemoteSettings = checked;
    m_settingsKeys.append("overrideRemoteSettings");
    sendSettings();
    displayEnabled();
}

void RemoteTCPInputGui::on_preFill_valueChanged(int value)
{
    m_settings.m_preFill = value/10.0f;
    ui->preFillText->setText(QString("%1s").arg(m_settings.m_preFill, 0, 'f', 2));
    m_settingsKeys.append("preFill");
    sendSettings();
}

void RemoteTCPInputGui::on_protocol_currentIndexChanged(int index)
{
    (void) index;

    m_settings.m_protocol = ui->protocol->currentText();
    m_settingsKeys.append("protocol");
    sendSettings();
    displayGains();
}

void RemoteTCPInputGui::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "RemoteTCPInputGui::updateHardware";
        RemoteTCPInput::MsgConfigureRemoteTCPInput* message =
                RemoteTCPInput::MsgConfigureRemoteTCPInput::create(m_settings, m_settingsKeys, m_forceSettings);
        m_sampleSource->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void RemoteTCPInputGui::updateStatus()
{
    if (m_connectionError)
    {
        ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
    }
    else
    {
        int state = m_deviceUISet->m_deviceAPI->state();

        switch(state)
        {
            case DeviceAPI::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DeviceAPI::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DeviceAPI::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DeviceAPI::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceAPI->errorMessage());
                break;
            default:
                break;
        }
    }
    displayEnabled();
}

void RemoteTCPInputGui::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_sampleSource->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(tr("%1").arg(powDbAvg, 0, 'f', 1));
    }

	m_tickCount++;
}

void RemoteTCPInputGui::openDeviceSettingsDialog(const QPoint& p)
{
    if (m_contextMenuType == ContextMenuDeviceSettings)
    {
        BasicDeviceSettingsDialog dialog(this);
        dialog.setReplayBytesPerSecond(m_settings.m_devSampleRate * 2 * sizeof(FixReal));
        dialog.setReplayLength(m_settings.m_replayLength);
        dialog.setReplayStep(m_settings.m_replayStep);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_replayLength = dialog.getReplayLength();
        m_settings.m_replayStep = dialog.getReplayStep();
        displayReplayLength();
        displayReplayOffset();
        displayReplayStep();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();

        sendSettings();
    }

    resetContextMenuType();
}

void RemoteTCPInputGui::displayReplayLength()
{
    bool replayEnabled = m_settings.m_replayLength > 0.0f;
    if (!replayEnabled) {
        ui->replayOffset->setMaximum(0);
    } else {
        ui->replayOffset->setMaximum(m_settings.m_replayLength * 10 - 1);
    }
    ui->replayLabel->setEnabled(replayEnabled);
    ui->replayOffset->setEnabled(replayEnabled);
    ui->replayOffsetText->setEnabled(replayEnabled);
    ui->replaySave->setEnabled(replayEnabled);
}

void RemoteTCPInputGui::displayReplayOffset()
{
    bool replayEnabled = m_settings.m_replayLength > 0.0f;
    ui->replayOffset->setValue(m_settings.m_replayOffset * 10);
    ui->replayOffsetText->setText(QString("%1s").arg(m_settings.m_replayOffset, 0, 'f', 1));
    ui->replayNow->setEnabled(replayEnabled && (m_settings.m_replayOffset > 0.0f));
    ui->replayPlus->setEnabled(replayEnabled && (std::round(m_settings.m_replayOffset * 10) < ui->replayOffset->maximum()));
    ui->replayMinus->setEnabled(replayEnabled && (m_settings.m_replayOffset > 0.0f));
}

void RemoteTCPInputGui::displayReplayStep()
{
    QString step;
    float intpart;
    float frac = modf(m_settings.m_replayStep, &intpart);
    if (frac == 0.0f) {
        step = QString::number((int)intpart);
    } else {
        step = QString::number(m_settings.m_replayStep, 'f', 1);
    }
    ui->replayPlus->setText(QString("+%1s").arg(step));
    ui->replayPlus->setToolTip(QString("Add %1 seconds to time delay").arg(step));
    ui->replayMinus->setText(QString("-%1s").arg(step));
    ui->replayMinus->setToolTip(QString("Remove %1 seconds from time delay").arg(step));
}

void RemoteTCPInputGui::on_replayOffset_valueChanged(int value)
{
    m_settings.m_replayOffset = value / 10.0f;
    displayReplayOffset();
    m_settingsKeys.append("replayOffset");
    sendSettings();
}

void RemoteTCPInputGui::on_replayNow_clicked()
{
    ui->replayOffset->setValue(0);
}

void RemoteTCPInputGui::on_replayPlus_clicked()
{
    ui->replayOffset->setValue(ui->replayOffset->value() + m_settings.m_replayStep * 10);
}

void RemoteTCPInputGui::on_replayMinus_clicked()
{
    ui->replayOffset->setValue(ui->replayOffset->value() - m_settings.m_replayStep * 10);
}

void RemoteTCPInputGui::on_replaySave_clicked()
{
    QFileDialog fileDialog(nullptr, "Select file to save IQ data to", "", "*.wav");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (fileNames.size() > 0)
        {
            RemoteTCPInput::MsgSaveReplay *message = RemoteTCPInput ::MsgSaveReplay::create(fileNames[0]);
            m_sampleSource->getInputMessageQueue()->push(message);
        }
    }
}

void RemoteTCPInputGui::on_replayLoop_toggled(bool checked)
{
    m_settings.m_replayLoop = checked;
    m_settingsKeys.append("replayLoop");
    sendSettings();
}

void RemoteTCPInputGui::on_sendMessage_clicked()
{
    QString message = ui->txMessage->text().trimmed();
    if (!message.isEmpty())
    {
        ui->messages->addItem(QString("< %1").arg(message));
        ui->messages->scrollToBottom();
        bool broadcast = ui->txAddress->currentText() == "All";
        QString callsign = MainCore::instance()->getSettings().getStationName();
        m_sampleSource->getInputMessageQueue()->push(RemoteTCPInput::MsgSendMessage::create(callsign, message, broadcast));
    }
}

void RemoteTCPInputGui::on_txMessage_returnPressed()
{
    on_sendMessage_clicked();
    ui->txMessage->selectAll();
}

void RemoteTCPInputGui::makeUIConnections()
{
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &RemoteTCPInputGui::on_startStop_toggled);
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &RemoteTCPInputGui::on_centerFrequency_changed);
    QObject::connect(ui->ppm, &QSlider::valueChanged, this, &RemoteTCPInputGui::on_ppm_valueChanged);
    QObject::connect(ui->dcOffset, &ButtonSwitch::toggled, this, &RemoteTCPInputGui::on_dcOffset_toggled);
    QObject::connect(ui->iqImbalance, &ButtonSwitch::toggled, this, &RemoteTCPInputGui::on_iqImbalance_toggled);
    QObject::connect(ui->biasTee, &ButtonSwitch::toggled, this, &RemoteTCPInputGui::on_biasTee_toggled);
    QObject::connect(ui->directSampling, &ButtonSwitch::toggled, this, &RemoteTCPInputGui::on_directSampling_toggled);
    QObject::connect(ui->devSampleRate, &ValueDial::changed, this, &RemoteTCPInputGui::on_devSampleRate_changed);
    QObject::connect(ui->decim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteTCPInputGui::on_decim_currentIndexChanged);
    QObject::connect(ui->gain1, &QDial::valueChanged, this, &RemoteTCPInputGui::on_gain1_valueChanged);
    QObject::connect(ui->gain2, &QDial::valueChanged, this, &RemoteTCPInputGui::on_gain2_valueChanged);
    QObject::connect(ui->gain3, &QDial::valueChanged, this, &RemoteTCPInputGui::on_gain3_valueChanged);
    QObject::connect(ui->agc, &ButtonSwitch::toggled, this, &RemoteTCPInputGui::on_agc_toggled);
    QObject::connect(ui->rfBW, &ValueDial::changed, this, &RemoteTCPInputGui::on_rfBW_changed);
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &RemoteTCPInputGui::on_deltaFrequency_changed);
    QObject::connect(ui->channelGain, &QDial::valueChanged, this, &RemoteTCPInputGui::on_channelGain_valueChanged);
    QObject::connect(ui->channelSampleRate, &ValueDial::changed, this, &RemoteTCPInputGui::on_channelSampleRate_changed);
    QObject::connect(ui->decimation, &ButtonSwitch::toggled, this, &RemoteTCPInputGui::on_decimation_toggled);
    QObject::connect(ui->sampleBits, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteTCPInputGui::on_sampleBits_currentIndexChanged);
    QObject::connect(ui->squelchEnabled, &ButtonSwitch::toggled, this, &RemoteTCPInputGui::on_squelchEnabled_toggled);
    QObject::connect(ui->squelch, &QDial::valueChanged, this, &RemoteTCPInputGui::on_squelch_valueChanged);
    QObject::connect(ui->squelchGate, &PeriodDial::valueChanged, this, &RemoteTCPInputGui::on_squelchGate_valueChanged);
    QObject::connect(ui->dataAddress->lineEdit(), &QLineEdit::editingFinished, this, &RemoteTCPInputGui::on_dataAddress_editingFinished);
    QObject::connect(ui->dataAddress, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteTCPInputGui::on_dataAddress_currentIndexChanged);
    QObject::connect(ui->dataPort, QOverload<int>::of(&QSpinBox::valueChanged), this, &RemoteTCPInputGui::on_dataPort_valueChanged);
    QObject::connect(ui->overrideRemoteSettings, &ButtonSwitch::toggled, this, &RemoteTCPInputGui::on_overrideRemoteSettings_toggled);
    QObject::connect(ui->preFill, &QDial::valueChanged, this, &RemoteTCPInputGui::on_preFill_valueChanged);
    QObject::connect(ui->protocol, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteTCPInputGui::on_protocol_currentIndexChanged);
    QObject::connect(ui->replayOffset, &QSlider::valueChanged, this, &RemoteTCPInputGui::on_replayOffset_valueChanged);
    QObject::connect(ui->replayNow, &QToolButton::clicked, this, &RemoteTCPInputGui::on_replayNow_clicked);
    QObject::connect(ui->replayPlus, &QToolButton::clicked, this, &RemoteTCPInputGui::on_replayPlus_clicked);
    QObject::connect(ui->replayMinus, &QToolButton::clicked, this, &RemoteTCPInputGui::on_replayMinus_clicked);
    QObject::connect(ui->replaySave, &QToolButton::clicked, this, &RemoteTCPInputGui::on_replaySave_clicked);
    QObject::connect(ui->replayLoop, &ButtonSwitch::toggled, this, &RemoteTCPInputGui::on_replayLoop_toggled);
    QObject::connect(ui->sendMessage, &QToolButton::clicked, this, &RemoteTCPInputGui::on_sendMessage_clicked);
    QObject::connect(ui->txMessage, &QLineEdit::returnPressed, this, &RemoteTCPInputGui::on_txMessage_returnPressed);
}
