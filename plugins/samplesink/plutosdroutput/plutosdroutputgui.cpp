///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include <stdio.h>
#include <QDebug>
#include <QMessageBox>

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "plutosdr/deviceplutosdr.h"
#include "plutosdroutput.h"
#include "plutosdroutputgui.h"
#include "ui_plutosdroutputgui.h"

PlutoSDROutputGUI::PlutoSDROutputGUI(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::PlutoSDROutputGUI),
    m_settings(),
    m_sampleRateMode(true),
    m_forceSettings(true),
    m_sampleSink(0),
    m_sampleRate(0),
    m_deviceCenterFrequency(0),
    m_lastEngineState(DeviceAPI::StNotStarted),
    m_doApplySettings(true),
    m_statusCounter(0)
{
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_sampleSink = (PlutoSDROutput*) m_deviceUISet->m_deviceAPI->getSampleSink();

    ui->setupUi(getContents());
    sizeToContents();
    getContents()->setStyleSheet("#PlutoSDROutputGUI { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesink/plutosdroutput/readme.md";
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    updateFrequencyLimits();

    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, DevicePlutoSDR::srLowLimitFreq, DevicePlutoSDR::srHighLimitFreq);

    ui->lpf->setColorMapper(ColorMapper(ColorMapper::GrayYellow));

    quint32 minLimit, maxLimit;
    ((PlutoSDROutput *) m_sampleSink)->getbbLPRange(minLimit, maxLimit);
    ui->lpf->setValueRange(5, minLimit/1000, maxLimit/1000);

    ui->lpFIR->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpFIR->setValueRange(5, 1U, 56000U); // will be dynamically recalculated

    ui->swInterpLabel->setText(QString::fromUtf8("S\u2191"));
    ui->lpFIRInterpolationLabel->setText(QString::fromUtf8("\u2191"));

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

    blockApplySettings(true);
    displaySettings();
    makeUIConnections();
    blockApplySettings(false);

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

PlutoSDROutputGUI::~PlutoSDROutputGUI()
{
    m_statusTimer.stop();
    m_updateTimer.stop();
    delete ui;
}

void PlutoSDROutputGUI::destroy()
{
    delete this;
}

void PlutoSDROutputGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    sendSettings(true);
}

QByteArray PlutoSDROutputGUI::serialize() const
{
    return m_settings.serialize();
}

bool PlutoSDROutputGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        sendSettings(true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool PlutoSDROutputGUI::handleMessage(const Message& message)
{
    (void) message;
    if (PlutoSDROutput::MsgConfigurePlutoSDR::match(message))
    {
        const PlutoSDROutput::MsgConfigurePlutoSDR& cfg = (PlutoSDROutput::MsgConfigurePlutoSDR&) message;

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
    else if (DevicePlutoSDRShared::MsgCrossReportToBuddy::match(message)) // message from buddy
    {
        DevicePlutoSDRShared::MsgCrossReportToBuddy& conf = (DevicePlutoSDRShared::MsgCrossReportToBuddy&) message;
        m_settings.m_devSampleRate = conf.getDevSampleRate();
        m_settings.m_lpfFIRlog2Interp = conf.getLpfFiRlog2IntDec();
        m_settings.m_lpfFIRBW = conf.getLpfFirbw();
        m_settings.m_LOppmTenths = conf.getLoPPMTenths();
        m_settings.m_lpfFIREnable = conf.isLpfFirEnable();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (PlutoSDROutput::MsgStartStop::match(message))
    {
        PlutoSDROutput::MsgStartStop& notif = (PlutoSDROutput::MsgStartStop&) message;
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

void PlutoSDROutputGUI::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        PlutoSDROutput::MsgStartStop *message = PlutoSDROutput::MsgStartStop::create(checked);
        m_sampleSink->getInputMessageQueue()->push(message);
    }
}

void PlutoSDROutputGUI::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void PlutoSDROutputGUI::on_loPPM_valueChanged(int value)
{
    ui->loPPMText->setText(QString("%1").arg(QString::number(value/10.0, 'f', 1)));
    m_settings.m_LOppmTenths = value;
    m_settingsKeys.append("LOppmTenths");
    sendSettings();
}

void PlutoSDROutputGUI::on_swInterp_currentIndexChanged(int index)
{
    m_settings.m_log2Interp = index > 5 ? 5 : index;
    displaySampleRate();
    m_settings.m_devSampleRate = ui->sampleRate->getValueNew();

    if (!m_sampleRateMode) {
        m_settings.m_devSampleRate <<= m_settings.m_log2Interp;
    }

    m_settingsKeys.append("log2Interp");
    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void PlutoSDROutputGUI::on_sampleRate_changed(quint64 value)
{
    m_settings.m_devSampleRate = value;

    if (!m_sampleRateMode) {
        m_settings.m_devSampleRate <<= m_settings.m_log2Interp;
    }

    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void PlutoSDROutputGUI::on_lpf_changed(quint64 value)
{
    m_settings.m_lpfBW = value * 1000;
    m_settingsKeys.append("lpfBW");
    sendSettings();
}

void PlutoSDROutputGUI::on_lpFIREnable_toggled(bool checked)
{
    m_settings.m_lpfFIREnable = checked;
    ui->lpFIRInterpolation->setEnabled(checked);
    ui->lpFIRGain->setEnabled(checked);
    m_settingsKeys.append("lpfFIREnable");
    sendSettings();
}

void PlutoSDROutputGUI::on_lpFIR_changed(quint64 value)
{
    m_settings.m_lpfFIRBW = value * 1000;
    m_settingsKeys.append("lpfFIRBW");
    sendSettings();
}

void PlutoSDROutputGUI::on_lpFIRInterpolation_currentIndexChanged(int index)
{
    m_settings.m_lpfFIRlog2Interp = index > 2 ? 2 : index;
    m_settingsKeys.append("lpfFIRlog2Interp");
    setSampleRateLimits();
    sendSettings();
}

void PlutoSDROutputGUI::on_lpFIRGain_currentIndexChanged(int index)
{
    m_settings.m_lpfFIRGain = 6*(index > 1 ? 1 : index) - 6;
    m_settingsKeys.append("lpfFIRGain");
    sendSettings();
}

void PlutoSDROutputGUI::on_att_valueChanged(int value)
{
    ui->attText->setText(QString("%1 dB").arg(QString::number(value*0.25, 'f', 2)));
    m_settings.m_att = value;
    m_settingsKeys.append("att");
    sendSettings();
}

void PlutoSDROutputGUI::on_antenna_currentIndexChanged(int index)
{
    m_settings.m_antennaPath = (PlutoSDROutputSettings::RFPath) (index < PlutoSDROutputSettings::RFPATH_END ? index : 0);
    m_settingsKeys.append("antennaPath");
    sendSettings();
}

void PlutoSDROutputGUI::on_transverter_clicked()
{
    m_settings.m_transverterMode = ui->transverter->getDeltaFrequencyAcive();
    m_settings.m_transverterDeltaFrequency = ui->transverter->getDeltaFrequency();
    qDebug("PlutoSDROutputGUI::on_transverter_clicked: %lld Hz %s", m_settings.m_transverterDeltaFrequency, m_settings.m_transverterMode ? "on" : "off");
    updateFrequencyLimits();
    m_settings.m_centerFrequency = ui->centerFrequency->getValueNew()*1000;
    m_settingsKeys.append("transverterMode");
    m_settingsKeys.append("transverterDeltaFrequency");
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void PlutoSDROutputGUI::on_sampleRateMode_toggled(bool checked)
{
    m_sampleRateMode = checked;
    displaySampleRate();
}

void PlutoSDROutputGUI::displaySampleRate()
{
    ui->sampleRate->blockSignals(true);

    if (m_sampleRateMode)
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(60,60,60); }");
        ui->sampleRateMode->setText("SR");
        ui->sampleRate->setValueRange(8, DevicePlutoSDR::srLowLimitFreq, DevicePlutoSDR::srHighLimitFreq);
        ui->sampleRate->setValue(m_settings.m_devSampleRate);
        ui->sampleRate->setToolTip("Host to device sample rate (S/s)");
        ui->deviceRateText->setToolTip("Baseband sample rate (S/s)");
        uint32_t basebandSampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2Interp);
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(basebandSampleRate / 1000.0f, 'g', 5)));
    }
    else
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(50,50,50); }");
        ui->sampleRateMode->setText("BB");
        ui->sampleRate->setValueRange(8, DevicePlutoSDR::srLowLimitFreq/(1<<m_settings.m_log2Interp), DevicePlutoSDR::srHighLimitFreq/(1<<m_settings.m_log2Interp));
        ui->sampleRate->setValue(m_settings.m_devSampleRate/(1<<m_settings.m_log2Interp));
        ui->sampleRate->setToolTip("Baseband sample rate (S/s)");
        ui->deviceRateText->setToolTip("Host to device sample rate (S/s)");
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(m_settings.m_devSampleRate / 1000.0f, 'g', 5)));
    }

    ui->sampleRate->blockSignals(false);
}

void PlutoSDROutputGUI::displaySettings()
{
    ui->transverter->setDeltaFrequency(m_settings.m_transverterDeltaFrequency);
    ui->transverter->setDeltaFrequencyActive(m_settings.m_transverterMode);
    updateFrequencyLimits();
    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    displaySampleRate();

    ui->loPPM->setValue(m_settings.m_LOppmTenths);
    ui->loPPMText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));

    ui->swInterp->setCurrentIndex(m_settings.m_log2Interp);

    ui->lpf->setValue(m_settings.m_lpfBW / 1000);

    ui->lpFIREnable->setChecked(m_settings.m_lpfFIREnable);
    ui->lpFIR->setValue(m_settings.m_lpfFIRBW / 1000);
    ui->lpFIRInterpolation->setCurrentIndex(m_settings.m_lpfFIRlog2Interp);
    ui->lpFIRGain->setCurrentIndex((m_settings.m_lpfFIRGain + 6)/6);
    ui->lpFIRInterpolation->setEnabled(m_settings.m_lpfFIREnable);
    ui->lpFIRGain->setEnabled(m_settings.m_lpfFIREnable);

    ui->att->setValue(m_settings.m_att);
    ui->attText->setText(QString("%1 dB").arg(QString::number(m_settings.m_att*0.25, 'f', 2)));

    ui->antenna->setCurrentIndex((int) m_settings.m_antennaPath);

    setFIRBWLimits();
    setSampleRateLimits();
}

void PlutoSDROutputGUI::sendSettings(bool forceSettings)
{
    m_forceSettings = forceSettings;

    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void PlutoSDROutputGUI::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "PlutoSDROutputGUI::updateHardware";
        PlutoSDROutput::MsgConfigurePlutoSDR* message = PlutoSDROutput::MsgConfigurePlutoSDR::create(m_settings, m_settingsKeys, m_forceSettings);
        m_sampleSink->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void PlutoSDROutputGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void PlutoSDROutputGUI::updateStatus()
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

    if (m_statusCounter % 2 == 0) // 1s
    {
        uint32_t dacRate = ((PlutoSDROutput *) m_sampleSink)->getDACSampleRate();

        if (dacRate < 100000000) {
            ui->dacRateLabel->setText(tr("%1k").arg(QString::number(dacRate / 1000.0f, 'g', 5)));
        } else {
            ui->dacRateLabel->setText(tr("%1M").arg(QString::number(dacRate / 1000000.0f, 'g', 5)));
        }
    }

    if (m_statusCounter % 4 == 0) // 2s
    {
        std::string rssiStr;
        ((PlutoSDROutput *) m_sampleSink)->getRSSI(rssiStr);
        ui->rssiText->setText(tr("-%1").arg(QString::fromStdString(rssiStr)));
    }

    if (m_statusCounter % 10 == 0) // 5s
    {
        if (m_deviceUISet->m_deviceAPI->isBuddyLeader()) {
            ((PlutoSDROutput *) m_sampleSink)->fetchTemperature();
        }

        ui->temperatureText->setText(tr("%1C").arg(QString::number(((PlutoSDROutput *) m_sampleSink)->getTemperature(), 'f', 0)));
    }

    m_statusCounter++;
}

void PlutoSDROutputGUI::setFIRBWLimits()
{
    float high = DevicePlutoSDR::firBWHighLimitFactor * ((PlutoSDROutput *) m_sampleSink)->getFIRSampleRate();
    float low = DevicePlutoSDR::firBWLowLimitFactor * ((PlutoSDROutput *) m_sampleSink)->getFIRSampleRate();
    ui->lpFIR->setValueRange(5, (int(low)/1000)+1, (int(high)/1000)+1);
    ui->lpFIR->setValue(m_settings.m_lpfFIRBW/1000);
}

void PlutoSDROutputGUI::setSampleRateLimits()
{
    uint32_t low = ui->lpFIREnable->isChecked() ? DevicePlutoSDR::srLowLimitFreq / (1<<ui->lpFIRInterpolation->currentIndex()) : DevicePlutoSDR::srLowLimitFreq;
    ui->sampleRate->setValueRange(8, low, DevicePlutoSDR::srHighLimitFreq);
    ui->sampleRate->setValue(m_settings.m_devSampleRate);
}

void PlutoSDROutputGUI::updateFrequencyLimits()
{
    qint64 minLimit, maxLimit;
    // values in kHz
    qint64 deltaFrequency = m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency/1000 : 0;
    ((PlutoSDROutput *) m_sampleSink)->getLORange(minLimit, maxLimit);

    minLimit = minLimit/1000 + deltaFrequency;
    maxLimit = maxLimit/1000 + deltaFrequency;

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
    qDebug("PlutoSDRInputGui::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);
}

void PlutoSDROutputGUI::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("PlutoSDROutputGUI::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("PlutoSDROutputGUI::handleInputMessages: DSPSignalNotification: SampleRate: %d, CenterFrequency: %llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();
            setFIRBWLimits();

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

void PlutoSDROutputGUI::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    displaySampleRate();
}

void PlutoSDROutputGUI::openDeviceSettingsDialog(const QPoint& p)
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

void PlutoSDROutputGUI::makeUIConnections()
{
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &PlutoSDROutputGUI::on_startStop_toggled);
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &PlutoSDROutputGUI::on_centerFrequency_changed);
    QObject::connect(ui->loPPM, &QSlider::valueChanged, this, &PlutoSDROutputGUI::on_loPPM_valueChanged);
    QObject::connect(ui->swInterp, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlutoSDROutputGUI::on_swInterp_currentIndexChanged);
    QObject::connect(ui->sampleRate, &ValueDial::changed, this, &PlutoSDROutputGUI::on_sampleRate_changed);
    QObject::connect(ui->lpf, &ValueDial::changed, this, &PlutoSDROutputGUI::on_lpf_changed);
    QObject::connect(ui->lpFIREnable, &ButtonSwitch::toggled, this, &PlutoSDROutputGUI::on_lpFIREnable_toggled);
    QObject::connect(ui->lpFIR, &ValueDial::changed, this, &PlutoSDROutputGUI::on_lpFIR_changed);
    QObject::connect(ui->lpFIRInterpolation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlutoSDROutputGUI::on_lpFIRInterpolation_currentIndexChanged);
    QObject::connect(ui->lpFIRGain, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlutoSDROutputGUI::on_lpFIRGain_currentIndexChanged);
    QObject::connect(ui->att, &QDial::valueChanged, this, &PlutoSDROutputGUI::on_att_valueChanged);
    QObject::connect(ui->antenna, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlutoSDROutputGUI::on_antenna_currentIndexChanged);
    QObject::connect(ui->transverter, &TransverterButton::clicked, this, &PlutoSDROutputGUI::on_transverter_clicked);
    QObject::connect(ui->sampleRateMode, &QToolButton::toggled, this, &PlutoSDROutputGUI::on_sampleRateMode_toggled);
}
