///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include "ui_audiooutputgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/basicdevicesettingsdialog.h"
#include "gui/audioselectdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "audiooutputgui.h"

#include "device/deviceapi.h"
#include "device/deviceuiset.h"

AudioOutputGui::AudioOutputGui(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::AudioOutputGui),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_settings(),
    m_centerFrequency(0)
{
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_audioOutput = (AudioOutput*) m_deviceUISet->m_deviceAPI->getSampleSink();

    ui->setupUi(getContents());
    sizeToContents();
    getContents()->setStyleSheet("#AudioOutputGui { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplesink/audiooutput/readme.md";

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

    m_sampleRate = m_audioOutput->getSampleRate();
    m_centerFrequency = m_audioOutput->getCenterFrequency();
    m_settings.m_deviceName = m_audioOutput->getDeviceName();
    updateSampleRateAndFrequency();
    displaySettings();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_audioOutput->setMessageQueueToGUI(&m_inputMessageQueue);

    makeUIConnections();
}

AudioOutputGui::~AudioOutputGui()
{
    m_updateTimer.stop();
    delete ui;
}

void AudioOutputGui::destroy()
{
    delete this;
}

void AudioOutputGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

QByteArray AudioOutputGui::serialize() const
{
    return m_settings.serialize();
}

bool AudioOutputGui::deserialize(const QByteArray& data)
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

bool AudioOutputGui::handleMessage(const Message& message)
{
    if (AudioOutput::MsgConfigureAudioOutput::match(message))
    {
        const AudioOutput::MsgConfigureAudioOutput& cfg = (AudioOutput::MsgConfigureAudioOutput&) message;

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
    else if (AudioOutput::MsgStartStop::match(message))
    {
        AudioOutput::MsgStartStop& notif = (AudioOutput::MsgStartStop&) message;
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

void AudioOutputGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("AudioOutputGui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_centerFrequency = notif->getCenterFrequency();
            qDebug("AudioOutputGui::handleInputMessages: DSPSignalNotification: SampleRate: %d", notif->getSampleRate());
            updateSampleRateAndFrequency();

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

void AudioOutputGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_centerFrequency);
    m_deviceUISet->getSpectrum()->setSsbSpectrum(false);
    m_deviceUISet->getSpectrum()->setLsbDisplay(false);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_sampleRate / 1000));
}

void AudioOutputGui::displaySettings()
{
    ui->deviceLabel->setText(m_settings.m_deviceName);
    ui->volume->setValue((int)(m_settings.m_volume*10.0f));
    ui->volumeText->setText(QString("%1").arg(m_settings.m_volume, 3, 'f', 1));
    ui->channels->setCurrentIndex((int)m_settings.m_iqMapping);
}

void AudioOutputGui::on_deviceSelect_clicked()
{
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_deviceName, false, this);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_deviceName = audioSelect.m_audioDeviceName;
        m_settingsKeys.append("deviceName");
        ui->deviceLabel->setText(m_settings.m_deviceName);
        sendSettings();
    }
}

void AudioOutputGui::on_volume_valueChanged(int value)
{
    m_settings.m_volume = value/10.0f;
    ui->volumeText->setText(QString("%1").arg(m_settings.m_volume, 3, 'f', 1));
    m_settingsKeys.append("volume");
    sendSettings();
}

void AudioOutputGui::on_channels_currentIndexChanged(int index)
{
    m_settings.m_iqMapping = (AudioOutputSettings::IQMapping) index;
    m_settingsKeys.append("iqMapping");
    updateSampleRateAndFrequency();
    sendSettings();
}

void AudioOutputGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        AudioOutput::MsgStartStop *message = AudioOutput::MsgStartStop::create(checked);
        m_audioOutput->getInputMessageQueue()->push(message);
    }
}

void AudioOutputGui::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void AudioOutputGui::updateHardware()
{
    if (m_doApplySettings)
    {
        AudioOutput::MsgConfigureAudioOutput* message = AudioOutput::MsgConfigureAudioOutput::create(m_settings, m_settingsKeys, m_forceSettings);
        m_audioOutput->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void AudioOutputGui::openDeviceSettingsDialog(const QPoint& p)
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

void AudioOutputGui::makeUIConnections()
{
    QObject::connect(ui->deviceSelect, &QPushButton::clicked, this, &AudioOutputGui::on_deviceSelect_clicked);
    QObject::connect(ui->volume, &QDial::valueChanged, this, &AudioOutputGui::on_volume_valueChanged);
    QObject::connect(ui->channels, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioOutputGui::on_channels_currentIndexChanged);
    QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &AudioOutputGui::on_startStop_toggled);
}
