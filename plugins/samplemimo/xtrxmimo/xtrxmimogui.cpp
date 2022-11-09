// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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
#include <QTime>
#include <QDateTime>
#include <QString>
#include <QMessageBox>
#include <QFileDialog>

#include "plugin/pluginapi.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspdevicemimoengine.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplestatic.h"
#include "util/db.h"
#include "xtrx/devicextrxshared.h"

#include "mainwindow.h"

#include "xtrxmimo.h"
#include "ui_xtrxmimogui.h"
#include "xtrxmimogui.h"

XTRXMIMOGUI::XTRXMIMOGUI(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::XTRXMIMOGUI),
    m_settings(),
    m_rxElseTx(true),
    m_streamIndex(0),
    m_spectrumRxElseTx(true),
    m_spectrumStreamIndex(0),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_xtrxMIMO(nullptr),
    m_tickCount(0),
    m_rxBasebandSampleRate(3072000),
    m_txBasebandSampleRate(3072000),
    m_rxDeviceCenterFrequency(435000*1000),
    m_txDeviceCenterFrequency(435000*1000),
    m_lastRxEngineState(DeviceAPI::StNotStarted),
    m_lastTxEngineState(DeviceAPI::StNotStarted),
    m_statusCounter(0),
    m_deviceStatusCounter(0),
    m_sampleRateMode(true)
{
    qDebug("XTRXMIMOGUI::XTRXMIMOGUI");
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    ui->setupUi(getContents());
    sizeToContents();
    getContents()->setStyleSheet("#XTRXMIMOGUI { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplemimo/xtrxmimo/readme.md";
    m_xtrxMIMO = (XTRXMIMO*) m_deviceUISet->m_deviceAPI->getSampleMIMO();

    float minF, maxF, stepF;

    m_xtrxMIMO->getLORange(minF, maxF, stepF);
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(7, ((uint32_t) minF)/1000, ((uint32_t) maxF)/1000); // frequency dial is in kHz

    m_xtrxMIMO->getSRRange(minF, maxF, stepF);
    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, (uint32_t) minF, (uint32_t) maxF);

    m_xtrxMIMO->getLPRange(minF, maxF, stepF);
    ui->lpf->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpf->setValueRange(6, (minF/1000)+1, maxF/1000);

    ui->ncoFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));

    displaySettings();

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_xtrxMIMO->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

    sendSettings();
    makeUIConnections();
}

XTRXMIMOGUI::~XTRXMIMOGUI()
{
    delete ui;
}

void XTRXMIMOGUI::destroy()
{
    delete this;
}

void XTRXMIMOGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

QByteArray XTRXMIMOGUI::serialize() const
{
    return m_settings.serialize();
}

bool XTRXMIMOGUI::deserialize(const QByteArray& data)
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

void XTRXMIMOGUI::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (handleMessage(*message)) {
            delete message;
        } else {
            qDebug("LimeSDRMIMOGUI::handleInputMessages: unhandled message: %s", message->getIdentifier());
        }
    }
}

bool XTRXMIMOGUI::handleMessage(const Message& message)
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

        qDebug("XTRXMIMOGUI::handleInputMessages: DSPMIMOSignalNotification: %s stream: %d SampleRate:%d, CenterFrequency:%llu",
                sourceOrSink ? "source" : "sink",
                istream,
                notif.getSampleRate(),
                notif.getCenterFrequency());

        updateSampleRateAndFrequency();

        return true;
    }
    else if (XTRXMIMO::MsgConfigureXTRXMIMO::match(message))
    {
        const XTRXMIMO::MsgConfigureXTRXMIMO& notif = (const XTRXMIMO::MsgConfigureXTRXMIMO&) message;

        if (notif.getForce()) {
            m_settings = notif.getSettings();
        } else {
            m_settings.applySettings(notif.getSettingsKeys(), notif.getSettings());
        }

        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (XTRXMIMO::MsgReportClockGenChange::match(message))
    {
        m_settings.m_rxDevSampleRate = m_xtrxMIMO->getRxDevSampleRate();
        m_settings.m_txDevSampleRate = m_xtrxMIMO->getTxDevSampleRate();
        m_settings.m_log2HardDecim   = m_xtrxMIMO->getLog2HardDecim();
        m_settings.m_log2HardInterp  = m_xtrxMIMO->getLog2HardInterp();

        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (XTRXMIMO::MsgReportStreamInfo::match(message))
    {
        XTRXMIMO::MsgReportStreamInfo& report = (XTRXMIMO::MsgReportStreamInfo&) message;

        if (report.getSuccess())
        {
            if (report.getActive()) {
                ui->streamStatusLabel->setStyleSheet("QLabel { background-color : green; }");
            } else {
                ui->streamStatusLabel->setStyleSheet("QLabel { background-color : blue; }");
            }

            ui->fifoBarRx->setMaximum(report.getFifoSize());
            ui->fifoBarRx->setValue(report.getFifoFilledCountRx());
            ui->fifoBarRx->setToolTip(tr("Rx FIFO fill %1/%2 samples")
                .arg(QString::number(report.getFifoFilledCountRx()))
                .arg(QString::number(report.getFifoSize())));

            ui->fifoBarTx->setMaximum(report.getFifoSize());
            ui->fifoBarTx->setValue(report.getFifoFilledCountTx());
            ui->fifoBarTx->setToolTip(tr("Tx FIFO fill %1/%2 samples")
                .arg(QString::number(report.getFifoFilledCountTx()))
                .arg(QString::number(report.getFifoSize())));
        }
        else
        {
            ui->streamStatusLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
        }

        return true;
    }
    else if (DeviceXTRXShared::MsgReportDeviceInfo::match(message))
    {
        DeviceXTRXShared::MsgReportDeviceInfo& report = (DeviceXTRXShared::MsgReportDeviceInfo&) message;
        ui->temperatureText->setText(tr("%1C").arg(QString::number(report.getTemperature(), 'f', 0)));

        if (report.getGPSLocked()) {
            ui->gpsStatusLabel->setStyleSheet("QLabel { background-color : green; }");
        } else {
            ui->gpsStatusLabel->setStyleSheet("QLabel { background:rgb(48,48,48); }");
        }

        return true;
    }
    else
    {
        return false;
    }
}

void XTRXMIMOGUI::displaySettings()
{
    ui->extClock->setExternalClockFrequency(m_settings.m_extClockFreq);
    ui->extClock->setExternalClockActive(m_settings.m_extClock);
    displaySampleRate();

    if (m_rxElseTx)
    {
        setRxCenterFrequencyDisplay();
        updateADCRate();

        ui->dcOffset->setChecked(m_settings.m_dcBlock);
        ui->iqImbalance->setChecked(m_settings.m_iqCorrection);
        ui->swDecim->setCurrentIndex(m_settings.m_log2SoftDecim);
        ui->hwDecim->setCurrentIndex(m_settings.m_log2HardDecim);
        ui->antenna->setCurrentIndex((int) m_settings.m_antennaPathRx);
        ui->ncoEnable->setChecked(m_settings.m_ncoEnableRx);

        if (m_streamIndex == 0)
        {
            ui->lpf->setValue(m_settings.m_lpfBWRx0 / 1000);
            ui->pwrmode->setCurrentIndex(m_settings.m_pwrmodeRx0);
            ui->gain->setValue(m_settings.m_gainRx0);
            ui->gainText->setText(tr("%1").arg(m_settings.m_gainRx0));
            ui->gainMode->setCurrentIndex((int) m_settings.m_gainModeRx0);
            ui->lnaGain->setValue(m_settings.m_lnaGainRx0);
            ui->tiaGain->setCurrentIndex(m_settings.m_tiaGainRx0 - 1);
            ui->pgaGain->setValue(m_settings.m_pgaGainRx0);

            if (m_settings.m_gainModeRx0 == XTRXMIMOSettings::GAIN_AUTO)
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
        }
        else
        {
            ui->lpf->setValue(m_settings.m_lpfBWRx1 / 1000);
            ui->pwrmode->setCurrentIndex(m_settings.m_pwrmodeRx1);
            ui->gain->setValue(m_settings.m_gainRx1);
            ui->gainText->setText(tr("%1").arg(m_settings.m_gainRx1));
            ui->gainMode->setCurrentIndex((int) m_settings.m_gainModeRx1);
            ui->lnaGain->setValue(m_settings.m_lnaGainRx1);
            ui->tiaGain->setCurrentIndex(m_settings.m_tiaGainRx1 - 1);
            ui->pgaGain->setValue(m_settings.m_pgaGainRx1);

            if (m_settings.m_gainModeRx1 == XTRXMIMOSettings::GAIN_AUTO)
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
        }
    }
    else
    {
        setTxCenterFrequencyDisplay();
        updateDACRate();

        ui->swDecim->setCurrentIndex(m_settings.m_log2SoftInterp);
        ui->hwDecim->setCurrentIndex(m_settings.m_log2HardInterp);
        ui->antenna->setCurrentIndex((int) m_settings.m_antennaPathTx);
        ui->ncoEnable->setChecked(m_settings.m_ncoEnableTx);

        if (m_streamIndex == 0)
        {
            ui->lpf->setValue(m_settings.m_lpfBWTx0 / 1000);
            ui->pwrmode->setCurrentIndex(m_settings.m_pwrmodeTx0);
            ui->gain->setValue(m_settings.m_gainTx0);
            ui->gainText->setText(tr("%1").arg(m_settings.m_gainTx0));
        }
        else
        {
            ui->lpf->setValue(m_settings.m_lpfBWTx1 / 1000);
            ui->pwrmode->setCurrentIndex(m_settings.m_pwrmodeTx1);
            ui->gain->setValue(m_settings.m_gainTx1);
            ui->gainText->setText(tr("%1").arg(m_settings.m_gainTx1));
        }
    }

    setNCODisplay();
}

void XTRXMIMOGUI::displaySampleRate()
{
    float minF, maxF, stepF;
    m_xtrxMIMO->getSRRange(minF, maxF, stepF);
    uint32_t devSampleRate = m_rxElseTx ? m_settings.m_rxDevSampleRate : m_settings.m_txDevSampleRate;
    uint32_t log2Soft = m_rxElseTx ? m_settings.m_log2SoftDecim : m_settings.m_log2SoftInterp;

    ui->sampleRate->blockSignals(true);

    if (m_sampleRateMode)
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(60,60,60); }");
        ui->sampleRateMode->setText("SR");
        ui->sampleRate->setValueRange(8, (uint32_t) minF, (uint32_t) maxF);
        ui->sampleRate->setValue(devSampleRate);
        ui->sampleRate->setToolTip("Device to host sample rate (S/s)");
        ui->deviceRateText->setToolTip("Baseband sample rate (S/s)");
        uint32_t basebandSampleRate = devSampleRate/(1<<log2Soft);
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(basebandSampleRate / 1000.0f, 'g', 5)));
    }
    else
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(50,50,50); }");
        ui->sampleRateMode->setText("BB");
        ui->sampleRate->setValueRange(8, (uint32_t) minF/(1<<log2Soft), (uint32_t) maxF/(1<<log2Soft));
        ui->sampleRate->setValue(devSampleRate/(1<<log2Soft));
        ui->sampleRate->setToolTip("Baseband sample rate (S/s)");
        ui->deviceRateText->setToolTip("Device to host sample rate (S/s)");
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(devSampleRate / 1000.0f, 'g', 5)));
    }

    ui->sampleRate->blockSignals(false);
}

void XTRXMIMOGUI::setNCODisplay()
{
    ui->ncoFrequency->blockSignals(true);

    if (m_rxElseTx)
    {
        int ncoHalfRange = (m_settings.m_rxDevSampleRate * (1<<(m_settings.m_log2HardDecim)))/2;
        ui->ncoFrequency->setValueRange(
                false,
                8,
                -ncoHalfRange,
                ncoHalfRange);
        ui->ncoFrequency->setToolTip(QString("NCO frequency shift in Hz (Range: +/- %1 kHz)").arg(ncoHalfRange/1000));
        ui->ncoFrequency->setValue(m_settings.m_ncoFrequencyRx);
        ui->ncoEnable->setChecked(m_settings.m_ncoEnableRx);
    }
    else
    {
        int ncoHalfRange = (m_settings.m_txDevSampleRate * (1<<(m_settings.m_log2HardInterp)))/2;
        ui->ncoFrequency->setValueRange(
                false,
                8,
                -ncoHalfRange,
                ncoHalfRange);
        ui->ncoFrequency->setToolTip(QString("NCO frequency shift in Hz (Range: +/- %1 kHz)").arg(ncoHalfRange/1000));
        ui->ncoFrequency->setValue(m_settings.m_ncoFrequencyTx);
        ui->ncoEnable->setChecked(m_settings.m_ncoEnableTx);
    }

    ui->ncoFrequency->blockSignals(false);
}

void XTRXMIMOGUI::setRxCenterFrequencyDisplay()
{
    int64_t centerFrequency = m_settings.m_rxCenterFrequency;
    ui->centerFrequency->setToolTip(QString("Main center frequency in kHz (LO: %1 kHz)").arg(centerFrequency/1000));

    if (m_settings.m_ncoEnableRx) {
        centerFrequency += m_settings.m_ncoFrequencyRx;
    }

    ui->centerFrequency->blockSignals(true);
    ui->centerFrequency->setValue(centerFrequency < 0 ? 0 : (uint64_t) centerFrequency/1000); // kHz
    ui->centerFrequency->blockSignals(false);
}

void XTRXMIMOGUI::setRxCenterFrequencySetting(uint64_t kHzValue)
{
    int64_t centerFrequency = kHzValue*1000;

    if (m_settings.m_ncoEnableRx) {
        centerFrequency -= m_settings.m_ncoFrequencyRx;
    }

    m_settings.m_rxCenterFrequency = centerFrequency < 0 ? 0 : (uint64_t) centerFrequency;
    m_settingsKeys.append("rxCenterFrequency");
    ui->centerFrequency->setToolTip(QString("Main center frequency in kHz (LO: %1 kHz)").arg(centerFrequency/1000));
}

void XTRXMIMOGUI::setTxCenterFrequencyDisplay()
{
    int64_t centerFrequency = m_settings.m_txCenterFrequency;
    ui->centerFrequency->setToolTip(QString("Main center frequency in kHz (LO: %1 kHz)").arg(centerFrequency/1000));

    if (m_settings.m_ncoEnableTx) {
        centerFrequency += m_settings.m_ncoFrequencyTx;
    }

    ui->centerFrequency->blockSignals(true);
    ui->centerFrequency->setValue(centerFrequency < 0 ? 0 : (uint64_t) centerFrequency/1000); // kHz
    ui->centerFrequency->blockSignals(false);
}

void XTRXMIMOGUI::setTxCenterFrequencySetting(uint64_t kHzValue)
{
    int64_t centerFrequency = kHzValue*1000;

    if (m_settings.m_ncoEnableTx) {
        centerFrequency -= m_settings.m_ncoFrequencyTx;
    }

    m_settings.m_txCenterFrequency = centerFrequency < 0 ? 0 : (uint64_t) centerFrequency;
    m_settingsKeys.append("txCenterFrequency");
    ui->centerFrequency->setToolTip(QString("Main center frequency in kHz (LO: %1 kHz)").arg(centerFrequency/1000));
}

void XTRXMIMOGUI::updateSampleRateAndFrequency()
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

void XTRXMIMOGUI::updateADCRate()
{
    uint32_t adcRate = m_xtrxMIMO->getClockGen() / 4;
    uint32_t log2HardDecim = m_xtrxMIMO->getLog2HardDecim();

    if (adcRate < 100000000) {
        ui->adcRateLabel->setText(tr("%1k").arg(QString::number(adcRate / 1000.0f, 'g', 5)));
    } else {
        ui->adcRateLabel->setText(tr("%1M").arg(QString::number(adcRate / 1000000.0f, 'g', 5)));
    }

    if (ui->hwDecim->currentIndex() != 0)
    {
        ui->hwDecim->blockSignals(true);
        ui->hwDecim->setCurrentIndex(log2HardDecim);
        ui->hwDecim->blockSignals(false);
    }
}

void XTRXMIMOGUI::updateDACRate()
{
    uint32_t dacRate = m_xtrxMIMO->getClockGen() / 4;
    uint32_t log2HardInterp = m_xtrxMIMO->getLog2HardInterp();

    if (dacRate < 100000000) {
        ui->adcRateLabel->setText(tr("%1k").arg(QString::number(dacRate / 1000.0f, 'g', 5)));
    } else {
        ui->adcRateLabel->setText(tr("%1M").arg(QString::number(dacRate / 1000000.0f, 'g', 5)));
    }

    if (ui->hwDecim->currentIndex() != 0)
    {
        ui->hwDecim->blockSignals(true);
        ui->hwDecim->setCurrentIndex(log2HardInterp);
        ui->hwDecim->blockSignals(false);
    }
}

void XTRXMIMOGUI::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void XTRXMIMOGUI::updateHardware()
{
    if (m_doApplySettings)
    {
        XTRXMIMO::MsgConfigureXTRXMIMO* message = XTRXMIMO::MsgConfigureXTRXMIMO::create(m_settings, m_settingsKeys, m_forceSettings);
        m_xtrxMIMO->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void XTRXMIMOGUI::updateStatus()
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
            QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceAPI->errorMessage());
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
            QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceAPI->errorMessage(1));
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
        XTRXMIMO::MsgGetStreamInfo* message = XTRXMIMO::MsgGetStreamInfo::create();
        m_xtrxMIMO->getInputMessageQueue()->push(message);
        m_statusCounter = 0;
    }

    if (m_deviceStatusCounter < 10)
    {
        m_deviceStatusCounter++;
    }
    else
    {
        XTRXMIMO::MsgGetDeviceInfo* message = XTRXMIMO::MsgGetDeviceInfo::create();
        m_xtrxMIMO->getInputMessageQueue()->push(message);
        m_deviceStatusCounter = 0;
    }
}

void XTRXMIMOGUI::on_streamSide_currentIndexChanged(int index)
{
    m_rxElseTx = index == 0;
    ui->gainMode->setEnabled(m_rxElseTx);
    ui->lnaGain->setEnabled(m_rxElseTx);
    ui->tiaGain->setEnabled(m_rxElseTx);
    ui->pgaGain->setEnabled(m_rxElseTx);

    ui->antenna->blockSignals(true);
    ui->antenna->clear();

    if (m_rxElseTx)
    {
        ui->antenna->addItem("Lo");
        ui->antenna->addItem("Wide");
        ui->antenna->addItem("Hi");
    }
    else
    {
        ui->antenna->addItem("Hi");
        ui->antenna->addItem("Wide");
    }

    ui->antenna->blockSignals(false);
    displaySettings();
}

void XTRXMIMOGUI::on_streamIndex_currentIndexChanged(int index)
{
    m_streamIndex = index < 0 ? 0 : index > 1 ? 1 : index;
    displaySettings();
}

void XTRXMIMOGUI::on_spectrumSide_currentIndexChanged(int index)
{
    m_spectrumRxElseTx = (index == 0);
    m_deviceUISet->m_spectrum->setDisplayedStream(m_spectrumRxElseTx, m_spectrumStreamIndex);
    m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(m_spectrumRxElseTx, m_spectrumStreamIndex);
    m_deviceUISet->setSpectrumScalingFactor(m_spectrumRxElseTx ? SDR_RX_SCALEF : SDR_TX_SCALEF);
    updateSampleRateAndFrequency();
}

void XTRXMIMOGUI::on_spectrumIndex_currentIndexChanged(int index)
{
    m_spectrumStreamIndex = index < 0 ? 0 : index > 1 ? 1 : index;
    m_deviceUISet->m_spectrum->setDisplayedStream(m_spectrumRxElseTx, m_spectrumStreamIndex);
    m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(m_spectrumRxElseTx, m_spectrumStreamIndex);
    updateSampleRateAndFrequency();
}

void XTRXMIMOGUI::on_startStopRx_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        XTRXMIMO::MsgStartStop *message = XTRXMIMO::MsgStartStop::create(checked, true);
        m_xtrxMIMO->getInputMessageQueue()->push(message);
    }
}

void XTRXMIMOGUI::on_startStopTx_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        XTRXMIMO::MsgStartStop *message = XTRXMIMO::MsgStartStop::create(checked, false);
        m_xtrxMIMO->getInputMessageQueue()->push(message);
    }
}

void XTRXMIMOGUI::on_centerFrequency_changed(quint64 value)
{
    if (m_rxElseTx) {
        setRxCenterFrequencySetting(value);
    } else {
        setTxCenterFrequencySetting(value);
    }

    sendSettings();
}

void XTRXMIMOGUI::on_ncoEnable_toggled(bool checked)
{
    if (m_rxElseTx)
    {
        m_settings.m_ncoEnableRx = checked;
        m_settingsKeys.append("ncoEnableRx");
        setRxCenterFrequencyDisplay();
    }
    else
    {
        m_settings.m_ncoEnableTx = checked;
        m_settingsKeys.append("ncoEnableTx");
        setTxCenterFrequencyDisplay();
    }

    sendSettings();
}

void XTRXMIMOGUI::on_ncoFrequency_changed(qint64 value)
{
    if (m_rxElseTx)
    {
        m_settings.m_ncoFrequencyRx = value;
        m_settingsKeys.append("ncoFrequencyRx");
        setRxCenterFrequencyDisplay();
    }
    else
    {
        m_settings.m_ncoFrequencyTx = value;
        m_settingsKeys.append("ncoFrequencyTx");
        setTxCenterFrequencyDisplay();
    }

    sendSettings();
}

void XTRXMIMOGUI::on_dcOffset_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    m_settingsKeys.append("dcBlock");
    sendSettings();
}

void XTRXMIMOGUI::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    m_settingsKeys.append("iqCorrection");
    sendSettings();
}

void XTRXMIMOGUI::on_extClock_clicked()
{
    m_settings.m_extClock = ui->extClock->getExternalClockActive();
    m_settings.m_extClockFreq = ui->extClock->getExternalClockFrequency();
    qDebug("XTRXMIMOGUI::on_extClock_clicked: %u Hz %s", m_settings.m_extClockFreq, m_settings.m_extClock ? "on" : "off");
    m_settingsKeys.append("extClock");
    m_settingsKeys.append("extClockFreq");
    sendSettings();
}

void XTRXMIMOGUI::on_hwDecim_currentIndexChanged(int index)
{
    if ((index <0) || (index > 6)) {
        return;
    }

    if (m_rxElseTx)
    {
        m_settings.m_log2HardDecim = index;
        m_settingsKeys.append("log2HardDecim");
    }
    else
    {
        m_settings.m_log2HardInterp = index;
        m_settingsKeys.append("log2HardInterp");
    }

    sendSettings();
}

void XTRXMIMOGUI::on_swDecim_currentIndexChanged(int index)
{
    if ((index <0) || (index > 6)) {
        return;
    }

    displaySampleRate();

    if (m_rxElseTx)
    {
        m_settings.m_log2SoftDecim = index;
        m_settingsKeys.append("log2SoftDecim");
        m_settingsKeys.append("rxDevSampleRate");

        if (m_sampleRateMode) {
            m_settings.m_rxDevSampleRate = ui->sampleRate->getValueNew();
        } else {
            m_settings.m_rxDevSampleRate = ui->sampleRate->getValueNew() * (1 << m_settings.m_log2SoftDecim);
        }
    }
    else
    {
        m_settings.m_log2SoftInterp = index;
        m_settingsKeys.append("log2SoftInterp");
        m_settingsKeys.append("txDevSampleRate");

        if (m_sampleRateMode) {
            m_settings.m_txDevSampleRate = ui->sampleRate->getValueNew();
        } else {
            m_settings.m_txDevSampleRate = ui->sampleRate->getValueNew() * (1 << m_settings.m_log2SoftInterp);
        }
    }

    sendSettings();
}

void XTRXMIMOGUI::on_sampleRateMode_toggled(bool checked)
{
    m_sampleRateMode = checked;
    displaySampleRate();
}

void XTRXMIMOGUI::on_sampleRate_changed(quint64 value)
{
    if (m_rxElseTx)
    {
        m_settingsKeys.append("rxDevSampleRate");

        if (m_sampleRateMode) {
            m_settings.m_rxDevSampleRate = value;
        } else {
            m_settings.m_rxDevSampleRate = value * (1 << m_settings.m_log2SoftDecim);
        }
    }
    else
    {
        m_settingsKeys.append("txDevSampleRate");

        if (m_sampleRateMode) {
            m_settings.m_txDevSampleRate = value;
        } else {
            m_settings.m_txDevSampleRate = value * (1 << m_settings.m_log2SoftInterp);
        }
    }

    sendSettings();
}

void XTRXMIMOGUI::on_lpf_changed(quint64 value)
{
    if (m_rxElseTx)
    {
        if (m_streamIndex == 0)
        {
            m_settings.m_lpfBWRx0 = value * 1000;
            m_settingsKeys.append("lpfBWRx0");
        }
        else if (m_streamIndex == 1)
        {
            m_settings.m_lpfBWRx1 = value * 1000;
            m_settingsKeys.append("lpfBWRx1");
        }
    }
    else
    {
        if (m_streamIndex == 0)
        {
            m_settings.m_lpfBWTx0 = value * 1000;
            m_settingsKeys.append("lpfBWTx0");
        }
        else if (m_streamIndex == 1)
        {
            m_settings.m_lpfBWTx1 = value * 1000;
            m_settingsKeys.append("lpfBWTx1");
        }
    }

    sendSettings();
}

void XTRXMIMOGUI::on_pwrmode_currentIndexChanged(int index)
{
    if (m_rxElseTx)
    {
        if (m_streamIndex == 0)
        {
            m_settings.m_pwrmodeRx0 = index;
            m_settingsKeys.append("pwrmodeRx0");
        }
        else if (m_streamIndex == 1)
        {
            m_settings.m_pwrmodeRx1 = index;
            m_settingsKeys.append("pwrmodeRx1");
        }
    }
    else
    {
        if (m_streamIndex == 0)
        {
            m_settings.m_pwrmodeTx0 = index;
            m_settingsKeys.append("pwrmodeTx0");
        }
        else if (m_streamIndex == 1)
        {
            m_settings.m_pwrmodeTx1 = index;
            m_settingsKeys.append("pwrmodeTx1");
        }
    }

    sendSettings();
}

void XTRXMIMOGUI::on_gainMode_currentIndexChanged(int index)
{
    if (!m_rxElseTx) {
        return;
    }

    if (m_streamIndex == 0)
    {
        m_settings.m_gainModeRx0 = (XTRXMIMOSettings::GainMode) index;
        m_settingsKeys.append("gainModeRx0");
    }
    else if (m_streamIndex == 1)
    {
        m_settings.m_gainModeRx1 = (XTRXMIMOSettings::GainMode) index;
        m_settingsKeys.append("gainModeRx1");
    }

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

    sendSettings();
}

void XTRXMIMOGUI::on_gain_valueChanged(int value)
{
    if (m_rxElseTx)
    {
        if (m_streamIndex == 0)
        {
            m_settings.m_gainRx0 = value;
            m_settingsKeys.append("gainRx0");
            ui->gainText->setText(tr("%1").arg(m_settings.m_gainRx0));
        }
        else if (m_streamIndex == 1)
        {
            m_settings.m_gainRx1 = value;
            m_settingsKeys.append("gainRx1");
            ui->gainText->setText(tr("%1").arg(m_settings.m_gainRx1));
        }
    }
    else
    {
        if (m_streamIndex == 0)
        {
            m_settings.m_gainTx0 = value;
            m_settingsKeys.append("gainTx0");
            ui->gainText->setText(tr("%1").arg(m_settings.m_gainTx0));
        }
        else if (m_streamIndex == 1)
        {
            m_settings.m_gainTx1 = value;
            m_settingsKeys.append("gainTx1");
            ui->gainText->setText(tr("%1").arg(m_settings.m_gainTx1));
        }
    }

    sendSettings();
}

void XTRXMIMOGUI::on_lnaGain_valueChanged(int value)
{
    if (!m_rxElseTx) {
        return;
    }

    if (m_streamIndex == 0)
    {
        m_settings.m_lnaGainRx0 = value;
        m_settingsKeys.append("lnaGainRx0");
        ui->lnaGainText->setText(tr("%1").arg(m_settings.m_lnaGainRx0));
    }
    else if (m_streamIndex == 1)
    {
        m_settings.m_lnaGainRx1 = value;
        m_settingsKeys.append("lnaGainRx1");
        ui->lnaGainText->setText(tr("%1").arg(m_settings.m_lnaGainRx1));
    }

    sendSettings();
}

void XTRXMIMOGUI::on_tiaGain_currentIndexChanged(int index)
{
    if (!m_rxElseTx) {
        return;
    }

    if (m_streamIndex == 0)
    {
        m_settings.m_tiaGainRx0 = index + 1;
        m_settingsKeys.append("tiaGainRx0");
    }
    else
    {
        m_settings.m_tiaGainRx1 = index + 1;
        m_settingsKeys.append("tiaGainRx1");
    }

    sendSettings();
}

void XTRXMIMOGUI::on_pgaGain_valueChanged(int value)
{
    if (!m_rxElseTx) {
        return;
    }

    if (m_streamIndex == 0)
    {
        m_settings.m_pgaGainRx0 = value;
        m_settingsKeys.append("pgaGainRx0");
        ui->pgaGainText->setText(tr("%1").arg(m_settings.m_pgaGainRx0));
    }
    else
    {
        m_settings.m_pgaGainRx1 = value;
        m_settingsKeys.append("pgaGainRx1");
        ui->pgaGainText->setText(tr("%1").arg(m_settings.m_pgaGainRx1));
    }

    sendSettings();
}

void XTRXMIMOGUI::on_antenna_currentIndexChanged(int index)
{
    if (m_rxElseTx)
    {
        m_settings.m_antennaPathRx = (XTRXMIMOSettings::RxAntenna) index;
        m_settingsKeys.append("antennaPathRx");
    }
    else
    {
        m_settings.m_antennaPathTx = (XTRXMIMOSettings::TxAntenna) index;
        m_settingsKeys.append("antennaPathTx");
    }

    sendSettings();
}

void XTRXMIMOGUI::openDeviceSettingsDialog(const QPoint& p)
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

void XTRXMIMOGUI::makeUIConnections()
{
    QObject::connect(ui->streamSide, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &XTRXMIMOGUI::on_streamSide_currentIndexChanged);
    QObject::connect(ui->streamIndex, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &XTRXMIMOGUI::on_streamIndex_currentIndexChanged);
    QObject::connect(ui->spectrumSide, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &XTRXMIMOGUI::on_spectrumSide_currentIndexChanged);
    QObject::connect(ui->spectrumIndex, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &XTRXMIMOGUI::on_spectrumIndex_currentIndexChanged);
    QObject::connect(ui->startStopRx, &ButtonSwitch::toggled, this, &XTRXMIMOGUI::on_startStopRx_toggled);
    QObject::connect(ui->startStopTx, &ButtonSwitch::toggled, this, &XTRXMIMOGUI::on_startStopTx_toggled);
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &XTRXMIMOGUI::on_centerFrequency_changed);
    QObject::connect(ui->ncoEnable, &ButtonSwitch::toggled, this, &XTRXMIMOGUI::on_ncoEnable_toggled);
    QObject::connect(ui->ncoFrequency, &ValueDialZ::changed, this, &XTRXMIMOGUI::on_ncoFrequency_changed);
    QObject::connect(ui->dcOffset, &ButtonSwitch::toggled, this, &XTRXMIMOGUI::on_dcOffset_toggled);
    QObject::connect(ui->iqImbalance, &ButtonSwitch::toggled, this, &XTRXMIMOGUI::on_iqImbalance_toggled);
    QObject::connect(ui->extClock, &ExternalClockButton::clicked, this, &XTRXMIMOGUI::on_extClock_clicked);
    QObject::connect(ui->hwDecim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &XTRXMIMOGUI::on_hwDecim_currentIndexChanged);
    QObject::connect(ui->swDecim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &XTRXMIMOGUI::on_swDecim_currentIndexChanged);
    QObject::connect(ui->sampleRateMode, &QToolButton::toggled, this, &XTRXMIMOGUI::on_sampleRateMode_toggled);
    QObject::connect(ui->sampleRate, &ValueDial::changed, this, &XTRXMIMOGUI::on_sampleRate_changed);
    QObject::connect(ui->lpf, &ValueDial::changed, this, &XTRXMIMOGUI::on_lpf_changed);
    QObject::connect(ui->pwrmode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &XTRXMIMOGUI::on_pwrmode_currentIndexChanged);
    QObject::connect(ui->gainMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &XTRXMIMOGUI::on_gainMode_currentIndexChanged);
    QObject::connect(ui->gain, &QDial::valueChanged, this, &XTRXMIMOGUI::on_gain_valueChanged);
    QObject::connect(ui->lnaGain, &QDial::valueChanged, this, &XTRXMIMOGUI::on_lnaGain_valueChanged);
    QObject::connect(ui->tiaGain, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &XTRXMIMOGUI::on_tiaGain_currentIndexChanged);
    QObject::connect(ui->pgaGain, &QDial::valueChanged, this, &XTRXMIMOGUI::on_pgaGain_valueChanged);
    QObject::connect(ui->antenna, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &XTRXMIMOGUI::on_antenna_currentIndexChanged);
}
