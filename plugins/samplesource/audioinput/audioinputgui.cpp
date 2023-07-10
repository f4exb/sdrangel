///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#include <QMessageBox>
#include <QFileDialog>

#include "ui_audioinputgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "audioinputgui.h"

#include "device/deviceapi.h"
#include "device/deviceuiset.h"

AudioInputGui::AudioInputGui(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::AudioInputGui),
    m_forceSettings(true),
    m_settings(),
    m_sampleSource(nullptr),
    m_centerFrequency(0)
{
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_sampleSource = (AudioInput*) m_deviceUISet->m_deviceAPI->getSampleSource();

    ui->setupUi(getContents());
    sizeToContents();
    getContents()->setStyleSheet("#AudioInputGui { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesource/audioinput/readme.md";

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));
    connect(deviceUISet->m_deviceAPI, &DeviceAPI::stateChanged, this, &AudioInputGui::updateStatus);
    updateStatus();

    displaySettings();
    makeUIConnections();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleSource->setMessageQueueToGUI(&m_inputMessageQueue);
    DialPopup::addPopupsToChildDials(this);
}

AudioInputGui::~AudioInputGui()
{
    m_updateTimer.stop();
    delete ui;
}

void AudioInputGui::destroy()
{
    delete this;
}

void AudioInputGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

QByteArray AudioInputGui::serialize() const
{
    return m_settings.serialize();
}

bool AudioInputGui::deserialize(const QByteArray& data)
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

bool AudioInputGui::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        const DSPSignalNotification& notif = (DSPSignalNotification&) message;
        m_sampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        qDebug("AudioInputGui::handleInputMessages: DSPSignalNotification: SampleRate: %d", notif.getSampleRate());
        updateSampleRateAndFrequency();
        updateSpectrum();

        return true;
    }
    else if (AudioInput::MsgConfigureAudioInput::match(message))
    {
        const AudioInput::MsgConfigureAudioInput& cfg = (AudioInput::MsgConfigureAudioInput&) message;

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
    else if (AudioInput::MsgStartStop::match(message))
    {
        AudioInput::MsgStartStop& notif = (AudioInput::MsgStartStop&) message;
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

void AudioInputGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("AudioInputGui::handleInputMessages: message: %s", message->getIdentifier());

        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void AudioInputGui::updateSampleRateAndFrequency()
{
/*
    // Can't seem to get main spectrum to only display real part for mono/I-channel only
    if (m_settings.m_iqMapping <= AudioInputSettings::IQMapping::R)
    {
        m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate / 2);
        m_deviceUISet->getSpectrum()->setCenterFrequency(m_sampleRate / 4);
        m_deviceUISet->getSpectrum()->setSsbSpectrum(true);
        m_deviceUISet->getSpectrum()->setLsbDisplay(false);
    }
    else
*/
    {
        m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
        m_deviceUISet->getSpectrum()->setCenterFrequency(m_centerFrequency);
        m_deviceUISet->getSpectrum()->setSsbSpectrum(false);
        m_deviceUISet->getSpectrum()->setLsbDisplay(false);
    }
    ui->deviceRateText->setText(tr("%1k").arg((float)m_sampleRate / 1000));
}

void AudioInputGui::refreshDeviceList()
{
    ui->device->blockSignals(true);
    AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
    const QList<AudioDeviceInfo>& audioList = audioDeviceManager->getInputDevices();

    ui->device->clear();
    for (const auto &itAudio : audioList)
    {
        ui->device->addItem(AudioInputSettings::getFullDeviceName(itAudio));
    }
    ui->device->blockSignals(false);
}

void AudioInputGui::refreshSampleRates(QString deviceName)
{
    ui->sampleRate->blockSignals(true);
    ui->sampleRate->clear();
    const auto deviceInfos = AudioDeviceInfo::availableInputDevices();
    for (const AudioDeviceInfo &deviceInfo : deviceInfos)
    {
        if (deviceName == AudioInputSettings::getFullDeviceName(deviceInfo))
        {
            QList<int> sampleRates = deviceInfo.supportedSampleRates();

            for (int i = 0; i < sampleRates.size(); ++i) {
                ui->sampleRate->addItem(QString("%1").arg(sampleRates[i]));
            }
        }
    }
    ui->sampleRate->blockSignals(false);

    int index = ui->sampleRate->findText(QString("%1").arg(m_settings.m_sampleRate));

    if (index >= 0) {
        ui->sampleRate->setCurrentIndex(index);
    } else {
        ui->sampleRate->setCurrentIndex(0);
    }
}

void AudioInputGui::displaySettings()
{
    refreshDeviceList();
    int index = ui->device->findText(m_settings.m_deviceName);

    if (index >= 0) {
        ui->device->setCurrentIndex(index);
    } else {
        ui->device->setCurrentIndex(0);
    }
    // Make sure on_device_currentIndexChanged is called. Index may be the same for a different device
    on_device_currentIndexChanged(ui->device->currentIndex());

    ui->decim->setCurrentIndex(m_settings.m_log2Decim);
    ui->volume->setValue((int)(m_settings.m_volume*10.0f));
    ui->volumeText->setText(QString("%1").arg(m_settings.m_volume, 3, 'f', 1));
    ui->channels->setCurrentIndex((int)m_settings.m_iqMapping);
	ui->dcOffset->setChecked(m_settings.m_dcBlock);
	ui->iqImbalance->setChecked(m_settings.m_iqImbalance);
    refreshSampleRates(ui->device->currentText());
    displayFcTooltip();
    updateSpectrum();
}

void AudioInputGui::displayFcTooltip()
{
    int32_t fShift = DeviceSampleSource::calculateFrequencyShift(
        m_settings.m_log2Decim,
        (DeviceSampleSource::fcPos_t) m_settings.m_fcPos,
        m_settings.m_sampleRate,
        DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD
    );
    ui->fcPos->setToolTip(tr("Relative position of device center frequency: %1 kHz").arg(QString::number(fShift / 1000.0f, 'g', 5)));
}

void AudioInputGui::updateSpectrum()
{
    bool realElseComplex = (m_settings.m_iqMapping == AudioInputSettings::L)
        || (m_settings.m_iqMapping == AudioInputSettings::R);
    m_deviceUISet->getSpectrum()->setCenterFrequency(0);
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setSsbSpectrum(realElseComplex);
}

void AudioInputGui::on_device_currentIndexChanged(int index)
{
    (void) index;
    m_settings.m_deviceName = ui->device->currentText();
    refreshSampleRates(m_settings.m_deviceName);
    m_settingsKeys.append("deviceName");
    sendSettings();
}

void AudioInputGui::on_sampleRate_currentIndexChanged(int index)
{
    (void) index;
    m_settings.m_sampleRate = ui->sampleRate->currentText().toInt();
    displayFcTooltip();
    m_settingsKeys.append("sampleRate");
    sendSettings();
}

void AudioInputGui::on_decim_currentIndexChanged(int index)
{
    if ((index < 0) || (index > 6)) {
        return;
    }

    m_settings.m_log2Decim = index;
    displayFcTooltip();
    m_settingsKeys.append("log2Decim");
    sendSettings();
}

void AudioInputGui::on_volume_valueChanged(int value)
{
    m_settings.m_volume = value/10.0f;
    ui->volumeText->setText(QString("%1").arg(m_settings.m_volume, 3, 'f', 1));
    m_settingsKeys.append("volume");
    sendSettings();
}

void AudioInputGui::on_channels_currentIndexChanged(int index)
{
    m_settings.m_iqMapping = (AudioInputSettings::IQMapping)index;
    updateSampleRateAndFrequency();
    m_settingsKeys.append("iqMapping");
    sendSettings();
}

void AudioInputGui::on_dcOffset_toggled(bool checked)
{
	m_settings.m_dcBlock = checked;
    m_settingsKeys.append("dcBlock");
	sendSettings();
}

void AudioInputGui::on_iqImbalance_toggled(bool checked)
{
	m_settings.m_iqImbalance = checked;
    m_settingsKeys.append("iqImbalance");
	sendSettings();
}

void AudioInputGui::on_fcPos_currentIndexChanged(int index)
{
    m_settings.m_fcPos = (AudioInputSettings::fcPos_t) (index < 0 ? 0 : index > 2 ? 2 : index);
    displayFcTooltip();
    m_settingsKeys.append("fcPos");
    sendSettings();
}

void AudioInputGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        AudioInput::MsgStartStop *message = AudioInput::MsgStartStop::create(checked);
        m_sampleSource->getInputMessageQueue()->push(message);
    }
}

void AudioInputGui::sendSettings()
{
    if(!m_updateTimer.isActive())
        m_updateTimer.start(100);
}

void AudioInputGui::updateHardware()
{
    if (m_doApplySettings)
    {
        AudioInput::MsgConfigureAudioInput* message = AudioInput::MsgConfigureAudioInput::create(m_settings, m_settingsKeys, m_forceSettings);
        m_sampleSource->getInputMessageQueue()->push(message);
        m_settingsKeys.clear();
        m_forceSettings = false;
        m_updateTimer.stop();
    }
}

void AudioInputGui::updateStatus()
{
    int state = m_deviceUISet->m_deviceAPI->state();

    switch (state)
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
}

void AudioInputGui::openDeviceSettingsDialog(const QPoint& p)
{
    if (m_contextMenuType == ContextMenuDeviceSettings)
    {
        BasicDeviceSettingsDialog dialog(this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);

        dialog.move(p);
        new DialogPositioner(&dialog, false);
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

void AudioInputGui::makeUIConnections()
{
    QObject::connect(ui->device, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioInputGui::on_device_currentIndexChanged);
    QObject::connect(ui->sampleRate, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioInputGui::on_sampleRate_currentIndexChanged);
    QObject::connect(ui->decim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioInputGui::on_decim_currentIndexChanged);
    QObject::connect(ui->volume, &QDial::valueChanged, this, &AudioInputGui::on_volume_valueChanged);
    QObject::connect(ui->channels, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioInputGui::on_channels_currentIndexChanged);
    QObject::connect(ui->dcOffset, &ButtonSwitch::toggled, this, &AudioInputGui::on_dcOffset_toggled);
    QObject::connect(ui->iqImbalance, &ButtonSwitch::toggled, this, &AudioInputGui::on_iqImbalance_toggled);
    QObject::connect(ui->fcPos, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioInputGui::on_fcPos_currentIndexChanged);
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &AudioInputGui::on_startStop_toggled);
}
