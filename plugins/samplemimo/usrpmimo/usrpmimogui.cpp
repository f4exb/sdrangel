///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <algorithm>

#include <QDebug>
#include <QString>

#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "gui/messagedialog.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "gui/dialogpositioner.h"
#include "mainspectrum/mainspectrumgui.h"
#include "dsp/dspcommands.h"
#include "usrp/deviceusrpshared.h"

#include "usrpmimo.h"
#include "ui_usrpmimogui.h"
#include "usrpmimogui.h"

USRPMIMOGUI::USRPMIMOGUI(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::USRPMIMOGUI),
    m_settings(),
    m_rxElseTx(true),
    m_streamIndex(0),
    m_spectrumRxElseTx(true),
    m_spectrumStreamIndex(0),
    m_gainLock(false),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_sampleMIMO(nullptr),
    m_rxBasebandSampleRate(3000000),
    m_txBasebandSampleRate(3000000),
    m_rxDeviceCenterFrequency(435000*1000),
    m_txDeviceCenterFrequency(435000*1000),
    m_lastRxEngineState(DeviceAPI::StNotStarted),
    m_lastTxEngineState(DeviceAPI::StNotStarted),
    m_sampleRateMode(true),
    m_statusCounter(0),
    m_deviceStatusCounter(10) // Get device info on first status update
{
    qDebug("USRPMIMOGUI::USRPMIMOGUI");
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    ui->setupUi(getContents());
    sizeToContents();
    getContents()->setStyleSheet("#USRPMIMOGUI { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplemimo/usrpmimo/readme.md";
    m_sampleMIMO = (USRPMIMO*) m_deviceUISet->m_deviceAPI->getSampleMIMO();

    for (int i = 0; i < 2; i++) {
        m_streamStatus[i] = StreamStatus{false, false, 0, 0};
    }

    // Limit stream selection to number of channels the device has
    updateStreamIndexItems(ui->streamIndex, m_rxElseTx);
    updateStreamIndexItems(ui->spectrumIndex, m_spectrumRxElseTx);

    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->bandwidth->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->loOffset->setColorMapper(ColorMapper(ColorMapper::GrayYellow));

    ui->clockSource->addItems(m_sampleMIMO->getClockSources());

    blockApplySettings(true);
    displaySettings();
    blockApplySettings(false);

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleMIMO->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

    sendSettings();
    makeUIConnections();
    m_resizer.enableChildMouseTracking();
}

USRPMIMOGUI::~USRPMIMOGUI()
{
    m_statusTimer.stop();
    m_updateTimer.stop();
    delete ui;
}

void USRPMIMOGUI::destroy()
{
    delete this;
}

void USRPMIMOGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    blockApplySettings(true);
    displaySettings();
    blockApplySettings(false);
    m_forceSettings = true;
    sendSettings();
}

QByteArray USRPMIMOGUI::serialize() const
{
    return m_settings.serialize();
}

bool USRPMIMOGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
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

// Should be called with settings blocked
void USRPMIMOGUI::displaySettings()
{
    float minF, maxF;

    setTitle(m_settings.m_title);
    getDeviceUISet()->m_mainSpectrumGUI->setTitle(m_settings.m_title);

    updateFrequencyLimits();

    if (m_rxElseTx)
    {
        ui->transverter->setDeltaFrequency(m_settings.m_rxTransverterDeltaFrequency);
        ui->transverter->setDeltaFrequencyActive(m_settings.m_rxTransverterMode);
        ui->centerFrequency->setValue(m_settings.m_rxCenterFrequency / 1000);
        ui->loOffset->setValue(m_settings.m_rxLOOffset / 1000);
        m_sampleMIMO->getRxLPRange(minF, maxF);
        ui->bandwidth->setValueRange(5, (minF / 1000) + 1, maxF / 1000);
        ui->bandwidth->setValue(m_settings.m_rxLpfBW / 1000);
        ui->dcOffset->setEnabled(true);
        ui->dcOffset->setChecked(m_settings.m_dcBlock);
        ui->iqImbalance->setEnabled(true);
        ui->iqImbalance->setChecked(m_settings.m_iqCorrection);
        ui->decim->setCurrentIndex(m_settings.m_log2SoftDecim);
        ui->label_decim->setText(QString("Dec"));
        ui->decim->setToolTip(QString("Software decimation factor"));
    }
    else
    {
        ui->transverter->setDeltaFrequency(m_settings.m_txTransverterDeltaFrequency);
        ui->transverter->setDeltaFrequencyActive(m_settings.m_txTransverterMode);
        ui->centerFrequency->setValue(m_settings.m_txCenterFrequency / 1000);
        ui->loOffset->setValue(m_settings.m_txLOOffset / 1000);
        m_sampleMIMO->getTxLPRange(minF, maxF);
        ui->bandwidth->setValueRange(5, (minF / 1000) + 1, maxF / 1000);
        ui->bandwidth->setValue(m_settings.m_txLpfBW / 1000);
        ui->dcOffset->setEnabled(false);
        ui->iqImbalance->setEnabled(false);
        ui->decim->setCurrentIndex(m_settings.m_log2SoftInterp);
        ui->label_decim->setText(QString("Int"));
        ui->decim->setToolTip(QString("Software interpolation factor"));
    }

    ui->clockSource->setCurrentIndex(ui->clockSource->findText(m_settings.m_clockSource));

    displaySampleRate();
    displayMasterClockRate();
    displayGain();
    displayAntennas();
    displayStreamStatus();
}

void USRPMIMOGUI::displaySampleRate()
{
    float minF, maxF;
    m_sampleMIMO->getSRRange(minF, maxF);
    quint32 log2Factor = m_rxElseTx ? m_settings.m_log2SoftDecim : m_settings.m_log2SoftInterp;

    ui->sampleRate->blockSignals(true);

    if (m_sampleRateMode)
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(60,60,60); }");
        ui->sampleRateMode->setText("SR");
        ui->sampleRate->setValueRange(8, (uint32_t) minF, (uint32_t) maxF);
        ui->sampleRate->setValue(m_settings.m_devSampleRate);
        ui->sampleRate->setToolTip("Device to host sample rate (S/s)");
        ui->deviceRateText->setToolTip("Baseband sample rate (S/s)");
        uint32_t basebandSampleRate = m_settings.m_devSampleRate / (1<<log2Factor);
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(basebandSampleRate / 1000.0f, 'g', 5)));
    }
    else
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(50,50,50); }");
        ui->sampleRateMode->setText("BB");
        ui->sampleRate->setValueRange(8, (uint32_t) minF / (1<<log2Factor), (uint32_t) maxF / (1<<log2Factor));
        ui->sampleRate->setValue(m_settings.m_devSampleRate / (1<<log2Factor));
        ui->sampleRate->setToolTip("Baseband sample rate (S/s)");
        ui->deviceRateText->setToolTip("Device to host sample rate (S/s)");
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(m_settings.m_devSampleRate / 1000.0f, 'g', 5)));
    }

    ui->sampleRate->blockSignals(false);

    // LO offset shouldn't be greater than half the sample rate
    int32_t maxLOOffset = m_settings.m_devSampleRate / 2 / 1000;
    ui->loOffset->setValueRange(false, 5, -maxLOOffset, maxLOOffset);
}

void USRPMIMOGUI::displayMasterClockRate()
{
    int cr = m_settings.m_masterClockRate;

    if (cr < 0) {
        ui->masterClockRateLabel->setText("-");
    } else if (cr < 100000000) {
        ui->masterClockRateLabel->setText(tr("%1k").arg(QString::number(cr / 1000.0f, 'g', 5)));
    } else {
        ui->masterClockRateLabel->setText(tr("%1M").arg(QString::number(cr / 1000000.0f, 'g', 5)));
    }
}

void USRPMIMOGUI::displayGain()
{
    float minF, maxF;
    int gain;

    if (m_rxElseTx)
    {
        m_sampleMIMO->getRxGainRange(minF, maxF);
        USRPMIMOSettings::GainMode gainMode = m_streamIndex == 0 ? m_settings.m_rx0GainMode : m_settings.m_rx1GainMode;
        gain = m_streamIndex == 0 ? m_settings.m_rx0Gain : m_settings.m_rx1Gain;
        ui->gainMode->setEnabled(true);
        ui->gainMode->setCurrentIndex((int) gainMode);
        ui->gain->setEnabled(gainMode == USRPMIMOSettings::GAIN_MANUAL);
    }
    else
    {
        m_sampleMIMO->getTxGainRange(minF, maxF);
        gain = m_streamIndex == 0 ? m_settings.m_tx0Gain : m_settings.m_tx1Gain;
        ui->gainMode->setCurrentIndex((int) USRPMIMOSettings::GAIN_MANUAL);
        ui->gainMode->setEnabled(false);
        ui->gain->setEnabled(true);
    }

    ui->gain->setRange((int) minF, (int) maxF);
    ui->gain->setValue(gain);
    ui->gainText->setText(tr("%1 dB").arg(gain));
}

void USRPMIMOGUI::displayAntennas()
{
    QStringList antennas;
    QString antenna;

    if (m_rxElseTx)
    {
        antennas = m_sampleMIMO->getRxAntennas();
        antenna = m_streamIndex == 0 ? m_settings.m_rx0AntennaPath : m_settings.m_rx1AntennaPath;
    }
    else
    {
        antennas = m_sampleMIMO->getTxAntennas();
        antenna = m_streamIndex == 0 ? m_settings.m_tx0AntennaPath : m_settings.m_tx1AntennaPath;
    }

    ui->antenna->blockSignals(true);
    ui->antenna->clear();
    ui->antenna->addItems(antennas);
    ui->antenna->setCurrentIndex(ui->antenna->findText(antenna));
    ui->antenna->blockSignals(false);
}

void USRPMIMOGUI::displayStreamStatus()
{
    int side = m_rxElseTx ? 0 : 1;
    const StreamStatus& status = m_streamStatus[side];

    if (status.m_success)
    {
        if (status.m_active) {
            ui->streamStatusLabel->setStyleSheet("QLabel { background-color : green; }");
        } else {
            ui->streamStatusLabel->setStyleSheet("QLabel { background-color : blue; }");
        }

        if (m_overUnderRunStats[side].getCount() > 0) {
            ui->overrunLabel->setStyleSheet("QLabel { background-color : red; }");
        } else {
            ui->overrunLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
        }

        if (m_timeoutDroppedStats[side].getCount() > 0) {
            ui->timeoutLabel->setStyleSheet("QLabel { background-color : red; }");
        } else {
            ui->timeoutLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
        }
    }
    else
    {
        ui->streamStatusLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
        ui->overrunLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
        ui->timeoutLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    }

    ui->overrunLabel->setText(m_rxElseTx ? "O" : "U");
    ui->timeoutLabel->setText(m_rxElseTx ? "T" : "D");

    if (m_rxElseTx)
    {
        ui->overrunLabel->setToolTip(m_overUnderRunStats[side].getToolTip("Red if Rx overruns occurred. Cleared when restarting Rx", "Overruns"));
        ui->timeoutLabel->setToolTip(m_timeoutDroppedStats[side].getToolTip("Red if Rx timeouts occurred. Cleared when restarting Rx", "Timeouts"));
    }
    else
    {
        ui->overrunLabel->setToolTip(m_overUnderRunStats[side].getToolTip("Red if Tx underruns occurred. Cleared when restarting Tx", "Underruns"));
        ui->timeoutLabel->setToolTip(m_timeoutDroppedStats[side].getToolTip("Red if Tx packets were dropped. Cleared when restarting Tx", "Dropped packets"));
    }
}

bool USRPMIMOGUI::handleMessage(const Message& message)
{
    if (DSPMIMOSignalNotification::match(message))
    {
        const DSPMIMOSignalNotification& notif = (const DSPMIMOSignalNotification&) message;
        int istream = notif.getIndex();
        bool sourceOrSink = notif.getSourceOrSink();

        if (sourceOrSink)
        {
            m_rxBasebandSampleRate = notif.getSampleRate();
            m_rxDeviceCenterFrequency = notif.getCenterFrequency();
        }
        else
        {
            m_txBasebandSampleRate = notif.getSampleRate();
            m_txDeviceCenterFrequency = notif.getCenterFrequency();
        }

        qDebug("USRPMIMOGUI::handleMessage: DSPMIMOSignalNotification: %s stream: %d SampleRate:%d, CenterFrequency:%llu",
                sourceOrSink ? "source" : "sink",
                istream,
                notif.getSampleRate(),
                notif.getCenterFrequency());

        updateSampleRateAndFrequency();

        return true;
    }
    else if (USRPMIMO::MsgConfigureUSRPMIMO::match(message))
    {
        const USRPMIMO::MsgConfigureUSRPMIMO& notif = (const USRPMIMO::MsgConfigureUSRPMIMO&) message;

        if (notif.getForce()) {
            m_settings = notif.getSettings();
        } else {
            // Don't overwrite edits that haven't been sent yet
            USRPMIMOSettings pending = m_settings;
            m_settings.applySettings(notif.getSettingsKeys(), notif.getSettings());
            m_settings.applySettings(m_settingsKeys, pending);
        }

        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (USRPMIMO::MsgStartStop::match(message))
    {
        const USRPMIMO::MsgStartStop& notif = (const USRPMIMO::MsgStartStop&) message;
        blockApplySettings(true);
        (notif.getRxElseTx() ? ui->startStopRx : ui->startStopTx)->setChecked(notif.getStartStop());
        blockApplySettings(false);

        return true;
    }
    else if (USRPMIMO::MsgReportStreamInfo::match(message))
    {
        const USRPMIMO::MsgReportStreamInfo& report = (const USRPMIMO::MsgReportStreamInfo&) message;
        int side = report.getRxElseTx() ? 0 : 1;
        m_streamStatus[side] = StreamStatus{
            report.getSuccess(),
            report.getActive(),
            report.getOverUnderRuns(),
            report.getTimeoutsDropped()
        };
        // Accumulate statistics, as the counts reported can be reset while running (E.g. when paused while the other side is started)
        m_overUnderRunStats[side].update(report.getSuccess(), report.getOverUnderRuns());
        m_timeoutDroppedStats[side].update(report.getSuccess(), report.getTimeoutsDropped());
        displayStreamStatus();

        return true;
    }
    else if (DeviceUSRPShared::MsgReportDeviceInfo::match(message))
    {
        const DeviceUSRPShared::MsgReportDeviceInfo& report = (const DeviceUSRPShared::MsgReportDeviceInfo&) message;

        if (report.getTemperatureValid()) {
            ui->temperatureText->setText(tr("%1C").arg(QString::number(report.getTemperature(), 'f', 0)));
        }
        // Hide if device doesn't have a temperature sensor
        ui->temperatureText->setVisible(report.getTemperatureValid());

        return true;
    }

    return false;
}

void USRPMIMOGUI::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()))
    {
        if (!handleMessage(*message)) {
            qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
        }
        delete message;
    }
}

void USRPMIMOGUI::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void USRPMIMOGUI::updateHardware()
{
    if (m_doApplySettings)
    {
        USRPMIMO::MsgConfigureUSRPMIMO* message = USRPMIMO::MsgConfigureUSRPMIMO::create(m_settings, m_settingsKeys, m_forceSettings);
        m_sampleMIMO->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void USRPMIMOGUI::updateSampleRateAndFrequency()
{
    if (m_spectrumRxElseTx)
    {
        m_deviceUISet->getSpectrum()->setSampleRate(m_rxBasebandSampleRate);
        m_deviceUISet->getSpectrum()->setCenterFrequency(m_rxDeviceCenterFrequency);
    }
    else
    {
        m_deviceUISet->getSpectrum()->setSampleRate(m_txBasebandSampleRate);
        m_deviceUISet->getSpectrum()->setCenterFrequency(m_txDeviceCenterFrequency);
    }
}

// Set stream index combo box items to number of channels for Rx or Tx side
void USRPMIMOGUI::updateStreamIndexItems(QComboBox *comboBox, bool rxElseTx)
{
    int count = std::max(1, rxElseTx ? m_sampleMIMO->getNbRxChannels() : m_sampleMIMO->getNbTxChannels());
    int index = std::max(0, comboBox->currentIndex());

    comboBox->blockSignals(true);
    comboBox->clear();

    for (int i = 0; i < count; i++) {
        comboBox->addItem(QString::number(i));
    }

    comboBox->setCurrentIndex(std::min(index, count - 1));
    comboBox->blockSignals(false);
}

void USRPMIMOGUI::on_streamSide_currentIndexChanged(int index)
{
    m_rxElseTx = index == 0;
    updateStreamIndexItems(ui->streamIndex, m_rxElseTx);
    m_streamIndex = ui->streamIndex->currentIndex();
    blockApplySettings(true);
    displaySettings();
    blockApplySettings(false);
}

void USRPMIMOGUI::on_streamIndex_currentIndexChanged(int index)
{
    m_streamIndex = index < 0 ? 0 : index > 1 ? 1 : index;
    blockApplySettings(true);
    displaySettings();
    blockApplySettings(false);
}

void USRPMIMOGUI::on_spectrumSide_currentIndexChanged(int index)
{
    m_spectrumRxElseTx = (index == 0);
    updateStreamIndexItems(ui->spectrumIndex, m_spectrumRxElseTx);
    m_spectrumStreamIndex = ui->spectrumIndex->currentIndex();
    m_deviceUISet->m_spectrum->setDisplayedStream(m_spectrumRxElseTx, m_spectrumStreamIndex);
    m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(m_spectrumRxElseTx, m_spectrumStreamIndex);
    m_deviceUISet->setSpectrumScalingFactor(m_spectrumRxElseTx ? SDR_RX_SCALEF : SDR_TX_SCALEF);
    updateSampleRateAndFrequency();
}

void USRPMIMOGUI::on_spectrumIndex_currentIndexChanged(int index)
{
    m_spectrumStreamIndex = index < 0 ? 0 : index > 1 ? 1 : index;
    m_deviceUISet->m_spectrum->setDisplayedStream(m_spectrumRxElseTx, m_spectrumStreamIndex);
    m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(m_spectrumRxElseTx, m_spectrumStreamIndex);
    updateSampleRateAndFrequency();
}

void USRPMIMOGUI::on_startStopRx_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        USRPMIMO::MsgStartStop *message = USRPMIMO::MsgStartStop::create(checked, true);
        m_sampleMIMO->getInputMessageQueue()->push(message);
    }
}

void USRPMIMOGUI::on_startStopTx_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        USRPMIMO::MsgStartStop *message = USRPMIMO::MsgStartStop::create(checked, false);
        m_sampleMIMO->getInputMessageQueue()->push(message);
    }
}

void USRPMIMOGUI::on_centerFrequency_changed(quint64 value)
{
    if (!m_doApplySettings) {
        return;
    }

    setCenterFrequencySetting(value);
    sendSettings();
}

void USRPMIMOGUI::on_loOffset_changed(qint64 value)
{
    if (!m_doApplySettings) {
        return;
    }

    if (m_rxElseTx)
    {
        m_settings.m_rxLOOffset = value * 1000;
        m_settingsKeys.append("rxLOOffset");
    }
    else
    {
        m_settings.m_txLOOffset = value * 1000;
        m_settingsKeys.append("txLOOffset");
    }

    sendSettings();
}

void USRPMIMOGUI::on_clockSource_currentIndexChanged(int index)
{
    if (!m_doApplySettings || (index < 0)) {
        return;
    }

    m_settings.m_clockSource = ui->clockSource->currentText();
    m_settingsKeys.append("clockSource");
    sendSettings();
}

void USRPMIMOGUI::on_dcOffset_toggled(bool checked)
{
    if (!m_doApplySettings) {
        return;
    }

    m_settings.m_dcBlock = checked;
    m_settingsKeys.append("dcBlock");
    sendSettings();
}

void USRPMIMOGUI::on_iqImbalance_toggled(bool checked)
{
    if (!m_doApplySettings) {
        return;
    }

    m_settings.m_iqCorrection = checked;
    m_settingsKeys.append("iqCorrection");
    sendSettings();
}

void USRPMIMOGUI::on_bandwidth_changed(quint64 value)
{
    if (!m_doApplySettings) {
        return;
    }

    if (m_rxElseTx)
    {
        m_settings.m_rxLpfBW = value * 1000;
        m_settingsKeys.append("rxLpfBW");
    }
    else
    {
        m_settings.m_txLpfBW = value * 1000;
        m_settingsKeys.append("txLpfBW");
    }

    sendSettings();
}

void USRPMIMOGUI::on_transverter_clicked()
{
    if (!m_doApplySettings) {
        return;
    }

    if (m_rxElseTx)
    {
        m_settings.m_rxTransverterMode = ui->transverter->getDeltaFrequencyAcive();
        m_settings.m_rxTransverterDeltaFrequency = ui->transverter->getDeltaFrequency();
        m_settingsKeys.append("rxTransverterMode");
        m_settingsKeys.append("rxTransverterDeltaFrequency");
        qDebug("USRPMIMOGUI::on_transverter_clicked: Rx: %lld Hz %s", m_settings.m_rxTransverterDeltaFrequency, m_settings.m_rxTransverterMode ? "on" : "off");
    }
    else
    {
        m_settings.m_txTransverterMode = ui->transverter->getDeltaFrequencyAcive();
        m_settings.m_txTransverterDeltaFrequency = ui->transverter->getDeltaFrequency();
        m_settingsKeys.append("txTransverterMode");
        m_settingsKeys.append("txTransverterDeltaFrequency");
        qDebug("USRPMIMOGUI::on_transverter_clicked: Tx: %lld Hz %s", m_settings.m_txTransverterDeltaFrequency, m_settings.m_txTransverterMode ? "on" : "off");
    }

    updateFrequencyLimits();
    setCenterFrequencySetting(ui->centerFrequency->getValueNew());
    sendSettings();
}

void USRPMIMOGUI::on_sampleRateMode_toggled(bool checked)
{
    m_sampleRateMode = checked;
    displaySampleRate();
}

void USRPMIMOGUI::on_sampleRate_changed(quint64 value)
{
    if (!m_doApplySettings) {
        return;
    }

    if (m_sampleRateMode) {
        m_settings.m_devSampleRate = value;
    } else {
        m_settings.m_devSampleRate = value * (1 << (m_rxElseTx ? m_settings.m_log2SoftDecim : m_settings.m_log2SoftInterp));
    }

    displaySampleRate();
    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void USRPMIMOGUI::on_decim_currentIndexChanged(int index)
{
    if (!m_doApplySettings || (index < 0) || (index > 6)) {
        return;
    }

    if (m_rxElseTx)
    {
        m_settings.m_log2SoftDecim = index;
        m_settingsKeys.append("log2SoftDecim");
    }
    else
    {
        m_settings.m_log2SoftInterp = index;
        m_settingsKeys.append("log2SoftInterp");
    }

    displaySampleRate();

    if (m_sampleRateMode) {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew();
    } else {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew() * (1 << index);
    }

    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void USRPMIMOGUI::on_gainLock_toggled(bool checked)
{
    if (!m_gainLock && checked)
    {
        m_settings.m_rx1Gain = m_settings.m_rx0Gain;
        m_settings.m_rx1GainMode = m_settings.m_rx0GainMode;
        m_settings.m_tx1Gain = m_settings.m_tx0Gain;
        m_settingsKeys.append("rx1Gain");
        m_settingsKeys.append("rx1GainMode");
        m_settingsKeys.append("tx1Gain");
        sendSettings();
    }

    m_gainLock = checked;
}

void USRPMIMOGUI::on_gainMode_currentIndexChanged(int index)
{
    if (!m_doApplySettings || !m_rxElseTx || (index < 0)) { // not for Tx
        return;
    }

    USRPMIMOSettings::GainMode gainMode = index == 0 ? USRPMIMOSettings::GAIN_AUTO : USRPMIMOSettings::GAIN_MANUAL;

    if ((m_streamIndex == 0) || m_gainLock)
    {
        m_settings.m_rx0GainMode = gainMode;
        m_settingsKeys.append("rx0GainMode");
    }
    if ((m_streamIndex == 1) || m_gainLock)
    {
        m_settings.m_rx1GainMode = gainMode;
        m_settingsKeys.append("rx1GainMode");
    }

    ui->gain->setEnabled(gainMode == USRPMIMOSettings::GAIN_MANUAL);
    sendSettings();
}

void USRPMIMOGUI::on_gain_valueChanged(int value)
{
    ui->gainText->setText(tr("%1 dB").arg(value));

    if (!m_doApplySettings) {
        return;
    }

    setGain(value);
    sendSettings();
}

void USRPMIMOGUI::setGain(int gain)
{
    if (m_rxElseTx)
    {
        if ((m_streamIndex == 0) || m_gainLock)
        {
            m_settings.m_rx0Gain = gain;
            m_settingsKeys.append("rx0Gain");
        }
        if ((m_streamIndex == 1) || m_gainLock)
        {
            m_settings.m_rx1Gain = gain;
            m_settingsKeys.append("rx1Gain");
        }
    }
    else
    {
        if ((m_streamIndex == 0) || m_gainLock)
        {
            m_settings.m_tx0Gain = gain;
            m_settingsKeys.append("tx0Gain");
        }
        if ((m_streamIndex == 1) || m_gainLock)
        {
            m_settings.m_tx1Gain = gain;
            m_settingsKeys.append("tx1Gain");
        }
    }
}

void USRPMIMOGUI::on_antenna_currentIndexChanged(int index)
{
    if (!m_doApplySettings || (index < 0)) {
        return;
    }

    QString antenna = ui->antenna->currentText();

    if (m_rxElseTx)
    {
        if (m_streamIndex == 0)
        {
            m_settings.m_rx0AntennaPath = antenna;
            m_settingsKeys.append("rx0AntennaPath");
        }
        else
        {
            m_settings.m_rx1AntennaPath = antenna;
            m_settingsKeys.append("rx1AntennaPath");
        }
    }
    else
    {
        if (m_streamIndex == 0)
        {
            m_settings.m_tx0AntennaPath = antenna;
            m_settingsKeys.append("tx0AntennaPath");
        }
        else
        {
            m_settings.m_tx1AntennaPath = antenna;
            m_settingsKeys.append("tx1AntennaPath");
        }
    }

    sendSettings();
}

void USRPMIMOGUI::updateFrequencyLimits()
{
    // values in kHz
    float minF, maxF;
    qint64 deltaFrequency;

    if (m_rxElseTx)
    {
        deltaFrequency = m_settings.m_rxTransverterMode ? m_settings.m_rxTransverterDeltaFrequency/1000 : 0;
        m_sampleMIMO->getRxLORange(minF, maxF);
    }
    else
    {
        deltaFrequency = m_settings.m_txTransverterMode ? m_settings.m_txTransverterDeltaFrequency/1000 : 0;
        m_sampleMIMO->getTxLORange(minF, maxF);
    }

    qint64 minLimit = minF/1000 + deltaFrequency;
    qint64 maxLimit = maxF/1000 + deltaFrequency;

    if (m_settings.m_rxTransverterMode || m_settings.m_txTransverterMode)
    {
        minLimit = minLimit < 0 ? 0 : minLimit > 999999999 ? 999999999 : minLimit;
        maxLimit = maxLimit < 0 ? 0 : maxLimit > 999999999 ? 999999999 : maxLimit;
        ui->centerFrequency->setValueRange(9, minLimit, maxLimit);
    }
    else
    {
        minLimit = minLimit < 0 ? 0 : minLimit > 9999999 ? 9999999 : minLimit;
        maxLimit = maxLimit < 0 ? 0 : maxLimit > 9999999 ? 9999999 : maxLimit;
        ui->centerFrequency->setValueRange(7, minLimit, maxLimit);
    }
}

void USRPMIMOGUI::setCenterFrequencySetting(uint64_t kHzValue)
{
    int64_t centerFrequency = kHzValue*1000;

    if (m_rxElseTx)
    {
        m_settings.m_rxCenterFrequency = centerFrequency < 0 ? 0 : (uint64_t) centerFrequency;
        m_settingsKeys.append("rxCenterFrequency");
    }
    else
    {
        m_settings.m_txCenterFrequency = centerFrequency < 0 ? 0 : (uint64_t) centerFrequency;
        m_settingsKeys.append("txCenterFrequency");
    }
}

void USRPMIMOGUI::updateStatus()
{
    int stateRx = m_deviceUISet->m_deviceAPI->state(0);
    int stateTx = m_deviceUISet->m_deviceAPI->state(1);

    if (m_lastRxEngineState != stateRx)
    {
        switch(stateRx)
        {
            case DeviceAPI::StNotStarted:
                ui->startStopRx->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DeviceAPI::StIdle:
                ui->startStopRx->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DeviceAPI::StRunning:
                ui->startStopRx->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DeviceAPI::StError:
                ui->startStopRx->setStyleSheet("QToolButton { background-color : red; }");
                MessageDialog::information(this, tr("Message"), m_deviceUISet->m_deviceAPI->errorMessage(0));
                break;
            default:
                break;
        }

        m_lastRxEngineState = stateRx;
    }

    if (m_lastTxEngineState != stateTx)
    {
        switch(stateTx)
        {
            case DeviceAPI::StNotStarted:
                ui->startStopTx->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DeviceAPI::StIdle:
                ui->startStopTx->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DeviceAPI::StRunning:
                ui->startStopTx->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DeviceAPI::StError:
                ui->startStopTx->setStyleSheet("QToolButton { background-color : red; }");
                MessageDialog::information(this, tr("Message"), m_deviceUISet->m_deviceAPI->errorMessage(1));
                break;
            default:
                break;
        }

        m_lastTxEngineState = stateTx;
    }

    if (m_statusCounter < 1)
    {
        m_statusCounter++;
    }
    else
    {
        m_sampleMIMO->getInputMessageQueue()->push(USRPMIMO::MsgGetStreamInfo::create());
        m_statusCounter = 0;
    }

    if (m_deviceStatusCounter < 10)
    {
        m_deviceStatusCounter++;
    }
    else
    {
        m_sampleMIMO->getInputMessageQueue()->push(USRPMIMO::MsgGetDeviceInfo::create());
        m_deviceStatusCounter = 0;
    }
}

void USRPMIMOGUI::openDeviceSettingsDialog(const QPoint& p)
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

        if (dialog.result() == QDialog::Accepted)
        {
            m_settings.m_title = dialog.getTitle();
            setTitle(m_settings.m_title);
            getDeviceUISet()->m_mainSpectrumGUI->setTitle(m_settings.m_title);
            m_settings.m_useReverseAPI = dialog.useReverseAPI();
            m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
            m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
            m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
            m_settingsKeys.append("title");
            m_settingsKeys.append("useReverseAPI");
            m_settingsKeys.append("reverseAPIAddress");
            m_settingsKeys.append("reverseAPIPort");
            m_settingsKeys.append("reverseAPIDeviceIndex");

            sendSettings();
        }
    }

    resetContextMenuType();
}

void USRPMIMOGUI::makeUIConnections()
{
    QObject::connect(ui->streamSide, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &USRPMIMOGUI::on_streamSide_currentIndexChanged);
    QObject::connect(ui->streamIndex, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &USRPMIMOGUI::on_streamIndex_currentIndexChanged);
    QObject::connect(ui->spectrumSide, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &USRPMIMOGUI::on_spectrumSide_currentIndexChanged);
    QObject::connect(ui->spectrumIndex, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &USRPMIMOGUI::on_spectrumIndex_currentIndexChanged);
    QObject::connect(ui->startStopRx, &ButtonSwitch::toggled, this, &USRPMIMOGUI::on_startStopRx_toggled);
    QObject::connect(ui->startStopTx, &ButtonSwitch::toggled, this, &USRPMIMOGUI::on_startStopTx_toggled);
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &USRPMIMOGUI::on_centerFrequency_changed);
    QObject::connect(ui->loOffset, &ValueDialZ::changed, this, &USRPMIMOGUI::on_loOffset_changed);
    QObject::connect(ui->clockSource, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &USRPMIMOGUI::on_clockSource_currentIndexChanged);
    QObject::connect(ui->dcOffset, &ButtonSwitch::toggled, this, &USRPMIMOGUI::on_dcOffset_toggled);
    QObject::connect(ui->iqImbalance, &ButtonSwitch::toggled, this, &USRPMIMOGUI::on_iqImbalance_toggled);
    QObject::connect(ui->bandwidth, &ValueDial::changed, this, &USRPMIMOGUI::on_bandwidth_changed);
    QObject::connect(ui->transverter, &TransverterButton::clicked, this, &USRPMIMOGUI::on_transverter_clicked);
    QObject::connect(ui->sampleRateMode, &QToolButton::toggled, this, &USRPMIMOGUI::on_sampleRateMode_toggled);
    QObject::connect(ui->sampleRate, &ValueDial::changed, this, &USRPMIMOGUI::on_sampleRate_changed);
    QObject::connect(ui->decim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &USRPMIMOGUI::on_decim_currentIndexChanged);
    QObject::connect(ui->gainLock, &QToolButton::toggled, this, &USRPMIMOGUI::on_gainLock_toggled);
    QObject::connect(ui->gainMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &USRPMIMOGUI::on_gainMode_currentIndexChanged);
    QObject::connect(ui->gain, &QSlider::valueChanged, this, &USRPMIMOGUI::on_gain_valueChanged);
    QObject::connect(ui->antenna, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &USRPMIMOGUI::on_antenna_currentIndexChanged);
}
