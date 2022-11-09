///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include "ui_limesdroutputgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "limesdroutputgui.h"

LimeSDROutputGUI::LimeSDROutputGUI(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::LimeSDROutputGUI),
    m_settings(),
    m_sampleRateMode(true),
    m_sampleRate(0),
    m_lastEngineState(DeviceAPI::StNotStarted),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_statusCounter(0),
    m_deviceStatusCounter(0)
{
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_limeSDROutput = (LimeSDROutput*) m_deviceUISet->m_deviceAPI->getSampleSink();

    ui->setupUi(getContents());
    sizeToContents();
    getContents()->setStyleSheet("#LimeSDROutputGUI { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesink/limesdroutput/readme.md";

    float minF, maxF;

    m_limeSDROutput->getLORange(minF, maxF);
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(7, ((uint32_t) minF)/1000, ((uint32_t) maxF)/1000); // frequency dial is in kHz

    m_limeSDROutput->getSRRange(minF, maxF);
    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, (uint32_t) minF, (uint32_t) maxF);

    m_limeSDROutput->getLPRange(minF, maxF);
    ui->lpf->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpf->setValueRange(6, (minF/1000)+1, maxF/1000);

    ui->lpFIR->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpFIR->setValueRange(5, 1U, 56000U);

    ui->ncoFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->channelNumberText->setText(tr("#%1").arg(m_limeSDROutput->getChannelIndex()));

    if (m_limeSDROutput->getLimeType() == DeviceLimeSDRParams::LimeMini)
    {
        ui->antenna->setItemText(1, "Hi");
        ui->antenna->setItemText(2, "Lo");
        ui->antenna->setToolTip("Hi: 2 - 3.5 GHz, Lo: 10 MHz - 2 GHz");
    }
    else
    {
        ui->antenna->setItemText(1, "Lo");
        ui->antenna->setItemText(2, "Hi");
        ui->antenna->setToolTip("Lo: L port, Hi: H port. All ports are full band");
    }

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    displaySettings();

    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_deviceUISet->m_deviceAPI->getDeviceUID());

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

    sendSettings();
    makeUIConnections();
}

LimeSDROutputGUI::~LimeSDROutputGUI()
{
    m_statusTimer.stop();
    m_updateTimer.stop();
    delete ui;
}

void LimeSDROutputGUI::destroy()
{
    delete this;
}

void LimeSDROutputGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

QByteArray LimeSDROutputGUI::serialize() const
{
    return m_settings.serialize();
}

bool LimeSDROutputGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
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

void LimeSDROutputGUI::updateFrequencyLimits()
{
    // values in kHz
    float minF, maxF;
    qint64 deltaFrequency = m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency/1000 : 0;
    m_limeSDROutput->getLORange(minF, maxF);
    qint64 minLimit = minF/1000 + deltaFrequency;
    qint64 maxLimit = maxF/1000 + deltaFrequency;
    // Min freq is 30MHz - NCO must be used to go below this
    qint64 minFreq = m_settings.m_ncoEnable ? 30000 + m_settings.m_ncoFrequency/1000 : 30000;

    if (m_settings.m_transverterMode)
    {
        minLimit = minLimit < minFreq ? minFreq : minLimit > 999999999 ? 999999999 : minLimit;
        maxLimit = maxLimit < 0 ? 0 : maxLimit > 999999999 ? 999999999 : maxLimit;
        ui->centerFrequency->setValueRange(9, minLimit, maxLimit);
    }
    else
    {
        minLimit = minLimit < minFreq ? minFreq : minLimit > 9999999 ? 9999999 : minLimit;
        maxLimit = maxLimit < 0 ? 0 : maxLimit > 9999999 ? 9999999 : maxLimit;
        ui->centerFrequency->setValueRange(7, minLimit, maxLimit);
    }
    qDebug("LimeSDROutputGUI::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);
}

bool LimeSDROutputGUI::handleMessage(const Message& message)
{
    if (LimeSDROutput::MsgConfigureLimeSDR::match(message))
    {
        const LimeSDROutput::MsgConfigureLimeSDR& cfg = (LimeSDROutput::MsgConfigureLimeSDR&) message;

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
    else if (DeviceLimeSDRShared::MsgReportBuddyChange::match(message))
    {
        DeviceLimeSDRShared::MsgReportBuddyChange& report = (DeviceLimeSDRShared::MsgReportBuddyChange&) message;
        m_settings.m_devSampleRate = report.getDevSampleRate();
        m_settings.m_log2HardInterp = report.getLog2HardDecimInterp();

        if (!report.getRxElseTx()) {
            m_settings.m_centerFrequency = report.getCenterFrequency();
        }

        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (DeviceLimeSDRShared::MsgReportClockSourceChange::match(message))
    {
        DeviceLimeSDRShared::MsgReportClockSourceChange& report = (DeviceLimeSDRShared::MsgReportClockSourceChange&) message;
        m_settings.m_extClockFreq = report.getExtClockFeq();
        m_settings.m_extClock = report.getExtClock();

        blockApplySettings(true);
        ui->extClock->setExternalClockFrequency(m_settings.m_extClockFreq);
        ui->extClock->setExternalClockActive(m_settings.m_extClock);
        blockApplySettings(false);

        return true;
    }
    else if (LimeSDROutput::MsgCalibrationResult::match(message))
    {
        LimeSDROutput::MsgCalibrationResult& report = (LimeSDROutput::MsgCalibrationResult&) message;

        if (report.getSuccess()) {
            ui->calibrationLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
        } else {
            ui->calibrationLabel->setStyleSheet("QLabel { background-color : red; }");
        }

        return true;
    }
    else if (LimeSDROutput::MsgReportStreamInfo::match(message))
    {
        LimeSDROutput::MsgReportStreamInfo& report = (LimeSDROutput::MsgReportStreamInfo&) message;

        if (report.getSuccess())
        {
            if (report.getActive()) {
                ui->streamStatusLabel->setStyleSheet("QLabel { background-color : green; }");
            } else {
                ui->streamStatusLabel->setStyleSheet("QLabel { background-color : blue; }");
            }

            ui->streamLinkRateText->setText(tr("%1 MB/s").arg(QString::number(report.getLinkRate() / 1000000.0f, 'f', 3)));

            if (report.getUnderrun() > 0) {
                ui->underrunLabel->setStyleSheet("QLabel { background-color : red; }");
            } else {
                ui->underrunLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
            }

            if (report.getOverrun() > 0) {
                ui->overrunLabel->setStyleSheet("QLabel { background-color : red; }");
            } else {
                ui->overrunLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
            }

            if (report.getDroppedPackets() > 0) {
                ui->droppedLabel->setStyleSheet("QLabel { background-color : red; }");
            } else {
                ui->droppedLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
            }

            ui->fifoBar->setMaximum(report.getFifoSize());
            ui->fifoBar->setValue(report.getFifoFilledCount());
            ui->fifoBar->setToolTip(tr("FIFO fill %1/%2 samples").arg(QString::number(report.getFifoFilledCount())).arg(QString::number(report.getFifoSize())));
        }
        else
        {
            ui->streamStatusLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
        }

        return true;
    }
    else if (DeviceLimeSDRShared::MsgReportDeviceInfo::match(message))
    {
        DeviceLimeSDRShared::MsgReportDeviceInfo& report = (DeviceLimeSDRShared::MsgReportDeviceInfo&) message;
        ui->temperatureText->setText(tr("%1C").arg(QString::number(report.getTemperature(), 'f', 0)));
        ui->gpioText->setText(tr("%1").arg(report.getGPIOPins(), 2, 16, QChar('0')).toUpper());
        return true;
    }

    return false;
}

void LimeSDROutputGUI::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (DSPSignalNotification::match(*message))
        {
            qDebug("LimeSDROutputGUI::handleInputMessages: message: %s", message->getIdentifier());
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("LimeSDROutputGUI::handleInputMessages: DSPSignalNotification: SampleRate: %d, CenterFrequency: %llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();

            delete message;
        }
        else if (LimeSDROutput::MsgStartStop::match(*message))
        {
            LimeSDROutput::MsgStartStop& notif = (LimeSDROutput::MsgStartStop&) *message;
            blockApplySettings(true);
            ui->startStop->setChecked(notif.getStartStop());
            blockApplySettings(false);
            delete message;
        }
        else
        {
            if (handleMessage(*message)) {
                delete message;
            }
        }
    }
}

void LimeSDROutputGUI::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    displaySampleRate();
    checkLPF();
}

// Check if LPF BW is set wide enough when down-converting using NCO
void LimeSDROutputGUI::checkLPF()
{
    bool highlightLPFLabel = false;
    int64_t centerFrequency = m_settings.m_centerFrequency;
    if (m_settings.m_ncoEnable) {
        centerFrequency += m_settings.m_ncoFrequency;
    }
    if (centerFrequency < 30000000)
    {
        int64_t requiredBW = 30000000 - centerFrequency;
        highlightLPFLabel = m_settings.m_lpfBW < requiredBW;
    }
    if (highlightLPFLabel)
    {
        ui->lpfLabel->setStyleSheet("QLabel { background-color : red; }");
        ui->lpfLabel->setToolTip("LPF BW is too low for selected center frequency");
    }
    else
    {
        ui->lpfLabel->setStyleSheet("QLabel { background-color: rgb(64, 64, 64); }");
        ui->lpfLabel->setToolTip("");
    }
}

void LimeSDROutputGUI::updateDACRate()
{
    uint32_t dacRate = m_settings.m_devSampleRate * (1<<m_settings.m_log2HardInterp);

    if (dacRate < 100000000) {
        ui->dacRateLabel->setText(tr("%1k").arg(QString::number(dacRate / 1000.0f, 'g', 5)));
    } else {
        ui->dacRateLabel->setText(tr("%1M").arg(QString::number(dacRate / 1000000.0f, 'g', 5)));
    }
}

void LimeSDROutputGUI::displaySampleRate()
{
    float minF, maxF;
    m_limeSDROutput->getSRRange(minF, maxF);

    ui->sampleRate->blockSignals(true);

    if (m_sampleRateMode)
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(60,60,60); }");
        ui->sampleRateMode->setText("SR");
        ui->sampleRate->setValueRange(8, (uint32_t) minF, (uint32_t) maxF);
        ui->sampleRate->setValue(m_settings.m_devSampleRate);
        ui->sampleRate->setToolTip("Host to device sample rate (S/s)");
        ui->deviceRateText->setToolTip("Baseband sample rate (S/s)");
        uint32_t basebandSampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp);
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(basebandSampleRate / 1000.0f, 'g', 5)));
    }
    else
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(50,50,50); }");
        ui->sampleRateMode->setText("BB");
        ui->sampleRate->setValueRange(8, (uint32_t) minF/(1<<m_settings.m_log2SoftInterp), (uint32_t) maxF/(1<<m_settings.m_log2SoftInterp));
        ui->sampleRate->setValue(m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp));
        ui->sampleRate->setToolTip("Baseband sample rate (S/s)");
        ui->deviceRateText->setToolTip("Host to device sample rate (S/s)");
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(m_settings.m_devSampleRate / 1000.0f, 'g', 5)));
    }

    ui->sampleRate->blockSignals(false);
}

void LimeSDROutputGUI::displaySettings()
{
    ui->transverter->setDeltaFrequency(m_settings.m_transverterDeltaFrequency);
    ui->transverter->setDeltaFrequencyActive(m_settings.m_transverterMode);

    ui->extClock->setExternalClockFrequency(m_settings.m_extClockFreq);
    ui->extClock->setExternalClockActive(m_settings.m_extClock);

    updateFrequencyLimits();
    setCenterFrequencyDisplay();
    displaySampleRate();

    ui->hwInterp->setCurrentIndex(m_settings.m_log2HardInterp);
    ui->swInterp->setCurrentIndex(m_settings.m_log2SoftInterp);

    updateDACRate();

    ui->lpf->setValue(m_settings.m_lpfBW / 1000);

    ui->lpFIREnable->setChecked(m_settings.m_lpfFIREnable);
    ui->lpFIR->setValue(m_settings.m_lpfFIRBW / 1000);

    ui->gain->setValue(m_settings.m_gain);
    ui->gainText->setText(tr("%1dB").arg(m_settings.m_gain));

    ui->antenna->setCurrentIndex((int) m_settings.m_antennaPath);

    setNCODisplay();

    ui->ncoEnable->setChecked(m_settings.m_ncoEnable);
}

void LimeSDROutputGUI::setNCODisplay()
{
    int ncoHalfRange = (m_settings.m_devSampleRate * (1<<(m_settings.m_log2HardInterp)))/2;
    ui->ncoFrequency->setValueRange(
            false,
            8,
            -ncoHalfRange,
            ncoHalfRange);

    ui->ncoFrequency->blockSignals(true);
    ui->ncoFrequency->setToolTip(QString("NCO frequency shift in Hz (Range: +/- %1 kHz)").arg(ncoHalfRange/1000));
    ui->ncoFrequency->setValue(m_settings.m_ncoFrequency);
    ui->ncoFrequency->blockSignals(false);
}

void LimeSDROutputGUI::setCenterFrequencyDisplay()
{
    int64_t centerFrequency = m_settings.m_centerFrequency;
    ui->centerFrequency->setToolTip(QString("Main center frequency in kHz (LO: %1 kHz)").arg(centerFrequency/1000));

    if (m_settings.m_ncoEnable) {
        centerFrequency += m_settings.m_ncoFrequency;
    }

    ui->centerFrequency->blockSignals(true);
    ui->centerFrequency->setValue(centerFrequency < 0 ? 0 : (uint64_t) centerFrequency/1000); // kHz
    ui->centerFrequency->blockSignals(false);
}

void LimeSDROutputGUI::setCenterFrequencySetting(uint64_t kHzValue)
{
    int64_t centerFrequency = kHzValue*1000;

    if (m_settings.m_ncoEnable) {
        centerFrequency -= m_settings.m_ncoFrequency;
    }

    m_settings.m_centerFrequency = centerFrequency < 0 ? 0 : (uint64_t) centerFrequency;
    ui->centerFrequency->setToolTip(QString("Main center frequency in kHz (LO: %1 kHz)").arg(centerFrequency/1000));
}

void LimeSDROutputGUI::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void LimeSDROutputGUI::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "LimeSDROutputGUI::updateHardware";
        LimeSDROutput::MsgConfigureLimeSDR* message = LimeSDROutput::MsgConfigureLimeSDR::create(m_settings, m_settingsKeys, m_forceSettings);
        m_limeSDROutput->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void LimeSDROutputGUI::updateStatus()
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
                QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }

    if (m_statusCounter < 1)
    {
        m_statusCounter++;
    }
    else
    {
        LimeSDROutput::MsgGetStreamInfo* message = LimeSDROutput::MsgGetStreamInfo::create();
        m_limeSDROutput->getInputMessageQueue()->push(message);
        m_statusCounter = 0;
    }

    if (m_deviceStatusCounter < 10)
    {
        m_deviceStatusCounter++;
    }
    else
    {
        if (m_deviceUISet->m_deviceAPI->isBuddyLeader())
        {
            LimeSDROutput::MsgGetDeviceInfo* message = LimeSDROutput::MsgGetDeviceInfo::create();
            m_limeSDROutput->getInputMessageQueue()->push(message);
        }

        m_deviceStatusCounter = 0;
    }
}

void LimeSDROutputGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void LimeSDROutputGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        LimeSDROutput::MsgStartStop *message = LimeSDROutput::MsgStartStop::create(checked);
        m_limeSDROutput->getInputMessageQueue()->push(message);
    }
}

void LimeSDROutputGUI::on_centerFrequency_changed(quint64 value)
{
    setCenterFrequencySetting(value);
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void LimeSDROutputGUI::on_ncoFrequency_changed(qint64 value)
{
    m_settings.m_ncoFrequency = value;
    updateFrequencyLimits();
    setCenterFrequencyDisplay();
    m_settingsKeys.append("ncoFrequency");
    sendSettings();
}

void LimeSDROutputGUI::on_ncoEnable_toggled(bool checked)
{
    m_settings.m_ncoEnable = checked;
    updateFrequencyLimits();
    setCenterFrequencyDisplay();
    m_settingsKeys.append("ncoEnable");
    sendSettings();
}

void LimeSDROutputGUI::on_sampleRate_changed(quint64 value)
{
    if (m_sampleRateMode) {
        m_settings.m_devSampleRate = value;
    } else {
        m_settings.m_devSampleRate = value * (1 << m_settings.m_log2SoftInterp);
    }

    updateDACRate();
    setNCODisplay();
    m_settingsKeys.append("devSampleRate");
    sendSettings();}

void LimeSDROutputGUI::on_hwInterp_currentIndexChanged(int index)
{
    if ((index <0) || (index > 5)) {
        return;
    }

    m_settings.m_log2HardInterp = index;
    updateDACRate();
    setNCODisplay();
    m_settingsKeys.append("log2HardInterp");
    sendSettings();
}

void LimeSDROutputGUI::on_swInterp_currentIndexChanged(int index)
{
    if ((index <0) || (index > 6)) {
        return;
    }

    m_settings.m_log2SoftInterp = index;
    displaySampleRate();

    if (m_sampleRateMode) {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew();
    } else {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew() * (1 << m_settings.m_log2SoftInterp);
    }

    m_settingsKeys.append("log2SoftInterp");
    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void LimeSDROutputGUI::on_lpf_changed(quint64 value)
{
    m_settings.m_lpfBW = value * 1000;
    checkLPF();
    m_settingsKeys.append("lpfBW");
    sendSettings();
}

void LimeSDROutputGUI::on_lpFIREnable_toggled(bool checked)
{
    m_settings.m_lpfFIREnable = checked;
    m_settingsKeys.append("lpfFIREnable");
    sendSettings();
}

void LimeSDROutputGUI::on_lpFIR_changed(quint64 value)
{
    m_settings.m_lpfFIRBW = value * 1000;
    m_settingsKeys.append("lpfFIRBW");
    sendSettings();
}

void LimeSDROutputGUI::on_gain_valueChanged(int value)
{
    m_settings.m_gain = value;
    ui->gainText->setText(tr("%1dB").arg(m_settings.m_gain));
    m_settingsKeys.append("gain");
    sendSettings();
}

void LimeSDROutputGUI::on_antenna_currentIndexChanged(int index)
{
    m_settings.m_antennaPath = (LimeSDROutputSettings::PathRFE) index;
    m_settingsKeys.append("antennaPath");
    sendSettings();
}

void LimeSDROutputGUI::on_extClock_clicked()
{
    m_settings.m_extClock = ui->extClock->getExternalClockActive();
    m_settings.m_extClockFreq = ui->extClock->getExternalClockFrequency();
    qDebug("LimeSDROutputGUI::on_extClock_clicked: %u Hz %s", m_settings.m_extClockFreq, m_settings.m_extClock ? "on" : "off");
    m_settingsKeys.append("extClock");
    m_settingsKeys.append("extClockFreq");
    sendSettings();
}

void LimeSDROutputGUI::on_transverter_clicked()
{
    m_settings.m_transverterMode = ui->transverter->getDeltaFrequencyAcive();
    m_settings.m_transverterDeltaFrequency = ui->transverter->getDeltaFrequency();
    qDebug("LimeSDRInputGUI::on_transverter_clicked: %lld Hz %s", m_settings.m_transverterDeltaFrequency, m_settings.m_transverterMode ? "on" : "off");
    updateFrequencyLimits();
    setCenterFrequencySetting(ui->centerFrequency->getValueNew());
    m_settingsKeys.append("transverterMode");
    m_settingsKeys.append("transverterDeltaFrequency");
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void LimeSDROutputGUI::on_sampleRateMode_toggled(bool checked)
{
    m_sampleRateMode = checked;
    displaySampleRate();
}

void LimeSDROutputGUI::openDeviceSettingsDialog(const QPoint& p)
{
    if (m_contextMenuType == ContextMenuDeviceSettings)
    {
        BasicDeviceSettingsDialog dialog(this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);

        dialog.move(p);
        dialog.exec();

        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settingsKeys.append("useReverseAPI");
        m_settingsKeys.append("reverseAPIAddress");
        m_settingsKeys.append("reverseAPIPort");
        m_settingsKeys.append("reverseAPIDeviceIndex");

        sendSettings();
    }

    resetContextMenuType();
}

void LimeSDROutputGUI::makeUIConnections()
{
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &LimeSDROutputGUI::on_startStop_toggled);
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &LimeSDROutputGUI::on_centerFrequency_changed);
    QObject::connect(ui->ncoFrequency, &ValueDialZ::changed, this, &LimeSDROutputGUI::on_ncoFrequency_changed);
    QObject::connect(ui->ncoEnable, &ButtonSwitch::toggled, this, &LimeSDROutputGUI::on_ncoEnable_toggled);
    QObject::connect(ui->sampleRate, &ValueDial::changed, this, &LimeSDROutputGUI::on_sampleRate_changed);
    QObject::connect(ui->hwInterp, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeSDROutputGUI::on_hwInterp_currentIndexChanged);
    QObject::connect(ui->swInterp, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeSDROutputGUI::on_swInterp_currentIndexChanged);
    QObject::connect(ui->lpf, &ValueDial::changed, this, &LimeSDROutputGUI::on_lpf_changed);
    QObject::connect(ui->lpFIREnable, &ButtonSwitch::toggled, this, &LimeSDROutputGUI::on_lpFIREnable_toggled);
    QObject::connect(ui->lpFIR, &ValueDial::changed, this, &LimeSDROutputGUI::on_lpFIR_changed);
    QObject::connect(ui->gain, &QSlider::valueChanged, this, &LimeSDROutputGUI::on_gain_valueChanged);
    QObject::connect(ui->antenna, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeSDROutputGUI::on_antenna_currentIndexChanged);
    QObject::connect(ui->extClock, &ExternalClockButton::clicked, this, &LimeSDROutputGUI::on_extClock_clicked);
    QObject::connect(ui->transverter, &TransverterButton::clicked, this, &LimeSDROutputGUI::on_transverter_clicked);
    QObject::connect(ui->sampleRateMode, &QToolButton::toggled, this, &LimeSDROutputGUI::on_sampleRateMode_toggled);
}
