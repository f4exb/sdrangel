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
#include <QCheckBox>

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/devicesourceapi.h"
#include "device/deviceuiset.h"
#include "util/simpleserializer.h"
#include "gui/glspectrum.h"
#include "soapygui/discreterangegui.h"
#include "soapygui/intervalrangegui.h"
#include "soapygui/stringrangegui.h"
#include "soapygui/dynamicitemsettinggui.h"
#include "soapygui/intervalslidergui.h"

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
    m_antennas(0),
    m_sampleRateGUI(0),
    m_bandwidthGUI(0),
    m_gainSliderGUI(0),
    m_autoGain(0)
{
    m_sampleSource = (SoapySDRInput*) m_deviceUISet->m_deviceSourceAPI->getSampleSource();
    ui->setupUi(this);

    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    uint64_t f_min, f_max;
    m_sampleSource->getFrequencyRange(f_min, f_max);
    ui->centerFrequency->setValueRange(7, f_min/1000, f_max/1000);

    createAntennasControl(m_sampleSource->getAntennas());
    createRangesControl(&m_sampleRateGUI, m_sampleSource->getRateRanges(), "SR", "S/s");
    createRangesControl(&m_bandwidthGUI, m_sampleSource->getBandwidthRanges(), "BW", "Hz");
    createTunableElementsControl(m_sampleSource->getTunableElements());
    createGlobalGainControl();
    createIndividualGainsControl(m_sampleSource->getIndividualGainsRanges());
    m_sampleSource->initGainSettings(m_settings);

    if (m_sampleRateGUI) {
        connect(m_sampleRateGUI, SIGNAL(valueChanged(double)), this, SLOT(sampleRateChanged(double)));
    }
    if (m_bandwidthGUI) {
        connect(m_bandwidthGUI,  SIGNAL(valueChanged(double)), this, SLOT(bandwidthChanged(double)));
    }

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

void SoapySDRInputGui::createRangesControl(
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
        IntervalRangeGUI *rangeGUI = new IntervalRangeGUI(ui->scrollAreaWidgetContents);
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

void SoapySDRInputGui::createAntennasControl(const std::vector<std::string>& antennaList)
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

void SoapySDRInputGui::createTunableElementsControl(const std::vector<DeviceSoapySDRParams::FrequencySetting>& tunableElementsList)
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

void SoapySDRInputGui::createGlobalGainControl()
{
    m_gainSliderGUI = new IntervalSliderGUI(this);
    int min, max;
    m_sampleSource->getGlobalGainRange(min, max);
    m_gainSliderGUI->setInterval(min, max);
    m_gainSliderGUI->setLabel(QString("Global gain"));
    m_gainSliderGUI->setUnits(QString(""));

    QVBoxLayout *layout = (QVBoxLayout *) ui->scrollAreaWidgetContents->layout();

    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);

    if (m_sampleSource->isAGCSupported())
    {
        m_autoGain = new QCheckBox(this);
        m_autoGain->setText(QString("AGC"));
        layout->addWidget(m_autoGain);

        connect(m_autoGain, SIGNAL(toggled(bool)), this, SLOT(autoGainChanged(bool)));
    }

    layout->addWidget(m_gainSliderGUI);

    connect(m_gainSliderGUI, SIGNAL(valueChanged(double)), this, SLOT(globalGainChanged(double)));
}

void SoapySDRInputGui::createIndividualGainsControl(const std::vector<DeviceSoapySDRParams::GainSetting>& individualGainsList)
{
    if (individualGainsList.size() == 0) { // Leave early if list of individual gains is empty
        return;
    }

    QVBoxLayout *layout = (QVBoxLayout *) ui->scrollAreaWidgetContents->layout();
    std::vector<DeviceSoapySDRParams::GainSetting>::const_iterator it = individualGainsList.begin();

    for (int i = 0; it != individualGainsList.end(); ++it, i++)
    {
        IntervalSliderGUI *gainGUI = new IntervalSliderGUI(this);
        gainGUI->setInterval(it->m_range.minimum(), it->m_range.maximum());
        gainGUI->setLabel(QString("%1 gain").arg(it->m_name.c_str()));
        gainGUI->setUnits(QString(""));
        DynamicItemSettingGUI *gui = new DynamicItemSettingGUI(gainGUI, QString(it->m_name.c_str()));
        layout->addWidget(gainGUI);
        m_individualGainsGUIs.push_back(gui);
        connect(m_individualGainsGUIs.back(), SIGNAL(valueChanged(QString, double)), this, SLOT(individualGainChanged(QString, double)));
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
    if (SoapySDRInput::MsgConfigureSoapySDRInput::match(message))
    {
        const SoapySDRInput::MsgConfigureSoapySDRInput& cfg = (SoapySDRInput::MsgConfigureSoapySDRInput&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (SoapySDRInput::MsgReportGainChange::match(message))
    {
        const SoapySDRInput::MsgReportGainChange& report = (SoapySDRInput::MsgReportGainChange&) message;
        const SoapySDRInputSettings& gainSettings = report.getSettings();

        if (report.getGlobalGain()) {
            m_settings.m_globalGain = gainSettings.m_globalGain;
        }
        if (report.getIndividualGains()) {
            m_settings.m_individualGains = gainSettings.m_individualGains;
        }

        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (SoapySDRInput::MsgStartStop::match(message))
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

void SoapySDRInputGui::antennasChanged()
{
    const std::string& antennaStr = m_antennas->getCurrentValue();
    m_settings.m_antenna = QString(antennaStr.c_str());

    sendSettings();
}

void SoapySDRInputGui::sampleRateChanged(double sampleRate)
{
    m_settings.m_devSampleRate = sampleRate;
    sendSettings();
}

void SoapySDRInputGui::bandwidthChanged(double bandwidth)
{
    m_settings.m_bandwidth = bandwidth;
    sendSettings();
}

void SoapySDRInputGui::tunableElementChanged(QString name, double value)
{
    m_settings.m_tunableElements[name] = value;
    sendSettings();
}

void SoapySDRInputGui::globalGainChanged(double gain)
{
    m_settings.m_globalGain = round(gain);
    sendSettings();
}

void SoapySDRInputGui::autoGainChanged(bool set)
{
    m_settings.m_autoGain = set;
    sendSettings();
}

void SoapySDRInputGui::individualGainChanged(QString name, double value)
{
    m_settings.m_individualGains[name] = value;
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

    if (m_antennas) {
        m_antennas->setValue(m_settings.m_antenna.toStdString());
    }
    if (m_sampleRateGUI) {
        m_sampleRateGUI->setValue(m_settings.m_devSampleRate);
    }
    if (m_bandwidthGUI) {
        m_bandwidthGUI->setValue(m_settings.m_bandwidth);
    }
    if (m_gainSliderGUI) {
        m_gainSliderGUI->setValue(m_settings.m_globalGain);
    }
    if (m_autoGain) {
        m_autoGain->setChecked(m_settings.m_autoGain);
    }

    ui->dcOffset->setChecked(m_settings.m_dcBlock);
    ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

    ui->decim->setCurrentIndex(m_settings.m_log2Decim);
    ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);

    ui->LOppm->setValue(m_settings.m_LOppmTenths);
    ui->LOppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));

    displayTunableElementsControlSettings();
    displayIndividualGainsControlSettings();

    blockApplySettings(false);
}

void SoapySDRInputGui::displayTunableElementsControlSettings()
{
    for (const auto &it : m_tunableElementsGUIs)
    {
        QMap<QString, double>::const_iterator elIt = m_settings.m_tunableElements.find(it->getName());

        if (elIt != m_settings.m_tunableElements.end()) {
            it->setValue(*elIt);
        }
    }
}

void SoapySDRInputGui::displayIndividualGainsControlSettings()
{
    for (const auto &it : m_individualGainsGUIs)
    {
        QMap<QString, double>::const_iterator elIt = m_settings.m_individualGains.find(it->getName());

        if (elIt != m_settings.m_individualGains.end()) {
            it->setValue(*elIt);
        }
    }
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

