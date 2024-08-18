///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
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

#ifndef INCLUDE_SSBMODBASEBAND_H
#define INCLUDE_SSBMODBASEBAND_H

#include <QObject>
#include <QRecursiveMutex>

#include "dsp/samplesourcefifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "ssbmodsource.h"

class UpChannelizer;
class BasebandSampleSink;
class SpectrumVis;
class ChannelAPI;

class SSBModBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureSSBModBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SSBModSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureSSBModBaseband* create(const SSBModSettings& settings, bool force)
        {
            return new MsgConfigureSSBModBaseband(settings, force);
        }

    private:
        SSBModSettings m_settings;
        bool m_force;

        MsgConfigureSSBModBaseband(const SSBModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    SSBModBaseband();
    ~SSBModBaseband();
    void reset();
	void pull(const SampleVector::iterator& begin, unsigned int nbSamples);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    void setCWKeyer(CWKeyer *cwKeyer) { m_source.setCWKeyer(cwKeyer); }
    double getMagSq() const { return m_source.getMagSq(); }
    int getAudioSampleRate() const { return m_source.getAudioSampleRate(); }
    int getFeedbackAudioSampleRate() const { return m_source.getFeedbackAudioSampleRate(); }
    int getChannelSampleRate() const;
    void setInputFileStream(std::ifstream *ifstream) { m_source.setInputFileStream(ifstream); }
    AudioFifo *getAudioFifo() { return m_source.getAudioFifo(); }
    AudioFifo *getFeedbackAudioFifo() { return m_source.getFeedbackAudioFifo(); }
    void setSpectrumSink(SpectrumVis *sampleSink) { m_spectrumVis = sampleSink; m_source.setSpectrumSink(sampleSink); }
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
    SSBModSource m_source;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    SSBModSettings m_settings;
    SpectrumVis *m_spectrumVis;
    QRecursiveMutex m_mutex;

    void processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd);
    bool handleMessage(const Message& cmd);
    void applySettings(const SSBModSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_SSBMODBASEBAND_H
