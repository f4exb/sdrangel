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

#include <QMessageBox>
#include <QFileDialog>

#include "ui_fcdprogui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "fcdprogui.h"

#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "fcdproconst.h"
#include "fcdtraits.h"

FCDProGui::FCDProGui(DeviceUISet *deviceUISet, QWidget* parent) :
	DeviceGUI(parent),
	ui(new Ui::FCDProGui),
	m_forceSettings(true),
	m_settings(),
	m_sampleSource(NULL),
	m_lastEngineState(DeviceAPI::StNotStarted)
{
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_sampleSource = (FCDProInput*) m_deviceUISet->m_deviceAPI->getSampleSource();

    ui->setupUi(getContents());
    sizeToContents();
    getContents()->setStyleSheet("#FCDProGui { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesource/fcdpro/readme.md";
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    updateFrequencyLimits();

	ui->lnaGain->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_lna_gain_nb_values(); i++)
	{
		ui->lnaGain->addItem(QString(FCDProConstants::lna_gains[i].label.c_str()), i);
	}

	ui->rfFilter->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_rf_filter_nb_values(); i++)
	{
		ui->rfFilter->addItem(QString(FCDProConstants::rf_filters[i].label.c_str()), i);
	}

	ui->lnaEnhance->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_lna_enhance_nb_values(); i++)
	{
		ui->lnaEnhance->addItem(QString(FCDProConstants::lna_enhances[i].label.c_str()), i);
	}

	ui->band->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_band_nb_values(); i++)
	{
		ui->band->addItem(QString(FCDProConstants::bands[i].label.c_str()), i);
	}

	ui->mixGain->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_mixer_gain_nb_values(); i++)
	{
		ui->mixGain->addItem(QString(FCDProConstants::mixer_gains[i].label.c_str()), i);
	}

	ui->mixFilter->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_mixer_filter_nb_values(); i++)
	{
		ui->mixFilter->addItem(QString(FCDProConstants::mixer_filters[i].label.c_str()), i);
	}

	ui->bias->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_bias_current_nb_values(); i++)
	{
		ui->bias->addItem(QString(FCDProConstants::bias_currents[i].label.c_str()), i);
	}

	ui->mode->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_gain_mode_nb_values(); i++)
	{
		ui->mode->addItem(QString(FCDProConstants::if_gain_modes[i].label.c_str()), i);
	}

	ui->gain1->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_gain1_nb_values(); i++)
	{
		ui->gain1->addItem(QString(FCDProConstants::if_gains1[i].label.c_str()), i);
	}

	ui->rcFilter->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_rc_filter_nb_values(); i++)
	{
		ui->rcFilter->addItem(QString(FCDProConstants::if_rc_filters[i].label.c_str()), i);
	}

	ui->gain2->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_gain2_nb_values(); i++)
	{
		ui->gain2->addItem(QString(FCDProConstants::if_gains2[i].label.c_str()), i);
	}

	ui->gain3->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_gain3_nb_values(); i++)
	{
		ui->gain3->addItem(QString(FCDProConstants::if_gains3[i].label.c_str()), i);
	}

	ui->gain4->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_gain4_nb_values(); i++)
	{
		ui->gain4->addItem(QString(FCDProConstants::if_gains4[i].label.c_str()), i);
	}

	ui->ifFilter->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_filter_nb_values(); i++)
	{
		ui->ifFilter->addItem(QString(FCDProConstants::if_filters[i].label.c_str()), i);
	}

	ui->gain5->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_gain5_nb_values(); i++)
	{
		ui->gain5->addItem(QString(FCDProConstants::if_gains5[i].label.c_str()), i);
	}

	ui->gain6->clear();
	for (int i = 0; i < FCDProConstants::fcdpro_if_gain6_nb_values(); i++)
	{
		ui->gain6->addItem(QString(FCDProConstants::if_gains6[i].label.c_str()), i);
	}

	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

	displaySettings();
    makeUIConnections();

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleSource->setMessageQueueToGUI(&m_inputMessageQueue);
}

FCDProGui::~FCDProGui()
{
    m_statusTimer.stop();
    m_updateTimer.stop();
	delete ui;
}

void FCDProGui::destroy()
{
	delete this;
}

void FCDProGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
	sendSettings();
}

QByteArray FCDProGui::serialize() const
{
	return m_settings.serialize();
}

bool FCDProGui::deserialize(const QByteArray& data)
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

bool FCDProGui::handleMessage(const Message& message)
{
    if (FCDProInput::MsgConfigureFCDPro::match(message))
    {
        const FCDProInput::MsgConfigureFCDPro& cfg = (FCDProInput::MsgConfigureFCDPro&) message;

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
    else if (FCDProInput::MsgStartStop::match(message))
    {
        FCDProInput::MsgStartStop& notif = (FCDProInput::MsgStartStop&) message;
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

void FCDProGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("FCDProGui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("FCDProGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
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

void FCDProGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_sampleRate / 1000));
}

void FCDProGui::updateFrequencyLimits()
{
    // values in kHz
    qint64 deltaFrequency = m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency/1000 : 0;
    qint64 minLimit = fcd_traits<Pro>::loLowLimitFreq/1000 + deltaFrequency;
    qint64 maxLimit = fcd_traits<Pro>::loHighLimitFreq/1000 + deltaFrequency;

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
    qDebug("FCDProGui::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);
}

void FCDProGui::displaySettings()
{
    ui->transverter->setDeltaFrequency(m_settings.m_transverterDeltaFrequency);
    ui->transverter->setDeltaFrequencyActive(m_settings.m_transverterMode);
    ui->transverter->setIQOrder(m_settings.m_iqOrder);
    updateFrequencyLimits();
	ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
	ui->ppm->setValue(m_settings.m_LOppmTenths);
	ui->ppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));
	ui->dcOffset->setChecked(m_settings.m_dcBlock);
	ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

	ui->lnaGain->setCurrentIndex(m_settings.m_lnaGainIndex);
	ui->rfFilter->setCurrentIndex(m_settings.m_rfFilterIndex);
	ui->lnaEnhance->setCurrentIndex(m_settings.m_lnaEnhanceIndex);
	ui->band->setCurrentIndex(m_settings.m_bandIndex);
	ui->mixGain->setCurrentIndex(m_settings.m_mixerGainIndex);
	ui->mixFilter->setCurrentIndex(m_settings.m_mixerFilterIndex);
	ui->bias->setCurrentIndex(m_settings.m_biasCurrentIndex);
	ui->mode->setCurrentIndex(m_settings.m_modeIndex);
	ui->gain1->setCurrentIndex(m_settings.m_gain1Index);
	ui->gain2->setCurrentIndex(m_settings.m_gain2Index);
	ui->gain3->setCurrentIndex(m_settings.m_gain3Index);
	ui->gain4->setCurrentIndex(m_settings.m_gain4Index);
	ui->gain5->setCurrentIndex(m_settings.m_gain5Index);
	ui->gain6->setCurrentIndex(m_settings.m_gain6Index);
	ui->decim->setCurrentIndex(m_settings.m_log2Decim);
	ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);
	ui->rcFilter->setCurrentIndex(m_settings.m_rcFilterIndex);
	ui->ifFilter->setCurrentIndex(m_settings.m_ifFilterIndex);
}

void FCDProGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

void FCDProGui::on_centerFrequency_changed(quint64 value)
{
	m_settings.m_centerFrequency = value * 1000;
    m_settingsKeys.append("centerFrequency");
	sendSettings();
}

void FCDProGui::on_ppm_valueChanged(int value)
{
	m_settings.m_LOppmTenths = value;
	displaySettings();
    m_settingsKeys.append("LOppmTenths");
	sendSettings();
}

void FCDProGui::on_dcOffset_toggled(bool checked)
{
	m_settings.m_dcBlock = checked;
    m_settingsKeys.append("dcBlock");
	sendSettings();
}

void FCDProGui::on_iqImbalance_toggled(bool checked)
{
	m_settings.m_iqCorrection = checked;
    m_settingsKeys.append("iqCorrection");
	sendSettings();
}

void FCDProGui::on_lnaGain_currentIndexChanged(int index)
{
	m_settings.m_lnaGainIndex = index;
    m_settingsKeys.append("lnaGainIndex");
	sendSettings();
}

void FCDProGui::on_rfFilter_currentIndexChanged(int index)
{
	m_settings.m_rfFilterIndex = index;
    m_settingsKeys.append("rfFilterIndex");
	sendSettings();
}

void FCDProGui::on_lnaEnhance_currentIndexChanged(int index)
{
	m_settings.m_lnaEnhanceIndex = index;
    m_settingsKeys.append("lnaEnhanceIndex");
	sendSettings();
}

void FCDProGui::on_band_currentIndexChanged(int index)
{
	m_settings.m_bandIndex = index;
    m_settingsKeys.append("bandIndex");
	sendSettings();
}

void FCDProGui::on_mixGain_currentIndexChanged(int index)
{
	m_settings.m_mixerGainIndex = index;
    m_settingsKeys.append("mixerGainIndex");
	sendSettings();
}

void FCDProGui::on_mixFilter_currentIndexChanged(int index)
{
	m_settings.m_mixerFilterIndex = index;
    m_settingsKeys.append("mixerFilterIndex");
	sendSettings();
}

void FCDProGui::on_bias_currentIndexChanged(int index)
{
	m_settings.m_biasCurrentIndex = index;
    m_settingsKeys.append("biasCurrentIndex");
	sendSettings();
}

void FCDProGui::on_mode_currentIndexChanged(int index)
{
	m_settings.m_modeIndex = index;
    m_settingsKeys.append("modeIndex");
	sendSettings();
}

void FCDProGui::on_gain1_currentIndexChanged(int index)
{
	m_settings.m_gain1Index = index;
    m_settingsKeys.append("gain1Index");
	sendSettings();
}

void FCDProGui::on_rcFilter_currentIndexChanged(int index)
{
	m_settings.m_rcFilterIndex = index;
    m_settingsKeys.append("rcFilterIndex");
	sendSettings();
}

void FCDProGui::on_gain2_currentIndexChanged(int index)
{
	m_settings.m_gain2Index = index;
    m_settingsKeys.append("gain2Index");
	sendSettings();
}

void FCDProGui::on_gain3_currentIndexChanged(int index)
{
	m_settings.m_gain3Index = index;
    m_settingsKeys.append("gain3Index");
	sendSettings();
}

void FCDProGui::on_gain4_currentIndexChanged(int index)
{
	m_settings.m_gain4Index = index;
    m_settingsKeys.append("gain4Index");
	sendSettings();
}

void FCDProGui::on_ifFilter_currentIndexChanged(int index)
{
	m_settings.m_ifFilterIndex = index;
    m_settingsKeys.append("ifFilterIndex");
	sendSettings();
}

void FCDProGui::on_gain5_currentIndexChanged(int index)
{
	m_settings.m_gain5Index = index;
    m_settingsKeys.append("gain5Index");
	sendSettings();
}

void FCDProGui::on_gain6_currentIndexChanged(int index)
{
	m_settings.m_gain6Index = index;
    m_settingsKeys.append("gain6Index");
	sendSettings();
}

void FCDProGui::on_decim_currentIndexChanged(int index)
{
	if ((index < 0) || (index > 6)) {
		return;
	}

	m_settings.m_log2Decim = index;
    m_settingsKeys.append("log2Decim");
	sendSettings();
}

void FCDProGui::on_fcPos_currentIndexChanged(int index)
{
	if (index == 0) {
		m_settings.m_fcPos = FCDProSettings::FC_POS_INFRA;
		sendSettings();
	} else if (index == 1) {
		m_settings.m_fcPos = FCDProSettings::FC_POS_SUPRA;
		sendSettings();
	} else if (index == 2) {
		m_settings.m_fcPos = FCDProSettings::FC_POS_CENTER;
		sendSettings();
	}
}

void FCDProGui::on_setDefaults_clicked(bool checked)
{
    (void) checked;
	m_settings.m_lnaGainIndex = 8;        // +15 dB
	//m_settings.rfFilterIndex = 0;
	m_settings.m_mixerGainIndex = 1;      // +12 dB
	m_settings.m_mixerFilterIndex = 8;    // 1.9 MHz
	m_settings.m_gain1Index = 1;          // +6 dB
	m_settings.m_rcFilterIndex = 15;      // 1.0 MHz
	m_settings.m_gain2Index = 1;          // +3 dB
	m_settings.m_gain3Index = 1;          // +3 dB
	m_settings.m_gain4Index = 0;          // 0 dB
	m_settings.m_ifFilterIndex = 31;      // 2.15 MHz
	m_settings.m_gain5Index = 0;          // +3 dB
	m_settings.m_gain6Index = 0;          // +3 dB
	m_settings.m_lnaEnhanceIndex = 0;     // Off
	m_settings.m_biasCurrentIndex = 3;    // V/U band
	m_settings.m_modeIndex = 0;           // Linearity
    m_settingsKeys.append("lnaGainIndex");
    m_settingsKeys.append("mixerGainIndex");
    m_settingsKeys.append("mixerFilterIndex");
    m_settingsKeys.append("gain1Index");
    m_settingsKeys.append("rcFilterIndex");
    m_settingsKeys.append("gain2Index");
    m_settingsKeys.append("gain3Index");
    m_settingsKeys.append("gain4Index");
    m_settingsKeys.append("ifFilterIndex");
    m_settingsKeys.append("gain5Index");
    m_settingsKeys.append("gain6Index");
    m_settingsKeys.append("lnaEnhanceIndex");
    m_settingsKeys.append("biasCurrentIndex");
    m_settingsKeys.append("modeIndex");
	displaySettings();
	sendSettings();
}

void FCDProGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        FCDProInput::MsgStartStop *message = FCDProInput::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void FCDProGui::on_transverter_clicked()
{
    m_settings.m_transverterMode = ui->transverter->getDeltaFrequencyAcive();
    m_settings.m_transverterDeltaFrequency = ui->transverter->getDeltaFrequency();
    m_settings.m_iqOrder = ui->transverter->getIQOrder();
    qDebug("FCDProGui::on_transverter_clicked: %lld Hz %s", m_settings.m_transverterDeltaFrequency, m_settings.m_transverterMode ? "on" : "off");
    updateFrequencyLimits();
    m_settings.m_centerFrequency = ui->centerFrequency->getValueNew()*1000;
    m_settingsKeys.append("transverterMode");
    m_settingsKeys.append("transverterDeltaFrequency");
    m_settingsKeys.append("iqOrder");
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void FCDProGui::updateStatus()
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
}

void FCDProGui::updateHardware()
{
	FCDProInput::MsgConfigureFCDPro* message = FCDProInput::MsgConfigureFCDPro::create(m_settings, m_settingsKeys, m_forceSettings);
	m_sampleSource->getInputMessageQueue()->push(message);
	m_forceSettings = false;
    m_settingsKeys.clear();
	m_updateTimer.stop();
}

void FCDProGui::openDeviceSettingsDialog(const QPoint& p)
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

void FCDProGui::makeUIConnections()
{
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &FCDProGui::on_centerFrequency_changed);
    QObject::connect(ui->ppm, &QSlider::valueChanged, this, &FCDProGui::on_ppm_valueChanged);
    QObject::connect(ui->dcOffset, &ButtonSwitch::toggled, this, &FCDProGui::on_dcOffset_toggled);
    QObject::connect(ui->iqImbalance, &ButtonSwitch::toggled, this, &FCDProGui::on_iqImbalance_toggled);
    QObject::connect(ui->lnaGain, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FCDProGui::on_lnaGain_currentIndexChanged);
    QObject::connect(ui->rfFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FCDProGui::on_rfFilter_currentIndexChanged);
    QObject::connect(ui->lnaEnhance, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FCDProGui::on_lnaEnhance_currentIndexChanged);
    QObject::connect(ui->band, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FCDProGui::on_band_currentIndexChanged);
    QObject::connect(ui->mixGain, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FCDProGui::on_mixGain_currentIndexChanged);
    QObject::connect(ui->mixFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FCDProGui::on_mixFilter_currentIndexChanged);
    QObject::connect(ui->bias, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FCDProGui::on_bias_currentIndexChanged);
    QObject::connect(ui->mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FCDProGui::on_mode_currentIndexChanged);
    QObject::connect(ui->rcFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FCDProGui::on_rcFilter_currentIndexChanged);
    QObject::connect(ui->ifFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FCDProGui::on_ifFilter_currentIndexChanged);
    QObject::connect(ui->decim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FCDProGui::on_decim_currentIndexChanged);
    QObject::connect(ui->fcPos, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FCDProGui::on_fcPos_currentIndexChanged);
    QObject::connect(ui->setDefaults, &QPushButton::clicked, this, &FCDProGui::on_setDefaults_clicked);
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &FCDProGui::on_startStop_toggled);
    QObject::connect(ui->transverter, &TransverterButton::clicked, this, &FCDProGui::on_transverter_clicked);
}
