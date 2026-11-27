///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014-2015 John Greb <hexameron@spam.no>                         //
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2021 Kacper Michajłow <kasper93@gmail.com>                      //
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

#include "rtlsdr2gui.h"

#include "device/deviceapi.h"
#include "device/deviceuiset.h"

#include "ui_rtlsdr2gui.h"
#include "gui/messagedialog.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "mainspectrum/mainspectrumgui.h"
#include "gui/basicdevicesettingsdialog.h"
#include "gui/dialogpositioner.h"
#include "dsp/dspcommands.h"

// The RTL2832U resampler accepts 225001-300000 and 900001-3200000 S/s.
// Crossing the gap wraps to the neighbouring band: down from the normal band
// lands at 300000, up from the low band lands at 900001.
static quint64 snapToValidDeviceRate(quint64 currentRate, quint64 proposedRate)
{
    const quint64 lowMin  = RTLSDR2Input::sampleRateLowRangeMin;
    const quint64 lowMax  = RTLSDR2Input::sampleRateLowRangeMax;
    const quint64 highMin = RTLSDR2Input::sampleRateHighRangeMin;
    const quint64 highMax = RTLSDR2Input::sampleRateHighRangeMax;

    if (proposedRate >= lowMin && proposedRate <= lowMax) {
        return proposedRate;
    }
    if (proposedRate >= highMin && proposedRate <= highMax) {
        return proposedRate;
    }
    if (proposedRate > highMax) {
        return highMax;
    }
    if (currentRate >= highMin && proposedRate < highMin) {
        return lowMax;   // downward past the gap: top of the low band
    }
    if (currentRate <= lowMax && proposedRate > lowMax) {
        return highMin;  // upward past the gap: bottom of the normal band
    }
    if (proposedRate < lowMin) {
        return lowMin;
    }
    return (proposedRate - lowMax) <= (highMin - proposedRate) ? lowMax : highMin;
}

RTLSDR2Gui::RTLSDR2Gui(DeviceUISet *deviceUISet, QWidget* parent) :
	DeviceGUI(parent),
	ui(new Ui::RTLSDR2Gui),
	m_doApplySettings(true),
	m_forceSettings(true),
	m_settings(),
	m_sampleSource(nullptr)
{
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_sampleSource = (RTLSDR2Input*) m_deviceUISet->m_deviceAPI->getSampleSource();

    ui->setupUi(getContents());
    sizeToContents();
    getContents()->setStyleSheet("#RTLSDR2Gui { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesource/rtlsdr/readme.md";
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	updateFrequencyLimits();

    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->basebandRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));

    ui->rfBW->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->rfBW->setValueRange(4, 0, 10000);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(deviceUISet->m_deviceAPI, &DeviceAPI::stateChanged, this, &RTLSDR2Gui::updateStatus);
	updateStatus();

	displaySettings();
    makeUIConnections();
    m_resizer.enableChildMouseTracking();

	m_gains = m_sampleSource->getGains();
	displayGains();

    rtlsdr_tuner tunerType = m_sampleSource->getTunerType();
    // Disable widgets not relevant for this tuner
    bool offsetTuningEnabled = (tunerType != RTLSDR_TUNER_R820T) && (tunerType != RTLSDR_TUNER_R828D);
    if (!offsetTuningEnabled) {
        ui->offsetTuning->setEnabled(false);
    }
    ui->tunerType->setText("Tuner: " + m_sampleSource->getTunerName());

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleSource->setMessageQueueToGUI(&m_inputMessageQueue);
}

RTLSDR2Gui::~RTLSDR2Gui()
{
    qDebug("RTLSDR2Gui::~RTLSDR2Gui");
	delete ui;
    qDebug("RTLSDR2Gui::~RTLSDR2Gui: end");
}

void RTLSDR2Gui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
    m_forceSettings = true;
	sendSettings();
}

void RTLSDR2Gui::on_dcOffset_toggled(bool checked)
{
	m_settings.m_dcBlock = checked;
    m_settingsKeys.append("dcBlock");
	sendSettings();
}

void RTLSDR2Gui::on_iqImbalance_toggled(bool checked)
{
	m_settings.m_iqImbalance = checked;
    m_settingsKeys.append("iqImbalance");
	sendSettings();
}

QByteArray RTLSDR2Gui::serialize() const
{
    return m_settings.serialize();
}

bool RTLSDR2Gui::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        displayGains();
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

bool RTLSDR2Gui::handleMessage(const Message& message)
{
	if (RTLSDR2Input::MsgConfigureRTLSDR2::match(message))
	{
        auto& cfg = (const RTLSDR2Input::MsgConfigureRTLSDR2&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        blockApplySettings(true);
        displayGains();
        displaySettings();
        blockApplySettings(false);
        return true;
	}
	else if (RTLSDR2Input::MsgStartStop::match(message))
    {
        auto& notif = (const RTLSDR2Input::MsgStartStop&) message;
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

void RTLSDR2Gui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        qDebug("RTLSDR2Gui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            auto* notif = (const DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("RTLSDR2Gui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();

            delete message;
        }
        else
        {
            if (!handleMessage(*message))
            {
                qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
            }
            delete message;
        }
    }
}

void RTLSDR2Gui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    displaySampleRate();
}

void RTLSDR2Gui::updateFrequencyLimits()
{
    // values in kHz
    qint64 deltaFrequency = m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency/1000 : 0;
    qint64 minLimit = (m_settings.m_noModMode ? RTLSDR2Input::frequencyLowRangeMin : m_sampleSource->getFrequencyHighRangeMin()) + deltaFrequency;
    qint64 maxLimit = (m_settings.m_noModMode ? RTLSDR2Input::frequencyLowRangeMax : RTLSDR2Input::frequencyHighRangeMax) + deltaFrequency;

    if (m_settings.m_transverterMode)
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
    qDebug("RTLSDR2Gui::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);
}

void RTLSDR2Gui::displayGains()
{
    if (!m_gains.empty())
    {
        int dist = abs(m_settings.m_gain - m_gains[0]);
        int pos = 0;

        for (uint i = 1; i < m_gains.size(); i++)
        {
            if (abs(m_settings.m_gain - m_gains[i]) < dist)
            {
                dist = abs(m_settings.m_gain - m_gains[i]);
                pos = i;
            }
        }

        ui->gainText->setText(tr("%1.%2").arg(m_gains[pos] / 10).arg(abs(m_gains[pos] % 10)));
        ui->gain->setMaximum((int) (m_gains.size() - 1));
        ui->gain->setEnabled(true);
        ui->gain->setValue(pos);
    }
    else
    {
        ui->gain->setMaximum(0);
        ui->gain->setEnabled(false);
        ui->gain->setValue(0);
    }
}

void RTLSDR2Gui::displaySampleRate()
{
    ui->sampleRate->blockSignals(true);
    ui->basebandRate->blockSignals(true);
    displayFcTooltip();

    ui->sampleRate->setValueRange(7, RTLSDR2Input::sampleRateLowRangeMin, RTLSDR2Input::sampleRateHighRangeMax);
    ui->sampleRate->setValue(m_settings.m_devSampleRate);
    ui->sampleRate->setToolTip(tr("Device-to-host IQ sample rate (S/s). Valid: 225001-300000 or 900001-3200000. Editing this box sets the device rate."));

    const int decimation = 1 << m_settings.m_log2Decim;
    const quint32 basebandSampleRate = m_settings.m_devSampleRate / decimation;
    ui->basebandRate->setValueRange(7, 0, RTLSDR2Input::sampleRateHighRangeMax);
    ui->basebandRate->setValue(basebandSampleRate);
    ui->basebandRate->setToolTip(tr("Baseband rate (S/s) = device rate / %1. Editing this box updates the device rate above.").arg(decimation));

    ui->deviceRateText->setToolTip(tr("Baseband sample rate (kS/s) = device rate / decimation: the rate delivered to the channels."));
    ui->deviceRateText->setText(tr("%1k").arg(QString::number((float) basebandSampleRate / 1000.0f, 'g', 5)));

    ui->sampleRate->blockSignals(false);
    ui->basebandRate->blockSignals(false);
}

void RTLSDR2Gui::displayFcTooltip()
{
    int32_t fShift = DeviceSampleSource::calculateFrequencyShift(
        m_settings.m_log2Decim,
        (DeviceSampleSource::fcPos_t) m_settings.m_fcPos,
        m_settings.m_devSampleRate,
        DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD
    );
    ui->fcPos->setToolTip(tr("Relative position of device center frequency: %1 kHz").arg(QString::number((float) fShift / 1000.0f, 'g', 5)));
}

void RTLSDR2Gui::displaySettings()
{
    setTitle(m_settings.m_title);
    getDeviceUISet()->m_mainSpectrumGUI->setTitle(m_settings.m_title);
    ui->transverter->setDeltaFrequency(m_settings.m_transverterDeltaFrequency);
    ui->transverter->setDeltaFrequencyActive(m_settings.m_transverterMode);
    ui->transverter->setIQOrder(m_settings.m_iqOrder);
    updateFrequencyLimits();
	ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
	displaySampleRate();
	ui->rfBW->setValue(m_settings.m_rfBandwidth / 1000);
	ui->dcOffset->setChecked(m_settings.m_dcBlock);
	ui->iqImbalance->setChecked(m_settings.m_iqImbalance);
	ui->ppm->setValue(m_settings.m_loPpmCorrection);
	ui->ppmText->setText(tr("%1").arg(m_settings.m_loPpmCorrection));
	ui->decim->setCurrentIndex(m_settings.m_log2Decim);
	ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);
	ui->checkBox->setChecked(m_settings.m_noModMode);
	ui->agc->setChecked(m_settings.m_agc);
	ui->tunerAgc->setChecked(m_settings.m_tunerAgc);
	ui->gain->setEnabled(!m_settings.m_tunerAgc);
	ui->gain->setStyleSheet(m_settings.m_tunerAgc ?
	    "QSlider::groove:horizontal { background: #404040; } QSlider::handle:horizontal { background: #707070; }" : "");
	ui->offsetTuning->setChecked(m_settings.m_offsetTuning);
	ui->biasT->setChecked(m_settings.m_biasTee);
    displayReplayLength();
    displayReplayOffset();
    displayReplayStep();
    ui->replayLoop->setChecked(m_settings.m_replayLoop);
}

void RTLSDR2Gui::sendSettings()
{
	if (!m_updateTimer.isActive()) {
		m_updateTimer.start(100);
	}
}

void RTLSDR2Gui::on_centerFrequency_changed(quint64 value)
{
	m_settings.m_centerFrequency = value * 1000;
    m_settingsKeys.append("centerFrequency");
	sendSettings();
}

void RTLSDR2Gui::on_decim_currentIndexChanged(int index)
{
	if ((index <0) || (index > 6)) {
		return;
	}

	m_settings.m_log2Decim = index;
    displaySampleRate();

    m_settingsKeys.append("log2Decim");
    m_settingsKeys.append("devSampleRate");
	sendSettings();
}

void RTLSDR2Gui::on_fcPos_currentIndexChanged(int index)
{
    m_settings.m_fcPos = (RTLSDR2Settings::fcPos_t) (index < 0 ? 0 : index > 2 ? 2 : index);
    displayFcTooltip();
    m_settingsKeys.append("fcPos");
    sendSettings();
}

void RTLSDR2Gui::on_ppm_valueChanged(int value)
{
	if ((value > 200) || (value < -200)) {
		return;
	}

	ui->ppmText->setText(tr("%1").arg(value));
	m_settings.m_loPpmCorrection = value;
    m_settingsKeys.append("loPpmCorrection");

	sendSettings();
}

void RTLSDR2Gui::on_gain_valueChanged(int value)
{
	if (value < 0 || value >= (int)m_gains.size()) {
		return;
	}

	int gain = m_gains[value];
	ui->gainText->setText(tr("%1.%2").arg(gain / 10).arg(abs(gain % 10)));
	m_settings.m_gain = gain;
    m_settingsKeys.append("gain");

	sendSettings();
}

void RTLSDR2Gui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        RTLSDR2Input::MsgStartStop *message = RTLSDR2Input::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void RTLSDR2Gui::on_transverter_clicked()
{
    m_settings.m_transverterMode = ui->transverter->getDeltaFrequencyAcive();
    m_settings.m_transverterDeltaFrequency = ui->transverter->getDeltaFrequency();
    m_settings.m_iqOrder = ui->transverter->getIQOrder();
    qDebug("RTLSDR2Gui::on_transverter_clicked: %lld Hz %s", m_settings.m_transverterDeltaFrequency, m_settings.m_transverterMode ? "on" : "off");
    updateFrequencyLimits();
    m_settings.m_centerFrequency = ui->centerFrequency->getValueNew()*1000;
    m_settingsKeys.append("transverterMode");
    m_settingsKeys.append("transverterDeltaFrequency");
    m_settingsKeys.append("iqOrder");
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}


void RTLSDR2Gui::on_biasT_stateChanged(int state)
{
	m_settings.m_biasTee = (state == Qt::Checked);
    m_settingsKeys.append("biasTee");
	sendSettings();
}

void RTLSDR2Gui::updateHardware()
{
    if (m_doApplySettings)
    {
        RTLSDR2Input::MsgConfigureRTLSDR2* message = RTLSDR2Input::MsgConfigureRTLSDR2::create(m_settings, m_settingsKeys, m_forceSettings);
        m_sampleSource->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void RTLSDR2Gui::updateStatus()
{
    int state = m_deviceUISet->m_deviceAPI->state();

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
}

void RTLSDR2Gui::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void RTLSDR2Gui::on_checkBox_stateChanged(int state)
{
	if (state == Qt::Checked)
	{
		ui->gain->setEnabled(false);
        ui->offsetTuning->setEnabled(false);
        m_settings.m_noModMode = true;
	    updateFrequencyLimits();
		ui->centerFrequency->setValue(7000);
		m_settings.m_centerFrequency = 7000 * 1000;
	}
	else
	{
		ui->gain->setEnabled(true);
        ui->offsetTuning->setEnabled(true);
        m_settings.m_noModMode = false;
	    updateFrequencyLimits();
		ui->centerFrequency->setValue(434000);
		ui->gain->setValue(0);
		m_settings.m_centerFrequency = 435000 * 1000;
	}

    m_settingsKeys.append("noModMode");
    m_settingsKeys.append("centerFrequency");
	sendSettings();
}

void RTLSDR2Gui::on_agc_stateChanged(int state)
{
    m_settings.m_agc = (state == Qt::Checked);
    m_settingsKeys.append("agc");
    sendSettings();
}

void RTLSDR2Gui::on_tunerAgc_stateChanged(int state)
{
    m_settings.m_tunerAgc = (state == Qt::Checked);
    ui->gain->setEnabled(!m_settings.m_tunerAgc);
    ui->gain->setStyleSheet(m_settings.m_tunerAgc ?
        "QSlider::groove:horizontal { background: #404040; } QSlider::handle:horizontal { background: #707070; }" : "");
    m_settingsKeys.append("tunerAgc");
    sendSettings();
}

void RTLSDR2Gui::on_sampleRate_changed(quint64 value)
{
    const quint64 snapped = snapToValidDeviceRate(m_settings.m_devSampleRate, value);

    if (snapped != value)
    {
        ui->sampleRate->blockSignals(true);
        ui->sampleRate->setValue(snapped);
        ui->sampleRate->blockSignals(false);
    }

    m_settings.m_devSampleRate = snapped;
    m_settings.m_lowSampleRate = snapped <= RTLSDR2Input::sampleRateLowRangeMax;
    displaySampleRate();
    displayFcTooltip();
    m_settingsKeys.append("devSampleRate");
    m_settingsKeys.append("lowSampleRate");
    sendSettings();
}

void RTLSDR2Gui::on_basebandRate_changed(quint64 value)
{
    const int decimation = 1 << m_settings.m_log2Decim;
    const quint64 proposedRate = value * decimation;
    const quint64 snapped = snapToValidDeviceRate(m_settings.m_devSampleRate, proposedRate);

    if (snapped != proposedRate)
    {
        ui->basebandRate->blockSignals(true);
        ui->basebandRate->setValue(snapped / decimation);
        ui->basebandRate->blockSignals(false);
    }

    m_settings.m_devSampleRate = snapped;
    m_settings.m_lowSampleRate = snapped <= RTLSDR2Input::sampleRateLowRangeMax;
    displaySampleRate();
    displayFcTooltip();
    m_settingsKeys.append("devSampleRate");
    m_settingsKeys.append("lowSampleRate");
    sendSettings();
}

void RTLSDR2Gui::on_offsetTuning_toggled(bool checked)
{
    m_settings.m_offsetTuning = checked;
    m_settingsKeys.append("offsetTuning");
    sendSettings();
}

void RTLSDR2Gui::on_rfBW_changed(quint64 value)
{
    m_settings.m_rfBandwidth = (quint32) (value * 1000);
    m_settingsKeys.append("rfBandwidth");
    sendSettings();
}

void RTLSDR2Gui::openDeviceSettingsDialog(const QPoint& p)
{
    if (m_contextMenuType == ContextMenuDeviceSettings)
    {
        BasicDeviceSettingsDialog dialog(this);
        dialog.setReplayBytesPerSecond(m_settings.m_devSampleRate * 2);
        dialog.setReplayLength(m_settings.m_replayLength);
        dialog.setReplayStep(m_settings.m_replayStep);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setDefaultTitle(getDefaultTitle());
        dialog.setTitle(m_settings.m_title);

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        if (dialog.result() == QDialog::Accepted)
        {
            m_settings.m_useReverseAPI = dialog.useReverseAPI();
            m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
            m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
            m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
            m_settings.m_replayLength = dialog.getReplayLength();
            m_settings.m_replayStep = dialog.getReplayStep();
            m_settings.m_title = dialog.getTitle();
            setTitle(m_settings.m_title);
            getDeviceUISet()->m_mainSpectrumGUI->setTitle(m_settings.m_title);
            displayReplayLength();
            displayReplayOffset();
            displayReplayStep();
            m_settingsKeys.append("useReverseAPI");
            m_settingsKeys.append("reverseAPIAddress");
            m_settingsKeys.append("reverseAPIPort");
            m_settingsKeys.append("reverseAPIDeviceIndex");
            m_settingsKeys.append("replayLength");
            m_settingsKeys.append("replayStep");
            m_settingsKeys.append("title");

            sendSettings();
        }
    }

    resetContextMenuType();
}

void RTLSDR2Gui::displayReplayLength()
{
    bool replayEnabled = m_settings.m_replayLength > 0.0f;
    if (!replayEnabled) {
        ui->replayOffset->setMaximum(0);
    } else {
        ui->replayOffset->setMaximum((int) (m_settings.m_replayLength * 10 - 1));
    }
    ui->replayLabel->setEnabled(replayEnabled);
    ui->replayOffset->setEnabled(replayEnabled);
    ui->replayOffsetText->setEnabled(replayEnabled);
    ui->replaySave->setEnabled(replayEnabled);
}

void RTLSDR2Gui::displayReplayOffset()
{
    bool replayEnabled = m_settings.m_replayLength > 0.0f;
    ui->replayOffset->setValue((int) (m_settings.m_replayOffset * 10));
    ui->replayOffsetText->setText(QString("%1s").arg(m_settings.m_replayOffset, 0, 'f', 1));
    ui->replayNow->setEnabled(replayEnabled && (m_settings.m_replayOffset > 0.0f));
    ui->replayPlus->setEnabled(replayEnabled && (std::round(m_settings.m_replayOffset * 10) < (float) ui->replayOffset->maximum()));
    ui->replayMinus->setEnabled(replayEnabled && (m_settings.m_replayOffset > 0.0f));
}

void RTLSDR2Gui::displayReplayStep()
{
    QString step;
    float intpart;
    float frac = modf(m_settings.m_replayStep, &intpart);
    if (frac == 0.0f) {
        step = QString::number((int)intpart);
    } else {
        step = QString::number(m_settings.m_replayStep, 'f', 1);
    }
    ui->replayPlus->setText(QString("+%1s").arg(step));
    ui->replayPlus->setToolTip(QString("Add %1 seconds to time delay").arg(step));
    ui->replayMinus->setText(QString("-%1s").arg(step));
    ui->replayMinus->setToolTip(QString("Remove %1 seconds from time delay").arg(step));
}

void RTLSDR2Gui::on_replayOffset_valueChanged(int value)
{
    m_settings.m_replayOffset = (float) value / 10.0f;
    displayReplayOffset();
    m_settingsKeys.append("replayOffset");
    sendSettings();
}

void RTLSDR2Gui::on_replayNow_clicked()
{
    ui->replayOffset->setValue(0);
}

void RTLSDR2Gui::on_replayPlus_clicked()
{
    ui->replayOffset->setValue((int) ((float) ui->replayOffset->value() + m_settings.m_replayStep * 10));
}

void RTLSDR2Gui::on_replayMinus_clicked()
{
    ui->replayOffset->setValue((int) ((float) ui->replayOffset->value() - m_settings.m_replayStep * 10));
}

void RTLSDR2Gui::on_replaySave_clicked()
{
    QFileDialog fileDialog(nullptr, "Select file to save IQ data to", "", "*.wav");
    fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    if (fileDialog.exec())
    {
        QStringList fileNames = fileDialog.selectedFiles();
        if (!fileNames.empty())
        {
            RTLSDR2Input::MsgSaveReplay *message = RTLSDR2Input::MsgSaveReplay::create(fileNames[0]);
            m_sampleSource->getInputMessageQueue()->push(message);
        }
    }
}

void RTLSDR2Gui::on_replayLoop_toggled(bool checked)
{
    m_settings.m_replayLoop = checked;
    m_settingsKeys.append("replayLoop");
    sendSettings();
}

void RTLSDR2Gui::setReplayTime(float time)
{
    ui->replayOffset->setValue((int) std::ceil(time * 10.0f));
}

void RTLSDR2Gui::makeUIConnections() const
{
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &RTLSDR2Gui::on_centerFrequency_changed);
    QObject::connect(ui->sampleRate, &ValueDial::changed, this, &RTLSDR2Gui::on_sampleRate_changed);
    QObject::connect(ui->basebandRate, &ValueDial::changed, this, &RTLSDR2Gui::on_basebandRate_changed);
    QObject::connect(ui->offsetTuning, &QCheckBox::toggled, this, &RTLSDR2Gui::on_offsetTuning_toggled);
    QObject::connect(ui->rfBW, &ValueDial::changed, this, &RTLSDR2Gui::on_rfBW_changed);
    QObject::connect(ui->dcOffset, &ButtonSwitch::toggled, this, &RTLSDR2Gui::on_dcOffset_toggled);
    QObject::connect(ui->iqImbalance, &ButtonSwitch::toggled, this, &RTLSDR2Gui::on_iqImbalance_toggled);
    QObject::connect(ui->decim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RTLSDR2Gui::on_decim_currentIndexChanged);
    QObject::connect(ui->fcPos, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RTLSDR2Gui::on_fcPos_currentIndexChanged);
    QObject::connect(ui->ppm, &QSlider::valueChanged, this, &RTLSDR2Gui::on_ppm_valueChanged);
    QObject::connect(ui->gain, &QSlider::valueChanged, this, &RTLSDR2Gui::on_gain_valueChanged);
    QObject::connect(ui->checkBox, &QCheckBox::stateChanged, this, &RTLSDR2Gui::on_checkBox_stateChanged);
    QObject::connect(ui->agc, &QCheckBox::stateChanged, this, &RTLSDR2Gui::on_agc_stateChanged);
    QObject::connect(ui->tunerAgc, &QCheckBox::stateChanged, this, &RTLSDR2Gui::on_tunerAgc_stateChanged);
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &RTLSDR2Gui::on_startStop_toggled);
    QObject::connect(ui->transverter, &TransverterButton::clicked, this, &RTLSDR2Gui::on_transverter_clicked);
    QObject::connect(ui->biasT, &QCheckBox::stateChanged, this, &RTLSDR2Gui::on_biasT_stateChanged);
    QObject::connect(ui->replayOffset, &QSlider::valueChanged, this, &RTLSDR2Gui::on_replayOffset_valueChanged);
    QObject::connect(ui->replayNow, &QToolButton::clicked, this, &RTLSDR2Gui::on_replayNow_clicked);
    QObject::connect(ui->replayPlus, &QToolButton::clicked, this, &RTLSDR2Gui::on_replayPlus_clicked);
    QObject::connect(ui->replayMinus, &QToolButton::clicked, this, &RTLSDR2Gui::on_replayMinus_clicked);
    QObject::connect(ui->replaySave, &QToolButton::clicked, this, &RTLSDR2Gui::on_replaySave_clicked);
    QObject::connect(ui->replayLoop, &ButtonSwitch::toggled, this, &RTLSDR2Gui::on_replayLoop_toggled);
}
