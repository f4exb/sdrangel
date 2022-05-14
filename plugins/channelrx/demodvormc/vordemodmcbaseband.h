///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_VORDEMODBASEBAND_H
#define INCLUDE_VORDEMODBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesinkfifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "vordemodmcsink.h"

class DownChannelizer;

class VORDemodMCBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureVORDemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const VORDemodMCSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureVORDemodBaseband* create(const VORDemodMCSettings& settings, bool force)
        {
            return new MsgConfigureVORDemodBaseband(settings, force);
        }

    private:
        VORDemodMCSettings m_settings;
        bool m_force;

        MsgConfigureVORDemodBaseband(const VORDemodMCSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    VORDemodMCBaseband();
    ~VORDemodMCBaseband();
    void reset();
    void startWork();
    void stopWork();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        avg = 0.0;
        peak = 0.0;
        nbSamples = 0;
        for (int i = 0; i < m_sinks.size(); i++)
        {
            double avg1, peak1;
            int nbSamples1;
            m_sinks[i]->getMagSqLevels(avg1, peak1, nbSamples1);
            if (avg1 > avg)
            {
                avg = avg1;
                nbSamples = nbSamples1;
            }
            avg += avg1;
            if (peak1 > peak)
                peak = peak1;
        }
    }
    void setMessageQueueToGUI(MessageQueue *messageQueue) {
        m_messageQueueToGUI = messageQueue;
        for (int i = 0; i < m_sinks.size(); i++)
            m_sinks[i]->setMessageQueueToGUI(messageQueue);
    }
    bool getSquelchOpen() const {
        for (int i = 0; i < m_sinks.size(); i++)
        {
            if (m_sinks[i]->getSquelchOpen())
                return true;
        }
        return false;
    }
    int getAudioSampleRate() const {
        if (m_sinks.size() > 0)
            return m_sinks[0]->getAudioSampleRate();
        else
            return 48000;
    }
    void setBasebandSampleRate(int sampleRate);
    double getMagSq() const {
        if (m_sinks.size() > 0)
            return m_sinks[0]->getMagSq();
        else
            return 0.0;
    }
    bool isRunning() const { return m_running; }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    QList<DownChannelizer *> m_channelizers;
    QList<VORDemodMCSink *> m_sinks;
    AudioFifo m_audioFifoBug; // FIXME: Removing this results in audio stopping when demod is closed
    MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    VORDemodMCSettings m_settings;
    bool m_running;
    QMutex m_mutex;
    MessageQueue *m_messageQueueToGUI;
    int m_basebandSampleRate;
    int m_centerFrequency;

    bool handleMessage(const Message& cmd);
    void calculateOffset(VORDemodMCSink *sink);
    void applySettings(const VORDemodMCSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_VORDEMODBASEBAND_H
