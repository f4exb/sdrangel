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
#include <QMap>
#include <QAudioDeviceInfo>

#include "audio/audioinput.h"
#include "audio/audiooutput.h"
#include "export.h"

class AudioFifo;

class SDRBASE_API AudioDeviceManager {
public:
	AudioDeviceManager(unsigned int defaultAudioSampleRate);
	~AudioDeviceManager();

	const QList<QAudioDeviceInfo>& getInputDevices() const { return m_inputDevicesInfo; }
    const QList<QAudioDeviceInfo>& getOutputDevices() const { return m_outputDevicesInfo; }

    int getOutputDeviceIndex(AudioFifo* audioFifo) const;
    int getInputDeviceIndex() const { return m_inputDeviceIndex; }
    float getInputVolume() const { return m_inputVolume; }

    void setInputDeviceIndex(int inputDeviceIndex);
    void setInputVolume(float inputVolume);

    void addAudioSink(AudioFifo* audioFifo, int outputDeviceIndex = -1); //!< Add the audio sink
    void removeAudioSink(AudioFifo* audioFifo); //!< Remove the audio sink

    void addAudioSource(AudioFifo* audioFifo);    //!< Add an audio source
    void removeAudioSource(AudioFifo* audioFifo); //!< Remove an audio source

    void setAudioInputVolume(float volume) { m_audioInput.setVolume(volume); }

private:
    unsigned int m_defaultAudioSampleRate;
	QList<QAudioDeviceInfo> m_inputDevicesInfo;
    QList<QAudioDeviceInfo> m_outputDevicesInfo;
    QMap<AudioFifo*, int> m_audioSinkFifos; //< Audio sink FIFO to audio output device index-1 map
    QMap<int, AudioOutput*> m_audioOutputs; //!< audio device index-1 to audio output map (index -1 is default device)
    QMap<int, unsigned int> m_audioOutputSampleRates; //!< audio device index-1 to audio sample rate
    int m_inputDeviceIndex;
    unsigned int m_audioInputSampleRate;
    AudioInput m_audioInput;
    float m_inputVolume;

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    void startAudioOutput(int outputDeviceIndex);
    void stopAudioOutput(int outputDeviceIndex);
    void startAudioInput();
    void stopAudioInput();

	friend class AudioDialog;
	friend class MainSettings;
};

#endif // INCLUDE_AUDIODEVICEMANGER_H
