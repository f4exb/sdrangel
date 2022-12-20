///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#include "ui_remotetcpinputgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/hbfilterchainconverter.h"
#include "mainwindow.h"
#include "util/simpleserializer.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "remotetcpinputgui.h"
#include "remotetcpinputtcphandler.h"

RemoteTCPInputGui::RemoteTCPInputGui(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::RemoteTCPInputGui),
    m_settings(),
    m_sampleSource(0),
    m_lastEngineState(DeviceAPI::StNotStarted),
    m_sampleRate(0),
    m_centerFrequency(0),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_deviceGains(nullptr),
    m_remoteDevice(RemoteTCPProtocol::RTLSDR_R820T),
    m_connectionError(false)
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

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

    displaySettings();

    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);
    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));

    m_sampleSource = (RemoteTCPInput*) m_deviceUISet->m_deviceAPI->getSampleSource();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleSource->setMessageQueueToGUI(&m_inputMessageQueue);

    m_forceSettings = true;
    sendSettings();
    makeUIConnections();
    DialPopup::addPopupsToChildDials(this);
}

RemoteTCPInputGui::~RemoteTCPInputGui()
{
    m_statusTimer.stop();
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
        const RemoteTCPInput::MsgConfigureRemoteTCPInput& cfg = (RemoteTCPInput::MsgConfigureRemoteTCPInput&) message;

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
        RemoteTCPInput::MsgStartStop& notif = (RemoteTCPInput::MsgStartStop&) message;
        m_connectionError = false;
        blockApplySettings(true);
        ui->startStop->setChecked(notif.getStartStop());
        blockApplySettings(false);
        return true;
    }
    else if (RemoteTCPInput::MsgReportTCPBuffer::match(message))
    {
        const RemoteTCPInput::MsgReportTCPBuffer& report = (RemoteTCPInput::MsgReportTCPBuffer&) message;
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
        const RemoteTCPInputTCPHandler::MsgReportRemoteDevice& report = (RemoteTCPInputTCPHandler::MsgReportRemoteDevice&) message;
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
        ui->protocol->setText(QString("Protocol: %1").arg(report.getProtocol()));

        // Update GUI so we only show widgets available for the protocol in use
        bool sdra = report.getProtocol() == "SDRA";
        if (sdra && (ui->sampleBits->count() != 4))
        {
            ui->sampleBits->addItem("16");
            ui->sampleBits->addItem("24");
            ui->sampleBits->addItem("32");
        }
        else if (!sdra && (ui->sampleBits->count() != 1))
        {
            while (ui->sampleBits->count() > 1) {
                ui->sampleBits->removeItem(ui->sampleBits->count() - 1);
            }
        }
        if (sdra && (ui->decim->count() != 7))
        {
            ui->decim->addItem("2");
            ui->decim->addItem("4");
            ui->decim->addItem("8");
            ui->decim->addItem("16");
            ui->decim->addItem("32");
            ui->decim->addItem("64");
        }
        else if (!sdra && (ui->decim->count() != 1))
        {
            while (ui->decim->count() > 1) {
                ui->decim->removeItem(ui->decim->count() - 1);
            }
        }
        if (!sdra) {
            ui->deltaFrequency->setValue(0);
            ui->channelGain->setValue(0);
            ui->decimation->setChecked(true);
        }
        ui->deltaFrequency->setEnabled(sdra);
        ui->channelGain->setEnabled(sdra);
        ui->decimation->setEnabled(sdra);
        if (sdra) {
            ui->centerFrequency->setValueRange(9, 0, 999999999); // Should add transverter control to protocol in the future
        } else {
            ui->centerFrequency->setValueRange(7, 0, 9999999);
        }

        displayGains();
        return true;
    }
    else if (RemoteTCPInputTCPHandler::MsgReportConnection::match(message))
    {
        const RemoteTCPInputTCPHandler::MsgReportConnection& report = (RemoteTCPInputTCPHandler::MsgReportConnection&) message;
        qDebug() << "RemoteTCPInputGui::handleMessage: MsgReportConnection connected: " << report.getConnected();
        if (report.getConnected())
        {
             m_connectionError = false;
             ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
        }
        else
        {
             m_connectionError = true;
             ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
        }
        return true;
    }
    else
    {
        return false;
    }
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
    ui->channelSampleRate->setEnabled(m_settings.m_channelDecimation);
    ui->sampleBits->setCurrentIndex(m_settings.m_sampleBits/8-1);

    ui->dataPort->setText(tr("%1").arg(m_settings.m_dataPort));
    ui->dataAddress->setText(m_settings.m_dataAddress);
    ui->overrideRemoteSettings->setChecked(m_settings.m_overrideRemoteSettings);

    ui->preFill->setValue((int)(m_settings.m_preFill * 10.0));
    ui->preFillText->setText(QString("%1s").arg(m_settings.m_preFill, 0, 'f', 2));

    displayGains();
    blockApplySettings(false);
}

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

// SDRplay LNA gain is device & frequency dependent (See sdrplayv3input.h SDRPlayV3LNA) sp we just fix as 0 for now
const RemoteTCPInputGui::DeviceGains::GainRange RemoteTCPInputGui::m_sdrplayV3LNAGainRange("LNA", {0});
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
    {RemoteTCPProtocol::AIRSPY, &m_airspyGains},
    {RemoteTCPProtocol::AIRSPY_HF, &m_airspyHFGains},
    {RemoteTCPProtocol::BLADE_RF1, &m_baldeRF1Gains},
    {RemoteTCPProtocol::FCD_PRO_PLUS, &m_funCubeProPlusGains},
    {RemoteTCPProtocol::HACK_RF, &m_hackRFGains},
    {RemoteTCPProtocol::KIWI_SDR, &m_kiwiGains},
    {RemoteTCPProtocol::LIME_SDR, &m_limeGains},
    {RemoteTCPProtocol::SDRPLAY_V3_RSP1, &m_sdrplayV3Gains},
    {RemoteTCPProtocol::SDRPLAY_V3_RSP1A, &m_sdrplayV3Gains},
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

    m_deviceGains = m_gains.value(m_remoteDevice);
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
        m_connectionError = false;
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
    sendSettings();
}

void RemoteTCPInputGui::on_sampleBits_currentIndexChanged(int index)
{
    m_settings.m_sampleBits = 8 * (index + 1);
    m_settingsKeys.append("sampleBits");
    sendSettings();
}

void RemoteTCPInputGui::on_dataAddress_editingFinished()
{
    m_settings.m_dataAddress = ui->dataAddress->text();
    m_settingsKeys.append("dataAddress");
    sendSettings();
}

void RemoteTCPInputGui::on_dataPort_editingFinished()
{
    bool ok;
    quint16 udpPort = ui->dataPort->text().toInt(&ok);

    if ((!ok) || (udpPort < 1024)) {
        udpPort = 9998;
    }

    m_settings.m_dataPort = udpPort;
    ui->dataPort->setText(tr("%1").arg(m_settings.m_dataPort));
    m_settingsKeys.append("dataPort");

    sendSettings();
}

void RemoteTCPInputGui::on_overrideRemoteSettings_toggled(bool checked)
{
    m_settings.m_overrideRemoteSettings = checked;
    m_settingsKeys.append("overrideRemoteSettings");
    sendSettings();
}

void RemoteTCPInputGui::on_preFill_valueChanged(int value)
{
    m_settings.m_preFill = value/10.0f;
    ui->preFillText->setText(QString("%1s").arg(m_settings.m_preFill, 0, 'f', 2));
    m_settingsKeys.append("preFill");
    sendSettings();
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
    int state = m_deviceUISet->m_deviceAPI->state();

    if (!m_connectionError && (m_lastEngineState != state))
    {
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

        m_lastEngineState = state;
    }
}

void RemoteTCPInputGui::openDeviceSettingsDialog(const QPoint& p)
{
    if (m_contextMenuType == ContextMenuDeviceSettings)
    {
        BasicDeviceSettingsDialog dialog(this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();

        sendSettings();
    }

    resetContextMenuType();
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
    QObject::connect(ui->dataAddress, &QLineEdit::editingFinished, this, &RemoteTCPInputGui::on_dataAddress_editingFinished);
    QObject::connect(ui->dataPort, &QLineEdit::editingFinished, this, &RemoteTCPInputGui::on_dataPort_editingFinished);
    QObject::connect(ui->overrideRemoteSettings, &ButtonSwitch::toggled, this, &RemoteTCPInputGui::on_overrideRemoteSettings_toggled);
    QObject::connect(ui->preFill, &QDial::valueChanged, this, &RemoteTCPInputGui::on_preFill_valueChanged);
}
