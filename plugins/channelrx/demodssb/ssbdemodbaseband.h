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

#ifndef INCLUDE_SSBDEMODBASEBAND_H
#define INCLUDE_SSBDEMODBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "dsp/downchannelizer.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "ssbdemodsink.h"

class ChannelAPI;
class SpectrumVis;

class SSBDemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureSSBDemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SSBDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureSSBDemodBaseband* create(const SSBDemodSettings& settings, bool force)
        {
            return new MsgConfigureSSBDemodBaseband(settings, force);
        }

    private:
        SSBDemodSettings m_settings;
        bool m_force;

        MsgConfigureSSBDemodBaseband(const SSBDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    SSBDemodBaseband();
    ~SSBDemodBaseband();
    void reset();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    int getChannelSampleRate() const;
	void setSpectrumSink(SpectrumVis* spectrumSink) { m_spectrumVis = spectrumSink; m_sink.setSpectrumSink(spectrumSink); }
    double getMagSq() const { return m_sink.getMagSq(); }
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) { m_sink.getMagSqLevels(avg, peak, nbSamples); }
    unsigned int getAudioSampleRate() const { return m_audioSampleRate; }
    bool getAudioActive() const { return m_sink.getAudioActive(); }
    void setBasebandSampleRate(int sampleRate);
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_messageQueueToGUI = messageQueue; }
    void setChannel(ChannelAPI *channel);
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }
    void setAudioFifoLabel(const QString& label) { m_sink.setAudioFifoLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer m_channelizer;
    SSBDemodSink m_sink;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    SSBDemodSettings m_settings;
    unsigned int m_audioSampleRate;
    int m_channelSampleRate;
    MessageQueue *m_messageQueueToGUI;
    SpectrumVis *m_spectrumVis;
    QMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const SSBDemodSettings& settings, bool force = false);
    MessageQueue *getMessageQueueToGUI() { return m_messageQueueToGUI; }

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_SSBDEMODBASEBAND_H
