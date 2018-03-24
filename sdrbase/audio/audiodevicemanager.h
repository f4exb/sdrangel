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

#ifndef INCLUDE_AUDIODEVICEMANGER_H
#define INCLUDE_AUDIODEVICEMANGER_H

#include <QStringList>
#include <QList>
#include <QAudioDeviceInfo>

#include "audio/audioinput.h"
#include "audio/audiooutput.h"
#include "export.h"

class AudioFifo;

class SDRBASE_API AudioDeviceManager {
public:
	AudioDeviceManager();

	const QList<QAudioDeviceInfo>& getInputDevices() const { return m_inputDevicesInfo; }
    const QList<QAudioDeviceInfo>& getOutputDevices() const { return m_outputDevicesInfo; }

    int getInputDeviceIndex() const { return m_inputDeviceIndex; }
    int getOutputDeviceIndex() const { return m_outputDeviceIndex; }
    float getInputVolume() const { return m_inputVolume; }

    void setInputDeviceIndex(int inputDeviceIndex);
    void setOutputDeviceIndex(int inputDeviceIndex);
    void setInputVolume(float inputVolume);

    unsigned int getAudioSampleRate() const { return m_audioOutputSampleRate; }

    void addAudioSink(AudioFifo* audioFifo);    //!< Add the audio sink
    void removeAudioSink(AudioFifo* audioFifo); //!< Remove the audio sink

    void addAudioSource(AudioFifo* audioFifo);    //!< Add an audio source
    void removeAudioSource(AudioFifo* audioFifo); //!< Remove an audio source

    void startAudioOutput();
    void stopAudioOutput();

    void startAudioInput();
    void stopAudioInput();
    void setAudioInputVolume(float volume) { m_audioInput.setVolume(volume); }

private:
	QList<QAudioDeviceInfo> m_inputDevicesInfo;
    QList<QAudioDeviceInfo> m_outputDevicesInfo;
    int m_inputDeviceIndex;
    int m_outputDeviceIndex;
    unsigned int m_audioOutputSampleRate;
    unsigned int m_audioInputSampleRate;
    AudioOutput m_audioOutput;
    AudioInput m_audioInput;
    float m_inputVolume;

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

	friend class AudioDialog;
	friend class MainSettings;
};

#endif // INCLUDE_AUDIODEVICEMANGER_H
