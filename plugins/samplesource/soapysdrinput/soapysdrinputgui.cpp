///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/devicesourceapi.h"
#include "device/deviceuiset.h"
#include "util/simpleserializer.h"
#include "gui/glspectrum.h"
#include "soapygui/discreterangegui.h"
#include "soapygui/intervalrangegui.h"

#include "ui_soapysdrinputgui.h"
#include "soapysdrinputgui.h"

SoapySDRInputGui::SoapySDRInputGui(DeviceUISet *deviceUISet, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::SoapySDRInputGui),
    m_deviceUISet(deviceUISet),
    m_forceSettings(true),
    m_doApplySettings(true),
    m_sampleSource(0),
    m_sampleRate(0),
    m_deviceCenterFrequency(0),
    m_lastEngineState(DSPDeviceSourceEngine::StNotStarted),
    m_sampleRateGUI(0)
{
    m_sampleSource = (SoapySDRInput*) m_deviceUISet->m_deviceSourceAPI->getSampleSource();
    ui->setupUi(this);

    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    uint64_t f_min, f_max;
    m_sampleSource->getFrequencyRange(f_min, f_max);
    ui->centerFrequency->setValueRange(7, f_min/1000, f_max/1000);

    createRangesControl(m_sampleSource->getRateRanges(), "SR", "kS/s");

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    displaySettings();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleSource->setMessageQueueToGUI(&m_inputMessageQueue);

    sendSettings();
}

SoapySDRInputGui::~SoapySDRInputGui()
{
    delete ui;
}

void SoapySDRInputGui::destroy()
{
    delete this;
}

void SoapySDRInputGui::createRangesControl(const SoapySDR::RangeList& rangeList, const QString& text, const QString& unit)
{
    if (rangeList.size() == 0) { // return early if the range list is empty
        return;
    }

    bool rangeDiscrete = true; // discretes values
    bool rangeInterval = true; // intervals

    for (const auto &it : rangeList)
    {
        if (it.minimum() != it.maximum()) {
            rangeDiscrete = false;
        } else {
            rangeInterval = false;
        }
    }

    if (rangeDiscrete)
    {
        DiscreteRangeGUI *rangeGUI = new DiscreteRangeGUI(ui->scrollAreaWidgetContents);
        rangeGUI->setLabel(text);
        rangeGUI->setUnits(unit);

        for (const auto &it : rangeList) {
            rangeGUI->addItem(QString("%1").arg(QString::number(it.minimum()/1000.0, 'f', 0)), it.minimum());
        }

        m_sampleRateGUI = rangeGUI;
        connect(m_sampleRateGUI, SIGNAL(valueChanged(double)), this, SLOT(sampleRateChanged(double)));
//        QHBoxLayout *layout = new QHBoxLayout();
//        QLabel *rangeLabel = new QLabel();
//        rangeLabel->setText(text);
//        QLabel *rangeUnit = new QLabel();
//        rangeUnit->setText(unit);
//        QComboBox *rangeCombo = new QComboBox();
//
//        for (const auto &it : rangeList) {
//            rangeCombo->addItem(QString("%1").arg(QString::number(it.minimum()/1000.0, 'f', 0)));
//        }
//
//        layout->addWidget(rangeLabel);
//        layout->addWidget(rangeCombo);
//        layout->addWidget(rangeUnit);
//        layout->setMargin(0);
//        layout->setSpacing(6);
//        rangeLabel->show();
//        rangeCombo->show();
//        QWidget *window = new QWidget(ui->scrollAreaWidgetContents);
//        window->setFixedWidth(300);
//        window->setFixedHeight(30);
//        window->setContentsMargins(0,0,0,0);
//        //window->setStyleSheet("background-color:black;");
//        window->setLayout(layout);
    }
    else if (rangeInterval)
    {
        IntervalRangeGUI *rangeGUI = new IntervalRangeGUI(ui->scrollAreaWidgetContents);
        rangeGUI->setLabel(text);
        rangeGUI->setUnits(unit);

        for (const auto &it : rangeList) {
            rangeGUI->addInterval(it.minimum(), it.maximum());
        }

        rangeGUI->reset();

        m_sampleRateGUI = rangeGUI;
        connect(m_sampleRateGUI, SIGNAL(valueChanged(double)), this, SLOT(sampleRateChanged(double)));
    }
}

void SoapySDRInputGui::setName(const QString& name)
{
    setObjectName(name);
}

QString SoapySDRInputGui::getName() const
{
    return objectName();
}

void SoapySDRInputGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    sendSettings();
}

qint64 SoapySDRInputGui::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void SoapySDRInputGui::setCenterFrequency(qint64 centerFrequency)
{
    m_settings.m_centerFrequency = centerFrequency;
    displaySettings();
    sendSettings();
}

QByteArray SoapySDRInputGui::serialize() const
{
    return m_settings.serialize();
}

bool SoapySDRInputGui::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data)) {
        displaySettings();
        m_forceSettings = true;
        sendSettings();
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}


bool SoapySDRInputGui::handleMessage(const Message& message)
{
    if (SoapySDRInput::MsgStartStop::match(message))
    {
        SoapySDRInput::MsgStartStop& notif = (SoapySDRInput::MsgStartStop&) message;
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

void SoapySDRInputGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("SoapySDRInputGui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("SoapySDRInputGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
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

void SoapySDRInputGui::sampleRateChanged(double sampleRate)
{
    m_settings.m_devSampleRate = sampleRate;
    sendSettings();
}

void SoapySDRInputGui::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    sendSettings();
}

void SoapySDRInputGui::on_dcOffset_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    sendSettings();
}

void SoapySDRInputGui::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    sendSettings();
}

void SoapySDRInputGui::on_decim_currentIndexChanged(int index)
{
    if ((index <0) || (index > 6))
        return;
    m_settings.m_log2Decim = index;
    sendSettings();
}

void SoapySDRInputGui::on_fcPos_currentIndexChanged(int index)
{
    if (index == 0) {
        m_settings.m_fcPos = SoapySDRInputSettings::FC_POS_INFRA;
        sendSettings();
    } else if (index == 1) {
        m_settings.m_fcPos = SoapySDRInputSettings::FC_POS_SUPRA;
        sendSettings();
    } else if (index == 2) {
        m_settings.m_fcPos = SoapySDRInputSettings::FC_POS_CENTER;
        sendSettings();
    }
}

void SoapySDRInputGui::on_transverter_clicked()
{
    m_settings.m_transverterMode = ui->transverter->getDeltaFrequencyAcive();
    m_settings.m_transverterDeltaFrequency = ui->transverter->getDeltaFrequency();
    qDebug("SoapySDRInputGui::on_transverter_clicked: %lld Hz %s", m_settings.m_transverterDeltaFrequency, m_settings.m_transverterMode ? "on" : "off");
    updateFrequencyLimits();
    setCenterFrequencySetting(ui->centerFrequency->getValueNew());
    sendSettings();
}

void SoapySDRInputGui::on_LOppm_valueChanged(int value)
{
    ui->LOppmText->setText(QString("%1").arg(QString::number(value/10.0, 'f', 1)));
    m_settings.m_LOppmTenths = value;
    sendSettings();
}

void SoapySDRInputGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        SoapySDRInput::MsgStartStop *message = SoapySDRInput::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void SoapySDRInputGui::on_record_toggled(bool checked)
{
    if (checked) {
        ui->record->setStyleSheet("QToolButton { background-color : red; }");
    } else {
        ui->record->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    SoapySDRInput::MsgFileRecord* message = SoapySDRInput::MsgFileRecord::create(checked);
    m_sampleSource->getInputMessageQueue()->push(message);
}

void SoapySDRInputGui::displaySettings()
{
    blockApplySettings(true);

    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    m_sampleRateGUI->setValue(m_settings.m_devSampleRate);

    ui->dcOffset->setChecked(m_settings.m_dcBlock);
    ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

    ui->decim->setCurrentIndex(m_settings.m_log2Decim);
    ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);

    ui->LOppm->setValue(m_settings.m_LOppmTenths);
    ui->LOppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));

    blockApplySettings(false);
}

void SoapySDRInputGui::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void SoapySDRInputGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg(QString::number(m_sampleRate / 1000.0f, 'g', 5)));
}

void SoapySDRInputGui::updateFrequencyLimits()
{
    // values in kHz
    uint64_t f_min, f_max;
    qint64 deltaFrequency = m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency/1000 : 0;
    m_sampleSource->getFrequencyRange(f_min, f_max);
    qint64 minLimit = f_min/1000 + deltaFrequency;
    qint64 maxLimit = f_max/1000 + deltaFrequency;

    minLimit = minLimit < 0 ? 0 : minLimit > 9999999 ? 9999999 : minLimit;
    maxLimit = maxLimit < 0 ? 0 : maxLimit > 9999999 ? 9999999 : maxLimit;

    qDebug("SoapySDRInputGui::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);

    ui->centerFrequency->setValueRange(7, minLimit, maxLimit);
}

void SoapySDRInputGui::setCenterFrequencySetting(uint64_t kHzValue)
{
    int64_t centerFrequency = kHzValue*1000;

    m_settings.m_centerFrequency = centerFrequency < 0 ? 0 : (uint64_t) centerFrequency;
    ui->centerFrequency->setToolTip(QString("Main center frequency in kHz (LO: %1 kHz)").arg(centerFrequency/1000));
}

void SoapySDRInputGui::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void SoapySDRInputGui::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "SoapySDRInputGui::updateHardware";
        SoapySDRInput::MsgConfigureSoapySDRInput* message = SoapySDRInput::MsgConfigureSoapySDRInput::create(m_settings, m_forceSettings);
        m_sampleSource->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_updateTimer.stop();
    }
}

void SoapySDRInputGui::updateStatus()
{
    int state = m_deviceUISet->m_deviceSourceAPI->state();

    if(m_lastEngineState != state)
    {
        switch(state)
        {
            case DSPDeviceSourceEngine::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DSPDeviceSourceEngine::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DSPDeviceSourceEngine::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DSPDeviceSourceEngine::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceSourceAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}

