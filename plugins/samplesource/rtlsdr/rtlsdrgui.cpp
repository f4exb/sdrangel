///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#include "rtlsdrgui.h"

#include "device/deviceapi.h"
#include "device/deviceuiset.h"

#include "ui_rtlsdrgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"

RTLSDRGui::RTLSDRGui(DeviceUISet *deviceUISet, QWidget* parent) :
	DeviceGUI(parent),
	ui(new Ui::RTLSDRGui),
	m_doApplySettings(true),
	m_forceSettings(true),
	m_settings(),
    m_sampleRateMode(true),
	m_sampleSource(0)
{
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_sampleSource = (RTLSDRInput*) m_deviceUISet->m_deviceAPI->getSampleSource();

    ui->setupUi(getContents());
    sizeToContents();
    getContents()->setStyleSheet("#RTLSDRGui { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesource/rtlsdr/readme.md";
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	updateFrequencyLimits();

    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));

    ui->rfBW->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->rfBW->setValueRange(4, 0, 10000);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(deviceUISet->m_deviceAPI, &DeviceAPI::stateChanged, this, &RTLSDRGui::updateStatus);
	updateStatus();

	displaySettings();
    makeUIConnections();

	m_gains = m_sampleSource->getGains();
	displayGains();

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleSource->setMessageQueueToGUI(&m_inputMessageQueue);
}

RTLSDRGui::~RTLSDRGui()
{
    qDebug("RTLSDRGui::~RTLSDRGui");
	delete ui;
    qDebug("RTLSDRGui::~RTLSDRGui: end");
}

void RTLSDRGui::destroy()
{
	delete this;
}

void RTLSDRGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
    m_forceSettings = true;
	sendSettings();
}

void RTLSDRGui::on_dcOffset_toggled(bool checked)
{
	m_settings.m_dcBlock = checked;
    m_settingsKeys.append("dcBlock");
	sendSettings();
}

void RTLSDRGui::on_iqImbalance_toggled(bool checked)
{
	m_settings.m_iqImbalance = checked;
    m_settingsKeys.append("iqImbalance");
	sendSettings();
}

QByteArray RTLSDRGui::serialize() const
{
    return m_settings.serialize();
}

bool RTLSDRGui::deserialize(const QByteArray& data)
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

bool RTLSDRGui::handleMessage(const Message& message)
{
	if (RTLSDRInput::MsgConfigureRTLSDR::match(message))
	{
	    const RTLSDRInput::MsgConfigureRTLSDR& cfg = (RTLSDRInput::MsgConfigureRTLSDR&) message;

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
	else if (RTLSDRInput::MsgStartStop::match(message))
    {
        RTLSDRInput::MsgStartStop& notif = (RTLSDRInput::MsgStartStop&) message;
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

void RTLSDRGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("RTLSDRGui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("RTLSDRGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
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

void RTLSDRGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    displaySampleRate();
}

void RTLSDRGui::updateFrequencyLimits()
{
    // values in kHz
    qint64 deltaFrequency = m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency/1000 : 0;
    qint64 minLimit = (m_settings.m_noModMode ? RTLSDRInput::frequencyLowRangeMin : RTLSDRInput::frequencyHighRangeMin) + deltaFrequency;
    qint64 maxLimit = (m_settings.m_noModMode ? RTLSDRInput::frequencyLowRangeMax : RTLSDRInput::frequencyHighRangeMax) + deltaFrequency;

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
    qDebug("RTLSDRGui::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);
}

void RTLSDRGui::displayGains()
{
    if (m_gains.size() > 0)
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
        ui->gain->setMaximum(m_gains.size() - 1);
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

void RTLSDRGui::displaySampleRate()
{
    ui->sampleRate->blockSignals(true);
    displayFcTooltip();

    if (m_sampleRateMode)
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(60,60,60); }");
        ui->sampleRateMode->setText("SR");

        if (m_settings.m_lowSampleRate) {
            ui->sampleRate->setValueRange(7, RTLSDRInput::sampleRateLowRangeMin, RTLSDRInput::sampleRateLowRangeMax);
        } else {
            ui->sampleRate->setValueRange(7, RTLSDRInput::sampleRateHighRangeMin, RTLSDRInput::sampleRateHighRangeMax);
        }

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

        if (m_settings.m_lowSampleRate) {
            ui->sampleRate->setValueRange(7, RTLSDRInput::sampleRateLowRangeMin/(1<<m_settings.m_log2Decim), RTLSDRInput::sampleRateLowRangeMax/(1<<m_settings.m_log2Decim));
        } else {
            ui->sampleRate->setValueRange(7, RTLSDRInput::sampleRateHighRangeMin/(1<<m_settings.m_log2Decim), RTLSDRInput::sampleRateHighRangeMax/(1<<m_settings.m_log2Decim));
        }

        ui->sampleRate->setValue(m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim));
        ui->sampleRate->setToolTip("Baseband sample rate (S/s)");
        ui->deviceRateText->setToolTip("Device to host sample rate (S/s)");
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(m_settings.m_devSampleRate / 1000.0f, 'g', 5)));
    }

    ui->sampleRate->blockSignals(false);
}

void RTLSDRGui::displayFcTooltip()
{
    int32_t fShift = DeviceSampleSource::calculateFrequencyShift(
        m_settings.m_log2Decim,
        (DeviceSampleSource::fcPos_t) m_settings.m_fcPos,
        m_settings.m_devSampleRate,
        DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD
    );
    ui->fcPos->setToolTip(tr("Relative position of device center frequency: %1 kHz").arg(QString::number(fShift / 1000.0f, 'g', 5)));
}

void RTLSDRGui::displaySettings()
{
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
	ui->lowSampleRate->setChecked(m_settings.m_lowSampleRate);
	ui->offsetTuning->setChecked(m_settings.m_offsetTuning);
	ui->biasT->setChecked(m_settings.m_biasTee);
}

void RTLSDRGui::sendSettings()
{
	if (!m_updateTimer.isActive()) {
		m_updateTimer.start(100);
	}
}

void RTLSDRGui::on_centerFrequency_changed(quint64 value)
{
	m_settings.m_centerFrequency = value * 1000;
    m_settingsKeys.append("centerFrequency");
	sendSettings();
}

void RTLSDRGui::on_decim_currentIndexChanged(int index)
{
	if ((index <0) || (index > 6)) {
		return;
	}

	m_settings.m_log2Decim = index;
    displaySampleRate();

    if (m_sampleRateMode) {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew();
    } else {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew() * (1 << m_settings.m_log2Decim);
    }

    m_settingsKeys.append("log2Decim");
    m_settingsKeys.append("devSampleRate");
	sendSettings();
}

void RTLSDRGui::on_fcPos_currentIndexChanged(int index)
{
    m_settings.m_fcPos = (RTLSDRSettings::fcPos_t) (index < 0 ? 0 : index > 2 ? 2 : index);
    displayFcTooltip();
    m_settingsKeys.append("fcPos");
    sendSettings();
}

void RTLSDRGui::on_ppm_valueChanged(int value)
{
	if ((value > 200) || (value < -200)) {
		return;
	}

	ui->ppmText->setText(tr("%1").arg(value));
	m_settings.m_loPpmCorrection = value;
    m_settingsKeys.append("loPpmCorrection");

	sendSettings();
}

void RTLSDRGui::on_gain_valueChanged(int value)
{
	if (value > (int)m_gains.size()) {
		return;
	}

	int gain = m_gains[value];
	ui->gainText->setText(tr("%1.%2").arg(gain / 10).arg(abs(gain % 10)));
	m_settings.m_gain = gain;
    m_settingsKeys.append("gain");

	sendSettings();
}

void RTLSDRGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        RTLSDRInput::MsgStartStop *message = RTLSDRInput::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void RTLSDRGui::on_transverter_clicked()
{
    m_settings.m_transverterMode = ui->transverter->getDeltaFrequencyAcive();
    m_settings.m_transverterDeltaFrequency = ui->transverter->getDeltaFrequency();
    m_settings.m_iqOrder = ui->transverter->getIQOrder();
    qDebug("RTLSDRGui::on_transverter_clicked: %lld Hz %s", m_settings.m_transverterDeltaFrequency, m_settings.m_transverterMode ? "on" : "off");
    updateFrequencyLimits();
    m_settings.m_centerFrequency = ui->centerFrequency->getValueNew()*1000;
    m_settingsKeys.append("transverterMode");
    m_settingsKeys.append("transverterDeltaFrequency");
    m_settingsKeys.append("iqOrder");
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void RTLSDRGui::on_sampleRateMode_toggled(bool checked)
{
    m_sampleRateMode = checked;
    displaySampleRate();
}

void RTLSDRGui::on_biasT_stateChanged(int state)
{
	m_settings.m_biasTee = (state == Qt::Checked);
    m_settingsKeys.append("biasTee");
	sendSettings();
}

void RTLSDRGui::updateHardware()
{
    if (m_doApplySettings)
    {
        RTLSDRInput::MsgConfigureRTLSDR* message = RTLSDRInput::MsgConfigureRTLSDR::create(m_settings, m_settingsKeys, m_forceSettings);
        m_sampleSource->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void RTLSDRGui::updateStatus()
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
            QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceAPI->errorMessage());
            break;
        default:
            break;
    }
}

void RTLSDRGui::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void RTLSDRGui::on_checkBox_stateChanged(int state)
{
	if (state == Qt::Checked)
	{
		ui->gain->setEnabled(false);
        m_settings.m_noModMode = true;
	    updateFrequencyLimits();
		ui->centerFrequency->setValue(7000);
		m_settings.m_centerFrequency = 7000 * 1000;
	}
	else
	{
		ui->gain->setEnabled(true);
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

void RTLSDRGui::on_agc_stateChanged(int state)
{
    m_settings.m_agc = (state == Qt::Checked);
    m_settingsKeys.append("agc");
    sendSettings();
}

void RTLSDRGui::on_sampleRate_changed(quint64 value)
{
    if (m_sampleRateMode) {
        m_settings.m_devSampleRate = value;
    } else {
        m_settings.m_devSampleRate = value * (1 << m_settings.m_log2Decim);
    }

    displayFcTooltip();
    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void RTLSDRGui::on_offsetTuning_toggled(bool checked)
{
    m_settings.m_offsetTuning = checked;
    m_settingsKeys.append("offsetTuning");
    sendSettings();
}

void RTLSDRGui::on_rfBW_changed(quint64 value)
{
    m_settings.m_rfBandwidth = value * 1000;
    m_settingsKeys.append("rfBandwidth");
    sendSettings();
}

void RTLSDRGui::on_lowSampleRate_toggled(bool checked)
{
    m_settings.m_lowSampleRate = checked;

    displaySampleRate();

    if (m_sampleRateMode) {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew();
    } else {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew() * (1 << m_settings.m_log2Decim);
    }

    qDebug("RTLSDRGui::on_lowSampleRate_toggled: %d S/s", m_settings.m_devSampleRate);

    m_settingsKeys.append("lowSampleRate");
    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void RTLSDRGui::openDeviceSettingsDialog(const QPoint& p)
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

void RTLSDRGui::makeUIConnections()
{
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &RTLSDRGui::on_centerFrequency_changed);
    QObject::connect(ui->sampleRate, &ValueDial::changed, this, &RTLSDRGui::on_sampleRate_changed);
    QObject::connect(ui->offsetTuning, &QCheckBox::toggled, this, &RTLSDRGui::on_offsetTuning_toggled);
    QObject::connect(ui->rfBW, &ValueDial::changed, this, &RTLSDRGui::on_rfBW_changed);
    QObject::connect(ui->lowSampleRate, &ButtonSwitch::toggled, this, &RTLSDRGui::on_lowSampleRate_toggled);
    QObject::connect(ui->dcOffset, &ButtonSwitch::toggled, this, &RTLSDRGui::on_dcOffset_toggled);
    QObject::connect(ui->iqImbalance, &ButtonSwitch::toggled, this, &RTLSDRGui::on_iqImbalance_toggled);
    QObject::connect(ui->decim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RTLSDRGui::on_decim_currentIndexChanged);
    QObject::connect(ui->fcPos, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RTLSDRGui::on_fcPos_currentIndexChanged);
    QObject::connect(ui->ppm, &QSlider::valueChanged, this, &RTLSDRGui::on_ppm_valueChanged);
    QObject::connect(ui->gain, &QSlider::valueChanged, this, &RTLSDRGui::on_gain_valueChanged);
    QObject::connect(ui->checkBox, &QCheckBox::stateChanged, this, &RTLSDRGui::on_checkBox_stateChanged);
    QObject::connect(ui->agc, &QCheckBox::stateChanged, this, &RTLSDRGui::on_agc_stateChanged);
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &RTLSDRGui::on_startStop_toggled);
    QObject::connect(ui->transverter, &TransverterButton::clicked, this, &RTLSDRGui::on_transverter_clicked);
    QObject::connect(ui->sampleRateMode, &QToolButton::toggled, this, &RTLSDRGui::on_sampleRateMode_toggled);
    QObject::connect(ui->biasT, &QCheckBox::stateChanged, this, &RTLSDRGui::on_biasT_stateChanged);
}
