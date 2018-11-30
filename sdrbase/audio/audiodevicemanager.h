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

class QDataStream;
class AudioFifo;
class MessageQueue;

class SDRBASE_API AudioDeviceManager {
public:
    class InputDeviceInfo
    {
    public:
        InputDeviceInfo() :
            sampleRate(m_defaultAudioSampleRate),
            volume(m_defaultAudioInputVolume)
        {}
        void resetToDefaults() {
            sampleRate = m_defaultAudioSampleRate;
            volume = m_defaultAudioInputVolume;
        }
        unsigned int sampleRate;
        float volume;
        friend QDataStream& operator<<(QDataStream& ds, const InputDeviceInfo& info);
        friend QDataStream& operator>>(QDataStream& ds, InputDeviceInfo& info);
    };

    class OutputDeviceInfo
    {
    public:
        OutputDeviceInfo() :
            sampleRate(m_defaultAudioSampleRate),
            udpAddress(m_defaultUDPAddress),
            udpPort(m_defaultUDPPort),
            copyToUDP(false),
            udpUseRTP(false),
            udpChannelMode(AudioOutput::UDPChannelLeft)
        {}
        void resetToDefaults() {
            sampleRate = m_defaultAudioSampleRate;
            udpAddress = m_defaultUDPAddress;
            udpPort = m_defaultUDPPort;
            copyToUDP = false;
            udpUseRTP = false;
            udpChannelMode = AudioOutput::UDPChannelLeft;
        }
        unsigned int sampleRate;
        QString udpAddress;
        quint16 udpPort;
        bool copyToUDP;
        bool udpUseRTP;
        AudioOutput::UDPChannelMode udpChannelMode;
        friend QDataStream& operator<<(QDataStream& ds, const OutputDeviceInfo& info);
        friend QDataStream& operator>>(QDataStream& ds, OutputDeviceInfo& info);
    };

	AudioDeviceManager();
	~AudioDeviceManager();

	const QList<QAudioDeviceInfo>& getInputDevices() const { return m_inputDevicesInfo; }
    const QList<QAudioDeviceInfo>& getOutputDevices() const { return m_outputDevicesInfo; }

    bool getOutputDeviceName(int outputDeviceIndex, QString &deviceName) const;
    bool getInputDeviceName(int inputDeviceIndex, QString &deviceName) const;
    int getOutputDeviceIndex(const QString &deviceName) const;
    int getInputDeviceIndex(const QString &deviceName) const;

    void addAudioSink(AudioFifo* audioFifo, MessageQueue *sampleSinkMessageQueue, int outputDeviceIndex = -1); //!< Add the audio sink
    void removeAudioSink(AudioFifo* audioFifo); //!< Remove the audio sink

    void addAudioSource(AudioFifo* audioFifo, MessageQueue *sampleSourceMessageQueue, int inputDeviceIndex = -1);    //!< Add an audio source
    void removeAudioSource(AudioFifo* audioFifo); //!< Remove an audio source

    bool getInputDeviceInfo(const QString& deviceName, InputDeviceInfo& deviceInfo) const;
    bool getOutputDeviceInfo(const QString& deviceName, OutputDeviceInfo& deviceInfo) const;
    int getInputSampleRate(int inputDeviceIndex = -1);
    int getOutputSampleRate(int outputDeviceIndex = -1);
    void setInputDeviceInfo(int inputDeviceIndex, const InputDeviceInfo& deviceInfo);
    void setOutputDeviceInfo(int outputDeviceIndex, const OutputDeviceInfo& deviceInfo);
    void unsetInputDeviceInfo(int inputDeviceIndex);
    void unsetOutputDeviceInfo(int outputDeviceIndex);
    void inputInfosCleanup();  //!< Remove input info from map for input devices not present
    void outputInfosCleanup(); //!< Remove output info from map for output devices not present

    static const unsigned int m_defaultAudioSampleRate = 48000;
    static const float m_defaultAudioInputVolume;
    static const QString m_defaultUDPAddress;
    static const quint16 m_defaultUDPPort = 9998;
    static const QString m_defaultDeviceName;

private:
    QList<QAudioDeviceInfo> m_inputDevicesInfo;
    QList<QAudioDeviceInfo> m_outputDevicesInfo;

    QMap<AudioFifo*, int> m_audioSinkFifos; //< audio sink FIFO to audio output device index-1 map
    QMap<AudioFifo*, MessageQueue*> m_audioFifoToSinkMessageQueues; //!< audio sink FIFO to attached sink message queue
    QMap<int, QList<MessageQueue*> > m_outputDeviceSinkMessageQueues; //!< sink message queues attached to device
    QMap<int, AudioOutput*> m_audioOutputs; //!< audio device index to audio output map (index -1 is default device)
    QMap<QString, OutputDeviceInfo> m_audioOutputInfos; //!< audio device name to audio output info

    QMap<AudioFifo*, int> m_audioSourceFifos; //< audio source FIFO to audio input device index-1 map
    QMap<AudioFifo*, MessageQueue*> m_audioFifoToSourceMessageQueues; //!< audio source FIFO to attached source message queue
    QMap<int, QList<MessageQueue*> > m_inputDeviceSourceMessageQueues; //!< sink message queues attached to device
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

	friend class MainSettings;
};

QDataStream& operator<<(QDataStream& ds, const AudioDeviceManager::InputDeviceInfo& info);
QDataStream& operator>>(QDataStream& ds, AudioDeviceManager::InputDeviceInfo& info);

QDataStream& operator<<(QDataStream& ds, const AudioDeviceManager::OutputDeviceInfo& info);
QDataStream& operator>>(QDataStream& ds, AudioDeviceManager::OutputDeviceInfo& info);

#endif // INCLUDE_AUDIODEVICEMANGER_H
