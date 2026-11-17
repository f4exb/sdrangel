///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2022-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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
#include <QFileDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QSignalBlocker>

#include <algorithm>
#include <limits>

#include "sdrplaygui.h"

#include "device/deviceapi.h"
#include "device/deviceuiset.h"

#include "ui_sdrplaygui.h"
#include "gui/messagedialog.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "mainspectrum/mainspectrumgui.h"
#include "dsp/dspcommands.h"


SDRPlayGui::SDRPlayGui(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::SDRPlayGui),
    m_doApplySettings(true),
    m_forceSettings(true)
{
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_sampleSource = (SDRPlayInput*) m_deviceUISet->m_deviceAPI->getSampleSource();

    ui->setupUi(getContents());
    setupFrontEndControls();
    setupTooltips();
    sizeToContents();
    getContents()->setStyleSheet("#SDRPlayGui { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesource/sdrplay/readme.md";
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(9, 10U, 999999999U);

    ui->ifFrequency->clear();
    for (unsigned int i = 0; i < SDRPlayIF::getNbIFs(); i++)
    {
        ui->ifFrequency->addItem(QString::number(SDRPlayIF::getIF(i)/1000));
    }

    ui->samplerate->clear();
    std::vector<unsigned int> sampleRateIndexes;
    for (unsigned int i = 0; i < SDRPlaySampleRates::getNbRates(); i++)
    {
        sampleRateIndexes.push_back(i);
    }
    std::sort(sampleRateIndexes.begin(), sampleRateIndexes.end(), [](unsigned int lhs, unsigned int rhs) {
        return SDRPlaySampleRates::getRate(lhs) < SDRPlaySampleRates::getRate(rhs);
    });
    for (unsigned int index : sampleRateIndexes)
    {
        ui->samplerate->addItem(
            QString::number(SDRPlaySampleRates::getRate(index) / 1000),
            index);
    }

    ui->bandwidth->clear();
    for (unsigned int i = 0; i < SDRPlayBandwidths::getNbBandwidths(); i++)
    {
        ui->bandwidth->addItem(QString::number(SDRPlayBandwidths::getBandwidth(i)/1000));
    }

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

    displaySettings();
    makeUIConnections();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleSource->setMessageQueueToGUI(&m_inputMessageQueue);
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();
}

SDRPlayGui::~SDRPlayGui()
{
    m_statusTimer.stop();
    m_updateTimer.stop();
    delete ui;
}

void SDRPlayGui::destroy()
{
    delete this;
}

void SDRPlayGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

QByteArray SDRPlayGui::serialize() const
{
    return m_settings.serialize();
}

bool SDRPlayGui::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        displaySettings();
        m_forceSettings = true;
        sendSettings();
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool SDRPlayGui::handleMessage(const Message& message)
{
    if (SDRPlayInput::MsgConfigureSDRPlay::match(message))
    {
        const SDRPlayInput::MsgConfigureSDRPlay& cfg = (SDRPlayInput::MsgConfigureSDRPlay&) message;

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
    else if (SDRPlayInput::MsgReportSDRPlayGains::match(message))
    {
    	qDebug() << "SDRPlayGui::handleMessage: MsgReportSDRPlayGains";

    	SDRPlayInput::MsgReportSDRPlayGains msg = (SDRPlayInput::MsgReportSDRPlayGains&) message;
        const QSignalBlocker tunerBlocker(ui->gainTuner);
        const QSignalBlocker lnaBlocker(ui->gainLNA);
        const QSignalBlocker mixerBlocker(ui->gainMixer);
        const QSignalBlocker basebandBlocker(ui->gainBaseband);

    	if (m_settings.m_tunerGainMode)
    	{
            ui->gainTuner->setValue(msg.getTunerGain());

            QString gainText = QString::number(msg.getTunerGain());
            ui->gainTunerText->setText(gainText);
        }
        else
        {
            ui->gainLNA->setChecked(msg.getLNAGain() != 0);
            ui->gainMixer->setChecked(msg.getMixerGain() != 0);
            ui->gainBaseband->setValue(msg.getBasebandGain());

            QString gainText = QString::number(msg.getBasebandGain());
            ui->gainBasebandText->setText(gainText);
    	}

        return true;
    }
    else if (SDRPlayInput::MsgReportSDRPlayCapabilities::match(message))
    {
        const SDRPlayInput::MsgReportSDRPlayCapabilities& report =
            (const SDRPlayInput::MsgReportSDRPlayCapabilities&) message;
        displayCapabilities(report);
        return true;
    }
    else if (SDRPlayInput::MsgStartStop::match(message))
    {
        SDRPlayInput::MsgStartStop& notif = (SDRPlayInput::MsgStartStop&) message;
        blockApplySettings(true);
        ui->startStop->setChecked(notif.getStartStop());
        blockApplySettings(false);

        return true;
    }
    else
    {
        return false;
    }
}

void SDRPlayGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("SDRPlayGui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("SDRPlayGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();

            delete message;
        }
        else
        {
            if (!handleMessage(*message)) {
                qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
            }
            delete message;
        }
    }
}

void SDRPlayGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_sampleRate / 1000));
}


void SDRPlayGui::displaySettings()
{
    const QSignalBlocker centerFrequencyBlocker(ui->centerFrequency);
    const QSignalBlocker ppmBlocker(ui->ppm);
    const QSignalBlocker sampleRateBlocker(ui->samplerate);
    const QSignalBlocker dcOffsetBlocker(ui->dcOffset);
    const QSignalBlocker iqImbalanceBlocker(ui->iqImbalance);
    const QSignalBlocker bandwidthBlocker(ui->bandwidth);
    const QSignalBlocker ifBlocker(ui->ifFrequency);
    const QSignalBlocker decimBlocker(ui->decim);
    const QSignalBlocker fcPosBlocker(ui->fcPos);
    const QSignalBlocker gainModeBlocker(ui->gainTunerOn);
    const QSignalBlocker manualModeBlocker(ui->gainManualOn);
    const QSignalBlocker tunerGainBlocker(ui->gainTuner);
    const QSignalBlocker lnaBlocker(ui->gainLNA);
    const QSignalBlocker mixerBlocker(ui->gainMixer);
    const QSignalBlocker basebandGainBlocker(ui->gainBaseband);

    setTitle(m_settings.m_title);
    getDeviceUISet()->m_mainSpectrumGUI->setTitle(m_settings.m_title);
    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);

    ui->ppm->setValue(m_settings.m_LOppmTenths);
    ui->ppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));

    ui->dcOffset->setChecked(m_settings.m_dcBlock);
    ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

    ui->bandwidth->setCurrentIndex(m_settings.m_bandwidthIndex);
    ui->ifFrequency->setCurrentIndex(m_settings.m_ifFrequencyIndex);
    ui->samplerate->setCurrentIndex(ui->samplerate->findData(m_settings.m_devSampleRateIndex));

    ui->decim->setCurrentIndex(m_settings.m_log2Decim);
    ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);

    ui->gainTunerOn->setChecked(m_settings.m_tunerGainMode);
    ui->gainManualOn->setChecked(!m_settings.m_tunerGainMode);

    if (m_settings.m_tunerGainMode)
    {
        ui->gainTuner->setEnabled(true);
        ui->gainLNA->setEnabled(false);
        ui->gainMixer->setEnabled(false);
        ui->gainBaseband->setEnabled(false);

        int gain = m_settings.m_tunerGain;
        ui->gainTuner->setValue(gain);
        QString gainText = QString::number(gain);
        ui->gainTunerText->setText(gainText);
        m_settings.m_tunerGain = gain;
    }
    else
    {
        ui->gainTuner->setEnabled(false);
        ui->gainLNA->setEnabled(true);
        ui->gainMixer->setEnabled(true);
        ui->gainBaseband->setEnabled(true);

        ui->gainLNA->setChecked(m_settings.m_lnaOn != 0);
        ui->gainMixer->setChecked(m_settings.m_mixerAmpOn != 0);

        int gain = m_settings.m_basebandGain;
        ui->gainBaseband->setValue(gain);
        QString gainText = QString::number(gain);
        ui->gainBasebandText->setText(gainText);
    }

    displayFrontEndSettings();
}

void SDRPlayGui::setupFrontEndControls()
{
    QWidget *frontEndWidget = new QWidget(getContents());
    QGridLayout *layout = new QGridLayout(frontEndWidget);
    layout->setContentsMargins(3, 3, 3, 3);
    layout->setHorizontalSpacing(4);
    layout->setVerticalSpacing(2);

    m_boardLabel = new QLabel(tr("Detecting device..."), frontEndWidget);
    m_tuner = new QComboBox(frontEndWidget);
    m_antenna = new QComboBox(frontEndWidget);
    m_attenuator = new QComboBox(frontEndWidget);
    m_directSampling = new QComboBox(frontEndWidget);
    m_biasT = new QCheckBox(tr("Bias-T"), frontEndWidget);
    m_notchFM = new QCheckBox(tr("FM"), frontEndWidget);
    m_notchDAB = new QCheckBox(tr("DAB"), frontEndWidget);
    m_notchMW = new QCheckBox(tr("MW"), frontEndWidget);
    m_amplifierBypass = new QCheckBox(tr("Amp bypass"), frontEndWidget);
    m_portTie = new QCheckBox(tr("Port tie"), frontEndWidget);
    m_referenceOutput = new QCheckBox(tr("24 MHz out"), frontEndWidget);

    m_directSampling->addItem(tr("Off"), MIRISDR_DS_OFF);
    m_directSampling->addItem(tr("On"), MIRISDR_DS_ON);
    m_directSampling->addItem(tr("Auto"), MIRISDR_DS_AUTO);

    layout->addWidget(m_boardLabel, 0, 0, 1, 4);
    layout->addWidget(new QLabel(tr("Tuner"), frontEndWidget), 1, 0);
    layout->addWidget(m_tuner, 1, 1);
    layout->addWidget(new QLabel(tr("Antenna"), frontEndWidget), 1, 2);
    layout->addWidget(m_antenna, 1, 3);
    layout->addWidget(new QLabel(tr("Attenuation"), frontEndWidget), 2, 0);
    layout->addWidget(m_attenuator, 2, 1);
    layout->addWidget(new QLabel(tr("Direct"), frontEndWidget), 2, 2);
    layout->addWidget(m_directSampling, 2, 3);
    layout->addWidget(m_biasT, 3, 0);
    layout->addWidget(m_amplifierBypass, 3, 1);
    layout->addWidget(m_portTie, 3, 2);
    layout->addWidget(m_referenceOutput, 3, 3);
    layout->addWidget(new QLabel(tr("Notches"), frontEndWidget), 4, 0);
    layout->addWidget(m_notchFM, 4, 1);
    layout->addWidget(m_notchDAB, 4, 2);
    layout->addWidget(m_notchMW, 4, 3);
    ui->verticalLayout->addWidget(frontEndWidget);
}

void SDRPlayGui::setupTooltips()
{
    ui->startStop->setToolTip(tr("Start or stop sample acquisition from the receiver."));
    ui->deviceRateText->setToolTip(tr("Actual complex sample rate delivered to SDRangel after decimation."));
    ui->centerFrequency->setToolTip(tr("RF center frequency in kHz."));
    ui->ppm->setToolTip(tr("Correct the receiver reference oscillator in 0.1 ppm steps."));
    ui->ppmText->setToolTip(ui->ppm->toolTip());
    ui->dcOffset->setToolTip(tr("Remove the DC spike produced at zero IF. Enable this before IQ correction."));
    ui->iqImbalance->setToolTip(tr("Correct amplitude and phase imbalance between I and Q. DC correction must also be enabled."));
    ui->bandwidth->setToolTip(tr("Select the receiver IF filter bandwidth in kHz. Use a bandwidth wide enough for the wanted signal."));
    ui->ifFrequency->setToolTip(tr("Select zero IF or a low IF in kHz. libmirisdr-6 shifts a selected low IF to 0 Hz in the output stream."));
    ui->samplerate->setToolTip(tr("Request the complex baseband sample rate before SDRangel decimation. libmirisdr-6 selects the converter and resampling plan."));
    ui->decim->setToolTip(tr("Divide the device sample rate by this power-of-two factor before sending samples to SDRangel."));
    ui->fcPos->setToolTip(tr("Choose where the tuned frequency appears after decimation: Inf below center, Sup above center, or Cen at center."));

    ui->gainTunerOn->setToolTip(tr("Total-gain mode: set one gain value and let libmirisdr-6 distribute it across the available gain stages."));
    ui->gainTuner->setToolTip(tr("Requested total receive gain in dB. The available maximum includes any board-level front-end amplifier."));
    ui->gainTunerText->setToolTip(ui->gainTuner->toolTip());
    ui->gainManualOn->setToolTip(tr("Manual-gain mode: control the LNA or mixer buffer, mixer amplifier, and baseband gain independently."));
    ui->gainLNA->setToolTip(tr("Enable the nominal 24 dB LNA above 50 MHz, or the mixer buffer below 50 MHz. Actual gain depends on frequency."));
    ui->gainMixer->setToolTip(tr("Enable the mixer amplifier, adding approximately 19 dB of gain."));
    ui->gainBaseband->setToolTip(tr("Set baseband amplifier gain from 0 to 59 dB."));
    ui->gainBasebandText->setToolTip(ui->gainBaseband->toolTip());

    m_boardLabel->setToolTip(tr("Detected receiver model, active tuner, and number of tuners reported by libmirisdr-6."));
    m_tuner->setToolTip(tr("Select the active RSPduo tuner. One tuner is streamed by this SDRangel device set at a time."));
    m_antenna->setToolTip(tr("Select the receiver antenna input. Available inputs depend on the device and tuner; Hi-Z is limited to tuner LOs below 30 MHz."));
    m_attenuator->setToolTip(tr("Apply front-end attenuation before the tuner. Available dB steps depend on the receiver and selected input."));
    m_directSampling->setToolTip(tr("Control direct sampling on supported RSPdx-family receivers. On uses it at or below 2 MHz; Auto uses it only when the complete requested band fits below 2 MHz; Off uses the tuner."));
    m_biasT->setToolTip(tr("Supply DC power through the supported antenna connector. Enable only when the connected antenna or accessory accepts Bias-T power."));
    m_notchFM->setToolTip(tr("Enable the broadcast-FM rejection filter, approximately 85 to 100 MHz."));
    m_notchDAB->setToolTip(tr("Enable the DAB rejection filter, approximately 165 to 230 MHz."));
    m_notchMW->setToolTip(tr("Enable the medium-wave rejection filter, approximately 0.4 to 1.6 MHz."));
    m_amplifierBypass->setToolTip(tr("Bypass the board-level front-end amplifier on supported receivers. This option is available in manual-gain mode."));
    m_portTie->setToolTip(tr("RSPduo only: tie the two 50-ohm antenna ports so either tuner can receive from either port. This can reduce signal level and increase noise."));
    m_referenceOutput->setToolTip(tr("Enable the receiver's 24 MHz reference output for clocking another compatible device."));
}

void SDRPlayGui::displayCapabilities(const SDRPlayInput::MsgReportSDRPlayCapabilities& report)
{
    const QSignalBlocker tunerBlocker(m_tuner);
    const QSignalBlocker antennaBlocker(m_antenna);
    const QSignalBlocker attenuatorBlocker(m_attenuator);
    m_frontendCaps = report.getCaps();

    ui->gainTuner->setMaximum(102 + m_frontendCaps.amp_max_db);
    m_boardLabel->setText(tr("%1 — tuner %2 of %3")
        .arg(report.getBoardName())
        .arg(report.getTunerIndex() + 1)
        .arg(report.getTunerCount()));

    m_tuner->clear();
    for (int i = 0; i < report.getTunerCount(); ++i) {
        m_tuner->addItem(tr("%1").arg(i + 1), i);
    }
    m_tuner->setCurrentIndex(report.getTunerIndex());
    m_tuner->setEnabled(report.getTunerCount() > 1);

    m_antenna->clear();
    const char *antennaNames[] = {"A", "B", "C", "Test"};
    for (int i = 0; i <= MIRISDR_ANTENNA_TEST; ++i)
    {
        if (m_frontendCaps.antennas & (1U << i)) {
            const QString name = i == m_frontendCaps.antenna_hiz ? tr("Hi-Z") : QString::fromLatin1(antennaNames[i]);
            m_antenna->addItem(name, i);
        }
    }
    m_antenna->setEnabled(m_antenna->count() > 1);
    updateCenterFrequencyRange();

    m_attenuator->clear();
    for (int i = 0; i < m_frontendCaps.attenuator_count; ++i) {
        m_attenuator->addItem(tr("%1 dB").arg(m_frontendCaps.attenuator[i]), m_frontendCaps.attenuator[i]);
    }
    m_attenuator->setEnabled(m_attenuator->count() > 1);
    m_biasT->setEnabled(m_frontendCaps.bias != 0);
    m_notchFM->setEnabled(m_frontendCaps.notches & MIRISDR_NOTCH_FM);
    m_notchDAB->setEnabled(m_frontendCaps.notches & MIRISDR_NOTCH_DAB);
    m_notchMW->setEnabled(m_frontendCaps.notches & MIRISDR_NOTCH_MW);
    m_directSampling->setEnabled(m_frontendCaps.direct_sampling != 0);
    m_amplifierBypass->setEnabled(m_frontendCaps.amp_bypass_hz != 0);
    m_portTie->setEnabled(m_frontendCaps.flags & MIRISDR_FRONTEND_PORT_TIE);
    m_referenceOutput->setEnabled(m_frontendCaps.flags & MIRISDR_FRONTEND_REF_OUT);
    displayFrontEndSettings();
    sizeToContents();
}

void SDRPlayGui::updateCenterFrequencyRange()
{
    if (m_frontendCaps.range_count <= 0) {
        return;
    }

    uint32_t minimumFrequency = std::numeric_limits<uint32_t>::max();
    uint32_t maximumFrequency = 0;

    for (int i = 0; i < m_frontendCaps.range_count; ++i)
    {
        minimumFrequency = std::min(minimumFrequency, m_frontendCaps.range[i][0]);
        maximumFrequency = std::max(maximumFrequency, m_frontendCaps.range[i][1]);
    }

    if (m_frontendCaps.antenna_hiz >= 0 && m_settings.m_antenna == m_frontendCaps.antenna_hiz) {
        maximumFrequency = std::min<uint32_t>(maximumFrequency, 30000000U);
    }

    if (maximumFrequency > minimumFrequency)
    {
        const QSignalBlocker centerFrequencyBlocker(ui->centerFrequency);
        const quint64 minimumFrequencyKHz = (static_cast<quint64>(minimumFrequency) + 999U) / 1000U;
        const quint64 maximumFrequencyKHz = (static_cast<quint64>(maximumFrequency) - 1U) / 1000U;
        ui->centerFrequency->setValueRange(9, minimumFrequencyKHz, maximumFrequencyKHz);
        ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000U);
    }
}

void SDRPlayGui::displayFrontEndSettings()
{
    if (!m_tuner) {
        return;
    }

    const QSignalBlocker tunerBlocker(m_tuner);
    const QSignalBlocker antennaBlocker(m_antenna);
    const QSignalBlocker attenuatorBlocker(m_attenuator);
    const QSignalBlocker directBlocker(m_directSampling);
    const QSignalBlocker biasBlocker(m_biasT);
    const QSignalBlocker fmBlocker(m_notchFM);
    const QSignalBlocker dabBlocker(m_notchDAB);
    const QSignalBlocker mwBlocker(m_notchMW);
    const QSignalBlocker ampBlocker(m_amplifierBypass);
    const QSignalBlocker tieBlocker(m_portTie);
    const QSignalBlocker refBlocker(m_referenceOutput);

    m_tuner->setCurrentIndex(m_tuner->findData(m_settings.m_tunerIndex));
    m_antenna->setCurrentIndex(m_antenna->findData(m_settings.m_antenna));
    updateCenterFrequencyRange();
    m_attenuator->setCurrentIndex(m_attenuator->findData(m_settings.m_attenuator));
    m_directSampling->setCurrentIndex(m_directSampling->findData(m_settings.m_directSampling));
    m_biasT->setChecked(m_settings.m_biasT);
    m_notchFM->setChecked(m_settings.m_notchMask & MIRISDR_NOTCH_FM);
    m_notchDAB->setChecked(m_settings.m_notchMask & MIRISDR_NOTCH_DAB);
    m_notchMW->setChecked(m_settings.m_notchMask & MIRISDR_NOTCH_MW);
    m_amplifierBypass->setChecked(m_settings.m_amplifierBypass);
    m_portTie->setChecked(m_settings.m_frontEndFlags & MIRISDR_FRONTEND_PORT_TIE);
    m_referenceOutput->setChecked(m_settings.m_frontEndFlags & MIRISDR_FRONTEND_REF_OUT);
    m_amplifierBypass->setEnabled(m_frontendCaps.amp_bypass_hz != 0 && !m_settings.m_tunerGainMode);
}

void SDRPlayGui::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void SDRPlayGui::updateHardware()
{
    qDebug() << "SDRPlayGui::updateHardware";
    SDRPlayInput::MsgConfigureSDRPlay* message = SDRPlayInput::MsgConfigureSDRPlay::create( m_settings, m_settingsKeys, m_forceSettings);
    m_sampleSource->getInputMessageQueue()->push(message);
    m_forceSettings = false;
    m_settingsKeys.clear();
    m_updateTimer.stop();
}

void SDRPlayGui::updateStatus()
{
    int state = m_deviceUISet->m_deviceAPI->state();

    if(m_lastEngineState != state)
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
                MessageDialog::information(this, tr("Message"), m_deviceUISet->m_deviceAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}

void SDRPlayGui::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void SDRPlayGui::on_ppm_valueChanged(int value)
{
    m_settings.m_LOppmTenths = value;
    ui->ppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));
    m_settingsKeys.append("LOppmTenths");
    sendSettings();
}

void SDRPlayGui::on_dcOffset_toggled(bool checked)
{
    qDebug("SDRPlayGui::on_dcOffset_toggled: %s", checked ? "on" : "off");
    m_settings.m_dcBlock = checked;
    m_settingsKeys.append("dcBlock");
    sendSettings();
}

void SDRPlayGui::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    m_settingsKeys.append("iqCorrection");
    sendSettings();
}

void SDRPlayGui::on_bandwidth_currentIndexChanged(int index)
{
    m_settings.m_bandwidthIndex = index;
    m_settingsKeys.append("bandwidthIndex");
    sendSettings();
}

void SDRPlayGui::on_samplerate_currentIndexChanged(int index)
{
    if (index < 0) {
        return;
    }

    m_settings.m_devSampleRateIndex = ui->samplerate->itemData(index).toUInt();
    m_settingsKeys.append("devSampleRateIndex");
    sendSettings();
}

void SDRPlayGui::on_ifFrequency_currentIndexChanged(int index)
{
    m_settings.m_ifFrequencyIndex = index;
    m_settingsKeys.append("ifFrequencyIndex");
    sendSettings();
}

void SDRPlayGui::on_decim_currentIndexChanged(int index)
{
    m_settings.m_log2Decim = index;
    m_settingsKeys.append("log2Decim");
    sendSettings();
}

void SDRPlayGui::on_fcPos_currentIndexChanged(int index)
{
    m_settings.m_fcPos = (SDRPlaySettings::fcPos_t) index;
    m_settingsKeys.append("fcPos");
    sendSettings();
}

void SDRPlayGui::on_gainTunerOn_toggled(bool checked)
{
    qDebug("SDRPlayGui::on_gainTunerOn_toggled: %s", checked ? "on" : "off");
    if (!checked) {
        return;
    }
    m_settings.m_tunerGainMode = true;
    m_settingsKeys.append("tunerGainMode");
    ui->gainTuner->setEnabled(true);
    ui->gainLNA->setEnabled(false);
    ui->gainMixer->setEnabled(false);
    ui->gainBaseband->setEnabled(false);
    m_amplifierBypass->setEnabled(false);

    sendSettings();
}

void SDRPlayGui::on_gainTuner_valueChanged(int value)
{
    int gain = value;
    QString gainText = QString::number(gain);
    ui->gainTunerText->setText(gainText);
    m_settings.m_tunerGain = gain;
    m_settingsKeys.append("tunerGain");

    sendSettings();
}

void SDRPlayGui::on_gainManualOn_toggled(bool checked)
{
    qDebug("SDRPlayGui::on_gainManualOn_toggled: %s", checked ? "on" : "off");
    if (!checked) {
        return;
    }
    m_settings.m_tunerGainMode = false;
    m_settingsKeys.append("tunerGainMode");
    ui->gainTuner->setEnabled(false);
    ui->gainLNA->setEnabled(true);
    ui->gainMixer->setEnabled(true);
    ui->gainBaseband->setEnabled(true);
    m_amplifierBypass->setEnabled(m_frontendCaps.amp_bypass_hz != 0);

    sendSettings();
}

void SDRPlayGui::on_gainLNA_toggled(bool checked)
{
	m_settings.m_lnaOn = checked ? 1 : 0;
    m_settingsKeys.append("lnaOn");
	sendSettings();
}

void SDRPlayGui::on_gainMixer_toggled(bool checked)
{
	m_settings.m_mixerAmpOn = checked ? 1 : 0;
    m_settingsKeys.append("mixerAmpOn");
	sendSettings();
}

void SDRPlayGui::on_gainBaseband_valueChanged(int value)
{
	m_settings.m_basebandGain = value;
    m_settingsKeys.append("basebandGain");

    QString gainText = QString::number(value);
    ui->gainBasebandText->setText(gainText);

	sendSettings();
}

void SDRPlayGui::on_tuner_currentIndexChanged(int index)
{
    if (!m_doApplySettings || index < 0) {
        return;
    }
    m_settings.m_tunerIndex = m_tuner->itemData(index).toUInt();
    m_settingsKeys.append("tunerIndex");
    sendSettings();
}

void SDRPlayGui::on_antenna_currentIndexChanged(int index)
{
    if (!m_doApplySettings || index < 0) {
        return;
    }
    m_settings.m_antenna = m_antenna->itemData(index).toInt();
    updateCenterFrequencyRange();
    m_settingsKeys.append("antenna");
    sendSettings();
}

void SDRPlayGui::on_attenuator_currentIndexChanged(int index)
{
    if (!m_doApplySettings || index < 0) {
        return;
    }
    m_settings.m_attenuator = m_attenuator->itemData(index).toInt();
    m_settingsKeys.append("attenuator");
    sendSettings();
}

void SDRPlayGui::on_directSampling_currentIndexChanged(int index)
{
    if (!m_doApplySettings || index < 0) {
        return;
    }
    m_settings.m_directSampling = m_directSampling->itemData(index).toInt();
    m_settingsKeys.append("directSampling");
    sendSettings();
}

void SDRPlayGui::on_biasT_toggled(bool checked)
{
    if (!m_doApplySettings) {
        return;
    }
    m_settings.m_biasT = checked;
    m_settingsKeys.append("biasT");
    sendSettings();
}

void SDRPlayGui::on_notch_toggled(bool checked)
{
    (void) checked;
    if (!m_doApplySettings) {
        return;
    }
    m_settings.m_notchMask =
        (m_notchFM->isChecked() ? MIRISDR_NOTCH_FM : 0) |
        (m_notchDAB->isChecked() ? MIRISDR_NOTCH_DAB : 0) |
        (m_notchMW->isChecked() ? MIRISDR_NOTCH_MW : 0);
    m_settingsKeys.append("notchMask");
    sendSettings();
}

void SDRPlayGui::on_amplifierBypass_toggled(bool checked)
{
    if (!m_doApplySettings) {
        return;
    }
    m_settings.m_amplifierBypass = checked;
    m_settingsKeys.append("amplifierBypass");
    sendSettings();
}

void SDRPlayGui::on_frontEndFlag_toggled(bool checked)
{
    (void) checked;
    if (!m_doApplySettings) {
        return;
    }
    m_settings.m_frontEndFlags =
        (m_portTie->isChecked() ? MIRISDR_FRONTEND_PORT_TIE : 0) |
        (m_referenceOutput->isChecked() ? MIRISDR_FRONTEND_REF_OUT : 0);
    m_settingsKeys.append("frontEndFlags");
    sendSettings();
}

void SDRPlayGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        SDRPlayInput::MsgStartStop *message = SDRPlayInput::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void SDRPlayGui::openDeviceSettingsDialog(const QPoint& p)
{
    if (m_contextMenuType == ContextMenuDeviceSettings)
    {
        BasicDeviceSettingsDialog dialog(this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setTitle(m_settings.m_title);
        dialog.setDefaultTitle(getDefaultTitle());

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_title = dialog.getTitle();
        setTitle(m_settings.m_title);
        getDeviceUISet()->m_mainSpectrumGUI->setTitle(m_settings.m_title);
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settingsKeys.append("useReverseAPI");
        m_settingsKeys.append("reverseAPIAddress");
        m_settingsKeys.append("reverseAPIPort");
        m_settingsKeys.append("reverseAPIDeviceIndex");
        m_settingsKeys.append("title");

        sendSettings();
    }

    resetContextMenuType();
}

void SDRPlayGui::makeUIConnections()
{
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &SDRPlayGui::on_centerFrequency_changed);
    QObject::connect(ui->ppm, &QSlider::valueChanged, this, &SDRPlayGui::on_ppm_valueChanged);
    QObject::connect(ui->dcOffset, &ButtonSwitch::toggled, this, &SDRPlayGui::on_dcOffset_toggled);
    QObject::connect(ui->iqImbalance, &ButtonSwitch::toggled, this, &SDRPlayGui::on_iqImbalance_toggled);
    QObject::connect(ui->bandwidth, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SDRPlayGui::on_bandwidth_currentIndexChanged);
    QObject::connect(ui->samplerate, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SDRPlayGui::on_samplerate_currentIndexChanged);
    QObject::connect(ui->ifFrequency, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SDRPlayGui::on_ifFrequency_currentIndexChanged);
    QObject::connect(ui->decim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SDRPlayGui::on_decim_currentIndexChanged);
    QObject::connect(ui->fcPos, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SDRPlayGui::on_fcPos_currentIndexChanged);
    QObject::connect(ui->gainTunerOn, &QRadioButton::toggled, this, &SDRPlayGui::on_gainTunerOn_toggled);
    QObject::connect(ui->gainTuner, &QDial::valueChanged, this, &SDRPlayGui::on_gainTuner_valueChanged);
    QObject::connect(ui->gainManualOn, &QRadioButton::toggled, this, &SDRPlayGui::on_gainManualOn_toggled);
    QObject::connect(ui->gainLNA, &ButtonSwitch::toggled, this, &SDRPlayGui::on_gainLNA_toggled);
    QObject::connect(ui->gainMixer, &ButtonSwitch::toggled, this, &SDRPlayGui::on_gainMixer_toggled);
    QObject::connect(ui->gainBaseband, &QDial::valueChanged, this, &SDRPlayGui::on_gainBaseband_valueChanged);
    QObject::connect(m_tuner, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SDRPlayGui::on_tuner_currentIndexChanged);
    QObject::connect(m_antenna, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SDRPlayGui::on_antenna_currentIndexChanged);
    QObject::connect(m_attenuator, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SDRPlayGui::on_attenuator_currentIndexChanged);
    QObject::connect(m_directSampling, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SDRPlayGui::on_directSampling_currentIndexChanged);
    QObject::connect(m_biasT, &QCheckBox::toggled, this, &SDRPlayGui::on_biasT_toggled);
    QObject::connect(m_notchFM, &QCheckBox::toggled, this, &SDRPlayGui::on_notch_toggled);
    QObject::connect(m_notchDAB, &QCheckBox::toggled, this, &SDRPlayGui::on_notch_toggled);
    QObject::connect(m_notchMW, &QCheckBox::toggled, this, &SDRPlayGui::on_notch_toggled);
    QObject::connect(m_amplifierBypass, &QCheckBox::toggled, this, &SDRPlayGui::on_amplifierBypass_toggled);
    QObject::connect(m_portTie, &QCheckBox::toggled, this, &SDRPlayGui::on_frontEndFlag_toggled);
    QObject::connect(m_referenceOutput, &QCheckBox::toggled, this, &SDRPlayGui::on_frontEndFlag_toggled);
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &SDRPlayGui::on_startStop_toggled);
}
