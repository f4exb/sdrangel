///////////////////////////////////////////////////////////////////////////////////
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
#include <QResizeEvent>

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
#include "limesdr/devicelimesdrshared.h"

#include "mainwindow.h"

#include "limesdrmimo.h"
#include "ui_limesdrmimogui.h"
#include "limesdrmimogui.h"

LimeSDRMIMOGUI::LimeSDRMIMOGUI(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::LimeSDRMIMOGUI),
    m_settings(),
    m_rxElseTx(true),
    m_streamIndex(0),
    m_spectrumRxElseTx(true),
    m_spectrumStreamIndex(0),
    m_gainLock(false),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_limeSDRMIMO(nullptr),
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
    qDebug("LimeSDRMIMOGUI::LimeSDRMIMOGUI");
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    ui->setupUi(getContents());
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    getContents()->setStyleSheet("#LimeSDRMIMOGUI { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplemimo/limesdrmimo/readme.md";
    m_limeSDRMIMO = (LimeSDRMIMO*) m_deviceUISet->m_deviceAPI->getSampleMIMO();

    m_limeSDRMIMO->getRxFrequencyRange(m_fMinRx, m_fMaxRx, m_fStepRx);
    m_limeSDRMIMO->getTxFrequencyRange(m_fMinTx, m_fMaxTx, m_fStepTx);
    m_limeSDRMIMO->getRxLPFRange(m_bwMinRx, m_bwMaxRx, m_bwStepRx);
    m_limeSDRMIMO->getTxLPFRange(m_bwMinTx, m_bwMaxTx, m_bwStepTx);
    m_limeSDRMIMO->getRxSampleRateRange(m_srMinRx, m_srMaxRx, m_srStepRx);
    m_limeSDRMIMO->getTxSampleRateRange(m_srMinTx, m_srMaxTx, m_srStepTx);

    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->lpf->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpFIR->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpFIR->setValueRange(5, 1U, 56000U);
    ui->ncoFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));

    updateFrequencyLimits();
    updateLPFLimits();
    displaySettings();

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_limeSDRMIMO->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

    sendSettings();
    makeUIConnections();
}

LimeSDRMIMOGUI::~LimeSDRMIMOGUI()
{
    delete ui;
}

void LimeSDRMIMOGUI::destroy()
{
    delete this;
}

void LimeSDRMIMOGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

QByteArray LimeSDRMIMOGUI::serialize() const
{
    return m_settings.serialize();
}

bool LimeSDRMIMOGUI::deserialize(const QByteArray& data)
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

void LimeSDRMIMOGUI::resizeEvent(QResizeEvent* size)
{
    adjustSize();
    size->accept();
}

void LimeSDRMIMOGUI::handleInputMessages()
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

bool LimeSDRMIMOGUI::handleMessage(const Message& message)
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

        qDebug("LimeSDRMIMOGUI::handleInputMessages: DSPMIMOSignalNotification: %s stream: %d SampleRate:%d, CenterFrequency:%llu",
                sourceOrSink ? "source" : "sink",
                istream,
                notif.getSampleRate(),
                notif.getCenterFrequency());

        updateSampleRateAndFrequency();

        return true;
    }
    else if (LimeSDRMIMO::MsgConfigureLimeSDRMIMO::match(message))
    {
        const LimeSDRMIMO::MsgConfigureLimeSDRMIMO& notif = (const LimeSDRMIMO::MsgConfigureLimeSDRMIMO&) message;

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
    else if (LimeSDRMIMO::MsgReportStreamInfo::match(message))
    {
        LimeSDRMIMO::MsgReportStreamInfo& report = (LimeSDRMIMO::MsgReportStreamInfo&) message;

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
    else
    {
        return false;
    }
}

void LimeSDRMIMOGUI::displaySettings()
{
    updateFrequencyLimits();
    updateLPFLimits();

    if (m_rxElseTx)
    {
        ui->antenna->blockSignals(true);
        ui->antenna->clear();
        ui->antenna->addItem("No");
        ui->antenna->addItem("Hi");
        ui->antenna->addItem("Lo");
        ui->antenna->addItem("Wi");
        ui->antenna->addItem("T1");
        ui->antenna->addItem("T2");
        ui->antenna->blockSignals(false);

        ui->transverter->setDeltaFrequency(m_settings.m_rxTransverterDeltaFrequency);
        ui->transverter->setDeltaFrequencyActive(m_settings.m_rxTransverterMode);
        ui->transverter->setIQOrder(m_settings.m_iqOrder);

        ui->extClock->setExternalClockFrequency(m_settings.m_extClockFreq);
        ui->extClock->setExternalClockActive(m_settings.m_extClock);

        setRxCenterFrequencyDisplay();
        displayRxSampleRate();

        ui->dcOffset->setChecked(m_settings.m_dcBlock);
        ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

        ui->hwDecim->setCurrentIndex(m_settings.m_log2HardDecim);
        ui->swDecim->setCurrentIndex(m_settings.m_log2SoftDecim);

        updateADCRate();

        if (m_streamIndex == 0)
        {
            ui->lpf->setValue(m_settings.m_lpfBWRx0 / 1000);
            ui->lpFIREnable->setChecked(m_settings.m_lpfFIREnableRx0);
            ui->lpFIR->setValue(m_settings.m_lpfFIRBWRx0 / 1000);
            ui->gain->setValue(m_settings.m_gainRx0);
            ui->gainText->setText(tr("%1").arg(m_settings.m_gainRx0));
            ui->antenna->setCurrentIndex((int) m_settings.m_antennaPathRx0);
            ui->gainMode->setCurrentIndex((int) m_settings.m_gainModeRx0);
            ui->lnaGain->setValue(m_settings.m_lnaGainRx0);
            ui->tiaGain->setCurrentIndex(m_settings.m_tiaGainRx0 - 1);
            ui->pgaGain->setValue(m_settings.m_pgaGainRx0);

            if (m_settings.m_gainModeRx0 == LimeSDRMIMOSettings::GAIN_AUTO)
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
        else if (m_streamIndex == 1)
        {
            ui->lpf->setValue(m_settings.m_lpfBWRx1 / 1000);
            ui->lpFIREnable->setChecked(m_settings.m_lpfFIREnableRx1);
            ui->lpFIR->setValue(m_settings.m_lpfFIRBWRx1 / 1000);
            ui->gain->setValue(m_settings.m_gainRx1);
            ui->gainText->setText(tr("%1").arg(m_settings.m_gainRx1));
            ui->antenna->setCurrentIndex((int) m_settings.m_antennaPathRx1);
            ui->gainMode->setCurrentIndex((int) m_settings.m_gainModeRx1);
            ui->lnaGain->setValue(m_settings.m_lnaGainRx1);
            ui->tiaGain->setCurrentIndex(m_settings.m_tiaGainRx1 - 1);
            ui->pgaGain->setValue(m_settings.m_pgaGainRx1);

            if (m_settings.m_gainModeRx1 == LimeSDRMIMOSettings::GAIN_AUTO)
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
        ui->antenna->blockSignals(true);
        ui->antenna->clear();
        ui->antenna->addItem("No");
        ui->antenna->addItem("Lo");
        ui->antenna->addItem("Hi");
        ui->antenna->blockSignals(false);

        ui->transverter->setDeltaFrequency(m_settings.m_txTransverterDeltaFrequency);
        ui->transverter->setDeltaFrequencyActive(m_settings.m_txTransverterMode);
        ui->transverter->setIQOrder(m_settings.m_iqOrder);

        ui->extClock->setExternalClockFrequency(m_settings.m_extClockFreq);
        ui->extClock->setExternalClockActive(m_settings.m_extClock);

        setTxCenterFrequencyDisplay();
        displayTxSampleRate();

        ui->hwDecim->setCurrentIndex(m_settings.m_log2HardInterp);
        ui->swDecim->setCurrentIndex(m_settings.m_log2SoftInterp);

        updateDACRate();

        if (m_streamIndex == 0)
        {
            ui->lpf->setValue(m_settings.m_lpfBWTx0 / 1000);
            ui->lpFIREnable->setChecked(m_settings.m_lpfFIREnableTx0);
            ui->lpFIR->setValue(m_settings.m_lpfFIRBWTx0 / 1000);
            ui->gain->setValue(m_settings.m_gainTx0);
            ui->gainText->setText(tr("%1").arg(m_settings.m_gainTx0));
            ui->antenna->setCurrentIndex((int) m_settings.m_antennaPathTx0);
        }
        else if (m_streamIndex == 1)
        {
            ui->lpf->setValue(m_settings.m_lpfBWTx1 / 1000);
            ui->lpFIREnable->setChecked(m_settings.m_lpfFIREnableTx1);
            ui->lpFIR->setValue(m_settings.m_lpfFIRBWTx1 / 1000);
            ui->gain->setValue(m_settings.m_gainTx1);
            ui->gainText->setText(tr("%1").arg(m_settings.m_gainTx1));
            ui->antenna->setCurrentIndex((int) m_settings.m_antennaPathTx1);
        }
    }

    setNCODisplay();
}

void LimeSDRMIMOGUI::displayRxSampleRate()
{
    int minF, maxF, stepF;
    m_limeSDRMIMO->getRxSampleRateRange(minF, maxF, stepF);

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

void LimeSDRMIMOGUI::setRxCenterFrequencyDisplay()
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

void LimeSDRMIMOGUI::setRxCenterFrequencySetting(uint64_t kHzValue)
{
    int64_t centerFrequency = kHzValue*1000;

    if (m_settings.m_ncoEnableRx) {
        centerFrequency -= m_settings.m_ncoFrequencyRx;
    }

    m_settings.m_rxCenterFrequency = centerFrequency < 0 ? 0 : (uint64_t) centerFrequency;
    ui->centerFrequency->setToolTip(QString("Main center frequency in kHz (LO: %1 kHz)").arg(centerFrequency/1000));
}

void LimeSDRMIMOGUI::updateADCRate()
{
    uint32_t adcRate = m_settings.m_devSampleRate * (1<<m_settings.m_log2HardDecim);

    if (adcRate < 100000000) {
        ui->adcRateLabel->setText(tr("%1k").arg(QString::number(adcRate / 1000.0f, 'g', 5)));
    } else {
        ui->adcRateLabel->setText(tr("%1M").arg(QString::number(adcRate / 1000000.0f, 'g', 5)));
    }
}

void LimeSDRMIMOGUI::setTxCenterFrequencyDisplay()
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

void LimeSDRMIMOGUI::setTxCenterFrequencySetting(uint64_t kHzValue)
{
    int64_t centerFrequency = kHzValue*1000;

    if (m_settings.m_ncoEnableTx) {
        centerFrequency -= m_settings.m_ncoFrequencyTx;
    }

    m_settings.m_txCenterFrequency = centerFrequency < 0 ? 0 : (uint64_t) centerFrequency;
    ui->centerFrequency->setToolTip(QString("Main center frequency in kHz (LO: %1 kHz)").arg(centerFrequency/1000));
}

void LimeSDRMIMOGUI::displayTxSampleRate()
{
    int minF, maxF, stepF;
    m_limeSDRMIMO->getTxSampleRateRange(minF, maxF, stepF);

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

void LimeSDRMIMOGUI::updateDACRate()
{
    uint32_t dacRate = m_settings.m_devSampleRate * (1<<m_settings.m_log2HardInterp);

    if (dacRate < 100000000) {
        ui->adcRateLabel->setText(tr("%1k").arg(QString::number(dacRate / 1000.0f, 'g', 5)));
    } else {
        ui->adcRateLabel->setText(tr("%1M").arg(QString::number(dacRate / 1000000.0f, 'g', 5)));
    }
}

void LimeSDRMIMOGUI::setNCODisplay()
{
    if (m_rxElseTx)
    {
        int ncoHalfRange = (m_settings.m_devSampleRate * (1<<(m_settings.m_log2HardDecim)))/2;
        ui->ncoFrequency->setValueRange(
                false,
                8,
                -ncoHalfRange,
                ncoHalfRange);

        ui->ncoFrequency->blockSignals(true);
        ui->ncoFrequency->setToolTip(QString("NCO frequency shift in Hz (Range: +/- %1 kHz)").arg(ncoHalfRange/1000));
        ui->ncoFrequency->setValue(m_settings.m_ncoFrequencyRx);
        ui->ncoEnable->setChecked(m_settings.m_ncoEnableRx);
        ui->ncoFrequency->blockSignals(false);
    }
    else
    {
        int ncoHalfRange = (m_settings.m_devSampleRate * (1<<(m_settings.m_log2HardInterp)))/2;
        ui->ncoFrequency->setValueRange(
                false,
                8,
                -ncoHalfRange,
                ncoHalfRange);

        ui->ncoFrequency->blockSignals(true);
        ui->ncoFrequency->setToolTip(QString("NCO frequency shift in Hz (Range: +/- %1 kHz)").arg(ncoHalfRange/1000));
        ui->ncoFrequency->setValue(m_settings.m_ncoFrequencyTx);
        ui->ncoEnable->setChecked(m_settings.m_ncoEnableTx);
        ui->ncoFrequency->blockSignals(false);
    }
}

void LimeSDRMIMOGUI::updateFrequencyLimits()
{
    // values in kHz
    float minF, maxF;
    qint64 deltaFrequency;

    if (m_rxElseTx)
    {
        minF = m_fMinRx;
        maxF = m_fMaxRx;
        deltaFrequency = m_settings.m_rxTransverterMode ? m_settings.m_rxTransverterDeltaFrequency/1000 : 0;
    }
    else
    {
        minF = m_fMinTx;
        maxF = m_fMaxTx;
        deltaFrequency = m_settings.m_txTransverterMode ? m_settings.m_txTransverterDeltaFrequency/1000 : 0;
    }


    qint64 minLimit = minF/1000 + deltaFrequency;
    qint64 maxLimit = maxF/1000 + deltaFrequency;

    if (m_settings.m_txTransverterMode || m_settings.m_rxTransverterMode)
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
    qDebug("LimeSDRMIMOGUI::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);
}

void LimeSDRMIMOGUI::updateLPFLimits()
{
    if (m_rxElseTx) {
        ui->lpf->setValueRange(6, (m_bwMinRx/1000)+1, m_bwMaxRx/1000);
    } else {
        ui->lpf->setValueRange(6, (m_bwMinTx/1000)+1, m_bwMaxTx/1000);
    }
}

void LimeSDRMIMOGUI::updateSampleRateAndFrequency()
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

void LimeSDRMIMOGUI::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void LimeSDRMIMOGUI::updateHardware()
{
    if (m_doApplySettings)
    {
        LimeSDRMIMO::MsgConfigureLimeSDRMIMO* message = LimeSDRMIMO::MsgConfigureLimeSDRMIMO::create(m_settings, m_settingsKeys, m_forceSettings);
        m_limeSDRMIMO->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void LimeSDRMIMOGUI::updateStatus()
{
    int stateRx = m_deviceUISet->m_deviceAPI->state(0);
    int stateTx = m_deviceUISet->m_deviceAPI->state(1);

    if (m_lastRxEngineState != stateRx)
    {
        qDebug("LimeSDRMIMOGUI::updateStatus: stateRx: %d", (int) stateRx);
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
                QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceAPI->errorMessage(0));
                break;
            default:
                break;
        }

        m_lastRxEngineState = stateRx;
    }

    if (m_lastTxEngineState != stateTx)
    {
        qDebug("LimeSDRMIMOGUI::updateStatus: stateTx: %d", (int) stateTx);
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
        LimeSDRMIMO::MsgGetStreamInfo* message = LimeSDRMIMO::MsgGetStreamInfo::create(m_rxElseTx, m_streamIndex);
        m_limeSDRMIMO->getInputMessageQueue()->push(message);
        m_statusCounter = 0;
    }

    if (m_deviceStatusCounter < 10)
    {
        m_deviceStatusCounter++;
    }
    else
    {
        LimeSDRMIMO::MsgGetDeviceInfo* message = LimeSDRMIMO::MsgGetDeviceInfo::create();
        m_limeSDRMIMO->getInputMessageQueue()->push(message);
        m_deviceStatusCounter = 0;
    }
}

void LimeSDRMIMOGUI::on_streamSide_currentIndexChanged(int index)
{
    m_rxElseTx = index == 0;
    updateFrequencyLimits();
    displaySettings();
}

void LimeSDRMIMOGUI::on_streamIndex_currentIndexChanged(int index)
{
    m_streamIndex = index < 0 ? 0 : index > 1 ? 1 : index;
    displaySettings();
}

void LimeSDRMIMOGUI::on_spectrumSide_currentIndexChanged(int index)
{
    m_spectrumRxElseTx = (index == 0);
    m_deviceUISet->m_spectrum->setDisplayedStream(m_spectrumRxElseTx, m_spectrumStreamIndex);
    m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(m_spectrumRxElseTx, m_spectrumStreamIndex);
    m_deviceUISet->setSpectrumScalingFactor(m_spectrumRxElseTx ? SDR_RX_SCALEF : SDR_TX_SCALEF);
    updateSampleRateAndFrequency();
    updateLPFLimits();
}

void LimeSDRMIMOGUI::on_spectrumIndex_currentIndexChanged(int index)
{
    m_spectrumStreamIndex = index < 0 ? 0 : index > 1 ? 1 : index;
    m_deviceUISet->m_spectrum->setDisplayedStream(m_spectrumRxElseTx, m_spectrumStreamIndex);
    m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(m_spectrumRxElseTx, m_spectrumStreamIndex);
    updateSampleRateAndFrequency();
}

void LimeSDRMIMOGUI::on_startStopRx_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        LimeSDRMIMO::MsgStartStop *message = LimeSDRMIMO::MsgStartStop::create(checked, true);
        m_limeSDRMIMO->getInputMessageQueue()->push(message);
    }
}

void LimeSDRMIMOGUI::on_startStopTx_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        LimeSDRMIMO::MsgStartStop *message = LimeSDRMIMO::MsgStartStop::create(checked, false);
        m_limeSDRMIMO->getInputMessageQueue()->push(message);
    }
}

void LimeSDRMIMOGUI::on_centerFrequency_changed(quint64 value)
{
    if (m_rxElseTx)
    {
        setRxCenterFrequencySetting(value);
        m_settingsKeys.append("rxCenterFrequency");
    }
    else
    {
        setTxCenterFrequencySetting(value);
        m_settingsKeys.append("txCenterFrequency");
    }

    sendSettings();
}

void LimeSDRMIMOGUI::on_ncoEnable_toggled(bool checked)
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

void LimeSDRMIMOGUI::on_ncoFrequency_changed(qint64 value)
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

void LimeSDRMIMOGUI::on_dcOffset_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    m_settingsKeys.append("dcBlock");
    sendSettings();
}

void LimeSDRMIMOGUI::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    m_settingsKeys.append("iqCorrection");
    sendSettings();
}

void LimeSDRMIMOGUI::on_extClock_clicked()
{
    m_settings.m_extClock = ui->extClock->getExternalClockActive();
    m_settings.m_extClockFreq = ui->extClock->getExternalClockFrequency();
    qDebug("LimeSDRMIMOGUI::on_extClock_clicked: %u Hz %s", m_settings.m_extClockFreq, m_settings.m_extClock ? "on" : "off");
    m_settingsKeys.append("extClock");
    m_settingsKeys.append("extClockFreq");
    sendSettings();
}

void LimeSDRMIMOGUI::on_hwDecim_currentIndexChanged(int index)
{
    if ((index <0) || (index > 5)) {
        return;
    }

    if (m_rxElseTx)
    {
        m_settings.m_log2HardDecim = index;
        m_settingsKeys.append("log2HardDecim");
        updateADCRate();
    }
    else
    {
        m_settings.m_log2HardInterp = index;
        m_settingsKeys.append("log2HardInterp");
        updateDACRate();
    }

    setNCODisplay();
    sendSettings();
}

void LimeSDRMIMOGUI::on_swDecim_currentIndexChanged(int index)
{
    if ((index <0) || (index > 6)) {
        return;
    }

    if (m_rxElseTx)
    {
        m_settings.m_log2SoftDecim = index;
        m_settingsKeys.append("log2SoftDecim");
        displayRxSampleRate();

        if (m_sampleRateMode) {
            m_settings.m_devSampleRate = ui->sampleRate->getValueNew();
        } else {
            m_settings.m_devSampleRate = ui->sampleRate->getValueNew() * (1 << m_settings.m_log2SoftDecim);
        }
    }
    else
    {
        m_settings.m_log2SoftInterp = index;
        m_settingsKeys.append("log2SoftInterp");
        displayTxSampleRate();

        if (m_sampleRateMode) {
            m_settings.m_devSampleRate = ui->sampleRate->getValueNew();
        } else {
            m_settings.m_devSampleRate = ui->sampleRate->getValueNew() * (1 << m_settings.m_log2SoftInterp);
        }
    }

    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void LimeSDRMIMOGUI::on_sampleRateMode_toggled(bool checked)
{
    m_sampleRateMode = checked;

    if (m_rxElseTx) {
        displayRxSampleRate();
    } else {
        displayTxSampleRate();
    }
}

void LimeSDRMIMOGUI::on_sampleRate_changed(quint64 value)
{
    if (m_rxElseTx)
    {
        if (m_sampleRateMode) {
            m_settings.m_devSampleRate = value;
        } else {
            m_settings.m_devSampleRate = value * (1 << m_settings.m_log2SoftDecim);
        }

        updateADCRate();
    }
    else
    {
        if (m_sampleRateMode) {
            m_settings.m_devSampleRate = value;
        } else {
            m_settings.m_devSampleRate = value * (1 << m_settings.m_log2SoftInterp);
        }

        updateDACRate();
    }

    setNCODisplay();
    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void LimeSDRMIMOGUI::on_lpf_changed(quint64 value)
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

void LimeSDRMIMOGUI::on_lpFIREnable_toggled(bool checked)
{
    if (m_rxElseTx)
    {
        if (m_streamIndex == 0)
        {
            m_settings.m_lpfFIREnableRx0 = checked;
            m_settingsKeys.append("lpfFIREnableRx0");
        }
        else if (m_streamIndex == 1)
        {
            m_settings.m_lpfFIREnableRx1 = checked;
            m_settingsKeys.append("lpfFIREnableRx1");
        }
    }
    else
    {
        if (m_streamIndex == 0)
        {
            m_settings.m_lpfFIREnableTx0 = checked;
            m_settingsKeys.append("lpfFIREnableTx0");
        }
        else if (m_streamIndex == 1)
        {
            m_settings.m_lpfFIREnableTx1 = checked;
            m_settingsKeys.append("lpfFIREnableTx1");
        }
    }

    sendSettings();
}

void LimeSDRMIMOGUI::on_lpFIR_changed(quint64 value)
{
    if (m_rxElseTx)
    {
        if (m_streamIndex == 0)
        {
            m_settings.m_lpfFIRBWRx0 = value * 1000;
            m_settingsKeys.append("lpfFIRBWRx0");
        }
        else if (m_streamIndex == 1)
        {
            m_settings.m_lpfFIRBWRx1 = value * 1000;
            m_settingsKeys.append("lpfFIRBWRx1");
        }
    }
    else
    {
        if (m_streamIndex == 0)
        {
            m_settings.m_lpfFIRBWTx0 = value * 1000;
            m_settingsKeys.append("lpfFIRBWTx0");
        }
        else if (m_streamIndex == 1)
        {
            m_settings.m_lpfFIRBWTx1 = value * 1000;
            m_settingsKeys.append("lpfFIRBWTx1");
        }
    }

    sendSettings();
}

void LimeSDRMIMOGUI::on_transverter_clicked()
{
    if (m_rxElseTx)
    {
        m_settings.m_rxTransverterMode = ui->transverter->getDeltaFrequencyAcive();
        m_settings.m_rxTransverterDeltaFrequency = ui->transverter->getDeltaFrequency();
        m_settings.m_iqOrder = ui->transverter->getIQOrder();
        m_settingsKeys.append("rxTransverterMode");
        m_settingsKeys.append("rxTransverterDeltaFrequency");
        m_settingsKeys.append("iqOrder");
        m_settingsKeys.append("rxCenterFrequency");
        qDebug("LimeSDRMIMOGUI::on_transverter_clicked: Rx %lld Hz %s", m_settings.m_rxTransverterDeltaFrequency, m_settings.m_rxTransverterMode ? "on" : "off");
    }
    else
    {
        m_settings.m_txTransverterMode = ui->transverter->getDeltaFrequencyAcive();
        m_settings.m_txTransverterDeltaFrequency = ui->transverter->getDeltaFrequency();
        m_settingsKeys.append("txTransverterMode");
        m_settingsKeys.append("txTransverterDeltaFrequency");
        m_settingsKeys.append("txCenterFrequency");
        qDebug("LimeSDRMIMOGUI::on_transverter_clicked: Tx %lld Hz %s", m_settings.m_txTransverterDeltaFrequency, m_settings.m_txTransverterMode ? "on" : "off");
    }

    updateFrequencyLimits();

    if (m_rxElseTx) {
        setRxCenterFrequencySetting(ui->centerFrequency->getValueNew());
    } else {
        setTxCenterFrequencySetting(ui->centerFrequency->getValueNew());
    }

    sendSettings();
}

void LimeSDRMIMOGUI::on_gainMode_currentIndexChanged(int index)
{
    if (!m_rxElseTx) {
        return;
    }

    if (m_streamIndex == 0)
    {
        m_settings.m_gainModeRx0 = (LimeSDRMIMOSettings::RxGainMode) index;
        m_settingsKeys.append("gainModeRx0");
    }
    else if (m_streamIndex == 0)
    {
        m_settings.m_gainModeRx1 = (LimeSDRMIMOSettings::RxGainMode) index;
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

void LimeSDRMIMOGUI::on_gain_valueChanged(int value)
{
    ui->gainText->setText(tr("%1").arg(value));

    if (m_rxElseTx)
    {
        if (m_streamIndex == 0)
        {
            m_settings.m_gainRx0 = value;
            m_settingsKeys.append("gainRx0");
        }
        else if (m_streamIndex == 1)
        {
            m_settings.m_gainRx1 = value;
            m_settingsKeys.append("gainRx1");
        }
    }
    else
    {
        if (m_streamIndex == 0)
        {
            m_settings.m_gainTx0 = value;
            m_settingsKeys.append("gainTx0");
        }
        else if (m_streamIndex == 1)
        {
            m_settings.m_gainTx1 = value;
            m_settingsKeys.append("gainTx1");
        }
    }

    sendSettings();
}

void LimeSDRMIMOGUI::on_lnaGain_valueChanged(int value)
{
    ui->lnaGainText->setText(tr("%1").arg(value));

    if (m_streamIndex == 0)
    {
        m_settings.m_lnaGainRx0 = value;
        m_settingsKeys.append("lnaGainRx0");
    }
    else if (m_streamIndex == 0)
    {
        m_settings.m_lnaGainRx1 = value;
        m_settingsKeys.append("lnaGainRx1");
    }

    sendSettings();
}

void LimeSDRMIMOGUI::on_tiaGain_currentIndexChanged(int index)
{
    if (m_streamIndex == 0)
    {
        m_settings.m_tiaGainRx0 = index + 1;
        m_settingsKeys.append("tiaGainRx0");
    }
    else if (m_streamIndex == 0)
    {
        m_settings.m_tiaGainRx1 = index + 1;
        m_settingsKeys.append("tiaGainRx1");
    }

    sendSettings();
}

void LimeSDRMIMOGUI::on_pgaGain_valueChanged(int value)
{
    ui->pgaGainText->setText(tr("%1").arg(value));

    if (m_streamIndex == 0)
    {
        m_settings.m_pgaGainRx0 = value;
        m_settingsKeys.append("pgaGainRx0");
    }
    else if (m_streamIndex == 0)
    {
        m_settings.m_pgaGainRx1 = value;
        m_settingsKeys.append("pgaGainRx1");
    }

    sendSettings();
}

void LimeSDRMIMOGUI::on_antenna_currentIndexChanged(int index)
{
    if (m_rxElseTx)
    {
        if (m_streamIndex == 0)
        {
            m_settings.m_antennaPathRx0 = (LimeSDRMIMOSettings::PathRxRFE) index;
            m_settingsKeys.append("antennaPathRx0");
        }
        else if (m_streamIndex == 1)
        {
            m_settings.m_antennaPathRx1 = (LimeSDRMIMOSettings::PathRxRFE) index;
            m_settingsKeys.append("antennaPathRx1");
        }
    }
    else
    {
        if (m_streamIndex == 0)
        {
            m_settings.m_antennaPathTx0 = (LimeSDRMIMOSettings::PathTxRFE) index;
            m_settingsKeys.append("antennaPathTx0");
        }
        else if (m_streamIndex == 1)
        {
            m_settings.m_antennaPathTx1 = (LimeSDRMIMOSettings::PathTxRFE) index;
            m_settingsKeys.append("antennaPathTx1");
        }
    }

    sendSettings();
}

void LimeSDRMIMOGUI::openDeviceSettingsDialog(const QPoint& p)
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

void LimeSDRMIMOGUI::makeUIConnections()
{
	QObject::connect(ui->streamSide, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeSDRMIMOGUI::on_streamSide_currentIndexChanged);
	QObject::connect(ui->streamIndex, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeSDRMIMOGUI::on_streamIndex_currentIndexChanged);
	QObject::connect(ui->spectrumSide, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeSDRMIMOGUI::on_spectrumSide_currentIndexChanged);
	QObject::connect(ui->spectrumIndex, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeSDRMIMOGUI::on_spectrumIndex_currentIndexChanged);
	QObject::connect(ui->startStopRx, &ButtonSwitch::toggled, this, &LimeSDRMIMOGUI::on_startStopRx_toggled);
	QObject::connect(ui->startStopTx, &ButtonSwitch::toggled, this, &LimeSDRMIMOGUI::on_startStopTx_toggled);
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &LimeSDRMIMOGUI::on_centerFrequency_changed);
    QObject::connect(ui->ncoEnable, &ButtonSwitch::toggled, this, &LimeSDRMIMOGUI::on_ncoEnable_toggled);
    QObject::connect(ui->ncoFrequency, &ValueDialZ::changed, this, &LimeSDRMIMOGUI::on_ncoFrequency_changed);
    QObject::connect(ui->dcOffset, &ButtonSwitch::toggled, this, &LimeSDRMIMOGUI::on_dcOffset_toggled);
    QObject::connect(ui->iqImbalance, &ButtonSwitch::toggled, this, &LimeSDRMIMOGUI::on_iqImbalance_toggled);
    QObject::connect(ui->extClock, &ExternalClockButton::clicked, this, &LimeSDRMIMOGUI::on_extClock_clicked);
    QObject::connect(ui->hwDecim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeSDRMIMOGUI::on_hwDecim_currentIndexChanged);
    QObject::connect(ui->swDecim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeSDRMIMOGUI::on_swDecim_currentIndexChanged);
    QObject::connect(ui->sampleRateMode, &QToolButton::toggled, this, &LimeSDRMIMOGUI::on_sampleRateMode_toggled);
    QObject::connect(ui->sampleRate, &ValueDial::changed, this, &LimeSDRMIMOGUI::on_sampleRate_changed);
    QObject::connect(ui->lpf, &ValueDial::changed, this, &LimeSDRMIMOGUI::on_lpf_changed);
    QObject::connect(ui->lpFIREnable, &ButtonSwitch::toggled, this, &LimeSDRMIMOGUI::on_lpFIREnable_toggled);
    QObject::connect(ui->lpFIR, &ValueDial::changed, this, &LimeSDRMIMOGUI::on_lpFIR_changed);
    QObject::connect(ui->transverter, &TransverterButton::clicked, this, &LimeSDRMIMOGUI::on_transverter_clicked);
    QObject::connect(ui->gainMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeSDRMIMOGUI::on_gainMode_currentIndexChanged);
    QObject::connect(ui->gain, &QDial::valueChanged, this, &LimeSDRMIMOGUI::on_gain_valueChanged);
    QObject::connect(ui->lnaGain, &QDial::valueChanged, this, &LimeSDRMIMOGUI::on_lnaGain_valueChanged);
    QObject::connect(ui->tiaGain, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeSDRMIMOGUI::on_tiaGain_currentIndexChanged);
    QObject::connect(ui->pgaGain, &QDial::valueChanged, this, &LimeSDRMIMOGUI::on_pgaGain_valueChanged);
    QObject::connect(ui->antenna, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LimeSDRMIMOGUI::on_antenna_currentIndexChanged);
}
