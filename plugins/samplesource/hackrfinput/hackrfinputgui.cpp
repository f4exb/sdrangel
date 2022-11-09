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

#include "hackrfinputgui.h"

#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>

#include <libhackrf/hackrf.h>

#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "hackrf/devicehackrfvalues.h"

#include "ui_hackrfinputgui.h"

HackRFInputGui::HackRFInputGui(DeviceUISet *deviceUISet, QWidget* parent) :
	DeviceGUI(parent),
	ui(new Ui::HackRFInputGui),
	m_settings(),
    m_sampleRateMode(true),
	m_forceSettings(true),
	m_doApplySettings(true),
	m_sampleSource(NULL),
	m_lastEngineState(DeviceAPI::StNotStarted)
{
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_sampleSource = (HackRFInput*) m_deviceUISet->m_deviceAPI->getSampleSource();

    ui->setupUi(getContents());
    sizeToContents();
    getContents()->setStyleSheet("#HackRFInputGui { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesource/hackrfinput/readme.md";
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	ui->centerFrequency->setValueRange(7, 0U, 9999999U);

    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, 1000000U, 20000000U);

	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

	displaySettings();
	displayBandwidths();

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleSource->setMessageQueueToGUI(&m_inputMessageQueue);

    sendSettings();
    makeUIConnections();
}

HackRFInputGui::~HackRFInputGui()
{
    qDebug("HackRFInputGui::~HackRFInputGui");
    m_statusTimer.stop();
    m_updateTimer.stop();
	delete ui;
    qDebug("HackRFInputGui::~HackRFInputGui: end");
}

void HackRFInputGui::destroy()
{
	delete this;
}

void HackRFInputGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
    m_forceSettings = true;
	sendSettings();
}

QByteArray HackRFInputGui::serialize() const
{
	return m_settings.serialize();
}

bool HackRFInputGui::deserialize(const QByteArray& data)
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

bool HackRFInputGui::handleMessage(const Message& message)
{
    if (HackRFInput::MsgConfigureHackRF::match(message))
    {
        const HackRFInput::MsgConfigureHackRF& cfg = (HackRFInput::MsgConfigureHackRF&) message;

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
    else if (HackRFInput::MsgReportHackRF::match(message))
    {
        displaySettings();
        return true;
    }
    else if (HackRFInput::MsgStartStop::match(message))
    {
        HackRFInput::MsgStartStop& notif = (HackRFInput::MsgStartStop&) message;
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

void HackRFInputGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("HackRFGui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("HackRFGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
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

void HackRFInputGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    displaySampleRate();
}

void HackRFInputGui::updateFrequencyLimits()
{
    // values in kHz
    qint64 deltaFrequency = m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency/1000 : 0;
    qint64 minLimit = (0U) + deltaFrequency;
    qint64 maxLimit = (7250000U) + deltaFrequency;

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
    qDebug("HackRFInputGui::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);
}

void HackRFInputGui::displaySampleRate()
{
    ui->sampleRate->blockSignals(true);
    displayFcTooltip();

    if (m_sampleRateMode)
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(60,60,60); }");
        ui->sampleRateMode->setText("SR");
        ui->sampleRate->setValueRange(8, 1000000U, 20000000U);
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
        ui->sampleRate->setValueRange(8, 1000000U/(1<<m_settings.m_log2Decim), 20000000U/(1<<m_settings.m_log2Decim));
        ui->sampleRate->setValue(m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim));
        ui->sampleRate->setToolTip("Baseband sample rate (S/s)");
        ui->deviceRateText->setToolTip("Device to host sample rate (S/s)");
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(m_settings.m_devSampleRate / 1000.0f, 'g', 5)));
    }

    ui->sampleRate->blockSignals(false);
}

void HackRFInputGui::displayFcTooltip()
{
    int32_t fShift = DeviceSampleSource::calculateFrequencyShift(
        m_settings.m_log2Decim,
        (DeviceSampleSource::fcPos_t) m_settings.m_fcPos,
        m_settings.m_devSampleRate,
        DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD
    );
    ui->fcPos->setToolTip(tr("Relative position of device center frequency: %1 kHz").arg(QString::number(fShift / 1000.0f, 'g', 5)));
}

void HackRFInputGui::displaySettings()
{
    blockApplySettings(true);

    ui->transverter->setDeltaFrequency(m_settings.m_transverterDeltaFrequency);
    ui->transverter->setDeltaFrequencyActive(m_settings.m_transverterMode);
    ui->transverter->setIQOrder(m_settings.m_iqOrder);

    updateFrequencyLimits();

	ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
	ui->LOppm->setValue(m_settings.m_LOppmTenths);
	ui->LOppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));
	ui->dcOffset->setChecked(m_settings.m_dcBlock);
	ui->iqImbalance->setChecked(m_settings.m_iqCorrection);
    ui->autoBBF->setChecked(m_settings.m_autoBBF);

    displaySampleRate();

	ui->biasT->setChecked(m_settings.m_biasT);
	ui->decim->setCurrentIndex(m_settings.m_log2Decim);
	ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);
	ui->lnaExt->setChecked(m_settings.m_lnaExt);
	ui->lnaGainText->setText(tr("%1dB").arg(m_settings.m_lnaGain));
	ui->lna->setValue(m_settings.m_lnaGain);

    unsigned int bandwidthIndex = HackRFBandwidths::getBandwidthIndex(m_settings.m_bandwidth/1000);
	ui->bbFilter->setCurrentIndex(bandwidthIndex);

	ui->vgaText->setText(tr("%1dB").arg(m_settings.m_vgaGain));
	ui->vga->setValue(m_settings.m_vgaGain);

	blockApplySettings(false);
}

void HackRFInputGui::displayBandwidths()
{
	unsigned int savedIndex = HackRFBandwidths::getBandwidthIndex(m_settings.m_bandwidth/1000);
	ui->bbFilter->blockSignals(true);
	ui->bbFilter->clear();

	for (unsigned int i = 0; i < HackRFBandwidths::m_nb_bw; i++)
	{
		ui->bbFilter->addItem(QString("%1M").arg(QString::number(HackRFBandwidths::m_bw_k[i]/1000.0, 'f', 2)));
	}

	ui->bbFilter->blockSignals(false);

	if (savedIndex < HackRFBandwidths::m_nb_bw)
	{
		ui->bbFilter->setCurrentIndex(savedIndex);
	}
	else
	{
		ui->bbFilter->setCurrentIndex((int) HackRFBandwidths::m_nb_bw-1);
	}
}

void HackRFInputGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

void HackRFInputGui::on_LOppm_valueChanged(int value)
{
	m_settings.m_LOppmTenths = value;
	ui->LOppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));
    m_settingsKeys.append("LOppmTenths");
	sendSettings();
}

void HackRFInputGui::on_dcOffset_toggled(bool checked)
{
	m_settings.m_dcBlock = checked;
    m_settingsKeys.append("dcBlock");
	sendSettings();
}

void HackRFInputGui::on_iqImbalance_toggled(bool checked)
{
	m_settings.m_iqCorrection = checked;
    m_settingsKeys.append("iqCorrection");
	sendSettings();
}

void HackRFInputGui::on_autoBBF_toggled(bool checked)
{
    m_settings.m_autoBBF = checked;

    if (checked)
    {
        m_settings.m_bandwidth = hackrf_compute_baseband_filter_bw(m_settings.m_devSampleRate);
        ui->bbFilter->blockSignals(true);
        displaySettings();
        ui->bbFilter->blockSignals(false);
        m_settingsKeys.append("bandwidth");
        sendSettings();
    }
}

void HackRFInputGui::on_bbFilter_currentIndexChanged(int index)
{
    int newBandwidth = HackRFBandwidths::getBandwidth(index);
	m_settings.m_bandwidth = newBandwidth * 1000;
    ui->autoBBF->setChecked(false);
    m_settingsKeys.append("bandwidth");
	sendSettings();
}

void HackRFInputGui::on_biasT_stateChanged(int state)
{
	m_settings.m_biasT = (state == Qt::Checked);
    m_settingsKeys.append("biasT");
	sendSettings();
}

void HackRFInputGui::on_lnaExt_stateChanged(int state)
{
	m_settings.m_lnaExt = (state == Qt::Checked);
    m_settingsKeys.append("lnaExt");
	sendSettings();
}

void HackRFInputGui::on_centerFrequency_changed(quint64 value)
{
	m_settings.m_centerFrequency = value * 1000;
    m_settingsKeys.append("centerFrequency");
	sendSettings();
}

void HackRFInputGui::on_sampleRate_changed(quint64 value)
{
    m_settings.m_devSampleRate = value;
    m_settingsKeys.append("devSampleRate");

    if (!m_sampleRateMode) {
        m_settings.m_devSampleRate <<= m_settings.m_log2Decim;
    }

    if (m_settings.m_autoBBF)
    {
        m_settings.m_bandwidth = hackrf_compute_baseband_filter_bw(m_settings.m_devSampleRate);
        m_settingsKeys.append("bandwidth");
        ui->bbFilter->blockSignals(true);
        displaySettings();
        ui->bbFilter->blockSignals(false);
    }

    displayFcTooltip();
    sendSettings();
}

void HackRFInputGui::on_decim_currentIndexChanged(int index)
{
	if ((index <0) || (index > 6)) {
		return;
    }

	m_settings.m_log2Decim = index;
    displaySampleRate();
    m_settings.m_devSampleRate = ui->sampleRate->getValueNew();

    if (!m_sampleRateMode) {
        m_settings.m_devSampleRate <<= m_settings.m_log2Decim;
    }

    m_settingsKeys.append("log2Decim");
    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void HackRFInputGui::on_fcPos_currentIndexChanged(int index)
{
    m_settings.m_fcPos = (HackRFInputSettings::fcPos_t) (index < 0 ? 0 : index > 2 ? 2 : index);
    m_settingsKeys.append("fcPos");
    displayFcTooltip();
    sendSettings();
}

void HackRFInputGui::on_lna_valueChanged(int value)
{
	if ((value < 0) || (value > 40))
		return;

	ui->lnaGainText->setText(tr("%1dB").arg(value));
	m_settings.m_lnaGain = value;
    m_settingsKeys.append("lnaGain");
	sendSettings();
}

void HackRFInputGui::on_vga_valueChanged(int value)
{
	if ((value < 0) || (value > 62))
		return;

	ui->vgaText->setText(tr("%1dB").arg(value));
	m_settings.m_vgaGain = value;
    m_settingsKeys.append("vgaGain");
	sendSettings();
}

void HackRFInputGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        HackRFInput::MsgStartStop *message = HackRFInput::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void HackRFInputGui::on_sampleRateMode_toggled(bool checked)
{
    m_sampleRateMode = checked;
    displaySampleRate();
}

void HackRFInputGui::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "HackRFGui::updateHardware";
        HackRFInput::MsgConfigureHackRF* message = HackRFInput::MsgConfigureHackRF::create(m_settings, m_settingsKeys, m_forceSettings);
        m_sampleSource->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void HackRFInputGui::on_transverter_clicked()
{
    m_settings.m_transverterMode = ui->transverter->getDeltaFrequencyAcive();
    m_settings.m_transverterDeltaFrequency = ui->transverter->getDeltaFrequency();
    m_settings.m_iqOrder = ui->transverter->getIQOrder();
    qDebug("HackRFInputGui::on_transverter_clicked: %lld Hz %s", m_settings.m_transverterDeltaFrequency, m_settings.m_transverterMode ? "on" : "off");
    updateFrequencyLimits();
    m_settings.m_centerFrequency = ui->centerFrequency->getValueNew()*1000;
    m_settingsKeys.append("transverterMode");
    m_settingsKeys.append("transverterDeltaFrequency");
    m_settingsKeys.append("iqOrder");
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void HackRFInputGui::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void HackRFInputGui::updateStatus()
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
                ui->startStop->setChecked(false);
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
}

void HackRFInputGui::openDeviceSettingsDialog(const QPoint& p)
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

        sendSettings();
    }

    resetContextMenuType();
}

void HackRFInputGui::makeUIConnections()
{
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &HackRFInputGui::on_centerFrequency_changed);
    QObject::connect(ui->sampleRate, &ValueDial::changed, this, &HackRFInputGui::on_sampleRate_changed);
    QObject::connect(ui->LOppm, &QSlider::valueChanged, this, &HackRFInputGui::on_LOppm_valueChanged);
    QObject::connect(ui->dcOffset, &ButtonSwitch::toggled, this, &HackRFInputGui::on_dcOffset_toggled);
    QObject::connect(ui->iqImbalance, &ButtonSwitch::toggled, this, &HackRFInputGui::on_iqImbalance_toggled);
    QObject::connect(ui->autoBBF, &ButtonSwitch::toggled, this, &HackRFInputGui::on_autoBBF_toggled);
    QObject::connect(ui->biasT, &QCheckBox::stateChanged, this, &HackRFInputGui::on_biasT_stateChanged);
    QObject::connect(ui->decim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &HackRFInputGui::on_decim_currentIndexChanged);
    QObject::connect(ui->fcPos, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &HackRFInputGui::on_fcPos_currentIndexChanged);
    QObject::connect(ui->lnaExt, &QCheckBox::stateChanged, this, &HackRFInputGui::on_lnaExt_stateChanged);
    QObject::connect(ui->lna, &QSlider::valueChanged, this, &HackRFInputGui::on_lna_valueChanged);
    QObject::connect(ui->bbFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &HackRFInputGui::on_bbFilter_currentIndexChanged);
    QObject::connect(ui->vga, &QSlider::valueChanged, this, &HackRFInputGui::on_vga_valueChanged);
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &HackRFInputGui::on_startStop_toggled);
    QObject::connect(ui->sampleRateMode, &QToolButton::toggled, this, &HackRFInputGui::on_sampleRateMode_toggled);
    QObject::connect(ui->transverter, &TransverterButton::clicked, this, &HackRFInputGui::on_transverter_clicked);
}
