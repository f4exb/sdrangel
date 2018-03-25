///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include "audio/audiodevicemanager.h"
#include "util/simpleserializer.h"

#include <QDataStream>
#include <QDebug>

const float AudioDeviceManager::m_defaultAudioInputVolume = 0.15f;
const QString AudioDeviceManager::m_defaultUDPAddress = "127.0.0.1";

QDataStream& operator<<(QDataStream& ds, const AudioDeviceManager::InputDeviceInfo& info)
{
    ds << info.sampleRate << info.volume;
    return ds;
}

QDataStream& operator>>(QDataStream& ds, AudioDeviceManager::InputDeviceInfo& info)
{
    ds >> info.sampleRate >> info.volume;
    return ds;
}

QDataStream& operator<<(QDataStream& ds, const AudioDeviceManager::OutputDeviceInfo& info)
{
    ds << info.sampleRate << info.udpAddress << info.udpPort << info.copyToUDP << info.udpStereo << info.udpUseRTP;
    return ds;
}

QDataStream& operator>>(QDataStream& ds, AudioDeviceManager::OutputDeviceInfo& info)
{
    ds >> info.sampleRate >> info.udpAddress >> info.udpPort >> info.copyToUDP >> info.udpStereo >> info.udpUseRTP;
    return ds;
}

AudioDeviceManager::AudioDeviceManager()
{
    m_inputDevicesInfo = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    m_outputDevicesInfo = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
}

AudioDeviceManager::~AudioDeviceManager()
{
    QMap<int, AudioOutput*>::iterator it = m_audioOutputs.begin();

    for (; it != m_audioOutputs.end(); ++it) {
        delete(*it);
    }
}

bool AudioDeviceManager::getOutputDeviceName(int outputDeviceIndex, QString &deviceName) const
{
    if (outputDeviceIndex < 0)
    {
        deviceName = "System default device";
        return true;
    }
    else
    {
        if (outputDeviceIndex < m_outputDevicesInfo.size())
        {
            deviceName = m_outputDevicesInfo[outputDeviceIndex].deviceName();
            return true;
        }
        else
        {
            return false;
        }
    }
}

bool AudioDeviceManager::getInputDeviceName(int inputDeviceIndex, QString &deviceName) const
{
    if (inputDeviceIndex < 0)
    {
        deviceName = "System default device";
        return true;
    }
    else
    {
        if (inputDeviceIndex < m_inputDevicesInfo.size())
        {
            deviceName = m_inputDevicesInfo[inputDeviceIndex].deviceName();
            return true;
        }
        else
        {
            return false;
        }
    }
}

void AudioDeviceManager::resetToDefaults()
{
}

QByteArray AudioDeviceManager::serialize() const
{
    qDebug("AudioDeviceManager::serialize");
    debugAudioInputInfos();
    debugAudioOutputInfos();

    SimpleSerializer s(1);
    QByteArray data;

    serializeInputMap(data);
    s.writeBlob(1, data);
    serializeOutputMap(data);
    s.writeBlob(2, data);

    return s.final();
}

void AudioDeviceManager::serializeInputMap(QByteArray& data) const
{
    QDataStream *stream = new QDataStream(&data, QIODevice::WriteOnly);
    *stream << m_audioInputInfos;
    delete stream;
}

void AudioDeviceManager::serializeOutputMap(QByteArray& data) const
{
    QDataStream *stream = new QDataStream(&data, QIODevice::WriteOnly);
    *stream << m_audioOutputInfos;
    delete stream;
}

bool AudioDeviceManager::deserialize(const QByteArray& data)
{
    qDebug("AudioDeviceManager::deserialize");

    SimpleDeserializer d(data);

    if(!d.isValid()) {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        QByteArray data;

        d.readBlob(1, &data);
        deserializeInputMap(data);
        d.readBlob(2, &data);
        deserializeOutputMap(data);

        debugAudioInputInfos();
        debugAudioOutputInfos();

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void AudioDeviceManager::deserializeInputMap(QByteArray& data)
{
    QDataStream readStream(&data, QIODevice::ReadOnly);
    readStream >> m_audioInputInfos;
}

void AudioDeviceManager::deserializeOutputMap(QByteArray& data)
{
    QDataStream readStream(&data, QIODevice::ReadOnly);
    readStream >> m_audioOutputInfos;
}

void AudioDeviceManager::addAudioSink(AudioFifo* audioFifo, MessageQueue *sampleSinkMessageQueue, int outputDeviceIndex)
{
    qDebug("AudioDeviceManager::addAudioSink: %d: %p", outputDeviceIndex, audioFifo);

    if (m_audioOutputs.find(outputDeviceIndex) == m_audioOutputs.end()) {
        m_audioOutputs[outputDeviceIndex] = new AudioOutput();
    }

    if (m_audioOutputs[outputDeviceIndex]->getNbFifos() == 0) {
        startAudioOutput(outputDeviceIndex);
    }

    if (m_audioSinkFifos.find(audioFifo) == m_audioSinkFifos.end()) // new FIFO
    {
        m_audioOutputs[outputDeviceIndex]->addFifo(audioFifo);
    }
    else
    {
        int audioOutputDeviceIndex = m_audioSinkFifos[audioFifo];

        if (audioOutputDeviceIndex != outputDeviceIndex) // change of audio device
        {
            removeAudioSink(audioFifo); // remove from current
            m_audioOutputs[outputDeviceIndex]->addFifo(audioFifo); // add to new
        }
    }

    m_audioSinkFifos[audioFifo] = outputDeviceIndex; // register audio FIFO
    m_sampleSinkMessageQueues[audioFifo] = sampleSinkMessageQueue;
}

void AudioDeviceManager::removeAudioSink(AudioFifo* audioFifo)
{
    qDebug("AudioDeviceManager::removeAudioSink: %p", audioFifo);

    if (m_audioSinkFifos.find(audioFifo) == m_audioSinkFifos.end())
    {
        qWarning("AudioDeviceManager::removeAudioSink: audio FIFO %p not found", audioFifo);
        return;
    }

    int audioOutputDeviceIndex = m_audioSinkFifos[audioFifo];
    m_audioOutputs[audioOutputDeviceIndex]->removeFifo(audioFifo);

    if (m_audioOutputs[audioOutputDeviceIndex]->getNbFifos() == 0) {
        stopAudioOutput(audioOutputDeviceIndex);
    }

    m_audioSinkFifos.remove(audioFifo); // unregister audio FIFO
    m_sampleSinkMessageQueues.remove(audioFifo);
}

void AudioDeviceManager::addAudioSource(AudioFifo* audioFifo, MessageQueue *sampleSourceMessageQueue, int inputDeviceIndex)
{
    qDebug("AudioDeviceManager::addAudioSource: %d: %p", inputDeviceIndex, audioFifo);

    if (m_audioInputs.find(inputDeviceIndex) == m_audioInputs.end()) {
        m_audioInputs[inputDeviceIndex] = new AudioInput();
    }

    if (m_audioInputs[inputDeviceIndex]->getNbFifos() == 0) {
        startAudioInput(inputDeviceIndex);
    }

    if (m_audioSourceFifos.find(audioFifo) == m_audioSourceFifos.end()) // new FIFO
    {
        m_audioInputs[inputDeviceIndex]->addFifo(audioFifo);
    }
    else
    {
        int audioInputDeviceIndex = m_audioSourceFifos[audioFifo];

        if (audioInputDeviceIndex != inputDeviceIndex) // change of audio device
        {
            removeAudioSource(audioFifo); // remove from current
            m_audioInputs[inputDeviceIndex]->addFifo(audioFifo); // add to new
        }
    }

    m_audioSourceFifos[audioFifo] = inputDeviceIndex; // register audio FIFO
    m_sampleSourceMessageQueues[audioFifo] = sampleSourceMessageQueue;
}

void AudioDeviceManager::removeAudioSource(AudioFifo* audioFifo)
{
    qDebug("AudioDeviceManager::removeAudioSource: %p", audioFifo);

    if (m_audioSourceFifos.find(audioFifo) == m_audioSourceFifos.end())
    {
        qWarning("AudioDeviceManager::removeAudioSource: audio FIFO %p not found", audioFifo);
        return;
    }

    int audioInputDeviceIndex = m_audioSourceFifos[audioFifo];
    m_audioInputs[audioInputDeviceIndex]->removeFifo(audioFifo);

    if (m_audioInputs[audioInputDeviceIndex]->getNbFifos() == 0) {
        stopAudioInput(audioInputDeviceIndex);
    }

    m_audioSourceFifos.remove(audioFifo); // unregister audio FIFO
    m_sampleSourceMessageQueues.remove(audioFifo);
}

void AudioDeviceManager::startAudioOutput(int outputDeviceIndex)
{
    unsigned int sampleRate;
    QString udpAddress;
    quint16 udpPort;
    bool copyAudioToUDP;
    bool udpStereo;
    bool udpUseRTP;
    QString deviceName;

    if (getOutputDeviceName(outputDeviceIndex, deviceName))
    {
        if (m_audioOutputInfos.find(deviceName) == m_audioOutputInfos.end())
        {
            sampleRate = m_defaultAudioSampleRate;
            udpAddress = m_defaultUDPAddress;
            udpPort = m_defaultUDPPort;
            copyAudioToUDP = false;
            udpStereo = false;
            udpUseRTP = false;
        }
        else
        {
            sampleRate = m_audioOutputInfos[deviceName].sampleRate;
            udpAddress = m_audioOutputInfos[deviceName].udpAddress;
            udpPort = m_audioOutputInfos[deviceName].udpPort;
            copyAudioToUDP = m_audioOutputInfos[deviceName].copyToUDP;
            udpStereo = m_audioOutputInfos[deviceName].udpStereo;
            udpUseRTP = m_audioOutputInfos[deviceName].udpUseRTP;
        }

        m_audioOutputs[outputDeviceIndex]->start(outputDeviceIndex, sampleRate);
        m_audioOutputInfos[deviceName].sampleRate = m_audioOutputs[outputDeviceIndex]->getRate(); // update with actual rate
        m_audioOutputInfos[deviceName].udpAddress = udpAddress;
        m_audioOutputInfos[deviceName].udpPort = udpPort;
        m_audioOutputInfos[deviceName].copyToUDP = copyAudioToUDP;
        m_audioOutputInfos[deviceName].udpStereo = udpStereo;
        m_audioOutputInfos[deviceName].udpUseRTP = udpUseRTP;
    }
    else
    {
        qWarning("AudioDeviceManager::startAudioOutput: unknown device index %d", outputDeviceIndex);
    }
}

void AudioDeviceManager::stopAudioOutput(int outputDeviceIndex)
{
    m_audioOutputs[outputDeviceIndex]->stop();
}

void AudioDeviceManager::startAudioInput(int inputDeviceIndex)
{
    unsigned int sampleRate;
    float volume;
    QString deviceName;

    if (getInputDeviceName(inputDeviceIndex, deviceName))
    {
        if (m_audioInputInfos.find(deviceName) == m_audioInputInfos.end())
        {
            sampleRate = m_defaultAudioSampleRate;
            volume = m_defaultAudioInputVolume;
        }
        else
        {
            sampleRate = m_audioInputInfos[deviceName].sampleRate;
            volume = m_audioInputInfos[deviceName].volume;
        }

        m_audioInputs[inputDeviceIndex]->start(inputDeviceIndex, sampleRate);
        m_audioInputs[inputDeviceIndex]->setVolume(volume);
        m_audioInputInfos[deviceName].sampleRate = m_audioInputs[inputDeviceIndex]->getRate();
        m_audioInputInfos[deviceName].volume = volume;
    }
    else
    {
        qWarning("AudioDeviceManager::startAudioInput: unknown device index %d", inputDeviceIndex);
    }
}

void AudioDeviceManager::stopAudioInput(int inputDeviceIndex)
{
    m_audioInputs[inputDeviceIndex]->stop();
}

void AudioDeviceManager::debugAudioInputInfos() const
{
    QMap<QString, InputDeviceInfo>::const_iterator it = m_audioInputInfos.begin();

    for (; it != m_audioInputInfos.end(); ++it)
    {
        qDebug() << "AudioDeviceManager::debugAudioInputInfos:"
                << " name: " << it.key()
                << " sampleRate: " << it.value().sampleRate
                << " volume: " << it.value().volume;
    }
}

void AudioDeviceManager::debugAudioOutputInfos() const
{
    QMap<QString, OutputDeviceInfo>::const_iterator it = m_audioOutputInfos.begin();

    for (; it != m_audioOutputInfos.end(); ++it)
    {
        qDebug() << "AudioDeviceManager::debugAudioOutputInfos:"
                << " name: " << it.key()
                << " sampleRate: " << it.value().sampleRate
                << " udpAddress: " << it.value().udpAddress
                << " udpPort: " << it.value().udpPort
                << " copyToUDP: " << it.value().copyToUDP
                << " udpStereo: " << it.value().udpStereo
                << " udpUseRTP: " << it.value().udpUseRTP;
    }
}
