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

#include <algorithm>

#include <QDebug>
#include <QMessageBox>
#include <QResizeEvent>

#include "ui_xtrxoutputgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "xtrxoutputgui.h"

XTRXOutputGUI::XTRXOutputGUI(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::XTRXOutputGUI),
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
    m_XTRXOutput = (XTRXOutput*) m_deviceUISet->m_deviceAPI->getSampleSink();

    ui->setupUi(getContents());
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    getContents()->setStyleSheet("#XTRXOutputGUI { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesink/xtrxoutput/readme.md";

    float minF, maxF, stepF;

    m_XTRXOutput->getLORange(minF, maxF, stepF);
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(7, ((uint32_t) minF)/1000, ((uint32_t) maxF)/1000); // frequency dial is in kHz

    m_XTRXOutput->getSRRange(minF, maxF, stepF);
    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, (uint32_t) minF, (uint32_t) maxF);

    m_XTRXOutput->getLPRange(minF, maxF, stepF);
    ui->lpf->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpf->setValueRange(6, (minF/1000)+1, maxF/1000);

    ui->ncoFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));

    ui->channelNumberText->setText(tr("#%1").arg(m_XTRXOutput->getChannelIndex()));

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

    displaySettings();
    makeUIConnections();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

XTRXOutputGUI::~XTRXOutputGUI()
{
    m_statusTimer.stop();
    m_updateTimer.stop();
    delete ui;
}

void XTRXOutputGUI::destroy()
{
    delete this;
}

void XTRXOutputGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

QByteArray XTRXOutputGUI::serialize() const
{
    return m_settings.serialize();
}

bool XTRXOutputGUI::deserialize(const QByteArray& data)
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

void XTRXOutputGUI::resizeEvent(QResizeEvent* size)
{
    adjustSize();
    size->accept();
}

bool XTRXOutputGUI::handleMessage(const Message& message)
{

    if (DeviceXTRXShared::MsgReportBuddyChange::match(message))
    {
        DeviceXTRXShared::MsgReportBuddyChange& report = (DeviceXTRXShared::MsgReportBuddyChange&) message;
        m_settings.m_devSampleRate = report.getDevSampleRate();
        m_settings.m_log2HardInterp = report.getLog2HardDecimInterp();

        if (!report.getRxElseTx()) {
            m_settings.m_centerFrequency = report.getCenterFrequency();
        }

        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (DeviceXTRXShared::MsgReportClockSourceChange::match(message))
    {
        DeviceXTRXShared::MsgReportClockSourceChange& report = (DeviceXTRXShared::MsgReportClockSourceChange&) message;
        m_settings.m_extClockFreq = report.getExtClockFeq();
        m_settings.m_extClock = report.getExtClock();

        blockApplySettings(true);
        ui->extClock->setExternalClockFrequency(m_settings.m_extClockFreq);
        ui->extClock->setExternalClockActive(m_settings.m_extClock);
        blockApplySettings(false);

        return true;
    }
    else if (XTRXOutput::MsgReportClockGenChange::match(message))
    {
        m_settings.m_devSampleRate = m_XTRXOutput->getDevSampleRate();
        m_settings.m_log2HardInterp = m_XTRXOutput->getLog2HardInterp();

        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (XTRXOutput::MsgReportStreamInfo::match(message))
    {
        XTRXOutput::MsgReportStreamInfo& report = (XTRXOutput::MsgReportStreamInfo&) message;

        if (report.getSuccess())
        {
            if (report.getActive()) {
                ui->streamStatusLabel->setStyleSheet("QLabel { background-color : green; }");
            } else {
                ui->streamStatusLabel->setStyleSheet("QLabel { background-color : blue; }");
            }

            ui->fifoBar->setMaximum(report.getFifoSize());
            ui->fifoBar->setValue(report.getFifoFilledCount());
            ui->fifoBar->setToolTip(tr("FIFO fill %1/%2 samples").arg(QString::number(report.getFifoFilledCount())).arg(QString::number(report.getFifoSize())));
        }
        else
        {
            ui->streamStatusLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
        }

        return true;
    }
    else if (DeviceXTRXShared::MsgReportDeviceInfo::match(message))
    {
        DeviceXTRXShared::MsgReportDeviceInfo& report = (DeviceXTRXShared::MsgReportDeviceInfo&) message;
        ui->temperatureText->setText(tr("%1C").arg(QString::number(report.getTemperature(), 'f', 0)));

        if (report.getGPSLocked()) {
            ui->gpsStatusLabel->setStyleSheet("QLabel { background-color : green; }");
        } else {
            ui->gpsStatusLabel->setStyleSheet("QLabel { background:rgb(48,48,48); }");
        }

        return true;
    }
    else if (XTRXOutput::MsgStartStop::match(message))
    {
        XTRXOutput::MsgStartStop& notif = (XTRXOutput::MsgStartStop&) message;
        blockApplySettings(true);
        ui->startStop->setChecked(notif.getStartStop());
        blockApplySettings(false);

        return true;
    }
    return false;
}

void XTRXOutputGUI::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()))
    {
        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("XTRXOutputGUI::handleInputMessages: DSPSignalNotification: SampleRate: %d, CenterFrequency: %llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();

            delete message;
        }
        else if (XTRXOutput::MsgConfigureXTRX::match(*message))
        {
            qDebug("XTRXOutputGUI::handleInputMessages: MsgConfigureXTRX");
            const XTRXOutput::MsgConfigureXTRX& cfg = (XTRXOutput::MsgConfigureXTRX&) *message;

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

void XTRXOutputGUI::updateDACRate()
{
    uint32_t dacRate = m_XTRXOutput->getClockGen() / 4;

    if (dacRate < 100000000) {
        ui->dacRateLabel->setText(tr("%1k").arg(QString::number(dacRate / 1000.0f, 'g', 5)));
    } else {
        ui->dacRateLabel->setText(tr("%1M").arg(QString::number(dacRate / 1000000.0f, 'g', 5)));
    }
}

void XTRXOutputGUI::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    displaySampleRate();
}

void XTRXOutputGUI::displaySampleRate()
{
    float minF, maxF, stepF;
    m_XTRXOutput->getSRRange(minF, maxF, stepF);

    ui->sampleRate->blockSignals(true);

    if (m_sampleRateMode)
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(60,60,60); }");
        ui->sampleRateMode->setText("SR");
        ui->sampleRate->setValueRange(8, (uint32_t) minF, (uint32_t) maxF);
        ui->sampleRate->setValue(m_settings.m_devSampleRate);
        ui->sampleRate->setToolTip("Device to host sample rate (S/s)");
        ui->deviceRateText->setToolTip("Baseband sample rate (S/s)");
        uint32_t basebandSampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp);
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(basebandSampleRate / 1000.0f, 'g', 5)));
    }
    else
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(50,50,50); }");
        ui->sampleRateMode->setText("BB");
        ui->sampleRate->setValueRange(8, (uint32_t) minF/(1<<m_settings.m_log2SoftInterp), (uint32_t) maxF/(1<<m_settings.m_log2SoftInterp));
        ui->sampleRate->setValue(m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp));
        ui->sampleRate->setToolTip("Baseband sample rate (S/s)");
        ui->deviceRateText->setToolTip("Device to host sample rate (S/s)");
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(m_settings.m_devSampleRate / 1000.0f, 'g', 5)));
    }

    ui->sampleRate->blockSignals(false);
}

void XTRXOutputGUI::displaySettings()
{
    ui->extClock->setExternalClockFrequency(m_settings.m_extClockFreq);
    ui->extClock->setExternalClockActive(m_settings.m_extClock);

    setCenterFrequencyDisplay();
    displaySampleRate();

    ui->hwInterp->setCurrentIndex(m_settings.m_log2HardInterp);
    ui->swInterp->setCurrentIndex(m_settings.m_log2SoftInterp);

    updateDACRate();

    ui->lpf->setValue(m_settings.m_lpfBW / 1000);
    ui->pwrmode->setCurrentIndex(m_settings.m_pwrmode);

    ui->gain->setValue(m_settings.m_gain);
    ui->gainText->setText(tr("%1").arg(m_settings.m_gain));

    ui->antenna->setCurrentIndex((int) m_settings.m_antennaPath - (int) XTRX_TX_H);

    setNCODisplay();

    ui->ncoEnable->setChecked(m_settings.m_ncoEnable);
}

void XTRXOutputGUI::setNCODisplay()
{
    int ncoHalfRange = (m_settings.m_devSampleRate * (1<<(m_settings.m_log2HardInterp)))/2;
    ui->ncoFrequency->setValueRange(
            false,
            8,
            -ncoHalfRange,
            ncoHalfRange);

    ui->ncoFrequency->blockSignals(true);
    ui->ncoFrequency->setToolTip(QString("NCO frequency shift in Hz (Range: +/- %1 kHz)").arg(ncoHalfRange/1000));
    ui->ncoFrequency->setValue(m_settings.m_ncoFrequency);
    ui->ncoFrequency->blockSignals(false);
}

void XTRXOutputGUI::setCenterFrequencyDisplay()
{
    int64_t centerFrequency = m_settings.m_centerFrequency;
    ui->centerFrequency->setToolTip(QString("Main center frequency in kHz (LO: %1 kHz)").arg(centerFrequency/1000));

    if (m_settings.m_ncoEnable) {
        centerFrequency += m_settings.m_ncoFrequency;
    }

    ui->centerFrequency->blockSignals(true);
    ui->centerFrequency->setValue(centerFrequency < 0 ? 0 : (uint64_t) centerFrequency/1000); // kHz
    ui->centerFrequency->blockSignals(false);
}

void XTRXOutputGUI::setCenterFrequencySetting(uint64_t kHzValue)
{
    int64_t centerFrequency = kHzValue*1000;

    if (m_settings.m_ncoEnable) {
        centerFrequency -= m_settings.m_ncoFrequency;
    }

    m_settings.m_centerFrequency = centerFrequency < 0 ? 0 : (uint64_t) centerFrequency;
    ui->centerFrequency->setToolTip(QString("Main center frequency in kHz (LO: %1 kHz)").arg(centerFrequency/1000));
}

void XTRXOutputGUI::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void XTRXOutputGUI::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "XTRXOutputGUI::updateHardware";
        XTRXOutput::MsgConfigureXTRX* message = XTRXOutput::MsgConfigureXTRX::create(m_settings, m_settingsKeys, m_forceSettings);
        m_XTRXOutput->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void XTRXOutputGUI::updateStatus()
{
    int state = m_deviceUISet->m_deviceAPI->state();

    if (m_lastEngineState != state)
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
        XTRXOutput::MsgGetStreamInfo* message = XTRXOutput::MsgGetStreamInfo::create();
        m_XTRXOutput->getInputMessageQueue()->push(message);
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
            XTRXOutput::MsgGetDeviceInfo* message = XTRXOutput::MsgGetDeviceInfo::create();
            m_XTRXOutput->getInputMessageQueue()->push(message);
        }

        m_deviceStatusCounter = 0;
    }
}

void XTRXOutputGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void XTRXOutputGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        XTRXOutput::MsgStartStop *message = XTRXOutput::MsgStartStop::create(checked);
        m_XTRXOutput->getInputMessageQueue()->push(message);
    }
}

void XTRXOutputGUI::on_centerFrequency_changed(quint64 value)
{
    setCenterFrequencySetting(value);
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void XTRXOutputGUI::on_ncoFrequency_changed(qint64 value)
{
    m_settings.m_ncoFrequency = value;
    setCenterFrequencyDisplay();
    m_settingsKeys.append("ncoFrequency");
    sendSettings();
}

void XTRXOutputGUI::on_ncoEnable_toggled(bool checked)
{
    m_settings.m_ncoEnable = checked;
    setCenterFrequencyDisplay();
    m_settingsKeys.append("ncoEnable");
    sendSettings();
}

void XTRXOutputGUI::on_sampleRate_changed(quint64 value)
{
    if (m_sampleRateMode) {
        m_settings.m_devSampleRate = value;
    } else {
        m_settings.m_devSampleRate = value * (1 << m_settings.m_log2SoftInterp);
    }

    updateDACRate();
    setNCODisplay();
    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void XTRXOutputGUI::on_hwInterp_currentIndexChanged(int index)
{
    if ((index <0) || (index > 5)) {
        return;
    }

    m_settings.m_log2HardInterp = index;
    m_settingsKeys.append("log2HardInterp");

    updateDACRate();
    setNCODisplay();
    sendSettings();
}

void XTRXOutputGUI::on_swInterp_currentIndexChanged(int index)
{
    if ((index <0) || (index > 6)) {
        return;
    }

    m_settings.m_log2SoftInterp = index;
    displaySampleRate();

    if (m_sampleRateMode) {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew();
    } else {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew() * (1 << m_settings.m_log2SoftInterp);
    }

    m_settingsKeys.append("log2SoftInterp");
    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void XTRXOutputGUI::on_lpf_changed(quint64 value)
{
    m_settings.m_lpfBW = value * 1000;
    m_settingsKeys.append("lpfBW");
    sendSettings();
}

void XTRXOutputGUI::on_gain_valueChanged(int value)
{
    m_settings.m_gain = value;
    ui->gainText->setText(tr("%1").arg(m_settings.m_gain));
    m_settingsKeys.append("gain");
    sendSettings();
}

void XTRXOutputGUI::on_antenna_currentIndexChanged(int index)
{
    m_settings.m_antennaPath = (xtrx_antenna_t) (index + (int) XTRX_TX_H);
    m_settingsKeys.append("antennaPath");
    sendSettings();
}

void XTRXOutputGUI::on_extClock_clicked()
{
    m_settings.m_extClock = ui->extClock->getExternalClockActive();
    m_settings.m_extClockFreq = ui->extClock->getExternalClockFrequency();
    qDebug("XTRXOutputGUI::on_extClock_clicked: %u Hz %s", m_settings.m_extClockFreq, m_settings.m_extClock ? "on" : "off");
    m_settingsKeys.append("extClock");
    m_settingsKeys.append("extClockFreq");
    sendSettings();
}

void XTRXOutputGUI::on_pwrmode_currentIndexChanged(int index)
{
    m_settings.m_pwrmode = index;
    m_settingsKeys.append("pwrmode");
    sendSettings();
}

void XTRXOutputGUI::on_sampleRateMode_toggled(bool checked)
{
    m_sampleRateMode = checked;
    displaySampleRate();
}

void XTRXOutputGUI::openDeviceSettingsDialog(const QPoint& p)
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

void XTRXOutputGUI::makeUIConnections()
{
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &XTRXOutputGUI::on_startStop_toggled);
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &XTRXOutputGUI::on_centerFrequency_changed);
    QObject::connect(ui->ncoFrequency, &ValueDialZ::changed, this, &XTRXOutputGUI::on_ncoFrequency_changed);
    QObject::connect(ui->ncoEnable, &ButtonSwitch::toggled, this, &XTRXOutputGUI::on_ncoEnable_toggled);
    QObject::connect(ui->sampleRate, &ValueDial::changed, this, &XTRXOutputGUI::on_sampleRate_changed);
    QObject::connect(ui->hwInterp, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &XTRXOutputGUI::on_hwInterp_currentIndexChanged);
    QObject::connect(ui->swInterp, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &XTRXOutputGUI::on_swInterp_currentIndexChanged);
    QObject::connect(ui->lpf, &ValueDial::changed, this, &XTRXOutputGUI::on_lpf_changed);
    QObject::connect(ui->gain, &QSlider::valueChanged, this, &XTRXOutputGUI::on_gain_valueChanged);
    QObject::connect(ui->antenna, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &XTRXOutputGUI::on_antenna_currentIndexChanged);
    QObject::connect(ui->extClock, &ExternalClockButton::clicked, this, &XTRXOutputGUI::on_extClock_clicked);
    QObject::connect(ui->pwrmode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &XTRXOutputGUI::on_pwrmode_currentIndexChanged);
    QObject::connect(ui->sampleRateMode, &QToolButton::toggled, this, &XTRXOutputGUI::on_sampleRateMode_toggled);
}
