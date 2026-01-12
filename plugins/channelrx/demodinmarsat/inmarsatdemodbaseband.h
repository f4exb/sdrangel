///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020-2026 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#ifndef INCLUDE_INMARSATDEMODBASEBAND_H
#define INCLUDE_INMARSATDEMODBASEBAND_H

#include <QObject>
#include <QRecursiveMutex>

#include "dsp/samplesinkfifo.h"
#include "dsp/scopevis.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "inmarsatdemodsink.h"

class DownChannelizer;
class ChannelAPI;
class InmarsatDemod;
class ScopeVis;

class InmarsatDemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureInmarsatDemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const InmarsatDemodSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureInmarsatDemodBaseband* create(const InmarsatDemodSettings& settings, const QStringList& settingsKeys, bool force)
        {
            return new MsgConfigureInmarsatDemodBaseband(settings, settingsKeys, force);
        }

    private:
        InmarsatDemodSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureInmarsatDemodBaseband(const InmarsatDemodSettings& settings, const QStringList& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    InmarsatDemodBaseband(InmarsatDemod *packetDemod);
    ~InmarsatDemodBaseband();
    void reset();
    void startWork();
    void stopWork();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        m_sink.getMagSqLevels(avg, peak, nbSamples);
    }
    void getPLLStatus(bool &locked,  Real &coarseFreqCurrent, Real &coarseFreqCurrentPower, Real &coarseFreq, Real &coarseFreqPower, Real &fineFreq, Real &evm, bool &synced) const {
        m_sink.getPLLStatus(locked, coarseFreqCurrent, coarseFreqCurrentPower, coarseFreq, coarseFreqPower, fineFreq, evm, synced);
    }
    void setMessageQueueToChannel(MessageQueue *messageQueue) { m_sink.setMessageQueueToChannel(messageQueue); }
    void setBasebandSampleRate(int sampleRate);
    int getChannelSampleRate() const;
    ScopeVis *getScopeSink() { return &m_scopeSink; }
    void setChannel(ChannelAPI *channel);
    double getMagSq() const { return m_sink.getMagSq(); }
    bool isRunning() const { return m_running; }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    InmarsatDemodSink m_sink;
    MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    InmarsatDemodSettings m_settings;
    ScopeVis m_scopeSink;
    bool m_running;
    QRecursiveMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const InmarsatDemodSettings& settings, const QStringList& settingsKeys, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_INMARSATDEMODBASEBAND_H
