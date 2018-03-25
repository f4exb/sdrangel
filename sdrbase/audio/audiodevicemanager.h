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
#include <QDataStream>

#include "audio/audioinput.h"
#include "audio/audiooutput.h"
#include "export.h"

class AudioFifo;

class SDRBASE_API AudioDeviceManager {
public:
    class InputDeviceInfo
    {
    public:
        unsigned int sampleRate;
        float volume;
        friend QDataStream& operator<<(QDataStream& ds, const InputDeviceInfo& info);
        friend QDataStream& operator>>(QDataStream& ds, InputDeviceInfo& info);
    };

	AudioDeviceManager();
	~AudioDeviceManager();


	const QList<QAudioDeviceInfo>& getInputDevices() const { return m_inputDevicesInfo; }
    const QList<QAudioDeviceInfo>& getOutputDevices() const { return m_outputDevicesInfo; }

    bool getOutputDeviceName(int outputDeviceIndex, QString &deviceName) const;
    bool getInputDeviceName(int outputDeviceIndex, QString &deviceName) const;

    void addAudioSink(AudioFifo* audioFifo, int outputDeviceIndex = -1); //!< Add the audio sink
    void removeAudioSink(AudioFifo* audioFifo); //!< Remove the audio sink

    void addAudioSource(AudioFifo* audioFifo, int inputDeviceIndex = -1);    //!< Add an audio source
    void removeAudioSource(AudioFifo* audioFifo); //!< Remove an audio source

    static const unsigned int m_defaultAudioSampleRate = 48000;
    static const float m_defaultAudioInputVolume;
private:

    QList<QAudioDeviceInfo> m_inputDevicesInfo;
    QList<QAudioDeviceInfo> m_outputDevicesInfo;

    QMap<AudioFifo*, int> m_audioSinkFifos; //< Audio sink FIFO to audio output device index-1 map
    QMap<int, AudioOutput*> m_audioOutputs; //!< audio device index to audio output map (index -1 is default device)
    QMap<QString, unsigned int> m_audioOutputSampleRates; //!< audio device name to audio sample rate

    QMap<AudioFifo*, int> m_audioSourceFifos; //< Audio source FIFO to audio input device index-1 map
    QMap<int, AudioInput*> m_audioInputs; //!< audio device index to audio input map (index -1 is default device)
    QMap<QString, InputDeviceInfo> m_audioInputInfos; //!< audio device name to audio input device info

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    void startAudioOutput(int outputDeviceIndex);
    void stopAudioOutput(int outputDeviceIndex);
    void startAudioInput(int inputDeviceIndex);
    void stopAudioInput(int inputDeviceIndex);

    void serializeInputMap(QByteArray& data) const;
    void deserializeInputMap(QByteArray& data);
    void debugAudioInputInfos() const;

    void serializeOutputMap(QByteArray& data) const;
    void deserializeOutputMap(QByteArray& data);
    void debugAudioOutputInfos() const;

	friend class AudioDialog;
	friend class MainSettings;
};

QDataStream& operator<<(QDataStream& ds, const AudioDeviceManager::InputDeviceInfo& info);

#endif // INCLUDE_AUDIODEVICEMANGER_H
