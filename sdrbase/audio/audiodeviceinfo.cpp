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
