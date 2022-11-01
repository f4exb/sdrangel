///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB                                   //
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
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "util/db.h"
#include "plutosdr/deviceplutosdr.h"

#include "mainwindow.h"

#include "plutosdrmimo.h"
#include "ui_plutosdrmimogui.h"
#include "plutosdrmimogui.h"

PlutoSDRMIMOGUI::PlutoSDRMIMOGUI(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::PlutoSDRMIMOGUI),
    m_settings(),
    m_rxElseTx(true),
    m_streamIndex(0),
    m_spectrumRxElseTx(true),
    m_spectrumStreamIndex(0),
    m_gainLock(false),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_sampleMIMO(nullptr),
    m_tickCount(0),
    m_rxBasebandSampleRate(3072000),
    m_txBasebandSampleRate(3072000),
    m_rxDeviceCenterFrequency(435000*1000),
    m_txDeviceCenterFrequency(435000*1000),
    m_lastRxEngineState(DeviceAPI::StNotStarted),
    m_lastTxEngineState(DeviceAPI::StNotStarted),
    m_statusCounter(0),
    m_sampleRateMode(true)
{
    qDebug("PlutoSDRMIMOGui::PlutoSDRMIMOGui");
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    ui->setupUi(getContents());
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    getContents()->setStyleSheet("#PlutoSDRMIMOGUI { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplemimo/plutosdrmimo/readme.md";
    m_sampleMIMO = (PlutoSDRMIMO*) m_deviceUISet->m_deviceAPI->getSampleMIMO();

    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    updateFrequencyLimits();

    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, DevicePlutoSDR::srLowLimitFreq, DevicePlutoSDR::srHighLimitFreq);

    ui->lpf->setColorMapper(ColorMapper(ColorMapper::GrayYellow));

    quint32 minLimit, maxLimit;
    ((PlutoSDRMIMO *) m_sampleMIMO)->getbbLPRange(minLimit, maxLimit);
    ui->lpf->setValueRange(5, minLimit/1000, maxLimit/1000);

    ui->lpFIR->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpFIR->setValueRange(5, 1U, 56000U); // will be dynamically recalculated

    ui->swDecimLabel->setText(QString::fromUtf8("S\u2193"));
    ui->lpFIRDecimationLabel->setText(QString::fromUtf8("\u2193"));

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

    blockApplySettings(true);
    displaySettings();
    blockApplySettings(false);

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleMIMO->setMessageQueueToGUI(&m_inputMessageQueue);

    makeUIConnections();
}

PlutoSDRMIMOGUI::~PlutoSDRMIMOGUI()
{
    delete ui;
}

void PlutoSDRMIMOGUI::destroy()
{
    delete this;
}

void PlutoSDRMIMOGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    sendSettings(true);
}

QByteArray PlutoSDRMIMOGUI::serialize() const
{
    return m_settings.serialize();
}

bool PlutoSDRMIMOGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        displaySettings();
        sendSettings(true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void PlutoSDRMIMOGUI::resizeEvent(QResizeEvent* size)
{
    adjustSize();
    size->accept();
}

void PlutoSDRMIMOGUI::displaySettings()
{
    if (m_rxElseTx)
    {
        ui->transverter->setDeltaFrequency(m_settings.m_rxTransverterDeltaFrequency);
        ui->transverter->setDeltaFrequencyActive(m_settings.m_rxTransverterMode);
        ui->transverter->setIQOrder(m_settings.m_iqOrder);
        updateFrequencyLimits();
        ui->centerFrequency->setValue(m_settings.m_rxCenterFrequency / 1000);
        displaySampleRate();

        ui->dcOffset->setChecked(m_settings.m_dcBlock);
        ui->iqImbalance->setChecked(m_settings.m_iqCorrection);
        ui->bbDCOffset->setChecked(m_settings.m_hwBBDCBlock);
        ui->rfDCOffset->setChecked(m_settings.m_hwRFDCBlock);
        ui->hwIQImbalance->setChecked(m_settings.m_hwIQCorrection);
        ui->loPPM->setValue(m_settings.m_LOppmTenths);
        ui->loPPMText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));

        ui->swDecim->setCurrentIndex(m_settings.m_log2Decim);
        ui->fcPos->setCurrentIndex((int) m_settings.m_fcPosRx);

        ui->lpf->setValue(m_settings.m_lpfBWRx / 1000);

        ui->lpFIREnable->setChecked(m_settings.m_lpfRxFIREnable);
        ui->lpFIR->setValue(m_settings.m_lpfRxFIRBW / 1000);
        ui->lpFIRDecimation->setCurrentIndex(m_settings.m_lpfRxFIRlog2Decim);
        updateGainCombo();
        ui->lpFIRGain->setCurrentIndex((m_settings.m_lpfRxFIRGain + 12)/6);
        ui->lpFIRDecimation->setEnabled(m_settings.m_lpfRxFIREnable);
        ui->lpFIRGain->setEnabled(m_settings.m_lpfRxFIREnable);

        ui->gainMode->setEnabled(true);
        ui->gain->setEnabled(true);
        ui->att->setEnabled(false);

        if (m_streamIndex == 0)
        {
            ui->gainMode->setCurrentIndex((int) m_settings.m_rx0GainMode);
            ui->gain->setValue(m_settings.m_rx0Gain);
            ui->gainText->setText(tr("%1").arg(m_settings.m_rx0Gain));
            ui->antenna->setCurrentIndex((int) m_settings.m_rx0AntennaPath);
        }
        else
        {
            ui->gainMode->setCurrentIndex((int) m_settings.m_rx1GainMode);
            ui->gain->setValue(m_settings.m_rx1Gain);
            ui->gainText->setText(tr("%1").arg(m_settings.m_rx1Gain));
            ui->antenna->setCurrentIndex((int) m_settings.m_rx1AntennaPath);
        }

        setFIRBWLimits();
        setSampleRateLimits();
    }
    else
    {
        ui->transverter->setDeltaFrequency(m_settings.m_txTransverterDeltaFrequency);
        ui->transverter->setDeltaFrequencyActive(m_settings.m_txTransverterMode);
        updateFrequencyLimits();
        ui->centerFrequency->setValue(m_settings.m_txCenterFrequency / 1000);
        displaySampleRate();

        ui->swDecim->setCurrentIndex(m_settings.m_log2Interp);
        ui->fcPos->setCurrentIndex((int) m_settings.m_fcPosTx);

        ui->lpf->setValue(m_settings.m_lpfBWTx / 1000);

        ui->lpFIREnable->setChecked(m_settings.m_lpfTxFIREnable);
        ui->lpFIR->setValue(m_settings.m_lpfTxFIRBW / 1000);
        ui->lpFIRDecimation->setCurrentIndex(m_settings.m_lpfTxFIRlog2Interp);
        updateGainCombo();
        ui->lpFIRGain->setCurrentIndex((m_settings.m_lpfTxFIRGain + 6)/6);
        ui->lpFIRDecimation->setEnabled(m_settings.m_lpfTxFIREnable);
        ui->lpFIRGain->setEnabled(m_settings.m_lpfTxFIREnable);

        ui->gainMode->setEnabled(false);
        ui->gain->setEnabled(false);
        ui->att->setEnabled(true);

        if (m_streamIndex == 0)
        {
            ui->att->setValue(m_settings.m_tx0Att);
            ui->attText->setText(QString("%1 dB").arg(QString::number(m_settings.m_tx0Att*0.25, 'f', 2)));
            ui->antenna->setCurrentIndex((int) m_settings.m_tx0AntennaPath);
        }
        else
        {
            ui->att->setValue(m_settings.m_tx1Att);
            ui->attText->setText(QString("%1 dB").arg(QString::number(m_settings.m_tx1Att*0.25, 'f', 2)));
            ui->antenna->setCurrentIndex((int) m_settings.m_tx1AntennaPath);
        }

        setFIRBWLimits();
        setSampleRateLimits();
    }
}

void PlutoSDRMIMOGUI::displaySampleRate()
{
    ui->sampleRate->blockSignals(true);
    displayFcTooltip();

    if (m_sampleRateMode)
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(60,60,60); }");
        ui->sampleRateMode->setText("SR");
        ui->sampleRate->setValueRange(8, DevicePlutoSDR::srLowLimitFreq, DevicePlutoSDR::srHighLimitFreq);
        ui->sampleRate->setValue(m_settings.m_devSampleRate);
        ui->sampleRate->setToolTip("Device to host sample rate (S/s)");
        ui->deviceRateText->setToolTip("Baseband sample rate (S/s)");
        uint32_t basebandSampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim);
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(basebandSampleRate / 1000.0f, 'g', 5)));
    }
    else
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(50,50,50); }");
        ui->sampleRateMode->setText("BB");
        ui->sampleRate->setValueRange(8, DevicePlutoSDR::srLowLimitFreq/(1<<m_settings.m_log2Decim), DevicePlutoSDR::srHighLimitFreq/(1<<m_settings.m_log2Decim));
        ui->sampleRate->setValue(m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim));
        ui->sampleRate->setToolTip("Baseband sample rate (S/s)");
        ui->deviceRateText->setToolTip("Device to host sample rate (S/s)");
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(m_settings.m_devSampleRate / 1000.0f, 'g', 5)));
    }

    ui->sampleRate->blockSignals(false);
}

void PlutoSDRMIMOGUI::displayFcTooltip()
{
    if (m_rxElseTx)
    {
        int32_t fShift = DeviceSampleSource::calculateFrequencyShift(
            m_settings.m_log2Decim,
            (DeviceSampleSource::fcPos_t) m_settings.m_fcPosRx,
            m_settings.m_devSampleRate,
            DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD
        );
        ui->fcPos->setToolTip(tr("Relative position of device center frequency: %1 kHz").arg(QString::number(fShift / 1000.0f, 'g', 5)));
    }
    else
    {
        int32_t fShift = DeviceSampleSink::calculateFrequencyShift(
            m_settings.m_log2Interp,
            (DeviceSampleSink::fcPos_t) m_settings.m_fcPosTx,
            m_settings.m_devSampleRate
        );
        ui->fcPos->setToolTip(tr("Relative position of device center frequency: %1 kHz").arg(QString::number(fShift / 1000.0f, 'g', 5)));
    }
}

void PlutoSDRMIMOGUI::sendSettings(bool forceSettings)
{
    m_forceSettings = forceSettings;

    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void PlutoSDRMIMOGUI::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "PlutoSDRMIMOGUI::updateHardware";
        PlutoSDRMIMO::MsgConfigurePlutoSDRMIMO* message = PlutoSDRMIMO::MsgConfigurePlutoSDRMIMO::create(m_settings, m_settingsKeys, m_forceSettings);
        m_sampleMIMO->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void PlutoSDRMIMOGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void PlutoSDRMIMOGUI::updateSampleRateAndFrequency()
{
    if (m_rxElseTx)
    {
        m_deviceUISet->getSpectrum()->setSampleRate(m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim));
        m_deviceUISet->getSpectrum()->setCenterFrequency(m_rxDeviceCenterFrequency);
    }
    else
    {
        m_deviceUISet->getSpectrum()->setSampleRate(m_settings.m_devSampleRate/(1<<m_settings.m_log2Interp));
        m_deviceUISet->getSpectrum()->setCenterFrequency(m_txDeviceCenterFrequency);
    }

    displaySampleRate();
}

void PlutoSDRMIMOGUI::setFIRBWLimits()
{
    if (m_rxElseTx)
    {
        float high = DevicePlutoSDR::firBWHighLimitFactor * m_sampleMIMO->getRxFIRSampleRate();
        float low = DevicePlutoSDR::firBWLowLimitFactor * m_sampleMIMO->getRxFIRSampleRate();
        ui->lpFIR->setValueRange(5, (int(low)/1000)+1, (int(high)/1000)+1);
        ui->lpFIR->setValue(m_settings.m_lpfRxFIRBW/1000);
    }
    else
    {
        float high = DevicePlutoSDR::firBWHighLimitFactor * m_sampleMIMO->getTxFIRSampleRate();
        float low = DevicePlutoSDR::firBWLowLimitFactor * m_sampleMIMO->getTxFIRSampleRate();
        ui->lpFIR->setValueRange(5, (int(low)/1000)+1, (int(high)/1000)+1);
        ui->lpFIR->setValue(m_settings.m_lpfTxFIRBW/1000);
    }
}

void PlutoSDRMIMOGUI::setSampleRateLimits()
{
    uint32_t low = ui->lpFIREnable->isChecked() ? DevicePlutoSDR::srLowLimitFreq / (1<<ui->lpFIRDecimation->currentIndex()) : DevicePlutoSDR::srLowLimitFreq;
    ui->sampleRate->setValueRange(8, low, DevicePlutoSDR::srHighLimitFreq);
    ui->sampleRate->setValue(m_settings.m_devSampleRate);
}

void PlutoSDRMIMOGUI::updateFrequencyLimits()
{
    qint64 minLimit, maxLimit;
    qint64 deltaFrequency;
    // values should be in kHz
    if (m_rxElseTx) {
        deltaFrequency = m_settings.m_rxTransverterMode ? m_settings.m_rxTransverterDeltaFrequency/1000 : 0;
    } else {
        deltaFrequency = m_settings.m_txTransverterMode ? m_settings.m_txTransverterDeltaFrequency/1000 : 0;
    }

    m_sampleMIMO->getLORange(minLimit, maxLimit);

    minLimit = minLimit/1000 + deltaFrequency;
    maxLimit = maxLimit/1000 + deltaFrequency;

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
    qDebug("PlutoSDRMIMOGUI::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);
}

bool PlutoSDRMIMOGUI::handleMessage(const Message& message)
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

        qDebug("PlutoSDRMIMOGUI::handleInputMessages: DSPMIMOSignalNotification: %s stream: %d SampleRate:%d, CenterFrequency:%llu",
                sourceOrSink ? "source" : "sink",
                istream,
                notif.getSampleRate(),
                notif.getCenterFrequency());

        updateSampleRateAndFrequency();

        return true;
    }
    else if (PlutoSDRMIMO::MsgConfigurePlutoSDRMIMO::match(message))
    {
        const PlutoSDRMIMO::MsgConfigurePlutoSDRMIMO& notif = (const PlutoSDRMIMO::MsgConfigurePlutoSDRMIMO&) message;

        if (notif.getForce()) {
            m_settings = notif.getSettings();
        } else {
            m_settings.applySettings(notif.getSettingsKeys(), notif.getSettings());
        }

        displaySettings();

        return true;
    }

    return false;
}

void PlutoSDRMIMOGUI::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (handleMessage(*message)) {
            delete message;
        } else {
            qDebug("PlutoSDRMIMOGUI::handleInputMessages: unhandled message: %s", message->getIdentifier());
        }
    }
}

void PlutoSDRMIMOGUI::updateStatus()
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
                QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastTxEngineState = stateTx;
    }


    if (m_statusCounter % 2 == 0) // 1s
    {
        uint32_t samplerRate;

        if (m_rxElseTx) {
            samplerRate = m_sampleMIMO->getADCSampleRate();
        } else {
            samplerRate = m_sampleMIMO->getDACSampleRate();
        }

        if (samplerRate < 100000000) {
            ui->adcRateText->setText(tr("%1k").arg(QString::number(samplerRate / 1000.0f, 'g', 5)));
        } else {
            ui->adcRateText->setText(tr("%1M").arg(QString::number(samplerRate / 1000000.0f, 'g', 5)));
        }
    }

    if (m_statusCounter % 4 == 0) // 2s
    {
        std::string rssiStr;

        if (m_rxElseTx) {
            m_sampleMIMO->getRxRSSI(rssiStr, m_streamIndex);
        } else {
            m_sampleMIMO->getTxRSSI(rssiStr, m_streamIndex);
        }

        ui->rssiText->setText(tr("-%1").arg(QString::fromStdString(rssiStr)));
        int gaindB;

        if (m_rxElseTx) {
            m_sampleMIMO->getRxGain(gaindB, m_streamIndex);
        } else {
            gaindB = 0;
        }

        ui->actualGainText->setText(tr("%1").arg(gaindB));
    }

    if (m_statusCounter % 10 == 0) // 5s
    {
        m_sampleMIMO->fetchTemperature();
        ui->temperatureText->setText(tr("%1C").arg(QString::number(m_sampleMIMO->getTemperature(), 'f', 0)));
    }

    m_statusCounter++;
}

void PlutoSDRMIMOGUI::updateGainCombo()
{
    ui->lpFIRGain->blockSignals(true);
    ui->lpFIRGain->clear();

    if (m_rxElseTx) {
        ui->lpFIRGain->addItems(QStringList{"-12", "-6", "0", "+6"});
    } else {
        ui->lpFIRGain->addItems(QStringList{"-6", "0"});
    }

    ui->lpFIRGain->blockSignals(false);
}

void PlutoSDRMIMOGUI::on_streamSide_currentIndexChanged(int index)
{
    m_rxElseTx = index == 0;
    displaySettings();
}

void PlutoSDRMIMOGUI::on_streamIndex_currentIndexChanged(int index)
{
    m_streamIndex = index < 0 ? 0 : index > 1 ? 1 : index;
    displaySettings();
}

void PlutoSDRMIMOGUI::on_spectrumSide_currentIndexChanged(int index)
{
    m_spectrumRxElseTx = (index == 0);
    m_deviceUISet->m_spectrum->setDisplayedStream(m_spectrumRxElseTx, m_spectrumStreamIndex);
    m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(m_spectrumRxElseTx, m_spectrumStreamIndex);
    m_deviceUISet->setSpectrumScalingFactor(m_spectrumRxElseTx ? SDR_RX_SCALEF : SDR_TX_SCALEF);
    updateSampleRateAndFrequency();
}

void PlutoSDRMIMOGUI::on_spectrumIndex_currentIndexChanged(int index)
{
    m_spectrumStreamIndex = index < 0 ? 0 : index > 1 ? 1 : index;
    m_deviceUISet->m_spectrum->setDisplayedStream(m_spectrumRxElseTx, m_spectrumStreamIndex);
    m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(m_spectrumRxElseTx, m_spectrumStreamIndex);
    updateSampleRateAndFrequency();
}

void PlutoSDRMIMOGUI::on_startStopRx_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        PlutoSDRMIMO::MsgStartStop *message = PlutoSDRMIMO::MsgStartStop::create(checked, true);
        m_sampleMIMO->getInputMessageQueue()->push(message);
    }
}

void PlutoSDRMIMOGUI::on_startStopTx_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        PlutoSDRMIMO::MsgStartStop *message = PlutoSDRMIMO::MsgStartStop::create(checked, false);
        m_sampleMIMO->getInputMessageQueue()->push(message);
    }
}

void PlutoSDRMIMOGUI::on_centerFrequency_changed(quint64 value)
{
    if (m_rxElseTx)
    {
        m_settings.m_rxCenterFrequency = value * 1000;
        m_settingsKeys.append("rxCenterFrequency");
    }
    else
    {
        m_settings.m_txCenterFrequency = value * 1000;
        m_settingsKeys.append("txCenterFrequency");
    }

    sendSettings();
}

void PlutoSDRMIMOGUI::on_loPPM_valueChanged(int value)
{
    ui->loPPMText->setText(QString("%1").arg(QString::number(value/10.0, 'f', 1)));
    m_settings.m_LOppmTenths = value;
    m_settingsKeys.append("LOppmTenths");
    sendSettings();
}

void PlutoSDRMIMOGUI::on_dcOffset_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    m_settingsKeys.append("dcBlock");
    sendSettings();
}

void PlutoSDRMIMOGUI::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    m_settingsKeys.append("iqCorrection");
    sendSettings();
}

void PlutoSDRMIMOGUI::on_rfDCOffset_toggled(bool checked)
{
    m_settings.m_hwRFDCBlock = checked;
    m_settingsKeys.append("hwRFDCBlock");
    sendSettings();
}

void PlutoSDRMIMOGUI::on_bbDCOffset_toggled(bool checked)
{
    m_settings.m_hwBBDCBlock = checked;
    m_settingsKeys.append("hwBBDCBlock");
    sendSettings();
}

void PlutoSDRMIMOGUI::on_hwIQImbalance_toggled(bool checked)
{
    m_settings.m_hwIQCorrection = checked;
    m_settingsKeys.append("hwIQCorrection");
    sendSettings();
}

void PlutoSDRMIMOGUI::on_swDecim_currentIndexChanged(int index)
{
    if (m_rxElseTx)
    {
        m_settings.m_log2Decim = index > 6 ? 6 : index;
        m_settingsKeys.append("log2Decim");
    }
    else
    {
        m_settings.m_log2Interp = index > 6 ? 6 : index;
        m_settingsKeys.append("log2Interp");
    }

    displaySampleRate();
    m_settings.m_devSampleRate = ui->sampleRate->getValueNew();

    if (!m_sampleRateMode)
    {
        if (m_rxElseTx) {
            m_settings.m_devSampleRate <<= m_settings.m_log2Decim;
        } else {
            m_settings.m_devSampleRate <<= m_settings.m_log2Interp;
        }
    }

    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void PlutoSDRMIMOGUI::on_fcPos_currentIndexChanged(int index)
{
    PlutoSDRMIMOSettings::fcPos_t fcPos = (PlutoSDRMIMOSettings::fcPos_t) (index < (int) PlutoSDRMIMOSettings::FC_POS_END ? index : PlutoSDRMIMOSettings::FC_POS_CENTER);

    if (m_rxElseTx)
    {
        m_settings.m_fcPosRx = fcPos;
        m_settingsKeys.append("fcPosRx");
    }
    else
    {
        m_settings.m_fcPosTx = fcPos;
        m_settingsKeys.append("fcPosTx");
    }

    displayFcTooltip();
    sendSettings();
}

void PlutoSDRMIMOGUI::on_sampleRate_changed(quint64 value)
{
    m_settings.m_devSampleRate = value;

    if (!m_sampleRateMode)
    {
        if (m_rxElseTx) {
            m_settings.m_devSampleRate <<= m_settings.m_log2Decim;
        } else {
            m_settings.m_devSampleRate <<= m_settings.m_log2Interp;
        }
    }

    displayFcTooltip();
    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void PlutoSDRMIMOGUI::on_lpf_changed(quint64 value)
{
    if (m_rxElseTx)
    {
        m_settings.m_lpfBWRx = value * 1000;
        m_settingsKeys.append("lpfBWRx");
    }
    else
    {
        m_settings.m_lpfBWTx = value * 1000;
        m_settingsKeys.append("lpfBWTx");
    }

    sendSettings();
}

void PlutoSDRMIMOGUI::on_lpFIREnable_toggled(bool checked)
{
    if (m_rxElseTx)
    {
        m_settings.m_lpfRxFIREnable = checked;
        m_settingsKeys.append("lpfRxFIREnable");
    }
    else
    {
        m_settings.m_lpfTxFIREnable = checked;
        m_settingsKeys.append("lpfTxFIREnable");
    }

    ui->lpFIRDecimation->setEnabled(checked);
    ui->lpFIRGain->setEnabled(checked);
    sendSettings();
}

void PlutoSDRMIMOGUI::on_lpFIR_changed(quint64 value)
{
    if (m_rxElseTx)
    {
        m_settings.m_lpfRxFIRBW = value * 1000;
        m_settingsKeys.append("lpfRxFIRBW");
    }
    else
    {
        m_settings.m_lpfTxFIRBW = value * 1000;
        m_settingsKeys.append("lpfTxFIRBW");

    }

    sendSettings();
}

void PlutoSDRMIMOGUI::on_lpFIRDecimation_currentIndexChanged(int index)
{
    if (m_rxElseTx)
    {
        m_settings.m_lpfRxFIRlog2Decim = index > 2 ? 2 : index;
        m_settingsKeys.append("lpfRxFIRlog2Decim");
    }
    else
    {
        m_settings.m_lpfTxFIRlog2Interp = index > 2 ? 2 : index;
        m_settingsKeys.append("lpfTxFIRlog2Interp");
    }

    setSampleRateLimits();
    sendSettings();
}

void PlutoSDRMIMOGUI::on_lpFIRGain_currentIndexChanged(int index)
{
    if (m_rxElseTx)
    {
        m_settings.m_lpfRxFIRGain = 6*(index > 3 ? 3 : index) - 12;
        m_settingsKeys.append("lpfRxFIRGain");
    }
    else
    {
        m_settings.m_lpfTxFIRGain = 6*(index > 3 ? 3 : index) - 12;
        m_settingsKeys.append("lpfTxFIRGain");
    }

    sendSettings();
}

void PlutoSDRMIMOGUI::on_gainLock_toggled(bool checked)
{
    if (!m_gainLock && checked)
    {
        m_settings.m_rx1GainMode = m_settings.m_rx0GainMode;
        m_settings.m_rx1Gain = m_settings.m_rx0Gain;
        m_settings.m_tx1Att = m_settings.m_rx0Gain;
        m_settingsKeys.append("rx1GainMode");
        m_settingsKeys.append("rx1Gain");
        m_settingsKeys.append("tx1Att");
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        sendSettings();
    }

    m_gainLock = checked;
}

void PlutoSDRMIMOGUI::on_gainMode_currentIndexChanged(int index)
{
    if (m_streamIndex == 0)
    {
        m_settings.m_rx0GainMode = (PlutoSDRMIMOSettings::GainMode) (index < PlutoSDRMIMOSettings::GAIN_END ? index : 0);
        ui->gain->setEnabled(m_settings.m_rx0GainMode == PlutoSDRMIMOSettings::GAIN_MANUAL);
        m_settingsKeys.append("rx0GainMode");
    }
    else
    {
        m_settings.m_rx1GainMode = (PlutoSDRMIMOSettings::GainMode) (index < PlutoSDRMIMOSettings::GAIN_END ? index : 0);
        ui->gain->setEnabled(m_settings.m_rx1GainMode == PlutoSDRMIMOSettings::GAIN_MANUAL);
        m_settingsKeys.append("rx1GainMode");
    }

    sendSettings();
}

void PlutoSDRMIMOGUI::on_gain_valueChanged(int value)
{
    ui->gainText->setText(tr("%1").arg(value));

    if (m_streamIndex == 0)
    {
        m_settings.m_rx0Gain = value;
        m_settingsKeys.append("rx0Gain");
    }
    else
    {
        m_settings.m_rx1Gain = value;
        m_settingsKeys.append("rx1Gain");
    }

    sendSettings();
}

void PlutoSDRMIMOGUI::on_att_valueChanged(int value)
{
    ui->attText->setText(QString("%1 dB").arg(QString::number(value*0.25, 'f', 2)));

    if (m_streamIndex == 0)
    {
        m_settings.m_tx0Att = value;
        m_settingsKeys.append("tx0Att");
    }
    else
    {
        m_settings.m_tx1Att = value;
        m_settingsKeys.append("tx1Att");
    }

    sendSettings();
}

void PlutoSDRMIMOGUI::on_antenna_currentIndexChanged(int index)
{
    if (m_rxElseTx)
    {
        if (m_streamIndex == 0)
        {
            m_settings.m_rx0AntennaPath = (PlutoSDRMIMOSettings::RFPathRx) (index < PlutoSDRMIMOSettings::RFPATHRX_END ? index : 0);
            m_settingsKeys.append("rx0AntennaPath");
        }
        else
        {
            m_settings.m_rx1AntennaPath = (PlutoSDRMIMOSettings::RFPathRx) (index < PlutoSDRMIMOSettings::RFPATHRX_END ? index : 0);
            m_settingsKeys.append("rx1AntennaPath");
        }
    }
    else
    {
        if (m_streamIndex == 0)
        {
            m_settings.m_tx0AntennaPath = (PlutoSDRMIMOSettings::RFPathTx) (index < PlutoSDRMIMOSettings::RFPATHTX_END ? index : 0);
            m_settingsKeys.append("tx0AntennaPath");
        }
        else
        {
            m_settings.m_tx1AntennaPath = (PlutoSDRMIMOSettings::RFPathTx) (index < PlutoSDRMIMOSettings::RFPATHTX_END ? index : 0);
            m_settingsKeys.append("tx1AntennaPath");
        }
    }

    sendSettings();
}

void PlutoSDRMIMOGUI::on_transverter_clicked()
{
    if (m_rxElseTx)
    {
        m_settings.m_rxTransverterMode = ui->transverter->getDeltaFrequencyAcive();
        m_settings.m_rxTransverterDeltaFrequency = ui->transverter->getDeltaFrequency();
        m_settings.m_iqOrder = ui->transverter->getIQOrder();
        m_settingsKeys.append("rxTransverterMode");
        m_settingsKeys.append("rxTransverterDeltaFrequency");
        m_settingsKeys.append("iqOrder");
        qDebug("PlutoSDRInputGui::on_transverter_clicked: %lld Hz %s",
            m_settings.m_rxTransverterDeltaFrequency, m_settings.m_rxTransverterMode ? "on" : "off");
    }
    else
    {
        m_settings.m_txTransverterMode = ui->transverter->getDeltaFrequencyAcive();
        m_settings.m_txTransverterDeltaFrequency = ui->transverter->getDeltaFrequency();
        m_settingsKeys.append("txTransverterMode");
        m_settingsKeys.append("txTransverterDeltaFrequency");
        qDebug("PlutoSDRInputGui::on_transverter_clicked: %lld Hz %s",
            m_settings.m_rxTransverterDeltaFrequency, m_settings.m_txTransverterMode ? "on" : "off");
    }

    updateFrequencyLimits();

    if (m_rxElseTx)
    {
        m_settings.m_rxCenterFrequency = ui->centerFrequency->getValueNew()*1000;
        m_settingsKeys.append("rxCenterFrequency");
    }
    else
    {
        m_settings.m_txCenterFrequency = ui->centerFrequency->getValueNew()*1000;
        m_settingsKeys.append("txCenterFrequency");
    }

    sendSettings();
}

void PlutoSDRMIMOGUI::on_sampleRateMode_toggled(bool checked)
{
    m_sampleRateMode = checked;
    displaySampleRate();
}

void PlutoSDRMIMOGUI::openDeviceSettingsDialog(const QPoint& p)
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

void PlutoSDRMIMOGUI::makeUIConnections()
{
	QObject::connect(ui->streamSide, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlutoSDRMIMOGUI::on_streamSide_currentIndexChanged);
	QObject::connect(ui->streamIndex, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlutoSDRMIMOGUI::on_streamIndex_currentIndexChanged);
	QObject::connect(ui->spectrumSide, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlutoSDRMIMOGUI::on_spectrumSide_currentIndexChanged);
	QObject::connect(ui->spectrumIndex, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlutoSDRMIMOGUI::on_spectrumIndex_currentIndexChanged);
	QObject::connect(ui->startStopRx, &ButtonSwitch::toggled, this, &PlutoSDRMIMOGUI::on_startStopRx_toggled);
	QObject::connect(ui->startStopTx, &ButtonSwitch::toggled, this, &PlutoSDRMIMOGUI::on_startStopTx_toggled);
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &PlutoSDRMIMOGUI::on_centerFrequency_changed);
    QObject::connect(ui->loPPM, &QSlider::valueChanged, this, &PlutoSDRMIMOGUI::on_loPPM_valueChanged);
    QObject::connect(ui->dcOffset, &ButtonSwitch::toggled, this, &PlutoSDRMIMOGUI::on_dcOffset_toggled);
    QObject::connect(ui->iqImbalance, &ButtonSwitch::toggled, this, &PlutoSDRMIMOGUI::on_iqImbalance_toggled);
	QObject::connect(ui->sampleRate, &ValueDial::changed, this, &PlutoSDRMIMOGUI::on_sampleRate_changed);
    QObject::connect(ui->sampleRateMode, &QToolButton::toggled, this, &PlutoSDRMIMOGUI::on_sampleRateMode_toggled);
    QObject::connect(ui->fcPos, QOverload<int>::of(&QComboBox::currentIndexChanged),this, &PlutoSDRMIMOGUI::on_fcPos_currentIndexChanged);
    QObject::connect(ui->swDecim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlutoSDRMIMOGUI::on_swDecim_currentIndexChanged);
    QObject::connect(ui->gainLock, &QToolButton::toggled, this, &PlutoSDRMIMOGUI::on_gainLock_toggled);
	QObject::connect(ui->gainMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlutoSDRMIMOGUI::on_gainMode_currentIndexChanged);
	QObject::connect(ui->gain, &QDial::valueChanged, this, &PlutoSDRMIMOGUI::on_gain_valueChanged);
	QObject::connect(ui->att, &QDial::valueChanged, this, &PlutoSDRMIMOGUI::on_att_valueChanged);
	QObject::connect(ui->transverter, &TransverterButton::clicked, this, &PlutoSDRMIMOGUI::on_transverter_clicked);
    QObject::connect(ui->rfDCOffset, &ButtonSwitch::toggled, this, &PlutoSDRMIMOGUI::on_rfDCOffset_toggled);
    QObject::connect(ui->bbDCOffset, &ButtonSwitch::toggled, this, &PlutoSDRMIMOGUI::on_bbDCOffset_toggled);
    QObject::connect(ui->hwIQImbalance, &ButtonSwitch::toggled, this, &PlutoSDRMIMOGUI::on_hwIQImbalance_toggled);
    QObject::connect(ui->lpf, &ValueDial::changed, this, &PlutoSDRMIMOGUI::on_lpf_changed);
    QObject::connect(ui->lpFIREnable, &ButtonSwitch::toggled, this, &PlutoSDRMIMOGUI::on_lpFIREnable_toggled);
    QObject::connect(ui->lpFIR, &ValueDial::changed, this, &PlutoSDRMIMOGUI::on_lpFIR_changed);
    QObject::connect(ui->lpFIRDecimation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlutoSDRMIMOGUI::on_lpFIRDecimation_currentIndexChanged);
    QObject::connect(ui->lpFIRGain, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlutoSDRMIMOGUI::on_lpFIRGain_currentIndexChanged);
    QObject::connect(ui->antenna, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlutoSDRMIMOGUI::on_antenna_currentIndexChanged);
}
