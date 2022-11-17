///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#include "audiodeviceinfo.h"

QString AudioDeviceInfo::deviceName() const
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return m_deviceInfo.description();
#else
    return m_deviceInfo.deviceName();
#endif
}

bool AudioDeviceInfo::isFormatSupported(const QAudioFormat &settings) const
{
    return m_deviceInfo.isFormatSupported(settings);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
QList<int> AudioDeviceInfo::supportedSampleRates() const
{
    // QAudioDevice is a bit more flexible than QAudioDeviceInfo, in that it supports
    // min and max rate, rather than a specific list
    // For now, we just list some common rates.
    QList<int> sampleRates = {8000, 11025, 22050, 44100, 48000, 96000, 192000};
    QList<int> supportedRates;
    for (auto sampleRate : sampleRates)
    {
        if ((sampleRate <= m_deviceInfo.maximumSampleRate()) && (sampleRate >= m_deviceInfo.minimumSampleRate())) {
            supportedRates.append(sampleRate);
        }
    }
    return supportedRates;
}
#else
QList<int> AudioDeviceInfo::supportedSampleRates() const
{
    return m_deviceInfo.supportedSampleRates();
}
#endif

QString AudioDeviceInfo::realm() const
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return ""; // Don't appear to have realms in Qt6
#else
    return m_deviceInfo.realm();
#endif
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
QList<AudioDeviceInfo> AudioDeviceInfo::availableInputDevices()
{
    QList<QAudioDevice> devInfos = QMediaDevices::audioInputs();
    QList<AudioDeviceInfo> list;

    for (auto devInfo : devInfos) {
        list.append(AudioDeviceInfo(devInfo));
    }

    return list;
}

QList<AudioDeviceInfo> AudioDeviceInfo::availableOutputDevices()
{
    QList<QAudioDevice> devInfos = QMediaDevices::audioOutputs();
    QList<AudioDeviceInfo> list;

    for (auto devInfo : devInfos) {
        list.append(AudioDeviceInfo(devInfo));
    }

    return list;
}
#else
QList<AudioDeviceInfo> AudioDeviceInfo::availableInputDevices()
{
    QList<QAudioDeviceInfo> devInfos = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    QList<AudioDeviceInfo> list;

    for (auto devInfo : devInfos) {
        list.append(AudioDeviceInfo(devInfo));
    }

    return list;
}

QList<AudioDeviceInfo> AudioDeviceInfo::availableOutputDevices()
{
    QList<QAudioDeviceInfo> devInfos = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
    QList<AudioDeviceInfo> list;

    for (auto devInfo : devInfos) {
        list.append(AudioDeviceInfo(devInfo));
    }

    return list;
}
#endif

AudioDeviceInfo AudioDeviceInfo::defaultOutputDevice()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return AudioDeviceInfo(QMediaDevices::defaultAudioOutput());
#else
    return AudioDeviceInfo(QAudioDeviceInfo::defaultOutputDevice());
#endif
}

AudioDeviceInfo AudioDeviceInfo::defaultInputDevice()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return AudioDeviceInfo(QMediaDevices::defaultAudioInput());
#else
    return AudioDeviceInfo(QAudioDeviceInfo::defaultInputDevice());
#endif
}
