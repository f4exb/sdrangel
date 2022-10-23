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
#include <QFileDialog>
#include <QResizeEvent>

#include <libbladeRF.h>

#include "ui_bladerf2inputgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"

#include "bladerf2inputgui.h"

BladeRF2InputGui::BladeRF2InputGui(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::Bladerf2InputGui),
    m_forceSettings(true),
    m_doApplySettings(true),
    m_settings(),
    m_sampleRateMode(true),
    m_sampleSource(0),
    m_sampleRate(0),
    m_lastEngineState(DeviceAPI::StNotStarted)
{
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_sampleSource = (BladeRF2Input*) m_deviceUISet->m_deviceAPI->getSampleSource();
    int max, min, step;
    float scale;
    uint64_t f_min, f_max;

    ui->setupUi(getContents());
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    getContents()->setStyleSheet("#Bladerf2InputGui { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesource/bladerf2input/readme.md";

    m_sampleSource->getFrequencyRange(f_min, f_max, step, scale);
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(7, f_min/1000, f_max/1000);

    m_sampleSource->getSampleRateRange(min, max, step, scale);
    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, min, max);

    m_sampleSource->getBandwidthRange(min, max, step, scale);
    ui->bandwidth->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->bandwidth->setValueRange(5, min/1000, max/1000);

    const std::vector<BladeRF2Input::GainMode>& modes = m_sampleSource->getGainModes();
    std::vector<BladeRF2Input::GainMode>::const_iterator it = modes.begin();

    ui->gainMode->blockSignals(true);

    for (; it != modes.end(); ++it) {
        ui->gainMode->addItem(it->m_name);
    }

    ui->gainMode->blockSignals(false);

    m_sampleSource->getGlobalGainRange(m_gainMin, m_gainMax, m_gainStep, m_gainScale);
    ui->gain->setMinimum(m_gainMin/m_gainStep);
    ui->gain->setMaximum(m_gainMax/m_gainStep);
    ui->gain->setPageStep(1);
    ui->gain->setSingleStep(1);

    ui->label_decim->setText(QString::fromUtf8("D\u2193"));

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

    displaySettings();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleSource->setMessageQueueToGUI(&m_inputMessageQueue);

    sendSettings();
    makeUIConnections();
}

BladeRF2InputGui::~BladeRF2InputGui()
{
    m_statusTimer.stop();
    m_updateTimer.stop();
    delete ui;
}

void BladeRF2InputGui::destroy()
{
    delete this;
}

void BladeRF2InputGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

QByteArray BladeRF2InputGui::serialize() const
{
    return m_settings.serialize();
}

bool BladeRF2InputGui::deserialize(const QByteArray& data)
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

void BladeRF2InputGui::resizeEvent(QResizeEvent* size)
{
    adjustSize();
    size->accept();
}

void BladeRF2InputGui::updateFrequencyLimits()
{
    // values in kHz
    uint64_t f_min, f_max;
    int step;
    float scale;
    qint64 deltaFrequency = m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency/1000 : 0;
    m_sampleSource->getFrequencyRange(f_min, f_max, step, scale);
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

void BladeRF2InputGui::setCenterFrequencySetting(uint64_t kHzValue)
{
    int64_t centerFrequency = kHzValue*1000;

    m_settings.m_centerFrequency = centerFrequency < 0 ? 0 : (uint64_t) centerFrequency;
    ui->centerFrequency->setToolTip(QString("Main center frequency in kHz (LO: %1 kHz)").arg(centerFrequency/1000));
}

bool BladeRF2InputGui::handleMessage(const Message& message)
{
    if (BladeRF2Input::MsgConfigureBladeRF2::match(message))
    {
        const BladeRF2Input::MsgConfigureBladeRF2& cfg = (BladeRF2Input::MsgConfigureBladeRF2&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        blockApplySettings(true);
        m_sampleSource->getGlobalGainRange(m_gainMin, m_gainMax, m_gainStep, m_gainScale);
        ui->gain->setMinimum(m_gainMin/m_gainStep);
        ui->gain->setMaximum(m_gainMax/m_gainStep);
        ui->gain->setPageStep(1);
        ui->gain->setSingleStep(1);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (BladeRF2Input::MsgReportGainRange::match(message))
    {
        const BladeRF2Input::MsgReportGainRange& cfg = (BladeRF2Input::MsgReportGainRange&) message;
        m_gainMin = cfg.getMin();
        m_gainMax = cfg.getMax();
        m_gainStep = cfg.getStep();
        m_gainScale = cfg.getScale();
        ui->gain->setMinimum(m_gainMin/m_gainStep);
        ui->gain->setMaximum(m_gainMax/m_gainStep);
        ui->gain->setPageStep(1);
        ui->gain->setSingleStep(1);

        return true;
    }
    else if (BladeRF2Input::MsgStartStop::match(message))
    {
        BladeRF2Input::MsgStartStop& notif = (BladeRF2Input::MsgStartStop&) message;
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

void BladeRF2InputGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("BladeRF2InputGui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("BladeRF2InputGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
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

void BladeRF2InputGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    displaySampleRate();
}

void BladeRF2InputGui::displaySampleRate()
{
    int max, min, step;
    float scale;
    m_sampleSource->getSampleRateRange(min, max, step, scale);

    ui->sampleRate->blockSignals(true);
    displayFcTooltip();

    if (m_sampleRateMode)
    {
        ui->sampleRateMode->setStyleSheet("QToolButton { background:rgb(60,60,60); }");
        ui->sampleRateMode->setText("SR");
        // BladeRF can go as low as 80 kS/s but because of buffering in practice experience is not good below 330 kS/s
        ui->sampleRate->setValueRange(8, min, max);
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
        // BladeRF can go as low as 80 kS/s but because of buffering in practice experience is not good below 330 kS/s
        ui->sampleRate->setValueRange(8, min/(1<<m_settings.m_log2Decim), max/(1<<m_settings.m_log2Decim));
        ui->sampleRate->setValue(m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim));
        ui->sampleRate->setToolTip("Baseband sample rate (S/s)");
        ui->deviceRateText->setToolTip("Device to host sample rate (S/s)");
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(m_settings.m_devSampleRate / 1000.0f, 'g', 5)));
    }

    ui->sampleRate->blockSignals(false);
}

void BladeRF2InputGui::displayFcTooltip()
{
    int32_t fShift = DeviceSampleSource::calculateFrequencyShift(
        m_settings.m_log2Decim,
        (DeviceSampleSource::fcPos_t) m_settings.m_fcPos,
        m_settings.m_devSampleRate,
        DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD
    );
    ui->fcPos->setToolTip(tr("Relative position of device center frequency: %1 kHz").arg(QString::number(fShift / 1000.0f, 'g', 5)));
}

void BladeRF2InputGui::displaySettings()
{
    blockApplySettings(true);

    ui->transverter->setDeltaFrequency(m_settings.m_transverterDeltaFrequency);
    ui->transverter->setDeltaFrequencyActive(m_settings.m_transverterMode);
    ui->transverter->setIQOrder(m_settings.m_iqOrder);

    updateFrequencyLimits();

    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    ui->LOppm->setValue(m_settings.m_LOppmTenths);
    ui->LOppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));

    displaySampleRate();

    ui->bandwidth->setValue(m_settings.m_bandwidth / 1000);
    ui->dcOffset->setChecked(m_settings.m_dcBlock);
    ui->iqImbalance->setChecked(m_settings.m_iqCorrection);
    ui->biasTee->setChecked(m_settings.m_biasTee);
    ui->decim->setCurrentIndex(m_settings.m_log2Decim);
    ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);
    ui->gainMode->setCurrentIndex(m_settings.m_gainMode);
    ui->gainText->setText(tr("%1 dB").arg(QString::number(m_settings.m_globalGain, 'f', 2)));
    ui->gain->setValue(getGainValue(m_settings.m_globalGain));

    if (m_settings.m_gainMode == BLADERF_GAIN_MANUAL) {
        ui->gain->setEnabled(true);
    } else {
        ui->gain->setEnabled(false);
    }

    blockApplySettings(false);
}

void BladeRF2InputGui::sendSettings()
{
    if(!m_updateTimer.isActive())
        m_updateTimer.start(100);
}

void BladeRF2InputGui::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void BladeRF2InputGui::on_LOppm_valueChanged(int value)
{
    ui->LOppmText->setText(QString("%1").arg(QString::number(value/10.0, 'f', 1)));
    m_settings.m_LOppmTenths = value;
    m_settingsKeys.append("LOppmTenths");
    sendSettings();
}

void BladeRF2InputGui::on_sampleRate_changed(quint64 value)
{
    if (m_sampleRateMode) {
        m_settings.m_devSampleRate = value;
    } else {
        m_settings.m_devSampleRate = value * (1 << m_settings.m_log2Decim);
    }

    displayFcTooltip();
    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void BladeRF2InputGui::on_dcOffset_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    m_settingsKeys.append("dcBlock");
    sendSettings();
}

void BladeRF2InputGui::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    m_settingsKeys.append("iqCorrection");
    sendSettings();
}

void BladeRF2InputGui::on_biasTee_toggled(bool checked)
{
    m_settings.m_biasTee = checked;
    m_settingsKeys.append("biasTee");
    sendSettings();
}

void BladeRF2InputGui::on_bandwidth_changed(quint64 value)
{
    m_settings.m_bandwidth = value * 1000;
    m_settingsKeys.append("bandwidth");
    sendSettings();
}

void BladeRF2InputGui::on_decim_currentIndexChanged(int index)
{
    if ((index <0) || (index > 6)) {
        return;
    }

    m_settings.m_log2Decim = index;
    displaySampleRate();

    if (m_sampleRateMode) {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew();
    } else {
        m_settings.m_devSampleRate = ui->sampleRate->getValueNew() * (1 << m_settings.m_log2Decim);
    }

    m_settingsKeys.append("log2Decim");
    m_settingsKeys.append("devSampleRate");
    sendSettings();
}

void BladeRF2InputGui::on_fcPos_currentIndexChanged(int index)
{
    m_settings.m_fcPos = (BladeRF2InputSettings::fcPos_t) (index < 0 ? 0 : index > 2 ? 2 : index);
    displayFcTooltip();
    m_settingsKeys.append("fcPos");
    sendSettings();
}

void BladeRF2InputGui::on_gainMode_currentIndexChanged(int index)
{
    const std::vector<BladeRF2Input::GainMode>& modes = m_sampleSource->getGainModes();
    unsigned int uindex = index < 0 ? 0 : (unsigned int) index;

    if (uindex < modes.size())
    {
        BladeRF2Input::GainMode mode = modes[index];

        if (m_settings.m_gainMode != mode.m_value)
        {
            if (mode.m_value == BLADERF_GAIN_MANUAL)
            {
                m_settings.m_globalGain = ui->gain->value();
                m_settingsKeys.append("globalGain");
                ui->gain->setEnabled(true);
            } else {
                ui->gain->setEnabled(false);
            }
        }

        m_settings.m_gainMode = mode.m_value;
        m_settingsKeys.append("gainMode");
        sendSettings();
    }
}

void BladeRF2InputGui::on_gain_valueChanged(int value)
{
    float displayableGain = getGainDB(value);
    ui->gainText->setText(tr("%1 dB").arg(QString::number(displayableGain, 'f', 2)));
    m_settings.m_globalGain = (int) displayableGain;
    m_settingsKeys.append("globalGain");
    sendSettings();
}

void BladeRF2InputGui::on_transverter_clicked()
{
    m_settings.m_transverterMode = ui->transverter->getDeltaFrequencyAcive();
    m_settings.m_transverterDeltaFrequency = ui->transverter->getDeltaFrequency();
    m_settings.m_iqOrder = ui->transverter->getIQOrder();
    qDebug("BladeRF2InputGui::on_transverter_clicked: %lld Hz %s", m_settings.m_transverterDeltaFrequency, m_settings.m_transverterMode ? "on" : "off");
    updateFrequencyLimits();
    setCenterFrequencySetting(ui->centerFrequency->getValueNew());
    m_settingsKeys.append("transverterMode");
    m_settingsKeys.append("transverterDeltaFrequency");
    m_settingsKeys.append("iqOrder");
    m_settingsKeys.append("centerFrequency");
    sendSettings();
}

void BladeRF2InputGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        BladeRF2Input::MsgStartStop *message = BladeRF2Input::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void BladeRF2InputGui::on_sampleRateMode_toggled(bool checked)
{
    m_sampleRateMode = checked;
    displaySampleRate();
}

void BladeRF2InputGui::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "BladeRF2InputGui::updateHardware";
        BladeRF2Input::MsgConfigureBladeRF2* message = BladeRF2Input::MsgConfigureBladeRF2::create(m_settings, m_settingsKeys, m_forceSettings);
        m_sampleSource->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void BladeRF2InputGui::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void BladeRF2InputGui::updateStatus()
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

void BladeRF2InputGui::openDeviceSettingsDialog(const QPoint& p)
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

float BladeRF2InputGui::getGainDB(int gainValue)
{
    float gain = gainValue*m_gainStep*m_gainScale;
    // qDebug("BladeRF2InputGui::getGainDB: gainValue: %d m_gainMin: %d m_gainMax: %d m_gainStep: %d m_gainScale: %f gain: %f",
    //     gainValue, m_gainMin, m_gainMax, m_gainStep, m_gainScale, gain);
    return gain;
}

int BladeRF2InputGui::getGainValue(float gainDB)
{
    int gain = (gainDB/m_gainScale) / m_gainStep;
    // qDebug("BladeRF2InputGui::getGainValue: gainDB: %f m_gainMin: %d m_gainMax: %d m_gainStep: %d m_gainScale: %f gain: %d",
    //     gainDB, m_gainMin, m_gainMax, m_gainStep, m_gainScale, gain);
    return gain;
}

void BladeRF2InputGui::makeUIConnections()
{
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &BladeRF2InputGui::on_centerFrequency_changed);
    QObject::connect(ui->LOppm, &QSlider::valueChanged, this, &BladeRF2InputGui::on_LOppm_valueChanged);
    QObject::connect(ui->sampleRate, &ValueDial::changed, this, &BladeRF2InputGui::on_sampleRate_changed);
    QObject::connect(ui->dcOffset, &ButtonSwitch::toggled, this, &BladeRF2InputGui::on_dcOffset_toggled);
    QObject::connect(ui->iqImbalance, &ButtonSwitch::toggled, this, &BladeRF2InputGui::on_iqImbalance_toggled);
    QObject::connect(ui->biasTee, &ButtonSwitch::toggled, this, &BladeRF2InputGui::on_biasTee_toggled);
    QObject::connect(ui->bandwidth, &ValueDial::changed, this, &BladeRF2InputGui::on_bandwidth_changed);
    QObject::connect(ui->decim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BladeRF2InputGui::on_decim_currentIndexChanged);
    QObject::connect(ui->fcPos, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BladeRF2InputGui::on_fcPos_currentIndexChanged);
    QObject::connect(ui->gainMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BladeRF2InputGui::on_gainMode_currentIndexChanged);
    QObject::connect(ui->gain, &QSlider::valueChanged, this, &BladeRF2InputGui::on_gain_valueChanged);
    QObject::connect(ui->transverter, &TransverterButton::clicked, this, &BladeRF2InputGui::on_transverter_clicked);
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &BladeRF2InputGui::on_startStop_toggled);
    QObject::connect(ui->sampleRateMode, &QToolButton::toggled, this, &BladeRF2InputGui::on_sampleRateMode_toggled);
}
