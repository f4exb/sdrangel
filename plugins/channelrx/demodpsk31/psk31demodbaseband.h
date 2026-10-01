///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                    //
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

#ifndef INCLUDE_PSK31DEMODBASEBAND_H
#define INCLUDE_PSK31DEMODBASEBAND_H

#include <QObject>
#include <QRecursiveMutex>

#include "dsp/samplesinkfifo.h"
#include "dsp/scopevis.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "psk31demodsink.h"

class DownChannelizer;
class ScopeVis;

class PSK31DemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigurePSK31DemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const PSK31DemodSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigurePSK31DemodBaseband* create(const QStringList& settingsKeys, const PSK31DemodSettings& settings, bool force)
        {
            return new MsgConfigurePSK31DemodBaseband(settingsKeys, settings, force);
        }

    private:
        PSK31DemodSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigurePSK31DemodBaseband(const QStringList& settingsKeys, const PSK31DemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    PSK31DemodBaseband();
    ~PSK31DemodBaseband();
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
    int getChannelSampleRate() const;
    ScopeVis *getScopeSink() { return &m_scopeSink; }
    double getMagSq() const { return m_sink.getMagSq(); }
    float getFrequencyOffset() const { return m_sink.getFrequencyOffset(); }
    float getSNR() const { return m_sink.getSNR(); }
    bool isLocked() const { return m_sink.isLocked(); }
    bool isRunning() const { return m_running; }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    PSK31DemodSink m_sink;
    MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    PSK31DemodSettings m_settings;
    ScopeVis m_scopeSink;
    bool m_running;
    QRecursiveMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const PSK31DemodSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_PSK31DEMODBASEBAND_H
