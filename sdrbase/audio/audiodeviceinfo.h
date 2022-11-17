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

#ifndef INCLUDE_AUDIODEVICEINFO_H
#define INCLUDE_AUDIODEVICEINFO_H

#include <QAudioFormat>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QMediaDevices>
#include <QAudioDevice>
#else
#include <QAudioDeviceInfo>
#endif

#include "export.h"

// Wrapper around QT6's QAudioDevice and and QT5's QAudioDeviceInfo
class SDRBASE_API AudioDeviceInfo {

public:

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    AudioDeviceInfo() :
        m_deviceInfo()
    {
    }

    AudioDeviceInfo(QAudioDevice deviceInfo) :
        m_deviceInfo(deviceInfo)
    {
    }

    QAudioDevice deviceInfo() { return m_deviceInfo; }
#else
    AudioDeviceInfo() :
        m_deviceInfo()
    {
    }

    AudioDeviceInfo(QAudioDeviceInfo deviceInfo) :
        m_deviceInfo(deviceInfo)
    {
    }

    QAudioDeviceInfo deviceInfo() { return m_deviceInfo; }
#endif

    QString deviceName() const;
    QString realm() const;
    bool isFormatSupported(const QAudioFormat &settings) const;
    QList<int> supportedSampleRates() const;

    static QList<AudioDeviceInfo> availableInputDevices();
    static QList<AudioDeviceInfo> availableOutputDevices();
    static AudioDeviceInfo defaultInputDevice();
    static AudioDeviceInfo defaultOutputDevice();

private:

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QAudioDevice m_deviceInfo;
#else
    QAudioDeviceInfo m_deviceInfo;
#endif

};

#endif // INCLUDE_AUDIODEVICEINFO_H
