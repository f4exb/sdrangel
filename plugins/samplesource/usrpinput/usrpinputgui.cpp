///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include "usrpinputgui.h"

#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QResizeEvent>

#include <algorithm>

#include "ui_usrpinputgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"

USRPInputGUI::USRPInputGUI(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::USRPInputGUI),
    m_settings(),
    m_sampleRateMode(true),
    m_sampleRate(0),
    m_lastEngineState(DeviceAPI::StNotStarted),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_statusCounter(0),
    m_deviceStatusCounter(0)
{
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_usrpInput = (USRPInput*) m_deviceUISet->m_deviceAPI->getSampleSource();

    ui->setupUi(getContents());
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    getContents()->setStyleSheet("#USRPInputGUI { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesource/usrpinput/readme.md";

    float minF, maxF;

    m_usrpInput->getLORange(minF, maxF);
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(7, ((uint32_t) minF)/1000, ((uint32_t) maxF)/1000); // frequency dial is in kHz

    m_usrpInput->getSRRange(minF, maxF);
    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, (uint32_t) minF, (uint32_t) maxF);

    ui->loOffset->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->loOffset->setValueRange(false, 5, (int32_t)-maxF/2/1000, (int32_t)maxF/2/1000); // LO offset shouldn't be greater than half the sample rate

    m_usrpInput->getLPRange(minF, maxF);
    ui->lpf->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpf->setValueRange(5, (minF/1000)+1, maxF/1000);

    m_usrpInput->getGainRange(minF, maxF);
    ui->gain->setRange((int)minF, (int)maxF);

    ui->channelNumberText->setText(tr("#%1").arg(m_usrpInput->getChannelIndex()));

    ui->antenna->addItems(m_usrpInput->getRxAntennas());
    ui->clockSource->addItems(m_usrpInput->getClockSources());

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    displaySettings();
    makeUIConnections();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_usrpInput->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));
}

USRPInputGUI::~USRPInputGUI()
{
    m_statusTimer.stop();
    m_updateTimer.stop();
    delete ui;
}

void USRPInputGUI::destroy()
{
    delete this;
}

void USRPInputGUI::setName(const QString& name)
{
    setObjectName(name);
}

QString USRPInputGUI::getName() const
{
    return objectName();
}

void USRPInputGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

qint64 USRPInputGUI::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void USRPInputGUI::setCenterFrequency(qint64 centerFrequency)
{
    m_settings.m_centerFrequency = centerFrequency;
    displaySettings();
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

QByteArray USRPInputGUI::serialize() const
{
    return m_settings.serialize();
}

bool USRPInputGUI::deserialize(const QByteArray& data)
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

void USRPInputGUI::resizeEvent(QResizeEvent* size)
{
    adjustSize();
    size->accept();
}

bool USRPInputGUI::handleMessage(const Message& message)
{
    if (USRPInput::MsgConfigureUSRP::match(message))
    {
        const USRPInput::MsgConfigureUSRP& cfg = (USRPInput::MsgConfigureUSRP&) message;

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
    else if (DeviceUSRPShared::MsgReportBuddyChange::match(message))
    {
        DeviceUSRPShared::MsgReportBuddyChange& report = (DeviceUSRPShared::MsgReportBuddyChange&) message;
        m_settings.m_masterClockRate = report.getMasterClockRate();

        if (report.getRxElseTx()) {
            m_settings.m_devSampleRate   = report.getDevSampleRate();
            m_settings.m_centerFrequency = report.getCenterFrequency();
            m_settings.m_loOffset        = report.getLOOffset();
        }

        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (DeviceUSRPShared::MsgReportClockSourceChange::match(message))
    {
        qDebug("USRPInputGUI::handleMessage MsgReportClockSourceChange");
        DeviceUSRPShared::MsgReportClockSourceChange& report = (DeviceUSRPShared::MsgReportClockSourceChange&) message;
        m_settings.m_clockSource = report.getClockSource();

        blockApplySettings(true);
        ui->clockSource->setCurrentIndex(ui->clockSource->findText(m_settings.m_clockSource));
        blockApplySettings(false);

        return true;
    }
    else if (USRPInput::MsgReportStreamInfo::match(message))
    {
        USRPInput::MsgReportStreamInfo& report = (USRPInput::MsgReportStreamInfo&) message;

        if (report.getSuccess())
        {
            if (report.getActive()) {
                ui->streamStatusLabel->setStyleSheet("QLabel { background-color : green; }");
            } else {
                ui->streamStatusLabel->setStyleSheet("QLabel { background-color : blue; }");
            }

            if (report.getOverruns() > 0) {
                ui->overrunLabel->setStyleSheet("QLabel { background-color : red; }");
            } else {
                ui->overrunLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
            }

            if (report.getTimeouts() > 0) {
                ui->timeoutLabel->setStyleSheet("QLabel { background-color : red; }");
            } else {
                ui->timeoutLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
            }
        }
        else
        {
            ui->streamStatusLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
        }

        return true;
    }
    else if (USRPInput::MsgStartStop::match(message))
    {
        USRPInput::MsgStartStop& notif = (USRPInput::MsgStartStop&) message;
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

void USRPInputGUI::updateFrequencyLimits()
{
    // values in kHz
    float minF, maxF;
    qint64 deltaFrequency = m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency/1000 : 0;
    m_usrpInput->getLORange(minF, maxF);
    qint64 minLimit = minF/1000 + deltaFrequency;
    qint64 maxLimit = maxF/1000 + deltaFrequency;

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
    qDebug("USRPInputGUI::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);
}

void USRPInputGUI::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("USRPInputGUI::handleInputMessages: DSPSignalNotification: SampleRate: %d, CenterFrequency: %llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();

            delete message;
        }
        else if (USRPInput::MsgConfigureUSRP::match(*message))
        {
            const USRPInput::MsgConfigureUSRP& cfg = (USRPInput::MsgConfigureUSRP&) *message;

            if (cfg.getForce()) {
                m_settings = cfg.getSettings();
            } else {
                m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
            }

            displaySettings();

            delete message;
        }
        else
        {
            if (handleMessage(*message)) {
                delete message;
            }
        }
    }
}

void USRPInputGUI::updateSampleRate()
{
    uint32_t sr = m_settings.m_devSampleRate;
    int cr = m_settings.m_masterClockRate;

    if (sr < 100000000) {
        ui->sampleRateLabel->setText(tr("%1k").arg(QString::number(sr / 1000.0f, 'g', 5)));
    } else {
        ui->sampleRateLabel->setText(tr("%1M").arg(QString::number(sr / 1000000.0f, 'g', 5)));
    }
    if (cr < 0) {
       ui->masterClockRateLabel->setText("-");
    } else if (cr < 100000000) {
        ui->masterClockRateLabel->setText(tr("%1k").arg(QString::number(cr / 1000.0f, 'g', 5)));
    } else {
        ui->masterClockRateLabel->setText(tr("%1M").arg(QString::number(cr / 1000000.0f, 'g', 5)));
    }
    // LO offset shouldn't be greater than half the sample rate
    ui->loOffset->setValueRange(false, 5, -(int32_t)sr/2/1000, (int32_t)sr/2/1000);
}

void USRPInputGUI::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    displaySampleRate();
}

void USRPInputGUI::displaySampleRate()
{
    float minF, maxF;
    m_usrpInput->getSRRange(minF, maxF);

    ui->sampleRate->blockSignals(true);

    if (m_sampleRateMode)
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(60,60,60); }");
        ui->sampleRateMode->setText("SR");
        ui->sampleRate->setValueRange(8, (uint32_t) minF, (uint32_t) maxF);
        ui->sampleRate->setValue(m_settings.m_devSampleRate);
        ui->sampleRate->setToolTip("Device to host sample rate (S/s)");
        ui->deviceRateText->setToolTip("Baseband sample rate (S/s)");
        uint32_t basebandSampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim);
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(basebandSampleRate / 1000.0f, 'g', 5)));
    }
    else
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(50,50,50); }");
        ui->sampleRateMode->setText("BB");
        ui->sampleRate->setValueRange(8, (uint32_t) minF/(1<<m_settings.m_log2SoftDecim), (uint32_t) maxF/(1<<m_settings.m_log2SoftDecim));
        ui->sampleRate->setValue(m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim));
        ui->sampleRate->setToolTip("Baseband sample rate (S/s)");
        ui->deviceRateText->setToolTip("Device to host sample rate (S/s)");
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(m_settings.m_devSampleRate / 1000.0f, 'g', 5)));
    }

    ui->sampleRate->blockSignals(false);
}


void USRPInputGUI::displaySettings()
{
    ui->transverter->setDeltaFrequency(m_settings.m_transverterDeltaFrequency);
    ui->transverter->setDeltaFrequencyActive(m_settings.m_transverterMode);

    ui->clockSource->setCurrentIndex(ui->clockSource->findText(m_settings.m_clockSource));

    updateFrequencyLimits();
    setCenterFrequencyDisplay();
    displaySampleRate();

    ui->dcOffset->setChecked(m_settings.m_dcBlock);
    ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

    ui->swDecim->setCurrentIndex(m_settings.m_log2SoftDecim);

    updateSampleRate();

    ui->lpf->setValue(m_settings.m_lpfBW / 1000);
    ui->loOffset->setValue(m_settings.m_loOffset / 1000);

    ui->gain->setValue(m_settings.m_gain);
    ui->gainText->setText(tr("%1").arg(m_settings.m_gain));

    ui->antenna->setCurrentIndex(ui->antenna->findText(m_settings.m_antennaPath));

    ui->gainMode->setCurrentIndex((int) m_settings.m_gainMode);

    if (m_settings.m_gainMode == USRPInputSettings::GAIN_AUTO) {
        ui->gain->setEnabled(false);
    } else {
        ui->gain->setEnabled(true);
    }
}

void USRPInputGUI::setCenterFrequencyDisplay()
{
    int64_t centerFrequency = m_settings.m_centerFrequency;
    ui->centerFrequency->setToolTip(QString("Main center frequency in kHz (LO: %1 kHz)").arg(centerFrequency/1000));

    ui->centerFrequency->blockSignals(true);
    ui->centerFrequency->setValue(centerFrequency < 0 ? 0 : (uint64_t) centerFrequency/1000); // kHz
    ui->centerFrequency->blockSignals(false);
}

void USRPInputGUI::setCenterFrequencySetting(uint64_t kHzValue)
{
    int64_t centerFrequency = kHzValue*1000;

    m_settings.m_centerFrequency = centerFrequency < 0 ? 0 : (uint64_t) centerFrequency;
    ui->centerFrequency->setToolTip(QString("Main center frequency in kHz (LO: %1 kHz)").arg(centerFrequency/1000));
}

void USRPInputGUI::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void USRPInputGUI::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "USRPInputGUI::updateHardware";
        USRPInput::MsgConfigureUSRP* message = USRPInput::MsgConfigureUSRP::create(m_settings, m_settingsKeys, m_forceSettings);
        m_usrpInput->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void USRPInputGUI::updateStatus()
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

    if (m_statusCounter < 1)
    {
        m_statusCounter++;
    }
    else
    {
        USRPInput::MsgGetStreamInfo* message = USRPInput::MsgGetStreamInfo::create();
        m_usrpInput->getInputMessageQueue()->push(message);
        m_statusCounter = 0;
    }

    if (m_deviceStatusCounter < 10)
    {
        m_deviceStatusCounter++;
    }
    else
    {
        if (m_deviceUISet->m_deviceAPI->isBuddyLeader())
        {
            USRPInput::MsgGetDeviceInfo* message = USRPInput::MsgGetDeviceInfo::create();
            m_usrpInput->getInputMessageQueue()->push(message);
        }

        m_deviceStatusCounter = 0;
    }
}

void USRPInputGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void USRPInputGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        USRPInput::MsgStartStop *message = USRPInput::MsgStartStop::create(checked);
        m_usrpInput->getInputMessageQueue()->push(message);
    }
}

void USRPInputGUI::on_centerFrequency_changed(quint64 value)
{
    setCenterFrequencySetting(value);
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void USRPInputGUI::on_dcOffset_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    m_settingsKeys.append("dcBlock");
    sendSettings();
}

void USRPInputGUI::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    m_settingsKeys.append("iqCorrection");
    sendSettings();
}

void USRPInputGUI::on_sampleRate_changed(quint64 value)
{
    if (m_sampleRateMode) {
        m_settings.m_devSampleRate = value;
    } else {
        m_settings.m_devSampleRate = value * (1 << m_settings.m_log2SoftDecim);
    }

    updateSampleRate();
    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void USRPInputGUI::on_swDecim_currentIndexChanged(int index)
{
    if ((index <0) || (index > 6)) {
        return;
    }

    m_settings.m_log2SoftDecim = index;
    m_settingsKeys.append("log2SoftDecim");
    displaySampleRate();

    if (m_sampleRateMode) {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew();
    } else {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew() * (1 << m_settings.m_log2SoftDecim);
    }

    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void USRPInputGUI::on_lpf_changed(quint64 value)
{
    m_settings.m_lpfBW = value * 1000;
    m_settingsKeys.append("lpfBW");
    sendSettings();
}

void USRPInputGUI::on_loOffset_changed(qint64 value)
{
    m_settings.m_loOffset = value * 1000;
    m_settingsKeys.append("loOffset");
    sendSettings();
}

void USRPInputGUI::on_gainMode_currentIndexChanged(int index)
{
    m_settings.m_gainMode = (USRPInputSettings::GainMode) index;
    m_settingsKeys.append("gainMode");

    if (index == 0) {
        ui->gain->setEnabled(false);
    } else {
        ui->gain->setEnabled(true);
    }

    sendSettings();
}

void USRPInputGUI::on_gain_valueChanged(int value)
{
    m_settings.m_gain = value;
    ui->gainText->setText(tr("%1").arg(m_settings.m_gain));
    m_settingsKeys.append("gain");
    sendSettings();
}

void USRPInputGUI::on_antenna_currentIndexChanged(int index)
{
    (void) index;
    m_settings.m_antennaPath = ui->antenna->currentText();
    m_settingsKeys.append("antennaPath");
    sendSettings();
}

void USRPInputGUI::on_clockSource_currentIndexChanged(int index)
{
    (void) index;
    m_settings.m_clockSource = ui->clockSource->currentText();
    m_settingsKeys.append("clockSource");
    sendSettings();
}

void USRPInputGUI::on_transverter_clicked()
{
    m_settings.m_transverterMode = ui->transverter->getDeltaFrequencyAcive();
    m_settings.m_transverterDeltaFrequency = ui->transverter->getDeltaFrequency();
    qDebug("USRPInputGUI::on_transverter_clicked: %lld Hz %s", m_settings.m_transverterDeltaFrequency, m_settings.m_transverterMode ? "on" : "off");
    updateFrequencyLimits();
    setCenterFrequencySetting(ui->centerFrequency->getValueNew());
    m_settingsKeys.append("transverterMode");
    m_settingsKeys.append("transverterDeltaFrequency");
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void USRPInputGUI::on_sampleRateMode_toggled(bool checked)
{
    m_sampleRateMode = checked;
    displaySampleRate();
}

void USRPInputGUI::openDeviceSettingsDialog(const QPoint& p)
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

void USRPInputGUI::makeUIConnections()
{
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &USRPInputGUI::on_startStop_toggled);
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &USRPInputGUI::on_centerFrequency_changed);
    QObject::connect(ui->dcOffset, &ButtonSwitch::toggled, this, &USRPInputGUI::on_dcOffset_toggled);
    QObject::connect(ui->iqImbalance, &ButtonSwitch::toggled, this, &USRPInputGUI::on_iqImbalance_toggled);
    QObject::connect(ui->sampleRate, &ValueDial::changed, this, &USRPInputGUI::on_sampleRate_changed);
    QObject::connect(ui->swDecim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &USRPInputGUI::on_swDecim_currentIndexChanged);
    QObject::connect(ui->lpf, &ValueDial::changed, this, &USRPInputGUI::on_lpf_changed);
    QObject::connect(ui->loOffset, &ValueDialZ::changed, this, &USRPInputGUI::on_loOffset_changed);
    QObject::connect(ui->gainMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &USRPInputGUI::on_gainMode_currentIndexChanged);
    QObject::connect(ui->gain, &QSlider::valueChanged, this, &USRPInputGUI::on_gain_valueChanged);
    QObject::connect(ui->antenna, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &USRPInputGUI::on_antenna_currentIndexChanged);
    QObject::connect(ui->clockSource, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &USRPInputGUI::on_clockSource_currentIndexChanged);
    QObject::connect(ui->transverter, &TransverterButton::clicked, this, &USRPInputGUI::on_transverter_clicked);
    QObject::connect(ui->sampleRateMode, &QToolButton::toggled, this, &USRPInputGUI::on_sampleRateMode_toggled);
}
