///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_REMOTETCPSINK_H_
#define INCLUDE_REMOTETCPSINK_H_

#include <QObject>
#include <QNetworkRequest>
#include <QThread>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "remotetcpsinkbaseband.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class ObjectPipe;

class RemoteTCPSink : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureRemoteTCPSink : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RemoteTCPSinkSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }
        bool getRemoteChange() const { return m_remoteChange; }

        static MsgConfigureRemoteTCPSink* create(const RemoteTCPSinkSettings& settings, const QList<QString>& settingsKeys, bool force, bool remoteChange = false)
        {
            return new MsgConfigureRemoteTCPSink(settings, settingsKeys, force, remoteChange);
        }

    private:
        RemoteTCPSinkSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;
        bool m_remoteChange; // This change of settings was requested by a remote client, so no need to restart server

        MsgConfigureRemoteTCPSink(const RemoteTCPSinkSettings& settings, const QList<QString>& settingsKeys, bool force, bool remoteChange) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force),
            m_remoteChange(remoteChange)
        { }
    };

    class MsgReportConnection : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getClients() const { return m_clients; }

        static MsgReportConnection* create(int clients)
        {
            return new MsgReportConnection(clients);
        }

    private:
        int m_clients;

        MsgReportConnection(int clients) :
            Message(),
            m_clients(clients)
        { }
    };

    // Message to report actual transmit bandwidth in bits per second
    class MsgReportBW : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        float getBW() const { return m_bw; }

        static MsgReportBW* create(float bw)
        {
            return new MsgReportBW(bw);
        }

    private:
        float m_bw;

        MsgReportBW(float bw) :
            Message(),
            m_bw(bw)
        { }
    };

    RemoteTCPSink(DeviceAPI *deviceAPI);
    virtual ~RemoteTCPSink();
    virtual void destroy() { delete this; }
    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }

    using BasebandSampleSink::feed;
    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
    virtual void start();
    virtual void stop();
    virtual void pushMessage(Message *msg) { m_inputMessageQueue.push(msg); }
    virtual QString getSinkName() { return objectName(); }

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) { title = "Remote Sink"; }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }
    virtual void setCenterFrequency(qint64 frequency);

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return m_settings.m_inputFrequencyOffset;
    }

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiWorkspaceGet(
            SWGSDRangel::SWGWorkspaceInfo& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    static void webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const RemoteTCPSinkSettings& settings);

    static void webapiUpdateChannelSettings(
            RemoteTCPSinkSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    uint32_t getNumberOfDeviceStreams() const;
    int getBasebandSampleRate() const { return m_basebandSampleRate; }

    void setMessageQueueToGUI(MessageQueue* queue) override {
        ChannelAPI::setMessageQueueToGUI(queue);
        m_basebandSink->setMessageQueueToGUI(queue);
    }

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    QThread m_thread;
    RemoteTCPSinkBaseband *m_basebandSink;
    RemoteTCPSinkSettings m_settings;

    uint64_t m_centerFrequency;
    int m_basebandSampleRate;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const RemoteTCPSinkSettings& settings, const QStringList& settingsKeys, bool force = false, bool remoteChange = false);
    void webapiReverseSendSettings(const QStringList& channelSettingsKeys, const RemoteTCPSinkSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        const QStringList& channelSettingsKeys,
        const RemoteTCPSinkSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const RemoteTCPSinkSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
};

#endif /* INCLUDE_REMOTETCPSINK_H_ */
