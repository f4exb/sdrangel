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

#ifndef INCLUDE_CHIRPCHATDEMODBASEBAND_H
#define INCLUDE_CHIRPCHATDEMODBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "chirpchatdemodsink.h"

class DownChannelizer;

class ChirpChatDemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureChirpChatDemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const ChirpChatDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureChirpChatDemodBaseband* create(const ChirpChatDemodSettings& settings, bool force)
        {
            return new MsgConfigureChirpChatDemodBaseband(settings, force);
        }

    private:
        ChirpChatDemodSettings m_settings;
        bool m_force;

        MsgConfigureChirpChatDemodBaseband(const ChirpChatDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    ChirpChatDemodBaseband();
    ~ChirpChatDemodBaseband();
    void reset();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    int getChannelSampleRate() const;
    bool getDemodActive() const { return m_sink.getDemodActive(); }
    double getCurrentNoiseLevel() const { return m_sink.getCurrentNoiseLevel(); }
    double getTotalPower() const { return m_sink.getTotalPower(); }
    void setBasebandSampleRate(int sampleRate);
    void setDecoderMessageQueue(MessageQueue *messageQueue) { m_sink.setDecoderMessageQueue(messageQueue); }
    void setSpectrumSink(BasebandSampleSink* spectrumSink) { m_sink.setSpectrumSink(spectrumSink); }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    ChirpChatDemodSink m_sink;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    ChirpChatDemodSettings m_settings;
    QMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const ChirpChatDemodSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_CHIRPCHATDEMODBASEBAND_H
