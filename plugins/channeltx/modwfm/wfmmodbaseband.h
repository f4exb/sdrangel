///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_WFMMODBASEBAND_H
#define INCLUDE_WFMMODBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesourcefifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "wfmmodsource.h"

class UpChannelizer;
class ChannelAPI;

class WFMModBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureWFMModBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const WFMModSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureWFMModBaseband* create(const WFMModSettings& settings, bool force)
        {
            return new MsgConfigureWFMModBaseband(settings, force);
        }

    private:
        WFMModSettings m_settings;
        bool m_force;

        MsgConfigureWFMModBaseband(const WFMModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    WFMModBaseband();
    ~WFMModBaseband();
    void reset();
	void pull(const SampleVector::iterator& begin, unsigned int nbSamples);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    CWKeyer& getCWKeyer() { return m_source.getCWKeyer(); }
    double getMagSq() const { return m_source.getMagSq(); }
    int getAudioSampleRate() const { return m_source.getAudioSampleRate(); }
    int getFeedbackAudioSampleRate() const { return m_source.getFeedbackAudioSampleRate(); }
    int getChannelSampleRate() const;
    void setInputFileStream(std::ifstream *ifstream) { m_source.setInputFileStream(ifstream); }
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
    WFMModSource m_source;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    WFMModSettings m_settings;
    QMutex m_mutex;

    void processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd);
    bool handleMessage(const Message& cmd);
    void applySettings(const WFMModSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};


#endif // INCLUDE_WFMMODBASEBAND_H
