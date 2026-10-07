///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_BFMMODBASEBAND_H
#define INCLUDE_BFMMODBASEBAND_H

#include <QObject>
#include <QRecursiveMutex>

#include "dsp/samplesourcefifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "bfmmodsource.h"

class UpChannelizer;
class ChannelAPI;
class AudioFileReader;

class BFMModBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureBFMModBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const BFMModSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureBFMModBaseband* create(const QStringList& settingsKeys, const BFMModSettings& settings, bool force)
        {
            return new MsgConfigureBFMModBaseband(settingsKeys, settings, force);
        }

    private:
        BFMModSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureBFMModBaseband(const QStringList& settingsKeys, const BFMModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgConfigureFileSampleRate : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSampleRate() const { return m_sampleRate; }

        static MsgConfigureFileSampleRate* create(int sampleRate)
        {
            return new MsgConfigureFileSampleRate(sampleRate);
        }

    private:
        int m_sampleRate;

        explicit MsgConfigureFileSampleRate(int sampleRate) :
            Message(),
            m_sampleRate(sampleRate)
        { }
    };

    BFMModBaseband();
    ~BFMModBaseband();
    void reset();
	void pull(const SampleVector::iterator& begin, unsigned int nbSamples);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    CWKeyer& getCWKeyer() { return m_source.getCWKeyer(); }
    double getMagSq() const { return m_source.getMagSq(); }
    int getAudioSampleRate() const { return m_source.getAudioSampleRate(); }
    int getFeedbackAudioSampleRate() const { return m_source.getFeedbackAudioSampleRate(); }
    int getChannelSampleRate() const;
    void setInputFileReader(AudioFileReader *audioFileReader) { m_source.setInputFileReader(audioFileReader); }
    AudioFifo *getAudioFifo() { return m_source.getAudioFifo(); }
    void setChannel(ChannelAPI *channel);

signals:
	/**
	 * Level changed
	 * \param rmsLevel RMS level in range 0.0 - 1.0
	 * \param peakLevel Peak level in range 0.0 - 1.0
	 * \param numSamples Number of audio samples analyzed
	 */
	void levelChanged(qreal rmsLevel, qreal peakLevel, int numSamples);

private:
    SampleSourceFifo m_sampleFifo;
    UpChannelizer *m_channelizer;
    BFMModSource m_source;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    BFMModSettings m_settings;
    int m_fileSampleRate;
    QRecursiveMutex m_mutex;

    void processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd);
    bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const BFMModSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};


#endif // INCLUDE_BFMMODBASEBAND_H
