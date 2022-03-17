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

#ifndef INCLUDE_NOISEFIGUREBASEBAND_H
#define INCLUDE_NOISEFIGUREBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "dsp/scopevis.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "noisefiguresink.h"

class DownChannelizer;
class ChannelAPI;
class NoiseFigure;

class NoiseFigureBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureNoiseFigureBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const NoiseFigureSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureNoiseFigureBaseband* create(const NoiseFigureSettings& settings, bool force)
        {
            return new MsgConfigureNoiseFigureBaseband(settings, force);
        }

    private:
        NoiseFigureSettings m_settings;
        bool m_force;

        MsgConfigureNoiseFigureBaseband(const NoiseFigureSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    NoiseFigureBaseband(NoiseFigure *aisDemod);
    ~NoiseFigureBaseband();
    void reset();
    void startWork();
    void stopWork();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        m_sink.getMagSqLevels(avg, peak, nbSamples);
    }
    void setMessageQueueToChannel(MessageQueue *messageQueue) { m_sink.setMessageQueueToChannel(messageQueue); }
    void setBasebandSampleRate(int sampleRate);
    void setChannel(ChannelAPI *channel);
    double getMagSq() const { return m_sink.getMagSq(); }
    bool isRunning() const { return m_running; }
    void startMeasurement() { m_sink.startMeasurement(); }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    NoiseFigureSink m_sink;
    MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    NoiseFigureSettings m_settings;
    bool m_running;
    QMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void calculateOffset(NoiseFigureSink *sink);
    void applySettings(const NoiseFigureSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_NOISEFIGUREBASEBAND_H
