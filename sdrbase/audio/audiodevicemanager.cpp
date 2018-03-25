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

#include <audio/audiodevicemanager.h>
#include "util/simpleserializer.h"

const float AudioDeviceManager::m_defaultAudioInputVolume = 0.15f;

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
    SimpleSerializer s(1);
    return s.final();
}

bool AudioDeviceManager::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid()) {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void AudioDeviceManager::addAudioSink(AudioFifo* audioFifo, int outputDeviceIndex)
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
}

void AudioDeviceManager::addAudioSource(AudioFifo* audioFifo, int inputDeviceIndex)
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
    m_audioOutputs[audioInputDeviceIndex]->removeFifo(audioFifo);

    if (m_audioOutputs[audioInputDeviceIndex]->getNbFifos() == 0) {
        stopAudioInput(audioInputDeviceIndex);
    }

    m_audioSourceFifos.remove(audioFifo); // unregister audio FIFO
}

void AudioDeviceManager::startAudioOutput(int outputDeviceIndex)
{
    unsigned int sampleRate;
    QString deviceName;

    if (getOutputDeviceName(outputDeviceIndex, deviceName))
    {
        if (m_audioOutputSampleRates.find(deviceName) == m_audioOutputSampleRates.end()) {
            sampleRate = m_defaultAudioSampleRate;
        } else {
            sampleRate = m_audioOutputSampleRates[deviceName];
        }

        m_audioOutputs[outputDeviceIndex]->start(outputDeviceIndex, sampleRate);
        m_audioOutputSampleRates[deviceName] = m_audioOutputs[outputDeviceIndex]->getRate(); // update with actual rate
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
        if (m_audioInputSampleRates.find(deviceName) == m_audioInputSampleRates.end()) {
            sampleRate = m_defaultAudioSampleRate;
        } else {
            sampleRate = m_audioInputSampleRates[deviceName];
        }

        if (m_audioInputVolumes.find(deviceName) == m_audioInputVolumes.end()) {
            volume = m_defaultAudioInputVolume;
        } else {
            volume = m_audioInputVolumes[deviceName];
        }

        m_audioInputs[inputDeviceIndex]->start(inputDeviceIndex, sampleRate);
        m_audioInputSampleRates[deviceName] = m_audioInputs[inputDeviceIndex]->getRate(); // update with actual rate
        m_audioInputs[inputDeviceIndex]->setVolume(volume);
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
