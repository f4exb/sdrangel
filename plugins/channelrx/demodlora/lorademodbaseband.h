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

#ifndef INCLUDE_LORADEMODBASEBAND_H
#define INCLUDE_LORADEMODBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "lorademodsink.h"

class DownChannelizer;

class LoRaDemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureLoRaDemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const LoRaDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureLoRaDemodBaseband* create(const LoRaDemodSettings& settings, bool force)
        {
            return new MsgConfigureLoRaDemodBaseband(settings, force);
        }

    private:
        LoRaDemodSettings m_settings;
        bool m_force;

        MsgConfigureLoRaDemodBaseband(const LoRaDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    LoRaDemodBaseband();
    ~LoRaDemodBaseband();
    void reset();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    int getChannelSampleRate() const;
    void setBasebandSampleRate(int sampleRate);
    void setSpectrumSink(BasebandSampleSink* spectrumSink) { m_sink.setSpectrumSink(spectrumSink); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    LoRaDemodSink m_sink;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    LoRaDemodSettings m_settings;
    QMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const LoRaDemodSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_LORADEMODBASEBAND_H
