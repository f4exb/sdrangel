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
#include <QResizeEvent>

#include "ui_fcdproplusgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "fcdproplusgui.h"

#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "fcdproplusconst.h"
#include "fcdtraits.h"

FCDProPlusGui::FCDProPlusGui(DeviceUISet *deviceUISet, QWidget* parent) :
	DeviceGUI(parent),
	ui(new Ui::FCDProPlusGui),
	m_forceSettings(true),
	m_settings(),
	m_sampleSource(NULL),
	m_lastEngineState(DeviceAPI::StNotStarted)
{
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_sampleSource = (FCDProPlusInput*) m_deviceUISet->m_deviceAPI->getSampleSource();

    ui->setupUi(getContents());
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    getContents()->setStyleSheet("#FCDProPlusGui { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesource/fcdproplus/readme.md";

	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	updateFrequencyLimits();

	ui->filterIF->clear();
	for (int i = 0; i < FCDProPlusConstants::fcdproplus_if_filter_nb_values(); i++)
	{
		ui->filterIF->addItem(QString(FCDProPlusConstants::if_filters[i].label.c_str()), i);
	}

	ui->filterRF->clear();
	for (int i = 0; i < FCDProPlusConstants::fcdproplus_rf_filter_nb_values(); i++)
	{
		ui->filterRF->addItem(QString(FCDProPlusConstants::rf_filters[i].label.c_str()), i);
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

FCDProPlusGui::~FCDProPlusGui()
{
    m_statusTimer.stop();
    m_updateTimer.stop();
	delete ui;
}

void FCDProPlusGui::destroy()
{
	delete this;
}

void FCDProPlusGui::resetToDefaults()
{
	m_settings.resetToDefaults();
	displaySettings();
    m_forceSettings = true;
	sendSettings();
}

QByteArray FCDProPlusGui::serialize() const
{
	return m_settings.serialize();
}

bool FCDProPlusGui::deserialize(const QByteArray& data)
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

void FCDProPlusGui::resizeEvent(QResizeEvent* size)
{
    adjustSize();
    size->accept();
}

bool FCDProPlusGui::handleMessage(const Message& message)
{
    if (FCDProPlusInput::MsgConfigureFCDProPlus::match(message))
    {
        const FCDProPlusInput::MsgConfigureFCDProPlus& cfg = (FCDProPlusInput::MsgConfigureFCDProPlus&) message;

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
    else if (FCDProPlusInput::MsgStartStop::match(message))
    {
        FCDProPlusInput::MsgStartStop& notif = (FCDProPlusInput::MsgStartStop&) message;
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

void FCDProPlusGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("FCDProPlusGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
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

void FCDProPlusGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_sampleRate / 1000));
}

void FCDProPlusGui::updateFrequencyLimits()
{
    // values in kHz
    qint64 deltaFrequency = m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency/1000 : 0;
    qint64 minLimit = fcd_traits<ProPlus>::loLowLimitFreq/1000 + deltaFrequency;
    qint64 maxLimit = fcd_traits<ProPlus>::loHighLimitFreq/1000 + deltaFrequency;

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
    qDebug("FCDProPlusGui::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);
}

void FCDProPlusGui::displaySettings()
{
    ui->transverter->setDeltaFrequency(m_settings.m_transverterDeltaFrequency);
    ui->transverter->setDeltaFrequencyActive(m_settings.m_transverterMode);
    ui->transverter->setIQOrder(m_settings.m_iqOrder);
    updateFrequencyLimits();
	ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    ui->decim->setCurrentIndex(m_settings.m_log2Decim);
    ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);
	ui->dcOffset->setChecked(m_settings.m_dcBlock);
	ui->iqImbalance->setChecked(m_settings.m_iqImbalance);
	ui->checkBoxG->setChecked(m_settings.m_lnaGain);
	ui->checkBoxB->setChecked(m_settings.m_biasT);
	ui->mixGain->setChecked(m_settings.m_mixGain);
	ui->ifGain->setValue(m_settings.m_ifGain);
	ui->ifGainText->setText(QString("%1dB").arg(m_settings.m_ifGain));
	ui->filterIF->setCurrentIndex(m_settings.m_ifFilterIndex);
	ui->filterRF->setCurrentIndex(m_settings.m_rfFilterIndex);
	ui->ppm->setValue(m_settings.m_LOppmTenths);
	ui->ppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));
}

void FCDProPlusGui::sendSettings()
{
	if(!m_updateTimer.isActive())
		m_updateTimer.start(100);
}

void FCDProPlusGui::on_centerFrequency_changed(quint64 value)
{
	m_settings.m_centerFrequency = value * 1000;
    m_settingsKeys.append("centerFrequency");
	sendSettings();
}

void FCDProPlusGui::on_decim_currentIndexChanged(int index)
{
	if ((index < 0) || (index > 6)) {
		return;
	}

	m_settings.m_log2Decim = index;
    m_settingsKeys.append("log2Decim");
	sendSettings();
}

void FCDProPlusGui::on_fcPos_currentIndexChanged(int index)
{
	if (index == 0) {
		m_settings.m_fcPos = FCDProPlusSettings::FC_POS_INFRA;
        m_settingsKeys.append("fcPos");
		sendSettings();
	} else if (index == 1) {
		m_settings.m_fcPos = FCDProPlusSettings::FC_POS_SUPRA;
        m_settingsKeys.append("fcPos");
		sendSettings();
	} else if (index == 2) {
		m_settings.m_fcPos = FCDProPlusSettings::FC_POS_CENTER;
        m_settingsKeys.append("fcPos");
		sendSettings();
	}
}

void FCDProPlusGui::on_dcOffset_toggled(bool checked)
{
	m_settings.m_dcBlock = checked;
    m_settingsKeys.append("dcBlock");
	sendSettings();
}

void FCDProPlusGui::on_iqImbalance_toggled(bool checked)
{
	m_settings.m_iqImbalance = checked;
    m_settingsKeys.append("iqImbalance");
	sendSettings();
}

void FCDProPlusGui::updateHardware()
{
	FCDProPlusInput::MsgConfigureFCDProPlus* message = FCDProPlusInput::MsgConfigureFCDProPlus::create(m_settings, m_settingsKeys, m_forceSettings);
	m_sampleSource->getInputMessageQueue()->push(message);
	m_forceSettings = false;
    m_settingsKeys.clear();
	m_updateTimer.stop();
}

void FCDProPlusGui::updateStatus()
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

void FCDProPlusGui::on_checkBoxG_stateChanged(int state)
{
	m_settings.m_lnaGain = (state == Qt::Checked);
    m_settingsKeys.append("lnaGain");
	sendSettings();
}

void FCDProPlusGui::on_checkBoxB_stateChanged(int state)
{
	m_settings.m_biasT = (state == Qt::Checked);
    m_settingsKeys.append("biasT");
	sendSettings();
}

void FCDProPlusGui::on_mixGain_stateChanged(int state)
{
	m_settings.m_mixGain = (state == Qt::Checked);
    m_settingsKeys.append("mixGain");
	sendSettings();
}

void FCDProPlusGui::on_filterIF_currentIndexChanged(int index)
{
	m_settings.m_ifFilterIndex = index;
    m_settingsKeys.append("ifFilterIndex");
	sendSettings();
}

void FCDProPlusGui::on_filterRF_currentIndexChanged(int index)
{
	m_settings.m_rfFilterIndex = index;
    m_settingsKeys.append("rfFilterIndex");
	sendSettings();
}

void FCDProPlusGui::on_ifGain_valueChanged(int value)
{
	m_settings.m_ifGain = value;
	displaySettings();
    m_settingsKeys.append("ifGain");
	sendSettings();
}

void FCDProPlusGui::on_ppm_valueChanged(int value)
{
	m_settings.m_LOppmTenths = value;
	displaySettings();
    m_settingsKeys.append("LOppmTenths");
	sendSettings();
}

void FCDProPlusGui::on_startStop_toggled(bool checked)
{
    FCDProPlusInput::MsgStartStop *message = FCDProPlusInput::MsgStartStop::create(checked);
    m_sampleSource->getInputMessageQueue()->push(message);
}

void FCDProPlusGui::on_transverter_clicked()
{
    m_settings.m_transverterMode = ui->transverter->getDeltaFrequencyAcive();
    m_settings.m_transverterDeltaFrequency = ui->transverter->getDeltaFrequency();
    m_settings.m_iqOrder = ui->transverter->getIQOrder();
    qDebug("FCDProPlusGui::on_transverter_clicked: %lld Hz %s", m_settings.m_transverterDeltaFrequency, m_settings.m_transverterMode ? "on" : "off");
    updateFrequencyLimits();
    m_settings.m_centerFrequency = ui->centerFrequency->getValueNew()*1000;
    m_settingsKeys.append("transverterMode");
    m_settingsKeys.append("transverterDeltaFrequency");
    m_settingsKeys.append("iqOrder");
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void FCDProPlusGui::openDeviceSettingsDialog(const QPoint& p)
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

void FCDProPlusGui::makeUIConnections()
{
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &FCDProPlusGui::on_centerFrequency_changed);
    QObject::connect(ui->decim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FCDProPlusGui::on_decim_currentIndexChanged);
    QObject::connect(ui->fcPos, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FCDProPlusGui::on_fcPos_currentIndexChanged);
    QObject::connect(ui->dcOffset, &ButtonSwitch::toggled, this, &FCDProPlusGui::on_dcOffset_toggled);
    QObject::connect(ui->iqImbalance, &ButtonSwitch::toggled, this, &FCDProPlusGui::on_iqImbalance_toggled);
    QObject::connect(ui->checkBoxG, &QCheckBox::stateChanged, this, &FCDProPlusGui::on_checkBoxG_stateChanged);
    QObject::connect(ui->checkBoxB, &QCheckBox::stateChanged, this, &FCDProPlusGui::on_checkBoxB_stateChanged);
    QObject::connect(ui->mixGain, &QCheckBox::stateChanged, this, &FCDProPlusGui::on_mixGain_stateChanged);
    QObject::connect(ui->ifGain, &QSlider::valueChanged, this, &FCDProPlusGui::on_ifGain_valueChanged);
    QObject::connect(ui->filterRF, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FCDProPlusGui::on_filterRF_currentIndexChanged);
    QObject::connect(ui->filterIF, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FCDProPlusGui::on_filterIF_currentIndexChanged);
    QObject::connect(ui->ppm, &QSlider::valueChanged, this, &FCDProPlusGui::on_ppm_valueChanged);
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &FCDProPlusGui::on_startStop_toggled);
    QObject::connect(ui->transverter, &TransverterButton::clicked, this, &FCDProPlusGui::on_transverter_clicked);
}
