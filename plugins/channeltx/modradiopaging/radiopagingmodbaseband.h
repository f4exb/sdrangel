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

#ifndef INCLUDE_RADIOPAGINGMODBASEBAND_H
#define INCLUDE_RADIOPAGINGMODBASEBAND_H

#include <QObject>
#include <QRecursiveMutex>

#include "dsp/samplesourcefifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "radiopagingmodsource.h"

class UpChannelizer;
class ChannelAPI;

class RadioPagingModBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureRadioPagingModBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RadioPagingModSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureRadioPagingModBaseband* create(const QStringList& settingsKeys, const RadioPagingModSettings& settings, bool force) {
            return new MsgConfigureRadioPagingModBaseband(settingsKeys, settings, force);
        }

    private:
        RadioPagingModSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureRadioPagingModBaseband(const QStringList& settingsKeys, const RadioPagingModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    //! A message to queue
    class MsgRequest : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RadioPagingModSource::Request& getRequest() const { return m_request; }

        static MsgRequest* create(const RadioPagingModSource::Request& request) {
            return new MsgRequest(request);
        }

    private:
        RadioPagingModSource::Request m_request;

        MsgRequest(const RadioPagingModSource::Request& request) :
            Message(),
            m_request(request)
        { }
    };

    //! Drop the messages waiting; with abort (one-shot), end the transmission being sent as well
    class MsgClear : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getAbort() const { return m_abort; }

        static MsgClear* create(bool abort) {
            return new MsgClear(abort);
        }

    private:
        bool m_abort;

        MsgClear(bool abort) :
            Message(),
            m_abort(abort)
        { }
    };

    RadioPagingModBaseband();
    ~RadioPagingModBaseband();
    void reset();
    void pull(const SampleVector::iterator& begin, unsigned int nbSamples);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    double getMagSq() const { return m_source.getMagSq(); }
    int getChannelSampleRate() const;
    int getSourceChannelSampleRate() const { return m_source.getChannelSampleRate(); }
    void setChannel(ChannelAPI *channel);
    RadioPagingModSource::Status getStatus() const { return m_source.getStatus(); }
    //! The source tells when a message was sent or will not be (see RadioPagingModSource::setNotify)
    void setNotify(const std::function<void(quint64, bool)>& notify) { m_source.setNotify(notify); }
    void setNotifyTransmitted(const std::function<void(quint64)>& notify) { m_source.setNotifyTransmitted(notify); }
    void setNotifyFailed(const std::function<void(quint64)>& notify) { m_source.setNotifyFailed(notify); }
    void setNotifyRepeated(const std::function<void(quint64)>& notify) { m_source.setNotifyRepeated(notify); }
    void setNotifyStatus(const std::function<void(quint64, bool, const RadioPagingFlexEncoder::SentStatus&)>& notify) {
        m_source.setNotifyStatus(notify);
    }
    //! Voice of the live voice page and its end, passed to the source (thread safe)
    void pushLive(const std::vector<float>& samples) { m_source.pushLive(samples); }
    //! FLEX System Event Notification, passed to the source (thread safe), see RadioPagingModSource::notifySystemEvent
    bool notifySystemEvent(unsigned events, int frames, std::string& error) { return m_source.notifySystemEvent(events, frames, error); }
    void endLive() { m_source.endLive(); }

signals:
    void levelChanged(qreal rmsLevel, qreal peakLevel, int numSamples);

private:
    SampleSourceFifo m_sampleFifo;
    UpChannelizer *m_channelizer;
    RadioPagingModSource m_source;
    MessageQueue m_inputMessageQueue;
    RadioPagingModSettings m_settings;
    QRecursiveMutex m_mutex;

    void processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd);
    bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const RadioPagingModSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData();
};

#endif // INCLUDE_RADIOPAGINGMODBASEBAND_H
