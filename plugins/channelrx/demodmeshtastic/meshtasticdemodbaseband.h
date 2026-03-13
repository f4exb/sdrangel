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

#ifndef INCLUDE_MESHTASTICDEMODBASEBAND_H
#define INCLUDE_MESHTASTICDEMODBASEBAND_H

#include <QObject>
#include <QRecursiveMutex>

#include "dsp/samplesinkfifo.h"
#include "dsp/downchannelizer.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "meshtasticdemodsink.h"

class MeshtasticDemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureMeshtasticDemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const MeshtasticDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureMeshtasticDemodBaseband* create(const MeshtasticDemodSettings& settings, bool force)
        {
            return new MsgConfigureMeshtasticDemodBaseband(settings, force);
        }

    private:
        MeshtasticDemodSettings m_settings;
        bool m_force;

        MsgConfigureMeshtasticDemodBaseband(const MeshtasticDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    MeshtasticDemodBaseband();
    ~MeshtasticDemodBaseband();
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
    DownChannelizer m_channelizer;
    MeshtasticDemodSink m_sink;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    MeshtasticDemodSettings m_settings;
    QRecursiveMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const MeshtasticDemodSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_MESHTASTICDEMODBASEBAND_H
