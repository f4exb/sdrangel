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
#include "device/devicesinkapi.h"
#include "device/deviceuiset.h"
#include "util/simpleserializer.h"
#include "ui_soapysdroutputgui.h"
#include "gui/glspectrum.h"
#include "soapygui/discreterangegui.h"
#include "soapygui/intervalrangegui.h"
#include "soapygui/stringrangegui.h"
#include "soapygui/dynamicitemsettinggui.h"

#include "soapysdroutputgui.h"

SoapySDROutputGui::SoapySDROutputGui(DeviceUISet *deviceUISet, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::SoapySDROutputGui),
    m_deviceUISet(deviceUISet),
    m_forceSettings(true),
    m_doApplySettings(true),
    m_sampleSink(0),
    m_sampleRate(0),
    m_lastEngineState(DSPDeviceSinkEngine::StNotStarted),
    m_antennas(0),
    m_sampleRateGUI(0),
    m_bandwidthGUI(0)
{
    m_sampleSink = (SoapySDROutput*) m_deviceUISet->m_deviceSinkAPI->getSampleSink();
    ui->setupUi(this);

    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    uint64_t f_min, f_max;
    m_sampleSink->getFrequencyRange(f_min, f_max);
    ui->centerFrequency->setValueRange(7, f_min/1000, f_max/1000);

    createAntennasControl(m_sampleSink->getAntennas());
    createRangesControl(&m_sampleRateGUI, m_sampleSink->getRateRanges(), "SR", "S/s");
    createRangesControl(&m_bandwidthGUI, m_sampleSink->getBandwidthRanges(), "BW", "Hz");
    createTunableElementsControl(m_sampleSink->getTunableElements());

    if (m_sampleRateGUI) {
        connect(m_sampleRateGUI, SIGNAL(valueChanged(double)), this, SLOT(sampleRateChanged(double)));
    }
    if (m_bandwidthGUI) {
        connect(m_bandwidthGUI, SIGNAL(valueChanged(double)), this, SLOT(bandwidthChanged(double)));
    }

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    displaySettings();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleSink->setMessageQueueToGUI(&m_inputMessageQueue);

    sendSettings();
}

SoapySDROutputGui::~SoapySDROutputGui()
{
    delete ui;
}

void SoapySDROutputGui::destroy()
{
    delete this;
}

void SoapySDROutputGui::createRangesControl(
        ItemSettingGUI **settingGUI,
        const SoapySDR::RangeList& rangeList,
        const QString& text,
        const QString& unit)
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
        DiscreteRangeGUI *rangeGUI = new DiscreteRangeGUI(this);
        rangeGUI->setLabel(text);
        rangeGUI->setUnits(QString("k%1").arg(unit));

        for (const auto &it : rangeList) {
            rangeGUI->addItem(QString("%1").arg(QString::number(it.minimum()/1000.0, 'f', 0)), it.minimum());
        }

        *settingGUI = rangeGUI;
        QVBoxLayout *layout = (QVBoxLayout *) ui->scrollAreaWidgetContents->layout();
        layout->addWidget(rangeGUI);
    }
    else if (rangeInterval)
    {
        IntervalRangeGUI *rangeGUI = new IntervalRangeGUI(this);
        rangeGUI->setLabel(text);
        rangeGUI->setUnits(unit);

        for (const auto &it : rangeList) {
            rangeGUI->addInterval(it.minimum(), it.maximum());
        }

        rangeGUI->reset();

        *settingGUI = rangeGUI;
        QVBoxLayout *layout = (QVBoxLayout *) ui->scrollAreaWidgetContents->layout();
        layout->addWidget(rangeGUI);
    }
}

void SoapySDROutputGui::createAntennasControl(const std::vector<std::string>& antennaList)
{
    if (antennaList.size() == 0) { // return early if the antenna list is empty
        return;
    }

    m_antennas = new StringRangeGUI(this);
    m_antennas->setLabel(QString("Antenna"));
    m_antennas->setUnits(QString("Port"));

    for (const auto &it : antennaList) {
        m_antennas->addItem(QString(it.c_str()), it);
    }

    QVBoxLayout *layout = (QVBoxLayout *) ui->scrollAreaWidgetContents->layout();
    layout->addWidget(m_antennas);

    connect(m_antennas, SIGNAL(valueChanged()), this, SLOT(antennasChanged()));
}

void SoapySDROutputGui::createTunableElementsControl(const std::vector<DeviceSoapySDRParams::FrequencySetting>& tunableElementsList)
{
    if (tunableElementsList.size() <= 1) { // This list is created for other elements than the main one (RF) which is always at index 0
        return;
    }

    std::vector<DeviceSoapySDRParams::FrequencySetting>::const_iterator it = tunableElementsList.begin() + 1;

    for (int i = 0; it != tunableElementsList.end(); ++it, i++)
    {
        ItemSettingGUI *rangeGUI;
        createRangesControl(
                &rangeGUI,
                it->m_ranges,
                QString("%1 freq").arg(it->m_name.c_str()),
                QString((it->m_name == "CORR") ? "ppm" : "Hz"));
        DynamicItemSettingGUI *gui = new DynamicItemSettingGUI(rangeGUI, QString(it->m_name.c_str()));
        m_tunableElementsGUIs.push_back(gui);
        connect(m_tunableElementsGUIs.back(), SIGNAL(valueChanged(QString, double)), this, SLOT(tunableElementChanged(QString, double)));
    }
}

void SoapySDROutputGui::setName(const QString& name)
{
    setObjectName(name);
}

QString SoapySDROutputGui::getName() const
{
    return objectName();
}

void SoapySDROutputGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    sendSettings();
}

qint64 SoapySDROutputGui::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void SoapySDROutputGui::setCenterFrequency(qint64 centerFrequency)
{
    m_settings.m_centerFrequency = centerFrequency;
    displaySettings();
    sendSettings();
}

QByteArray SoapySDROutputGui::serialize() const
{
    return m_settings.serialize();
}

bool SoapySDROutputGui::deserialize(const QByteArray& data)
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


bool SoapySDROutputGui::handleMessage(const Message& message)
{
    if (SoapySDROutput::MsgConfigureSoapySDROutput::match(message))
    {
        const SoapySDROutput::MsgConfigureSoapySDROutput& cfg = (SoapySDROutput::MsgConfigureSoapySDROutput&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (SoapySDROutput::MsgStartStop::match(message))
    {
        SoapySDROutput::MsgStartStop& notif = (SoapySDROutput::MsgStartStop&) message;
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

void SoapySDROutputGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("SoapySDROutputGui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("SoapySDROutputGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
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

void SoapySDROutputGui::sampleRateChanged(double sampleRate)
{
    m_settings.m_devSampleRate = sampleRate;
    sendSettings();
}

void SoapySDROutputGui::antennasChanged()
{
    const std::string& antennaStr = m_antennas->getCurrentValue();
    m_settings.m_antenna = QString(antennaStr.c_str());
    sendSettings();
}

void SoapySDROutputGui::bandwidthChanged(double bandwidth)
{
    m_settings.m_bandwidth = bandwidth;
    sendSettings();
}

void SoapySDROutputGui::tunableElementChanged(QString name, double value)
{
    m_settings.m_tunableElements[name] = value;
    sendSettings();
}

void SoapySDROutputGui::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    sendSettings();
}

void SoapySDROutputGui::on_interp_currentIndexChanged(int index)
{
    if ((index <0) || (index > 6))
        return;
    m_settings.m_log2Interp = index;
    sendSettings();
}

void SoapySDROutputGui::on_transverter_clicked()
{
    m_settings.m_transverterMode = ui->transverter->getDeltaFrequencyAcive();
    m_settings.m_transverterDeltaFrequency = ui->transverter->getDeltaFrequency();
    qDebug("SoapySDROutputGui::on_transverter_clicked: %lld Hz %s", m_settings.m_transverterDeltaFrequency, m_settings.m_transverterMode ? "on" : "off");
    updateFrequencyLimits();
    setCenterFrequencySetting(ui->centerFrequency->getValueNew());
    sendSettings();
}

void SoapySDROutputGui::on_LOppm_valueChanged(int value)
{
    ui->LOppmText->setText(QString("%1").arg(QString::number(value/10.0, 'f', 1)));
    m_settings.m_LOppmTenths = value;
    sendSettings();
}

void SoapySDROutputGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        SoapySDROutput::MsgStartStop *message = SoapySDROutput::MsgStartStop::create(checked);
        m_sampleSink->getInputMessageQueue()->push(message);
    }
}

void SoapySDROutputGui::displaySettings()
{
    blockApplySettings(true);

    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);

    if (m_antennas) {
        m_antennas->setValue(m_settings.m_antenna.toStdString());
    }
    if (m_sampleRateGUI) {
        m_sampleRateGUI->setValue(m_settings.m_devSampleRate);
    }
    if (m_bandwidthGUI) {
        m_bandwidthGUI->setValue(m_settings.m_bandwidth);
    }

    ui->interp->setCurrentIndex(m_settings.m_log2Interp);

    ui->LOppm->setValue(m_settings.m_LOppmTenths);
    ui->LOppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));

    displayTunableElementsControlSettings();

    blockApplySettings(false);
}

void SoapySDROutputGui::displayTunableElementsControlSettings()
{
    for (const auto &it : m_tunableElementsGUIs)
    {
        QMap<QString, double>::const_iterator elIt = m_settings.m_tunableElements.find(it->getName());

        if (elIt != m_settings.m_tunableElements.end()) {
            it->setValue(*elIt);
        }
    }
}

void SoapySDROutputGui::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void SoapySDROutputGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg(QString::number(m_sampleRate / 1000.0f, 'g', 5)));
}

void SoapySDROutputGui::updateFrequencyLimits()
{
    // values in kHz
    uint64_t f_min, f_max;
    qint64 deltaFrequency = m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency/1000 : 0;
    m_sampleSink->getFrequencyRange(f_min, f_max);
    qint64 minLimit = f_min/1000 + deltaFrequency;
    qint64 maxLimit = f_max/1000 + deltaFrequency;

    minLimit = minLimit < 0 ? 0 : minLimit > 9999999 ? 9999999 : minLimit;
    maxLimit = maxLimit < 0 ? 0 : maxLimit > 9999999 ? 9999999 : maxLimit;

    qDebug("SoapySDRInputGui::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);

    ui->centerFrequency->setValueRange(7, minLimit, maxLimit);
}

void SoapySDROutputGui::setCenterFrequencySetting(uint64_t kHzValue)
{
    int64_t centerFrequency = kHzValue*1000;

    m_settings.m_centerFrequency = centerFrequency < 0 ? 0 : (uint64_t) centerFrequency;
    ui->centerFrequency->setToolTip(QString("Main center frequency in kHz (LO: %1 kHz)").arg(centerFrequency/1000));
}

void SoapySDROutputGui::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "SoapySDROutputGui::updateHardware";
        SoapySDROutput::MsgConfigureSoapySDROutput* message = SoapySDROutput::MsgConfigureSoapySDROutput::create(m_settings, m_forceSettings);
        m_sampleSink->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_updateTimer.stop();
    }
}

void SoapySDROutputGui::updateStatus()
{
    int state = m_deviceUISet->m_deviceSinkAPI->state();

    if(m_lastEngineState != state)
    {
        switch(state)
        {
            case DSPDeviceSinkEngine::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DSPDeviceSinkEngine::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DSPDeviceSinkEngine::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DSPDeviceSinkEngine::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceSinkAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}

