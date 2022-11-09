///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include <libbladeRF.h>

#include "ui_bladerf2outputgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"

#include "bladerf2outputgui.h"

BladeRF2OutputGui::BladeRF2OutputGui(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::BladeRF2OutputGui),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_settings(),
    m_sampleRateMode(true),
    m_sampleRate(0),
    m_lastEngineState(DeviceAPI::StNotStarted)
{
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_sampleSink = (BladeRF2Output*) m_deviceUISet->m_deviceAPI->getSampleSink();
    int max, min, step;
    float scale;
    uint64_t f_min, f_max;

    ui->setupUi(getContents());
    sizeToContents();
    getContents()->setStyleSheet("#BladeRF2OutputGui { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesink/bladerf2output/readme.md";	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));

    m_sampleSink->getFrequencyRange(f_min, f_max, step, scale);
    qDebug("BladeRF2OutputGui::BladeRF2OutputGui: getFrequencyRange: [%lu,%lu] step: %d", f_min, f_max, step);
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(7, f_min/1000, f_max/1000);

    m_sampleSink->getSampleRateRange(min, max, step, scale);
    qDebug("BladeRF2OutputGui::BladeRF2OutputGui: getSampleRateRange: [%d,%d] step: %d", min, max, step);
    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, min, max);

    m_sampleSink->getBandwidthRange(min, max, step, scale);
    qDebug("BladeRF2OutputGui::BladeRF2OutputGui: getBandwidthRange: [%d,%d] step: %d", min, max, step);
    ui->bandwidth->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->bandwidth->setValueRange(5, min/1000, max/1000);

    m_sampleSink->getGlobalGainRange(m_gainMin, m_gainMax, m_gainStep, m_gainScale);
    qDebug("BladeRF2OutputGui::BladeRF2OutputGui: getGlobalGainRange: [%d,%d] step: %d scale: %f", m_gainMin, m_gainMax, m_gainStep, m_gainScale);
    ui->gain->setMinimum(m_gainMin/m_gainStep);
    ui->gain->setMaximum(m_gainMax/m_gainStep);
    ui->gain->setPageStep(1);
    ui->gain->setSingleStep(1);

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

    displaySettings();
    makeUIConnections();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleSink->setMessageQueueToGUI(&m_inputMessageQueue);
}

BladeRF2OutputGui::~BladeRF2OutputGui()
{
    m_statusTimer.stop();
    m_updateTimer.stop();
    delete ui;
}

void BladeRF2OutputGui::destroy()
{
    delete this;
}

void BladeRF2OutputGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

QByteArray BladeRF2OutputGui::serialize() const
{
    return m_settings.serialize();
}

bool BladeRF2OutputGui::deserialize(const QByteArray& data)
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

void BladeRF2OutputGui::updateFrequencyLimits()
{
    // values in kHz
    uint64_t f_min, f_max;
    int step;
    float scale;
    qint64 deltaFrequency = m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency/1000 : 0;
    m_sampleSink->getFrequencyRange(f_min, f_max, step, scale);
    qint64 minLimit = f_min/1000 + deltaFrequency;
    qint64 maxLimit = f_max/1000 + deltaFrequency;

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
    qDebug("BladeRF2OutputGui::updateFrequencyLimits: delta: %lld min: %lld max: %lld", deltaFrequency, minLimit, maxLimit);
}

void BladeRF2OutputGui::setCenterFrequencySetting(uint64_t kHzValue)
{
    int64_t centerFrequency = kHzValue*1000;

    m_settings.m_centerFrequency = centerFrequency < 0 ? 0 : (uint64_t) centerFrequency;
    ui->centerFrequency->setToolTip(QString("Main center frequency in kHz (LO: %1 kHz)").arg(centerFrequency/1000));
}

bool BladeRF2OutputGui::handleMessage(const Message& message)
{
    if (BladeRF2Output::MsgConfigureBladeRF2::match(message))
    {
        const BladeRF2Output::MsgConfigureBladeRF2& cfg = (BladeRF2Output::MsgConfigureBladeRF2&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        blockApplySettings(true);
        m_sampleSink->getGlobalGainRange(m_gainMin, m_gainMax, m_gainStep, m_gainScale);
        ui->gain->setMinimum(m_gainMin/m_gainStep);
        ui->gain->setMaximum(m_gainMax/m_gainStep);
        ui->gain->setPageStep(1);
        ui->gain->setSingleStep(1);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (BladeRF2Output::MsgReportGainRange::match(message))
    {
        const BladeRF2Output::MsgReportGainRange& cfg = (BladeRF2Output::MsgReportGainRange&) message;
        m_gainMin = cfg.getMin();
        m_gainMax = cfg.getMax();
        m_gainStep = cfg.getStep();
        m_gainScale = cfg.getScale();
        ui->gain->setMinimum(m_gainMin/m_gainStep);
        ui->gain->setMaximum(m_gainMax/m_gainStep);
        ui->gain->setSingleStep(1);
        ui->gain->setPageStep(1);

        return true;
    }
    else if (BladeRF2Output::MsgStartStop::match(message))
    {
        BladeRF2Output::MsgStartStop& notif = (BladeRF2Output::MsgStartStop&) message;
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

void BladeRF2OutputGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("BladeRF2OutputGui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("BladeRF2OutputGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
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

void BladeRF2OutputGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    displaySampleRate();
}

void BladeRF2OutputGui::displaySampleRate()
{
    int max, min, step;
    float scale;
    m_sampleSink->getSampleRateRange(min, max, step, scale);

    ui->sampleRate->blockSignals(true);

    if (m_sampleRateMode)
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(60,60,60); }");
        ui->sampleRateMode->setText("SR");
        ui->sampleRate->setValueRange(8, min, max);
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
        ui->sampleRate->setValueRange(8, min/(1<<m_settings.m_log2Interp), max/(1<<m_settings.m_log2Interp));
        ui->sampleRate->setValue(m_settings.m_devSampleRate/(1<<m_settings.m_log2Interp));
        ui->sampleRate->setToolTip("Baseband sample rate (S/s)");
        ui->deviceRateText->setToolTip("Host to device sample rate (S/s)");
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(m_settings.m_devSampleRate / 1000.0f, 'g', 5)));
    }

    ui->sampleRate->blockSignals(false);
}

void BladeRF2OutputGui::displaySettings()
{
    blockApplySettings(true);

    ui->transverter->setDeltaFrequency(m_settings.m_transverterDeltaFrequency);
    ui->transverter->setDeltaFrequencyActive(m_settings.m_transverterMode);

    updateFrequencyLimits();

    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    ui->LOppm->setValue(m_settings.m_LOppmTenths);
    ui->LOppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));

    displaySampleRate();

    ui->bandwidth->setValue(m_settings.m_bandwidth / 1000);
    ui->interp->setCurrentIndex(m_settings.m_log2Interp);
    ui->gainText->setText(tr("%1 dB").arg(QString::number(m_settings.m_globalGain, 'f', 2)));
    ui->gain->setValue(getGainValue(m_settings.m_globalGain));
    ui->biasTee->setChecked(m_settings.m_biasTee);

    blockApplySettings(false);
}

void BladeRF2OutputGui::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void BladeRF2OutputGui::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void BladeRF2OutputGui::on_LOppm_valueChanged(int value)
{
    ui->LOppmText->setText(QString("%1").arg(QString::number(value/10.0, 'f', 1)));
    m_settings.m_LOppmTenths = value;
    m_settingsKeys.append("LOppmTenths");
    sendSettings();
}

void BladeRF2OutputGui::on_sampleRate_changed(quint64 value)
{
    if (m_sampleRateMode) {
        m_settings.m_devSampleRate = value;
    } else {
        m_settings.m_devSampleRate = value * (1 << m_settings.m_log2Interp);
    }

    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void BladeRF2OutputGui::on_biasTee_toggled(bool checked)
{
    m_settings.m_biasTee = checked;
    m_settingsKeys.append("biasTee");
    sendSettings();
}

void BladeRF2OutputGui::on_bandwidth_changed(quint64 value)
{
    m_settings.m_bandwidth = value * 1000;
    m_settingsKeys.append("bandwidth");
    sendSettings();
}

void BladeRF2OutputGui::on_interp_currentIndexChanged(int index)
{
    if ((index <0) || (index > 6)) {
        return;
    }

    m_settings.m_log2Interp = index;
    m_settingsKeys.append("log2Interp");
    displaySampleRate();

    if (m_sampleRateMode) {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew();
    } else {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew() * (1 << m_settings.m_log2Interp);
    }

    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void BladeRF2OutputGui::on_gain_valueChanged(int value)
{
    float displayableGain = getGainDB(value);
    ui->gainText->setText(tr("%1 dB").arg(QString::number(displayableGain, 'f', 2)));
    m_settings.m_globalGain = (int) displayableGain;
    m_settingsKeys.append("globalGain");
    sendSettings();
}

void BladeRF2OutputGui::on_transverter_clicked()
{
    m_settings.m_transverterMode = ui->transverter->getDeltaFrequencyAcive();
    m_settings.m_transverterDeltaFrequency = ui->transverter->getDeltaFrequency();
    qDebug("LimeSDRInputGUI::on_transverter_clicked: %lld Hz %s", m_settings.m_transverterDeltaFrequency, m_settings.m_transverterMode ? "on" : "off");
    updateFrequencyLimits();
    setCenterFrequencySetting(ui->centerFrequency->getValueNew());
    m_settingsKeys.append("transverterMode");
    m_settingsKeys.append("transverterDeltaFrequency");
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void BladeRF2OutputGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        BladeRF2Output::MsgStartStop *message = BladeRF2Output::MsgStartStop::create(checked);
        m_sampleSink->getInputMessageQueue()->push(message);
    }
}

void BladeRF2OutputGui::on_sampleRateMode_toggled(bool checked)
{
    m_sampleRateMode = checked;
    displaySampleRate();
}

void BladeRF2OutputGui::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "BladeRF2OutputGui::updateHardware";
        BladeRF2Output::MsgConfigureBladeRF2* message = BladeRF2Output::MsgConfigureBladeRF2::create(m_settings, m_settingsKeys, m_forceSettings);
        m_sampleSink->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void BladeRF2OutputGui::updateStatus()
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

void BladeRF2OutputGui::openDeviceSettingsDialog(const QPoint& p)
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

float BladeRF2OutputGui::getGainDB(int gainValue)
{
    float gain = gainValue*m_gainStep*m_gainScale;
    // qDebug("BladeRF2OutputGui::getGainDB: gainValue: %d m_gainMin: %d m_gainMax: %d m_gainStep: %d gain: %f",
    //     gainValue, m_gainMin, m_gainMax, m_gainStep, gain);
    return gain;
}

int BladeRF2OutputGui::getGainValue(float gainDB)
{
    int gain = (gainDB/m_gainScale) / m_gainStep;
    // qDebug("BladeRF2OutputGui::getGainValue: gainDB: %f m_gainMin: %d m_gainMax: %d m_gainStep: %d gain: %d",
    //     gainDB, m_gainMin, m_gainMax, m_gainStep, gain);
    return gain;
}

void BladeRF2OutputGui::makeUIConnections()
{
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &BladeRF2OutputGui::on_centerFrequency_changed);
    QObject::connect(ui->LOppm, &QSlider::valueChanged, this, &BladeRF2OutputGui::on_LOppm_valueChanged);
    QObject::connect(ui->biasTee, &ButtonSwitch::toggled, this, &BladeRF2OutputGui::on_biasTee_toggled);
    QObject::connect(ui->sampleRate, &ValueDial::changed, this, &BladeRF2OutputGui::on_sampleRate_changed);
    QObject::connect(ui->bandwidth, &ValueDial::changed, this, &BladeRF2OutputGui::on_bandwidth_changed);
    QObject::connect(ui->interp, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BladeRF2OutputGui::on_interp_currentIndexChanged);
    QObject::connect(ui->gain, &QSlider::valueChanged, this, &BladeRF2OutputGui::on_gain_valueChanged);
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &BladeRF2OutputGui::on_startStop_toggled);
    QObject::connect(ui->transverter, &TransverterButton::clicked, this, &BladeRF2OutputGui::on_transverter_clicked);
    QObject::connect(ui->sampleRateMode, &QToolButton::toggled, this, &BladeRF2OutputGui::on_sampleRateMode_toggled);
}
