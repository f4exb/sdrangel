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

AudioDeviceManager::AudioDeviceManager(unsigned int defaultAudioSampleRate) :
    m_defaultAudioSampleRate(defaultAudioSampleRate),
    m_inputDeviceIndex(-1),  // default device
    m_audioInputSampleRate(48000),  // Use default input device at 48 kHz
    m_inputVolume(1.0f)
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

void AudioDeviceManager::resetToDefaults()
{
    m_inputDeviceIndex = -1;
    m_inputVolume = 1.0f;
}

QByteArray AudioDeviceManager::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputDeviceIndex);
    s.writeFloat(3, m_inputVolume);
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
        d.readS32(1, &m_inputDeviceIndex, -1);
        d.readFloat(3, &m_inputVolume, 1.0f);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

int AudioDeviceManager::getOutputDeviceIndex(AudioFifo* audioFifo) const
{
    if (m_audioSinkFifos.find(audioFifo) == m_audioSinkFifos.end()) {
        return -2; // error
    } else {
        return m_audioSinkFifos[audioFifo];
    }
}

void AudioDeviceManager::setInputDeviceIndex(int inputDeviceIndex)
{
    int nbDevices = m_inputDevicesInfo.size();
    m_inputDeviceIndex = inputDeviceIndex < -1 ? -1 : inputDeviceIndex >= nbDevices ? nbDevices-1 : inputDeviceIndex;
}

void AudioDeviceManager::setInputVolume(float inputVolume)
{
    m_inputVolume = inputVolume < 0.0 ? 0.0 : inputVolume > 1.0 ? 1.0 : inputVolume;
}

void AudioDeviceManager::addAudioSink(AudioFifo* audioFifo, int outputDeviceIndex)
{
    qDebug("AudioDeviceManager::addAudioSink: %d: %p", outputDeviceIndex, audioFifo);

    if (m_audioOutputs.find(outputDeviceIndex) == m_audioOutputs.end())
    {
        m_audioOutputs[outputDeviceIndex] = new AudioOutput();
        m_audioOutputSampleRates[outputDeviceIndex] = m_defaultAudioSampleRate;
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

void AudioDeviceManager::addAudioSource(AudioFifo* audioFifo)
{
    qDebug("AudioDeviceManager::addAudioSource");

    if (m_audioInput.getNbFifos() == 0) {
        startAudioInput();
    }

    m_audioInput.addFifo(audioFifo);
}

void AudioDeviceManager::removeAudioSource(AudioFifo* audioFifo)
{
    qDebug("AudioDeviceManager::removeAudioSource");

    m_audioInput.removeFifo(audioFifo);

    if (m_audioInput.getNbFifos() == 0) {
        stopAudioInput();
    }
}

void AudioDeviceManager::startAudioOutput(int outputDeviceIndex)
{
    m_audioOutputs[outputDeviceIndex]->start(outputDeviceIndex, m_audioOutputSampleRates[outputDeviceIndex]);
    m_audioOutputSampleRates[outputDeviceIndex] = m_audioOutputs[outputDeviceIndex]->getRate(); // update with actual rate
//    m_audioOutput.start(m_outputDeviceIndex, m_audioOutputSampleRate);
//    m_audioOutputSampleRate = m_audioOutput.getRate(); // update with actual rate
}

void AudioDeviceManager::stopAudioOutput(int outputDeviceIndex)
{
    m_audioOutputs[outputDeviceIndex]->stop();
}

void AudioDeviceManager::startAudioInput()
{
    m_audioInput.start(m_inputDeviceIndex, m_audioInputSampleRate);
    m_audioInputSampleRate = m_audioInput.getRate(); // update with actual rate
}

void AudioDeviceManager::stopAudioInput()
{
    m_audioInput.stop();
}
