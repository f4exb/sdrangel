///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#ifndef INCLUDE_AUDIODEVICEINFO_H
#define INCLUDE_AUDIODEVICEINFO_H

#include <QStringList>
#include <QList>
#include <QAudioDeviceInfo>

#include "util/export.h"

class SDRANGEL_API AudioDeviceInfo {
public:
	AudioDeviceInfo();

	const QList<QAudioDeviceInfo>& getInputDevices() const { return m_inputDevicesInfo; }
    const QList<QAudioDeviceInfo>& getOutputDevices() const { return m_outputDevicesInfo; }
    int getInputDeviceIndex() const { return m_inputDeviceIndex; }
    int getOutputDeviceIndex() const { return m_outputDeviceIndex; }
    float getInputVolume() const { return m_inputVolume; }

private:
	QList<QAudioDeviceInfo> m_inputDevicesInfo = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    QList<QAudioDeviceInfo> m_outputDevicesInfo = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    int m_inputDeviceIndex;
    int m_outputDeviceIndex;
    float m_inputVolume;

	friend class AudioDialog;
};

#endif // INCLUDE_AUDIODEVICEINFO_H
