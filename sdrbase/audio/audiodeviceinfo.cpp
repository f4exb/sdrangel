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

#include "audio/audiodeviceinfo.h"
#include "util/simpleserializer.h"

AudioDeviceInfo::AudioDeviceInfo() :
    m_inputDeviceIndex(-1),  // default device
    m_outputDeviceIndex(-1), // default device
    m_audioOutputSampleRate(48000), // Use default output device at 48 kHz
    m_audioInputSampleRate(48000),  // Use default input device at 48 kHz
    m_inputVolume(1.0f)
{
    m_inputDevicesInfo = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    m_outputDevicesInfo = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
}

void AudioDeviceInfo::resetToDefaults()
{
    m_inputDeviceIndex = -1;
    m_outputDeviceIndex = -1;
    m_inputVolume = 1.0f;
}

QByteArray AudioDeviceInfo::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputDeviceIndex);
    s.writeS32(2, m_outputDeviceIndex);
    s.writeFloat(3, m_inputVolume);
    return s.final();
}

bool AudioDeviceInfo::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid()) {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        d.readS32(1, &m_inputDeviceIndex, -1);
        d.readS32(2, &m_outputDeviceIndex, -1);
        d.readFloat(3, &m_inputVolume, 1.0f);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void AudioDeviceInfo::setInputDeviceIndex(int inputDeviceIndex)
{
    int nbDevices = m_inputDevicesInfo.size();
    m_inputDeviceIndex = inputDeviceIndex < -1 ? -1 : inputDeviceIndex >= nbDevices ? nbDevices-1 : inputDeviceIndex;
}

void AudioDeviceInfo::setOutputDeviceIndex(int outputDeviceIndex)
{
    int nbDevices = m_outputDevicesInfo.size();
    m_outputDeviceIndex = outputDeviceIndex < -1 ? -1 : outputDeviceIndex >= nbDevices ? nbDevices-1 : outputDeviceIndex;
}

void AudioDeviceInfo::setInputVolume(float inputVolume)
{
    m_inputVolume = inputVolume < 0.0 ? 0.0 : inputVolume > 1.0 ? 1.0 : inputVolume;
}

void AudioDeviceInfo::addAudioSink(AudioFifo* audioFifo)
{
    qDebug("AudioDeviceInfo::addAudioSink");
    m_audioOutput.addFifo(audioFifo);
}

void AudioDeviceInfo::removeAudioSink(AudioFifo* audioFifo)
{
    qDebug("AudioDeviceInfo::removeAudioSink");
    m_audioOutput.removeFifo(audioFifo);
}

void AudioDeviceInfo::addAudioSource(AudioFifo* audioFifo)
{
    qDebug("AudioDeviceInfo::addAudioSource");
    m_audioInput.addFifo(audioFifo);
}

void AudioDeviceInfo::removeAudioSource(AudioFifo* audioFifo)
{
    qDebug("AudioDeviceInfo::removeAudioSource");
    m_audioInput.removeFifo(audioFifo);
}

void AudioDeviceInfo::startAudioOutput()
{
    m_audioOutput.start(m_outputDeviceIndex, m_audioOutputSampleRate);
    m_audioOutputSampleRate = m_audioOutput.getRate(); // update with actual rate
}

void AudioDeviceInfo::stopAudioOutput()
{
    m_audioOutput.stop();
}

void AudioDeviceInfo::startAudioOutputImmediate()
{
    m_audioOutput.startImmediate(m_outputDeviceIndex, m_audioOutputSampleRate);
    m_audioOutputSampleRate = m_audioOutput.getRate(); // update with actual rate
}

void AudioDeviceInfo::stopAudioOutputImmediate()
{
    m_audioOutput.stopImmediate();
}

void AudioDeviceInfo::startAudioInput()
{
    m_audioInput.start(m_inputDeviceIndex, m_audioInputSampleRate);
    m_audioInputSampleRate = m_audioInput.getRate(); // update with actual rate
}

void AudioDeviceInfo::stopAudioInput()
{
    m_audioInput.stop();
}

void AudioDeviceInfo::startAudioInputImmediate()
{
    m_audioInput.startImmediate(m_inputDeviceIndex, m_audioInputSampleRate);
    m_audioInputSampleRate = m_audioInput.getRate(); // update with actual rate
}

void AudioDeviceInfo::stopAudioInputImmediate()
{
    m_audioInput.stopImmediate();
}

