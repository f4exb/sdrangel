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

#include "limesdrinputgui.h"

#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QResizeEvent>

#include <algorithm>

#include "ui_limesdrinputgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"

LimeSDRInputGUI::LimeSDRInputGUI(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::LimeSDRInputGUI),
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
    m_limeSDRInput = (LimeSDRInput*) m_deviceUISet->m_deviceAPI->getSampleSource();

    ui->setupUi(getContents());
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    getContents()->setStyleSheet("#LimeSDRInputGUI { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesource/limesdrinput/readme.md";

    float minF, maxF;

    m_limeSDRInput->getLORange(minF, maxF);
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(9, ((uint32_t) minF)/1000, ((uint32_t) maxF)/1000); // frequency dial is in kHz

    m_limeSDRInput->getSRRange(minF, maxF);
    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, (uint32_t) minF, (uint32_t) maxF);

    m_limeSDRInput->getLPRange(minF, maxF);
    ui->lpf->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpf->setValueRange(6, (minF/1000)+1, maxF/1000);

    ui->lpFIR->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpFIR->setValueRange(5, 1U, 56000U);

    ui->ncoFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));

    ui->channelNumberText->setText(tr("#%1").arg(m_limeSDRInput->getChannelIndex()));

    if (m_limeSDRInput->getLimeType() == DeviceLimeSDRParams::LimeMini)
    {
        ui->antenna->setItemText(2, "NC");
        ui->antenna->setItemText(3, "Lo");
        ui->antenna->setItemText(4, "NC");
        ui->antenna->setItemText(5, "NC");
        ui->antenna->setToolTip("Antenna select: No: none, NC: not connected, Hi: 2 - 3.5 GHz, Lo: 10 MHz - 2 GHz");
    }
    else
    {
        ui->antenna->setItemText(2, "Lo");
        ui->antenna->setItemText(3, "Wi");
        ui->antenna->setItemText(4, "T1");
        ui->antenna->setItemText(5, "T2");
        ui->antenna->setToolTip("Antenna select: No: none, NC: not connected, Hi: >1.5 GHz, Lo: <1.5 GHz Wi: full band, T1: Tx1 LB, T2: Tx2 LB");
    }

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    displaySettings();
    makeUIConnections();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_limeSDRInput->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));
}

LimeSDRInputGUI::~LimeSDRInputGUI()
{
    m_statusTimer.stop();
    m_updateTimer.stop();
    delete ui;
}

void LimeSDRInputGUI::destroy()
{
    delete this;
}

void LimeSDRInputGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

QByteArray LimeSDRInputGUI::serialize() const
{
    return m_settings.serialize();
}

bool LimeSDRInputGUI::deserialize(const QByteArray& data)
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

void LimeSDRInputGUI::resizeEvent(QResizeEvent* size)
{
    adjustSize();
    size->accept();
}

bool LimeSDRInputGUI::handleMessage(const Message& message)
{
    if (LimeSDRInput::MsgConfigureLimeSDR::match(message))
    {
        const LimeSDRInput::MsgConfigureLimeSDR& cfg = (LimeSDRInput::MsgConfigureLimeSDR&) message;

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
        m_settings.m_log2HardDecim = report.getLog2HardDecimInterp();

        if (report.getRxElseTx()) {
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
    else if (LimeSDRInput::MsgCalibrationResult::match(message))
    {
        LimeSDRInput::MsgCalibrationResult& report = (LimeSDRInput::MsgCalibrationResult&) message;

        if (report.getSuccess()) {
            ui->calibrationLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
        } else {
            ui->calibrationLabel->setStyleSheet("QLabel { background-color : red; }");
        }

        return true;
    }
    else if (LimeSDRInput::MsgReportStreamInfo::match(message))
    {
        LimeSDRInput::MsgReportStreamInfo& report = (LimeSDRInput::MsgReportStreamInfo&) message;

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
    else if (LimeSDRInput::MsgStartStop::match(message))
    {
        LimeSDRInput::MsgStartStop& notif = (LimeSDRInput::MsgStartStop&) message;
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

void LimeSDRInputGUI::updateFrequencyLimits()
{
    // values in kHz
    float minF, maxF;
    qint64 deltaFrequency = m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency/1000 : 0;
    m_limeSDRInput->getLORange(minF, maxF);
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
    qDebug("LimeSDRInputGUI::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);
}

void LimeSDRInputGUI::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("LimeSDRInputGUI::handleInputMessages: DSPSignalNotification: SampleRate: %d, CenterFrequency: %llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();

            delete message;
        }
        else if (LimeSDRInput::MsgConfigureLimeSDR::match(*message))
        {
            const LimeSDRInput::MsgConfigureLimeSDR& cfg = (LimeSDRInput::MsgConfigureLimeSDR&) *message;

            if (cfg.getForce()) {
                m_settings = cfg.getSettings();
            } else {
                m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
            }

            displaySettings();

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

void LimeSDRInputGUI::updateADCRate()
{
    uint32_t adcRate = m_settings.m_devSampleRate * (1<<m_settings.m_log2HardDecim);

    if (adcRate < 100000000) {
        ui->adcRateLabel->setText(tr("%1k").arg(QString::number(adcRate / 1000.0f, 'g', 5)));
    } else {
        ui->adcRateLabel->setText(tr("%1M").arg(QString::number(adcRate / 1000000.0f, 'g', 5)));
    }
}

void LimeSDRInputGUI::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    displaySampleRate();
    checkLPF();
}

// Check if LPF BW is set wide enough when up-converting using NCO
void LimeSDRInputGUI::checkLPF()
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

void LimeSDRInputGUI::displaySampleRate()
{
    float minF, maxF;
    m_limeSDRInput->getSRRange(minF, maxF);

    ui->sampleRate->blockSignals(true);

    if (m_sampleRateMode)
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(60,60,60); }");
        ui->sampleRateMode->setText("SR");
        ui->sampleRate->setValueRange(8, (uint32_t) minF, (uint32_t) maxF);
        ui->sampleRate->setValue(m_settings.m_devSampleRate);
        ui->sampleRate->setToolTip("Device to host sample rate (S/s)");
        ui->deviceRateText->setToolTip("Baseband sample rate (S/s)");
        uint32_t basebandSampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim);
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(basebandSampleRate / 1000.0f, 'g', 5)));
    }
    else
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(50,50,50); }");
        ui->sampleRateMode->setText("BB");
        ui->sampleRate->setValueRange(8, (uint32_t) minF/(1<<m_settings.m_log2SoftDecim), (uint32_t) maxF/(1<<m_settings.m_log2SoftDecim));
        ui->sampleRate->setValue(m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim));
        ui->sampleRate->setToolTip("Baseband sample rate (S/s)");
        ui->deviceRateText->setToolTip("Device to host sample rate (S/s)");
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(m_settings.m_devSampleRate / 1000.0f, 'g', 5)));
    }

    ui->sampleRate->blockSignals(false);
}


void LimeSDRInputGUI::displaySettings()
{
    ui->transverter->setDeltaFrequency(m_settings.m_transverterDeltaFrequency);
    ui->transverter->setDeltaFrequencyActive(m_settings.m_transverterMode);
    ui->transverter->setIQOrder(m_settings.m_iqOrder);

    ui->extClock->setExternalClockFrequency(m_settings.m_extClockFreq);
    ui->extClock->setExternalClockActive(m_settings.m_extClock);

    updateFrequencyLimits();
    setCenterFrequencyDisplay();
    displaySampleRate();

    ui->dcOffset->setChecked(m_settings.m_dcBlock);
    ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

    ui->hwDecim->setCurrentIndex(m_settings.m_log2HardDecim);
    ui->swDecim->setCurrentIndex(m_settings.m_log2SoftDecim);

    updateADCRate();

    ui->lpf->setValue(m_settings.m_lpfBW / 1000);

    ui->lpFIREnable->setChecked(m_settings.m_lpfFIREnable);
    ui->lpFIR->setValue(m_settings.m_lpfFIRBW / 1000);

    ui->gain->setValue(m_settings.m_gain);
    ui->gainText->setText(tr("%1").arg(m_settings.m_gain));

    ui->antenna->setCurrentIndex((int) m_settings.m_antennaPath);

    ui->gainMode->setCurrentIndex((int) m_settings.m_gainMode);
    ui->lnaGain->setValue(m_settings.m_lnaGain);
    ui->tiaGain->setCurrentIndex(m_settings.m_tiaGain - 1);
    ui->pgaGain->setValue(m_settings.m_pgaGain);

    if (m_settings.m_gainMode == LimeSDRInputSettings::GAIN_AUTO)
    {
        ui->gain->setEnabled(true);
        ui->lnaGain->setEnabled(false);
        ui->tiaGain->setEnabled(false);
        ui->pgaGain->setEnabled(false);
    }
    else
    {
        ui->gain->setEnabled(false);
        ui->lnaGain->setEnabled(true);
        ui->tiaGain->setEnabled(true);
        ui->pgaGain->setEnabled(true);
    }

    setNCODisplay();

    ui->ncoEnable->setChecked(m_settings.m_ncoEnable);
}

void LimeSDRInputGUI::setNCODisplay()
{
    int ncoHalfRange = (m_settings.m_devSampleRate * (1<<(m_settings.m_log2HardDecim)))/2;
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

void LimeSDRInputGUI::setCenterFrequencyDisplay()
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

void LimeSDRInputGUI::setCenterFrequencySetting(uint64_t kHzValue)
{
    int64_t centerFrequency = kHzValue*1000;

    if (m_settings.m_ncoEnable) {
        centerFrequency -= m_settings.m_ncoFrequency;
    }

    m_settings.m_centerFrequency = centerFrequency < 0 ? 0 : (uint64_t) centerFrequency;
    ui->centerFrequency->setToolTip(QString("Main center frequency in kHz (LO: %1 kHz)").arg(centerFrequency/1000));
}

void LimeSDRInputGUI::sendSettings()
{
    if(!m_updateTimer.isActive())
        m_updateTimer.start(100);
}

void LimeSDRInputGUI::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "LimeSDRInputGUI::updateHardware";
        LimeSDRInput::MsgConfigureLimeSDR* message = LimeSDRInput::MsgConfigureLimeSDR::create(m_settings, m_settingsKeys, m_forceSettings);
        m_limeSDRInput->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void LimeSDRInputGUI::updateStatus()
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
        LimeSDRInput::MsgGetStreamInfo* message = LimeSDRInput::MsgGetStreamInfo::create();
        m_limeSDRInput->getInputMessageQueue()->push(message);
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
            LimeSDRInput::MsgGetDeviceInfo* message = LimeSDRInput::MsgGetDeviceInfo::create();
            m_limeSDRInput->getInputMessageQueue()->push(message);
        }

        m_deviceStatusCounter = 0;
    }
}

void LimeSDRInputGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void LimeSDRInputGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        LimeSDRInput::MsgStartStop *message = LimeSDRInput::MsgStartStop::create(checked);
        m_limeSDRInput->getInputMessageQueue()->push(message);
    }
}

void LimeSDRInputGUI::on_centerFrequency_changed(quint64 value)
{
    setCenterFrequencySetting(value);
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void LimeSDRInputGUI::on_ncoFrequency_changed(qint64 value)
{
    m_settings.m_ncoFrequency = value;
    updateFrequencyLimits();
    setCenterFrequencyDisplay();
    m_settingsKeys.append("ncoFrequency");
    sendSettings();
}

void LimeSDRInputGUI::on_ncoEnable_toggled(bool checked)
{
    m_settings.m_ncoEnable = checked;
    updateFrequencyLimits();
    setCenterFrequencyDisplay();
    m_settingsKeys.append("ncoEnable");
    sendSettings();
}

void LimeSDRInputGUI::on_dcOffset_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    m_settingsKeys.append("dcBlock");
    sendSettings();
}

void LimeSDRInputGUI::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    m_settingsKeys.append("iqCorrection");
    sendSettings();
}

void LimeSDRInputGUI::on_sampleRate_changed(quint64 value)
{
    if (m_sampleRateMode) {
        m_settings.m_devSampleRate = value;
    } else {
        m_settings.m_devSampleRate = value * (1 << m_settings.m_log2SoftDecim);
    }

    updateADCRate();
    setNCODisplay();
    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void LimeSDRInputGUI::on_hwDecim_currentIndexChanged(int index)
{
    if ((index <0) || (index > 5)) {
        return;
    }

    m_settings.m_log2HardDecim = index;
    updateADCRate();
    setNCODisplay();
    m_settingsKeys.append("log2HardDecim");
    sendSettings();
}

void LimeSDRInputGUI::on_swDecim_currentIndexChanged(int index)
{
    if ((index <0) || (index > 6)) {
        return;
    }

    m_settings.m_log2SoftDecim = index;
    displaySampleRate();

    if (m_sampleRateMode) {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew();
    } else {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew() * (1 << m_settings.m_log2SoftDecim);
    }

    m_settingsKeys.append("log2SoftDecim");
    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void LimeSDRInputGUI::on_lpf_changed(quint64 value)
{
    m_settings.m_lpfBW = value * 1000;
    checkLPF();
    m_settingsKeys.append("lpfBW");
    sendSettings();
}

void LimeSDRInputGUI::on_lpFIREnable_toggled(bool checked)
{
    m_settings.m_lpfFIREnable = checked;
    m_settingsKeys.append("lpfFIREnable");
    sendSettings();
}

void LimeSDRInputGUI::on_lpFIR_changed(quint64 value)
{
    m_settings.m_lpfFIRBW = value * 1000;
    m_settingsKeys.append("lpfFIRBW");
    sendSettings();
}

void LimeSDRInputGUI::on_gainMode_currentIndexChanged(int index)
{
    m_settings.m_gainMode = (LimeSDRInputSettings::GainMode) index;

    if (index == 0)
    {
        ui->gain->setEnabled(true);
        ui->lnaGain->setEnabled(false);
        ui->tiaGain->setEnabled(false);
        ui->pgaGain->setEnabled(false);
    }
    else
    {
        ui->gain->setEnabled(false);
        ui->lnaGain->setEnabled(true);
        ui->tiaGain->setEnabled(true);
        ui->pgaGain->setEnabled(true);
    }

    m_settingsKeys.append("gainMode");
    sendSettings();
}

void LimeSDRInputGUI::on_gain_valueChanged(int value)
{
    m_settings.m_gain = value;
    ui->gainText->setText(tr("%1").arg(m_settings.m_gain));
    m_settingsKeys.append("gain");
    sendSettings();
}

void LimeSDRInputGUI::on_lnaGain_valueChanged(int value)
{
    m_settings.m_lnaGain = value;
    ui->lnaGainText->setText(tr("%1").arg(m_settings.m_lnaGain));
    m_settingsKeys.append("lnaGain");
    sendSettings();
}

void LimeSDRInputGUI::on_tiaGain_currentIndexChanged(int index)
{
    m_settings.m_tiaGain = index + 1;
    m_settingsKeys.append("tiaGain");
    sendSettings();
}

void LimeSDRInputGUI::on_pgaGain_valueChanged(int value)
{
    m_settings.m_pgaGain = value;
    ui->pgaGainText->setText(tr("%1").arg(m_settings.m_pgaGain));
    m_settingsKeys.append("pgaGain");
    sendSettings();
}

void LimeSDRInputGUI::on_antenna_currentIndexChanged(int index)
{
    m_settings.m_antennaPath = (LimeSDRInputSettings::PathRFE) index;
    m_settingsKeys.append("antennaPath");
    sendSettings();
}

void LimeSDRInputGUI::on_extClock_clicked()
{
    m_settings.m_extClock = ui->extClock->getExternalClockActive();
    m_settings.m_extClockFreq = ui->extClock->getExternalClockFrequency();
    qDebug("LimeSDRInputGUI::on_extClock_clicked: %u Hz %s", m_settings.m_extClockFreq, m_settings.m_extClock ? "on" : "off");
    m_settingsKeys.append("extClock");
    m_settingsKeys.append("extClockFreq");
    sendSettings();
}

void LimeSDRInputGUI::on_transverter_clicked()
{
    m_settings.m_transverterMode = ui->transverter->getDeltaFrequencyAcive();
    m_settings.m_transverterDeltaFrequency = ui->transverter->getDeltaFrequency();
    m_settings.m_iqOrder = ui->transverter->getIQOrder();
    qDebug("LimeSDRInputGUI::on_transverter_clicked: %lld Hz %s", m_settings.m_transverterDeltaFrequency, m_settings.m_transverterMode ? "on" : "off");
    updateFrequencyLimits();
    setCenterFrequencySetting(ui->centerFrequency->getValueNew());
    m_settingsKeys.append("transverterMode");
    m_settingsKeys.append("transverterDeltaFrequency");
    m_settingsKeys.append("iqOrder");
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void LimeSDRInputGUI::on_sampleRateMode_toggled(bool checked)
{
    m_sampleRateMode = checked;
    displaySampleRate();
}

void LimeSDRInputGUI::openDeviceSettingsDialog(const QPoint& p)
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

void LimeSDRInputGUI::makeUIConnections()
{
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &LimeSDRInputGUI::on_startStop_toggled);
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &LimeSDRInputGUI::on_centerFrequency_changed);
    QObject::connect(ui->ncoFrequency, &ValueDialZ::changed, this, &LimeSDRInputGUI::on_ncoFrequency_changed);
    QObject::connect(ui->ncoEnable, &ButtonSwitch::toggled, this, &LimeSDRInputGUI::on_ncoEnable_toggled);
    QObject::connect(ui->dcOffset, &ButtonSwitch::toggled, this, &LimeSDRInputGUI::on_dcOffset_toggled);
    QObject::connect(ui->iqImbalance, &ButtonSwitch::toggled, this, &LimeSDRInputGUI::on_iqImbalance_toggled);
    QObject::connect(ui->sampleRate, &ValueDial::changed, this, &LimeSDRInputGUI::on_sampleRate_changed);
    QObject::connect(ui->hwDecim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeSDRInputGUI::on_hwDecim_currentIndexChanged);
    QObject::connect(ui->swDecim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeSDRInputGUI::on_swDecim_currentIndexChanged);
    QObject::connect(ui->lpf, &ValueDial::changed, this, &LimeSDRInputGUI::on_lpf_changed);
    QObject::connect(ui->lpFIREnable, &ButtonSwitch::toggled, this, &LimeSDRInputGUI::on_lpFIREnable_toggled);
    QObject::connect(ui->lpFIR, &ValueDial::changed, this, &LimeSDRInputGUI::on_lpFIR_changed);
    QObject::connect(ui->gainMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeSDRInputGUI::on_gainMode_currentIndexChanged);
    QObject::connect(ui->gain, &QDial::valueChanged, this, &LimeSDRInputGUI::on_gain_valueChanged);
    QObject::connect(ui->lnaGain, &QDial::valueChanged, this, &LimeSDRInputGUI::on_lnaGain_valueChanged);
    QObject::connect(ui->tiaGain, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeSDRInputGUI::on_tiaGain_currentIndexChanged);
    QObject::connect(ui->pgaGain, &QDial::valueChanged, this, &LimeSDRInputGUI::on_pgaGain_valueChanged);
    QObject::connect(ui->antenna, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeSDRInputGUI::on_antenna_currentIndexChanged);
    QObject::connect(ui->extClock, &ExternalClockButton::clicked, this, &LimeSDRInputGUI::on_extClock_clicked);
    QObject::connect(ui->transverter, &TransverterButton::clicked, this, &LimeSDRInputGUI::on_transverter_clicked);
    QObject::connect(ui->sampleRateMode, &QToolButton::toggled, this, &LimeSDRInputGUI::on_sampleRateMode_toggled);
}
