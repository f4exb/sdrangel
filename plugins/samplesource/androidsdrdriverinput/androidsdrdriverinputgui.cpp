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

#include "ui_androidsdrdriverinputgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "gui/dialogpositioner.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/hbfilterchainconverter.h"
#include "mainwindow.h"
#include "util/simpleserializer.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "androidsdrdriverinputgui.h"
#include "androidsdrdriverinputtcphandler.h"

AndroidSDRDriverInputGui::AndroidSDRDriverInputGui(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::AndroidSDRDriverInputGui),
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
    getContents()->setStyleSheet("#AndroidSDRDriverInputGui { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesource/androidsdrdriverinput/readme.md";

    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(9, 0, 999999999); // frequency dial is in kHz

    ui->devSampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->devSampleRate->setValueRange(8, 0, 99999999);
    ui->rfBW->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->rfBW->setValueRange(5, 0, 99999);    // In kHz

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

    displaySettings();

    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);
    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));

    m_sampleSource = (AndroidSDRDriverInput*) m_deviceUISet->m_deviceAPI->getSampleSource();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleSource->setMessageQueueToGUI(&m_inputMessageQueue);

    m_forceSettings = true;
    sendSettings();
    makeUIConnections();
}

AndroidSDRDriverInputGui::~AndroidSDRDriverInputGui()
{
    m_statusTimer.stop();
    m_updateTimer.stop();
    delete ui;
}

void AndroidSDRDriverInputGui::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void AndroidSDRDriverInputGui::destroy()
{
    delete this;
}

void AndroidSDRDriverInputGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

QByteArray AndroidSDRDriverInputGui::serialize() const
{
    return m_settings.serialize();
}

bool AndroidSDRDriverInputGui::deserialize(const QByteArray& data)
{
    qDebug("AndroidSDRDriverInputGui::deserialize");

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

bool AndroidSDRDriverInputGui::handleMessage(const Message& message)
{
    if (AndroidSDRDriverInput::MsgConfigureAndroidSDRDriverInput::match(message))
    {
        const AndroidSDRDriverInput::MsgConfigureAndroidSDRDriverInput& cfg = (AndroidSDRDriverInput::MsgConfigureAndroidSDRDriverInput&) message;

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
    else if (AndroidSDRDriverInput::MsgStartStop::match(message))
    {
        AndroidSDRDriverInput::MsgStartStop& notif = (AndroidSDRDriverInput::MsgStartStop&) message;
        m_connectionError = false;
        blockApplySettings(true);
        ui->startStop->setChecked(notif.getStartStop());
        blockApplySettings(false);
        return true;
    }
    else if (AndroidSDRDriverInputTCPHandler::MsgReportRemoteDevice::match(message))
    {
        const AndroidSDRDriverInputTCPHandler::MsgReportRemoteDevice& report = (AndroidSDRDriverInputTCPHandler::MsgReportRemoteDevice&) message;
        QHash<RemoteTCPProtocol::Device, QString> devices = {
             {RemoteTCPProtocol::RTLSDR_E4000, "RTLSDR E4000"},
             {RemoteTCPProtocol::RTLSDR_FC0012, "RTLSDR FC0012"},
             {RemoteTCPProtocol::RTLSDR_FC0013, "RTLSDR FC0013"},
             {RemoteTCPProtocol::RTLSDR_FC2580, "RTLSDR FC2580"},
             {RemoteTCPProtocol::RTLSDR_R820T, "RTLSDR R820T"},
             {RemoteTCPProtocol::RTLSDR_R828D, "RTLSDR R828D"},
             {RemoteTCPProtocol::HACK_RF, "HackRF"},
             {RemoteTCPProtocol::SDRPLAY_V3_RSPDUO, "SDRplay"},     // MIR0 protocol doesn't distinguish between devices - AndroidSDRDriverInputTCPHandler::dataReadyRead always sends SDRPLAY_V3_RSPDUO
        };
        QString device = "Unknown";
        m_remoteDevice = report.getDevice();
        if (devices.contains(m_remoteDevice)) {
            device = devices.value(m_remoteDevice);
        }

        // Update GUI so we only show widgets available for the protocol in use
        bool mir0 = report.getProtocol() == "MIR0";
        if (mir0 && (ui->sampleBits->count() != 2))
        {
            ui->sampleBits->addItem("16");
        }
        else if (!mir0 && (ui->sampleBits->count() != 1))
        {
            while (ui->sampleBits->count() > 1) {
                ui->sampleBits->removeItem(ui->sampleBits->count() - 1);
            }
        }
        ui->centerFrequency->setValueRange(7, 0, 9999999);

        displayGains();
        setStatus(device);
        return true;
    }
    else if (AndroidSDRDriverInputTCPHandler::MsgReportConnection::match(message))
    {
        const AndroidSDRDriverInputTCPHandler::MsgReportConnection& report = (AndroidSDRDriverInputTCPHandler::MsgReportConnection&) message;
        qDebug() << "AndroidSDRDriverInputGui::handleMessage: MsgReportConnection connected: " << report.getConnected();
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

void AndroidSDRDriverInputGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_centerFrequency = notif->getCenterFrequency();
            qDebug("AndroidSDRDriverInputGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
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

void AndroidSDRDriverInputGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_centerFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_sampleRate / 1000));
}

void AndroidSDRDriverInputGui::displaySettings()
{
    blockApplySettings(true);

    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    ui->ppm->setValue(m_settings.m_loPpmCorrection);

    ui->dcOffset->setChecked(m_settings.m_dcBlock);
    ui->iqImbalance->setChecked(m_settings.m_iqCorrection);
    ui->biasTee->setChecked(m_settings.m_biasTee);
    ui->directSampling->setChecked(m_settings.m_directSampling);

    ui->devSampleRate->setValue(m_settings.m_devSampleRate);

    ui->agc->setChecked(m_settings.m_agc);

    ui->rfBW->setValue(m_settings.m_rfBW / 1000);

    ui->deviceRateText->setText(tr("%1k").arg(m_settings.m_devSampleRate / 1000.0));
    ui->sampleBits->setCurrentIndex(m_settings.m_sampleBits/8-1);

    ui->dataPort->setText(tr("%1").arg(m_settings.m_dataPort));

    displayGains();
    blockApplySettings(false);
}

const AndroidSDRDriverInputGui::DeviceGains::GainRange AndroidSDRDriverInputGui::m_rtlSDR34kGainRange(
    "Gain",
    {
         -10, 15, 40, 65, 90, 115, 140, 165, 190, 215,
        240, 290, 340, 420
    }
);
const AndroidSDRDriverInputGui::DeviceGains AndroidSDRDriverInputGui::m_rtlSDRe4kGains({AndroidSDRDriverInputGui::m_rtlSDR34kGainRange}, true, false);

const AndroidSDRDriverInputGui::DeviceGains::GainRange AndroidSDRDriverInputGui::m_rtlSDRR820GainRange(
    "Gain",
    {
        0, 9, 14, 27, 37, 77, 87, 125, 144, 157,
        166, 197, 207, 229, 254, 280, 297, 328,
        338, 364, 372, 386, 402, 421, 434, 439,
        445, 480, 496
    }
);
const AndroidSDRDriverInputGui::DeviceGains AndroidSDRDriverInputGui::m_rtlSDRR820Gains({AndroidSDRDriverInputGui::m_rtlSDRR820GainRange}, true, true);

const AndroidSDRDriverInputGui::DeviceGains::GainRange AndroidSDRDriverInputGui::m_hackRFLNAGainRange("LNA", 0, 40, 8);
const AndroidSDRDriverInputGui::DeviceGains::GainRange AndroidSDRDriverInputGui::m_hackRFVGAGainRange("VGA", 0, 62, 2);
const AndroidSDRDriverInputGui::DeviceGains AndroidSDRDriverInputGui::m_hackRFGains({m_hackRFLNAGainRange, m_hackRFVGAGainRange}, false, true);

// SDRplay LNA gain is device & frequency dependent (See sdrplayv3input.h SDRPlayV3LNA)
const AndroidSDRDriverInputGui::DeviceGains::GainRange AndroidSDRDriverInputGui::m_sdrplayV3LNAGainRange("LNA", 0, 9, 1, "");
const AndroidSDRDriverInputGui::DeviceGains::GainRange AndroidSDRDriverInputGui::m_sdrplayV3IFGainRange("IF", -59, -20, 1);
const AndroidSDRDriverInputGui::DeviceGains AndroidSDRDriverInputGui::m_sdrplayV3Gains({m_sdrplayV3LNAGainRange, m_sdrplayV3IFGainRange}, true, true);

const QHash<RemoteTCPProtocol::Device, const AndroidSDRDriverInputGui::DeviceGains *> AndroidSDRDriverInputGui::m_gains =
{
    {RemoteTCPProtocol::RTLSDR_E4000, &m_rtlSDRe4kGains},
    {RemoteTCPProtocol::RTLSDR_R820T, &m_rtlSDRR820Gains},
    {RemoteTCPProtocol::HACK_RF, &m_hackRFGains},
    {RemoteTCPProtocol::SDRPLAY_V3_RSP1, &m_sdrplayV3Gains},
    {RemoteTCPProtocol::SDRPLAY_V3_RSP1A, &m_sdrplayV3Gains},
    {RemoteTCPProtocol::SDRPLAY_V3_RSP2, &m_sdrplayV3Gains},
    {RemoteTCPProtocol::SDRPLAY_V3_RSPDUO, &m_sdrplayV3Gains},
    {RemoteTCPProtocol::SDRPLAY_V3_RSPDX, &m_sdrplayV3Gains},
};

QString AndroidSDRDriverInputGui::gainText(int stage)
{
    if (m_deviceGains) {
        return QString("%1.%2%3").arg(m_settings.m_gain[stage] / 10).arg(abs(m_settings.m_gain[stage] % 10)).arg(m_deviceGains->m_gains[stage].m_units);
    } else {
        return "";
    }
}

void AndroidSDRDriverInputGui::displayGains()
{
    QLabel *gainLabels[] = {ui->gain1Label, ui->gain2Label};
    QSlider *gain[] = {ui->gain1, ui->gain2};
    QLabel *gainTexts[] = {ui->gain1Text, ui->gain2Text};
    QWidget *gainLine[] = {ui->gainLine1};

    m_deviceGains = m_gains.value(m_remoteDevice);
    if (m_deviceGains)
    {
        ui->agc->setVisible(m_deviceGains->m_agc);
        ui->biasTee->setVisible(m_deviceGains->m_biasTee);
        ui->directSampling->setVisible(m_remoteDevice <= RemoteTCPProtocol::RTLSDR_R828D);
        for (int i = 0; i < 2; i++)
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
                gain[i]->blockSignals(true);
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
                gain[i]->blockSignals(false);
                gainTexts[i]->setText(gainText(i));
            }
        }
    }
    else
    {
        qDebug() << "AndroidSDRDriverInputGui::displayGains: No gains for " << m_remoteDevice;
    }
}

void AndroidSDRDriverInputGui::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void AndroidSDRDriverInputGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        m_connectionError = false;
        AndroidSDRDriverInput::MsgStartStop *message = AndroidSDRDriverInput::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void AndroidSDRDriverInputGui::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void AndroidSDRDriverInputGui::on_devSampleRate_changed(quint64 value)
{
    m_settings.m_devSampleRate = value;
    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void AndroidSDRDriverInputGui::on_ppm_valueChanged(int value)
{
    m_settings.m_loPpmCorrection = value;
    ui->ppmText->setText(tr("%1").arg(value));
    m_settingsKeys.append("loPpmCorrection");
    sendSettings();
}

void AndroidSDRDriverInputGui::on_dcOffset_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    m_settingsKeys.append("dcBlock");
    sendSettings();
}

void AndroidSDRDriverInputGui::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    m_settingsKeys.append("iqCorrection");
    sendSettings();
}

void AndroidSDRDriverInputGui::on_biasTee_toggled(bool checked)
{
    m_settings.m_biasTee = checked;
    m_settingsKeys.append("biasTee");
    sendSettings();
}

void AndroidSDRDriverInputGui::on_directSampling_toggled(bool checked)
{
    m_settings.m_directSampling = checked;
    m_settingsKeys.append("directSampling");
    sendSettings();
}

void AndroidSDRDriverInputGui::on_agc_toggled(bool checked)
{
    m_settings.m_agc = checked;
    m_settingsKeys.append("agc");
    sendSettings();
}

void AndroidSDRDriverInputGui::on_gain1_valueChanged(int value)
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

void AndroidSDRDriverInputGui::on_gain2_valueChanged(int value)
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

void AndroidSDRDriverInputGui::on_rfBW_changed(int value)
{
    m_settings.m_rfBW = value * 1000;
    m_settingsKeys.append("rfBW");
    sendSettings();
}

void AndroidSDRDriverInputGui::on_sampleBits_currentIndexChanged(int index)
{
    m_settings.m_sampleBits = 8 * (index + 1);
    m_settingsKeys.append("sampleBits");
    sendSettings();
}

void AndroidSDRDriverInputGui::on_dataPort_editingFinished()
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

void AndroidSDRDriverInputGui::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "AndroidSDRDriverInputGui::updateHardware";
        AndroidSDRDriverInput::MsgConfigureAndroidSDRDriverInput* message =
                AndroidSDRDriverInput::MsgConfigureAndroidSDRDriverInput::create(m_settings, m_settingsKeys, m_forceSettings);
        m_sampleSource->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void AndroidSDRDriverInputGui::updateStatus()
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

void AndroidSDRDriverInputGui::openDeviceSettingsDialog(const QPoint& p)
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

void AndroidSDRDriverInputGui::makeUIConnections()
{
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &AndroidSDRDriverInputGui::on_startStop_toggled);
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &AndroidSDRDriverInputGui::on_centerFrequency_changed);
    QObject::connect(ui->ppm, &QSlider::valueChanged, this, &AndroidSDRDriverInputGui::on_ppm_valueChanged);
    QObject::connect(ui->dcOffset, &ButtonSwitch::toggled, this, &AndroidSDRDriverInputGui::on_dcOffset_toggled);
    QObject::connect(ui->iqImbalance, &ButtonSwitch::toggled, this, &AndroidSDRDriverInputGui::on_iqImbalance_toggled);
    QObject::connect(ui->biasTee, &ButtonSwitch::toggled, this, &AndroidSDRDriverInputGui::on_biasTee_toggled);
    QObject::connect(ui->directSampling, &ButtonSwitch::toggled, this, &AndroidSDRDriverInputGui::on_directSampling_toggled);
    QObject::connect(ui->devSampleRate, &ValueDial::changed, this, &AndroidSDRDriverInputGui::on_devSampleRate_changed);
    QObject::connect(ui->gain1, &QSlider::valueChanged, this, &AndroidSDRDriverInputGui::on_gain1_valueChanged);
    QObject::connect(ui->gain2, &QSlider::valueChanged, this, &AndroidSDRDriverInputGui::on_gain2_valueChanged);
    QObject::connect(ui->agc, &ButtonSwitch::toggled, this, &AndroidSDRDriverInputGui::on_agc_toggled);
    QObject::connect(ui->rfBW, &ValueDial::changed, this, &AndroidSDRDriverInputGui::on_rfBW_changed);
    QObject::connect(ui->sampleBits, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AndroidSDRDriverInputGui::on_sampleBits_currentIndexChanged);
    QObject::connect(ui->dataPort, &QLineEdit::editingFinished, this, &AndroidSDRDriverInputGui::on_dataPort_editingFinished);
}
