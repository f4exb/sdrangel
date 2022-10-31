///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include "mainwindow.h"

#include "bladerf2mimo.h"
#include "ui_bladerf2mimogui.h"
#include "bladerf2mimogui.h"

BladeRF2MIMOGui::BladeRF2MIMOGui(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::BladeRF2MIMOGui),
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
    m_sampleRateMode(true)
{
    qDebug("BladeRF2MIMOGui::BladeRF2MIMOGui");
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    ui->setupUi(getContents());
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    getContents()->setStyleSheet("#BladeRF2MIMOGui { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplemimo/bladerf2mimo/readme.md";
    m_sampleMIMO = (BladeRF2MIMO*) m_deviceUISet->m_deviceAPI->getSampleMIMO();

    m_sampleMIMO->getRxFrequencyRange(m_fMinRx, m_fMaxRx, m_fStepRx, m_fScaleRx);
    m_sampleMIMO->getTxFrequencyRange(m_fMinTx, m_fMaxTx, m_fStepTx, m_fScaleTx);
    m_sampleMIMO->getRxBandwidthRange(m_bwMinRx, m_bwMaxRx, m_bwStepRx, m_bwScaleRx);
    m_sampleMIMO->getTxBandwidthRange(m_bwMinTx, m_bwMaxTx, m_bwStepTx, m_bwScaleTx);

    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->bandwidth->setColorMapper(ColorMapper(ColorMapper::GrayYellow));

    int minRx, maxRx, stepRx, minTx, maxTx, stepTx;
    m_sampleMIMO->getRxSampleRateRange(minRx, maxRx, stepRx, m_srScaleRx);
    m_sampleMIMO->getTxSampleRateRange(minTx, maxTx, stepTx, m_srScaleTx);
    m_srMin = std::max(minRx, minTx);
    m_srMax = std::min(maxRx, maxTx);
    m_sampleMIMO->getRxGlobalGainRange(m_gainMinRx, m_gainMaxRx, m_gainStepRx, m_gainScaleRx);
    m_sampleMIMO->getTxGlobalGainRange(m_gainMinTx, m_gainMaxTx, m_gainStepTx, m_gainScaleTx);

    displayGainModes();
    displaySettings();

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleMIMO->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

    sendSettings();
    makeUIConnections();
}

BladeRF2MIMOGui::~BladeRF2MIMOGui()
{
    delete ui;
}

void BladeRF2MIMOGui::destroy()
{
    delete this;
}

void BladeRF2MIMOGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

QByteArray BladeRF2MIMOGui::serialize() const
{
    return m_settings.serialize();
}

bool BladeRF2MIMOGui::deserialize(const QByteArray& data)
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

void BladeRF2MIMOGui::resizeEvent(QResizeEvent* size)
{
    adjustSize();
    size->accept();
}

void BladeRF2MIMOGui::displaySettings()
{
    updateFrequencyLimits();

    if (m_rxElseTx)
    {
        ui->transverter->setDeltaFrequency(m_settings.m_rxTransverterDeltaFrequency);
        ui->transverter->setDeltaFrequencyActive(m_settings.m_rxTransverterMode);
        ui->transverter->setIQOrder(m_settings.m_iqOrder);
        ui->centerFrequency->setValueRange(7, m_fMinRx / 1000, m_fMaxRx / 1000);
        ui->centerFrequency->setValue(m_settings.m_rxCenterFrequency / 1000);
        ui->bandwidth->setValueRange(5, m_bwMinRx / 1000, m_bwMaxRx / 1000);
        ui->bandwidth->setValue(m_settings.m_rxBandwidth / 1000);
        uint32_t basebandSampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim);
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(basebandSampleRate / 1000.0f, 'g', 5)));
        ui->dcOffset->setEnabled(true);
        ui->dcOffset->setChecked(m_settings.m_dcBlock);
        ui->iqImbalance->setEnabled(true);
        ui->iqImbalance->setChecked(m_settings.m_iqCorrection);
        ui->biasTee->setChecked(m_settings.m_rxBiasTee);
        ui->decim->setCurrentIndex(m_settings.m_log2Decim);
        ui->label_decim->setText(QString("Dec"));
        ui->decim->setToolTip(QString("Decimation factor"));
        ui->gainMode->setEnabled(true);
        ui->fcPos->setCurrentIndex((int) m_settings.m_fcPosRx);

        if (m_streamIndex == 0) {
            ui->gainMode->setCurrentIndex(m_settings.m_rx0GainMode);
        } else if (m_streamIndex == 1) {
            ui->gainMode->setCurrentIndex(m_settings.m_rx1GainMode);
        }
    }
    else
    {
        ui->transverter->setDeltaFrequency(m_settings.m_txTransverterDeltaFrequency);
        ui->transverter->setDeltaFrequencyActive(m_settings.m_txTransverterMode);
        ui->transverter->setIQOrder(m_settings.m_iqOrder);
        ui->centerFrequency->setValueRange(7, m_fMinTx / 1000, m_fMaxTx / 1000);
        ui->centerFrequency->setValue(m_settings.m_txCenterFrequency / 1000);
        ui->bandwidth->setValueRange(5, m_bwMinTx / 1000, m_bwMaxTx / 1000);
        ui->bandwidth->setValue(m_settings.m_txBandwidth / 1000);
        uint32_t basebandSampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2Interp);
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(basebandSampleRate / 1000.0f, 'g', 5)));
        ui->dcOffset->setEnabled(false);
        ui->iqImbalance->setEnabled(false);
        ui->biasTee->setChecked(m_settings.m_txBiasTee);
        ui->decim->setCurrentIndex(m_settings.m_log2Interp);
        ui->label_decim->setText(QString("Int"));
        ui->decim->setToolTip(QString("Interpolation factor"));
        ui->gainMode->setEnabled(false);
        ui->fcPos->setCurrentIndex((int) m_settings.m_fcPosTx);
    }

    displayGain();

    ui->sampleRate->setValue(m_settings.m_devSampleRate);
    ui->LOppm->setValue(m_settings.m_LOppmTenths);
    ui->LOppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));

    displaySampleRate();
}

void BladeRF2MIMOGui::displaySampleRate()
{
    ui->sampleRate->blockSignals(true);
    displayFcTooltip();
    quint32 log2Factor = m_rxElseTx ? m_settings.m_log2Decim : m_settings.m_log2Interp;

    if (m_sampleRateMode)
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(60,60,60); }");
        ui->sampleRateMode->setText("SR");
        // BladeRF can go as low as 80 kS/s but because of buffering in practice experience is not good below 330 kS/s
        ui->sampleRate->setValueRange(8, m_srMin, m_srMax);
        ui->sampleRate->setValue(m_settings.m_devSampleRate);
        ui->sampleRate->setToolTip("Device to host sample rate (S/s)");
        ui->deviceRateText->setToolTip("Baseband sample rate (S/s)");
        uint32_t basebandSampleRate = m_settings.m_devSampleRate/(1<<log2Factor);
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(basebandSampleRate / 1000.0f, 'g', 5)));
    }
    else
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(50,50,50); }");
        ui->sampleRateMode->setText("BB");
        // BladeRF can go as low as 80 kS/s but because of buffering in practice experience is not good below 330 kS/s
        ui->sampleRate->setValueRange(8, m_srMin/(1<<log2Factor), m_srMax/(1<<log2Factor));
        ui->sampleRate->setValue(m_settings.m_devSampleRate/(1<<log2Factor));
        ui->sampleRate->setToolTip("Baseband sample rate (S/s)");
        ui->deviceRateText->setToolTip("Device to host sample rate (S/s)");
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(m_settings.m_devSampleRate / 1000.0f, 'g', 5)));
    }

    ui->sampleRate->blockSignals(false);
}

void BladeRF2MIMOGui::displayFcTooltip()
{
    int32_t fShift;

    if (m_rxElseTx)
    {
        fShift = DeviceSampleStatic::calculateSourceFrequencyShift(
            m_settings.m_log2Decim,
            (DeviceSampleStatic::fcPos_t) m_settings.m_fcPosRx,
            m_settings.m_devSampleRate,
            DeviceSampleStatic::FrequencyShiftScheme::FSHIFT_STD
        );
    }
    else
    {
        fShift = DeviceSampleStatic::calculateSinkFrequencyShift(
            m_settings.m_log2Interp,
            (DeviceSampleStatic::fcPos_t) m_settings.m_fcPosTx,
            m_settings.m_devSampleRate
        );
    }

    ui->fcPos->setToolTip(tr("Relative position of device center frequency: %1 kHz").arg(QString::number(fShift / 1000.0f, 'g', 5)));
}

void BladeRF2MIMOGui::displayGainModes()
{
    ui->gainMode->blockSignals(true);

    if (m_rxElseTx)
    {
        const std::vector<BladeRF2MIMO::GainMode>& modes = m_sampleMIMO->getRxGainModes();
        std::vector<BladeRF2MIMO::GainMode>::const_iterator it = modes.begin();

        for (; it != modes.end(); ++it) {
            ui->gainMode->addItem(it->m_name);
        }
    }
    else
    {
        ui->gainMode->clear();
        ui->gainMode->addItem("automatic");
    }

    ui->gainMode->blockSignals(false);
}

void BladeRF2MIMOGui::displayGain()
{
    int min, max, step, gainDB;
    float scale;

    if (m_rxElseTx)
    {
        m_sampleMIMO->getRxGlobalGainRange(min, max, step, scale);

        if (m_streamIndex == 0) {
            gainDB = m_settings.m_rx0GlobalGain;
        } else {
            gainDB = m_settings.m_rx1GlobalGain;
        }
    }
    else
    {
        m_sampleMIMO->getTxGlobalGainRange(min, max, step, scale);

        if (m_streamIndex == 0) {
            gainDB = m_settings.m_tx0GlobalGain;
        } else {
            gainDB = m_settings.m_tx1GlobalGain;
        }
    }

    ui->gain->setMinimum(min/step);
    ui->gain->setMaximum(max/step);
    ui->gain->setSingleStep(1);
    ui->gain->setPageStep(1);
    ui->gain->setValue(getGainValue(gainDB, min, max, step, scale));
    ui->gainText->setText(tr("%1 dB").arg(QString::number(gainDB, 'f', 2)));
}

bool BladeRF2MIMOGui::handleMessage(const Message& message)
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

        qDebug("BladeRF2MIMOGui::handleInputMessages: DSPMIMOSignalNotification: %s stream: %d SampleRate:%d, CenterFrequency:%llu",
                sourceOrSink ? "source" : "sink",
                istream,
                notif.getSampleRate(),
                notif.getCenterFrequency());

        updateSampleRateAndFrequency();

        return true;
    }
    else if (BladeRF2MIMO::MsgConfigureBladeRF2MIMO::match(message))
    {
        const BladeRF2MIMO::MsgConfigureBladeRF2MIMO& notif = (const BladeRF2MIMO::MsgConfigureBladeRF2MIMO&) message;

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

void BladeRF2MIMOGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (handleMessage(*message)) {
            delete message;
        } else {
            qDebug("BladeRF2MIMOGui::handleInputMessages: unhandled message: %s", message->getIdentifier());
        }
    }
}

void BladeRF2MIMOGui::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void BladeRF2MIMOGui::updateHardware()
{
    if (m_doApplySettings)
    {
        BladeRF2MIMO::MsgConfigureBladeRF2MIMO* message = BladeRF2MIMO::MsgConfigureBladeRF2MIMO::create(m_settings, m_settingsKeys, m_forceSettings);
        m_sampleMIMO->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void BladeRF2MIMOGui::updateSampleRateAndFrequency()
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

void BladeRF2MIMOGui::on_streamSide_currentIndexChanged(int index)
{
    m_rxElseTx = index == 0;
    displayGainModes();
    displaySettings();
}

void BladeRF2MIMOGui::on_streamIndex_currentIndexChanged(int index)
{
    m_streamIndex = index < 0 ? 0 : index > 1 ? 1 : index;
    displaySettings();
}

void BladeRF2MIMOGui::on_spectrumSide_currentIndexChanged(int index)
{
    m_spectrumRxElseTx = (index == 0);
    m_deviceUISet->m_spectrum->setDisplayedStream(m_spectrumRxElseTx, m_spectrumStreamIndex);
    m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(m_spectrumRxElseTx, m_spectrumStreamIndex);
    m_deviceUISet->setSpectrumScalingFactor(m_spectrumRxElseTx ? SDR_RX_SCALEF : SDR_TX_SCALEF);
    updateSampleRateAndFrequency();
}

void BladeRF2MIMOGui::on_spectrumIndex_currentIndexChanged(int index)
{
    m_spectrumStreamIndex = index < 0 ? 0 : index > 1 ? 1 : index;
    m_deviceUISet->m_spectrum->setDisplayedStream(m_spectrumRxElseTx, m_spectrumStreamIndex);
    m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(m_spectrumRxElseTx, m_spectrumStreamIndex);
    updateSampleRateAndFrequency();
}

void BladeRF2MIMOGui::on_startStopRx_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        BladeRF2MIMO::MsgStartStop *message = BladeRF2MIMO::MsgStartStop::create(checked, true);
        m_sampleMIMO->getInputMessageQueue()->push(message);
    }
}

void BladeRF2MIMOGui::on_startStopTx_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        BladeRF2MIMO::MsgStartStop *message = BladeRF2MIMO::MsgStartStop::create(checked, false);
        m_sampleMIMO->getInputMessageQueue()->push(message);
    }
}

void BladeRF2MIMOGui::on_centerFrequency_changed(quint64 value)
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

void BladeRF2MIMOGui::on_LOppm_valueChanged(int value)
{
    ui->LOppmText->setText(QString("%1").arg(QString::number(value/10.0, 'f', 1)));
    m_settings.m_LOppmTenths = value;
    m_settingsKeys.append("LOppmTenths");
    sendSettings();
}

void BladeRF2MIMOGui::on_dcOffset_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    m_settingsKeys.append("dcBlock");
    sendSettings();
}

void BladeRF2MIMOGui::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    m_settingsKeys.append("iqCorrection");
    sendSettings();
}

void BladeRF2MIMOGui::on_bandwidth_changed(quint64 value)
{
    if (m_rxElseTx)
    {
        m_settings.m_rxBandwidth = value * 1000;
        m_settingsKeys.append("rxBandwidth");
    }
    else
    {
        m_settings.m_txBandwidth = value * 1000;
        m_settingsKeys.append("txBandwidth");
    }

    sendSettings();
}

void BladeRF2MIMOGui::on_sampleRate_changed(quint64 value)
{
    if (m_sampleRateMode)
    {
        m_settings.m_devSampleRate = value;
    }
    else
    {
        if (m_rxElseTx) {
            m_settings.m_devSampleRate = value * (1 << m_settings.m_log2Decim);
        } else {
            m_settings.m_devSampleRate = value * (1 << m_settings.m_log2Interp);
        }
    }

    displaySampleRate();
    displayFcTooltip();
    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void BladeRF2MIMOGui::on_fcPos_currentIndexChanged(int index)
{
    if (m_rxElseTx)
    {
        m_settings.m_fcPosRx = (BladeRF2MIMOSettings::fcPos_t) (index < 0 ? 0 : index > 2 ? 2 : index);
        m_settingsKeys.append("fcPosRx");
    }
    else
    {
        m_settings.m_fcPosTx = (BladeRF2MIMOSettings::fcPos_t) (index < 0 ? 0 : index > 2 ? 2 : index);
        m_settingsKeys.append("fcPosTx");
    }

    displayFcTooltip();
    sendSettings();
}

void BladeRF2MIMOGui::on_decim_currentIndexChanged(int index)
{
    if ((index <0) || (index > 6)) {
        return;
    }

    if (m_rxElseTx)
    {
        m_settings.m_log2Decim = index;
        m_settingsKeys.append("log2Decim");
    }
    else
    {
        m_settings.m_log2Interp = index;
        m_settingsKeys.append("log2Interp");
    }

    displaySampleRate();

    if (m_sampleRateMode) {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew();
    } else {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew() * (1 << (m_rxElseTx ? m_settings.m_log2Decim : m_settings.m_log2Interp));
    }

    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void BladeRF2MIMOGui::on_gainLock_toggled(bool checked)
{
    if (!m_gainLock && checked)
    {
        m_settings.m_rx1GlobalGain = m_settings.m_rx0GlobalGain;
        m_settings.m_rx1GainMode = m_settings.m_rx0GainMode;
        m_settings.m_tx1GlobalGain = m_settings.m_tx0GlobalGain;
        m_settingsKeys.append("rx1GlobalGain");
        m_settingsKeys.append("rx1GainMode");
        m_settingsKeys.append("tx1GlobalGain");
        sendSettings();
    }

    m_gainLock = checked;
}

void BladeRF2MIMOGui::on_gainMode_currentIndexChanged(int index)
{
    if (!m_rxElseTx) { // not for Tx
        return;
    }

    const std::vector<BladeRF2MIMO::GainMode>& modes = m_sampleMIMO->getRxGainModes();
    unsigned int uindex = index < 0 ? 0 : (unsigned int) index;

    if (uindex < modes.size())
    {
        BladeRF2MIMO::GainMode mode = modes[index];

        if (m_streamIndex == 0 || m_gainLock)
        {
            if (m_settings.m_rx0GainMode != mode.m_value)
            {
                if (mode.m_value == BLADERF_GAIN_MANUAL)
                {
                    setGainFromValue(ui->gain->value());
                    ui->gain->setEnabled(true);
                } else {
                    ui->gain->setEnabled(false);
                }
            }

            m_settings.m_rx0GainMode = mode.m_value;
            m_settingsKeys.append("rx0GainMode");
        }

        if (m_streamIndex == 1 || m_gainLock)
        {
            if (m_settings.m_rx1GainMode != mode.m_value)
            {
                if (mode.m_value == BLADERF_GAIN_MANUAL)
                {
                    setGainFromValue(ui->gain->value());
                    ui->gain->setEnabled(true);
                } else {
                    ui->gain->setEnabled(false);
                }
            }

            m_settings.m_rx1GainMode = mode.m_value;
            m_settingsKeys.append("rx1GainMode");
        }

        sendSettings();
    }
}

void BladeRF2MIMOGui::on_gain_valueChanged(int value)
{
    float gainDB = setGainFromValue(value);
    ui->gainText->setText(tr("%1 dB").arg(QString::number(gainDB, 'f', 2)));
    sendSettings();
}

float BladeRF2MIMOGui::setGainFromValue(int value)
{
    int min, max, step;
    float scale, gainDB;

    if (m_rxElseTx)
    {
        m_sampleMIMO->getRxGlobalGainRange(min, max, step, scale);
        gainDB = getGainDB(value, min, max, step, scale);

        if (m_streamIndex == 0 || m_gainLock)
        {
            m_settings.m_rx0GlobalGain = (int) gainDB;
            m_settingsKeys.append("rx0GlobalGain");
        }
        if (m_streamIndex == 1 || m_gainLock)
        {
            m_settings.m_rx1GlobalGain = (int) gainDB;
            m_settingsKeys.append("rx1GlobalGain");
        }
    }
    else
    {
        m_sampleMIMO->getTxGlobalGainRange(min, max, step, scale);
        gainDB = getGainDB(value, min, max, step, scale);

        if (m_streamIndex == 0 || m_gainLock)
        {
            m_settings.m_tx0GlobalGain = (int) gainDB;
            m_settingsKeys.append("tx0GlobalGain");
        }
        if (m_streamIndex == 1 || m_gainLock)
        {
            m_settings.m_tx1GlobalGain = (int) gainDB;
            m_settingsKeys.append("tx1GlobalGain");
        }
    }

    return gainDB;
}

void BladeRF2MIMOGui::on_biasTee_toggled(bool checked)
{
    if (m_rxElseTx)
    {
        m_settings.m_rxBiasTee = checked;
        m_settingsKeys.append("rxBiasTee");
    }
    else
    {
        m_settings.m_txBiasTee = checked;
        m_settingsKeys.append("txBiasTee");
    }

    sendSettings();
}

void BladeRF2MIMOGui::on_transverter_clicked()
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
        qDebug("BladeRF2InputGui::on_transverter_clicked: Rx: %lld Hz %s", m_settings.m_rxTransverterDeltaFrequency, m_settings.m_rxTransverterMode ? "on" : "off");
    }
    else
    {
        m_settings.m_txTransverterMode = ui->transverter->getDeltaFrequencyAcive();
        m_settings.m_txTransverterDeltaFrequency = ui->transverter->getDeltaFrequency();
        m_settingsKeys.append("txTransverterMode");
        m_settingsKeys.append("txTransverterDeltaFrequency");
        m_settingsKeys.append("txCenterFrequency");
        qDebug("BladeRF2InputGui::on_transverter_clicked: Tx: %lld Hz %s", m_settings.m_txTransverterDeltaFrequency, m_settings.m_txTransverterMode ? "on" : "off");
    }

    updateFrequencyLimits();
    setCenterFrequencySetting(ui->centerFrequency->getValueNew());
    sendSettings();
}

void BladeRF2MIMOGui::updateFrequencyLimits()
{
    // values in kHz
    uint64_t f_min, f_max;
    int step;
    float scale;

    if (m_rxElseTx)
    {
        qint64 deltaFrequency = m_settings.m_rxTransverterMode ? m_settings.m_rxTransverterDeltaFrequency/1000 : 0;
        m_sampleMIMO->getRxFrequencyRange(f_min, f_max, step, scale);
        qint64 minLimit = f_min/1000 + deltaFrequency;
        qint64 maxLimit = f_max/1000 + deltaFrequency;

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
        qDebug("BladeRF2MIMOGui::updateFrequencyLimits: Rx: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);
    }
    else
    {
        qint64 deltaFrequency = m_settings.m_txTransverterMode ? m_settings.m_txTransverterDeltaFrequency/1000 : 0;
        m_sampleMIMO->getRxFrequencyRange(f_min, f_max, step, scale);
        qint64 minLimit = f_min/1000 + deltaFrequency;
        qint64 maxLimit = f_max/1000 + deltaFrequency;

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
        qDebug("BladeRF2MIMOGui::updateFrequencyLimits: Rx: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);
    }
}

void BladeRF2MIMOGui::setCenterFrequencySetting(uint64_t kHzValue)
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

    ui->centerFrequency->setToolTip(QString("Main center frequency in kHz (LO: %1 kHz)").arg(centerFrequency/1000));
}

void BladeRF2MIMOGui::updateStatus()
{
    int stateRx = m_deviceUISet->m_deviceAPI->state(0);
    int stateTx = m_deviceUISet->m_deviceAPI->state(1);

    if (m_lastRxEngineState != stateRx)
    {
        qDebug("BladeRF2MIMOGui::updateStatus: stateRx: %d", (int) stateRx);
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
        qDebug("BladeRF2MIMOGui::updateStatus: stateTx: %d", (int) stateTx);
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
}

void BladeRF2MIMOGui::openDeviceSettingsDialog(const QPoint& p)
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

float BladeRF2MIMOGui::getGainDB(int gainValue, int gainMin, int gainMax, int gainStep, float gainScale)
{
    float gain = gainValue * gainStep * gainScale;
    qDebug("BladeRF2MIMOGui::getGainDB: gainValue: %d gainMin: %d gainMax: %d gainStep: %d gainScale: %f gain: %f",
        gainValue, gainMin, gainMax, gainStep, gainScale, gain);
    return gain;
}

int BladeRF2MIMOGui::getGainValue(float gainDB, int gainMin, int gainMax, int gainStep, float gainScale)
{
    int gain = (gainDB/gainScale) / gainStep;
    qDebug("BladeRF2MIMOGui::getGainValue: gainDB: %f m_gainMin: %d m_gainMax: %d m_gainStep: %d gainScale: %f gain: %d",
        gainDB, gainMin, gainMax, gainStep, gainScale, gain);
    return gain;
}

void BladeRF2MIMOGui::makeUIConnections()
{
	QObject::connect(ui->streamSide, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BladeRF2MIMOGui::on_streamSide_currentIndexChanged);
	QObject::connect(ui->streamIndex, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BladeRF2MIMOGui::on_streamIndex_currentIndexChanged);
	QObject::connect(ui->spectrumSide, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BladeRF2MIMOGui::on_spectrumSide_currentIndexChanged);
	QObject::connect(ui->spectrumIndex, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BladeRF2MIMOGui::on_spectrumIndex_currentIndexChanged);
	QObject::connect(ui->startStopRx, &ButtonSwitch::toggled, this, &BladeRF2MIMOGui::on_startStopRx_toggled);
	QObject::connect(ui->startStopTx, &ButtonSwitch::toggled, this, &BladeRF2MIMOGui::on_startStopTx_toggled);
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &BladeRF2MIMOGui::on_centerFrequency_changed);
	QObject::connect(ui->LOppm, &QSlider::valueChanged, this, &BladeRF2MIMOGui::on_LOppm_valueChanged);
    QObject::connect(ui->dcOffset, &ButtonSwitch::toggled, this, &BladeRF2MIMOGui::on_dcOffset_toggled);
    QObject::connect(ui->iqImbalance, &ButtonSwitch::toggled, this, &BladeRF2MIMOGui::on_iqImbalance_toggled);
	QObject::connect(ui->bandwidth, &ValueDial::changed, this, &BladeRF2MIMOGui::on_bandwidth_changed);
	QObject::connect(ui->sampleRate, &ValueDial::changed, this, &BladeRF2MIMOGui::on_sampleRate_changed);
	QObject::connect(ui->fcPos, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BladeRF2MIMOGui::on_fcPos_currentIndexChanged);
	QObject::connect(ui->decim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BladeRF2MIMOGui::on_decim_currentIndexChanged);
    QObject::connect(ui->gainLock, &QToolButton::toggled, this, &BladeRF2MIMOGui::on_gainLock_toggled);
	QObject::connect(ui->gainMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BladeRF2MIMOGui::on_gainMode_currentIndexChanged);
	QObject::connect(ui->gain, &QSlider::valueChanged, this, &BladeRF2MIMOGui::on_gain_valueChanged);
	QObject::connect(ui->biasTee, &ButtonSwitch::toggled, this, &BladeRF2MIMOGui::on_biasTee_toggled);
	QObject::connect(ui->transverter, &TransverterButton::clicked, this, &BladeRF2MIMOGui::on_transverter_clicked);

}
