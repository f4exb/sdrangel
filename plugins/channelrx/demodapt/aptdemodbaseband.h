///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_APTDEMODBASEBAND_H
#define INCLUDE_APTDEMODBASEBAND_H

#include <QObject>
#include <QRecursiveMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "aptdemodsink.h"

class DownChannelizer;
class APTDemod;

class APTDemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureAPTDemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const APTDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureAPTDemodBaseband* create(const APTDemodSettings& settings, bool force)
        {
            return new MsgConfigureAPTDemodBaseband(settings, force);
        }

    private:
        APTDemodSettings m_settings;
        bool m_force;

        MsgConfigureAPTDemodBaseband(const APTDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    APTDemodBaseband();
    ~APTDemodBaseband();
    void reset();
    void startWork();
    void stopWork();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        m_sink.getMagSqLevels(avg, peak, nbSamples);
    }
    void setImagWorkerMessageQueue(MessageQueue *messageQueue) { m_sink.setImageWorkerMessageQueue(messageQueue); }
    void setBasebandSampleRate(int sampleRate);
    double getMagSq() const { return m_sink.getMagSq(); }
    bool isRunning() const { return m_running; }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    APTDemodSink m_sink;
    MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    APTDemodSettings m_settings;
    bool m_running;
    QRecursiveMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void calculateOffset(APTDemodSink *sink);
    void applySettings(const APTDemodSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_APTDEMODBASEBAND_H
