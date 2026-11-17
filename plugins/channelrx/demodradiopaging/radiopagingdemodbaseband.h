///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#ifndef INCLUDE_RADIOPAGINGDEMODBASEBAND_H
#define INCLUDE_RADIOPAGINGDEMODBASEBAND_H

#include <QObject>
#include <QRecursiveMutex>

#include "dsp/samplesinkfifo.h"
#include "dsp/scopevis.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "radiopagingdemodsink.h"

class DownChannelizer;
class ChannelAPI;

class RadioPagingDemodBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureRadioPagingDemodBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RadioPagingDemodSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureRadioPagingDemodBaseband* create(const QStringList& settingsKeys, const RadioPagingDemodSettings& settings, bool force)
        {
            return new MsgConfigureRadioPagingDemodBaseband(settingsKeys, settings, force);
        }

    private:
        RadioPagingDemodSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureRadioPagingDemodBaseband(const QStringList& settingsKeys, const RadioPagingDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgResetProtocol : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getProtocol() const { return m_protocol; }

        static MsgResetProtocol* create(int protocol) { return new MsgResetProtocol(protocol); }

    private:
        int m_protocol;     //!< 0 POCSAG, 1 FLEX, 2 GSC, -1 all

        MsgResetProtocol(int protocol) : Message(), m_protocol(protocol) { }
    };

    RadioPagingDemodBaseband();
    ~RadioPagingDemodBaseband();
    void reset();
    void startWork();
    void stopWork();
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);
    //!< The input stopped: the samples still queued are processed, then the messages being received are released
    //!< and every protocol's state is cleared
    void inputStopped();
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        m_sink.getMagSqLevels(avg, peak, nbSamples);
    }
    void setMessageQueueToChannel(MessageQueue *messageQueue) { m_sink.setMessageQueueToChannel(messageQueue); }
    void setBasebandSampleRate(int sampleRate);
    int getChannelSampleRate() const;
    void setChannel(ChannelAPI *channel);
    double getMagSq() const { return m_sink.getMagSq(); }
    RadioPagingDemodSink::Status getStatus() const { return m_sink.getStatus(); }
    void setAudioFifoLabel(const QString& label) { m_sink.setAudioFifoLabel(label); }
    ScopeVis *getScopeSink() { return &m_scopeSink; }
    void setScopeEnabled(bool enabled) { m_sink.setScopeEnabled(enabled); }
    bool isRunning() const { return m_running; }
    void setFifoLabel(const QString& label) { m_sampleFifo.setLabel(label); }

private:
    SampleSinkFifo m_sampleFifo;
    DownChannelizer *m_channelizer;
    RadioPagingDemodSink m_sink;
    ScopeVis m_scopeSink;           //!< The waveforms of the scope of the GUI
    MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    RadioPagingDemodSettings m_settings;
    bool m_running;
    QRecursiveMutex m_mutex;

    bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const RadioPagingDemodSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};

#endif // INCLUDE_RADIOPAGINGDEMODBASEBAND_H
