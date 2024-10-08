///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
// Copyright (C) 2022-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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
        bool getRestartRequired() const { return m_restartRequired; }

        static MsgConfigureRemoteTCPSink* create(const RemoteTCPSinkSettings& settings, const QList<QString>& settingsKeys, bool force, bool restartRequired = false)
        {
            return new MsgConfigureRemoteTCPSink(settings, settingsKeys, force, restartRequired);
        }

    private:
        RemoteTCPSinkSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;
        bool m_restartRequired;

        MsgConfigureRemoteTCPSink(const RemoteTCPSinkSettings& settings, const QList<QString>& settingsKeys, bool force, bool restartRequired) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force),
            m_restartRequired(restartRequired)
        { }
    };

    class MsgReportConnection : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getClients() const { return m_clients; }
        const QHostAddress& getAddress() const { return m_address; }
          int getPort() const { return m_port; }


        static MsgReportConnection* create(int clients, const QHostAddress& address, quint16 port)
        {
            return new MsgReportConnection(clients, address, port);
        }

    private:
        int m_clients;
        QHostAddress m_address;
        quint16 m_port;

        MsgReportConnection(int clients, const QHostAddress& address, quint16 port) :
            Message(),
            m_clients(clients),
            m_address(address),
            m_port(port)
        { }
    };

    class MsgReportDisconnect : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getClients() const { return m_clients; }
        const QHostAddress& getAddress() const { return m_address; }
        quint16 getPort() const { return m_port; }

        static MsgReportDisconnect* create(int clients, const QHostAddress& address, quint16 port)
        {
            return new MsgReportDisconnect(clients, address, port);
        }

    private:
        int m_clients;
        QHostAddress m_address;
        quint16 m_port;

        MsgReportDisconnect(int clients, const QHostAddress& address, quint16 port) :
            Message(),
            m_clients(clients),
            m_address(address),
            m_port(port)
        { }
    };

    // Message to report actual transmit bandwidth in bits per second and compression ratio
    class MsgReportBW : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        float getBW() const { return m_bw; }
        float getNetworkBW() const { return m_networkBW; }
        qint64 getBytesUncompressed() const { return m_bytesUncompressed; }
        qint64 getBytesCompressed() const { return m_bytesCompressed; }
        qint64 getBytesTransmitted() const { return m_bytesTransmitted; }

        static MsgReportBW* create(float bw, float networkBW, qint64 bytesUncompressed, qint64 bytesCompressed, qint64 bytesTransmitted)
        {
            return new MsgReportBW(bw, networkBW, bytesUncompressed, bytesCompressed, bytesTransmitted);
        }

    private:
        float m_bw;
        float m_networkBW;
        qint64 m_bytesUncompressed;
        qint64 m_bytesCompressed;
        qint64 m_bytesTransmitted;

        MsgReportBW(float bw, float networkBW, qint64 bytesUncompressed, qint64 bytesCompressed, qint64 bytesTransmitted) :
            Message(),
            m_bw(bw),
            m_networkBW(networkBW),
            m_bytesUncompressed(bytesUncompressed),
            m_bytesCompressed(bytesCompressed),
            m_bytesTransmitted(bytesTransmitted)
        { }
    };

    // Send a text message to a client (or received message from client)
    class MsgSendMessage : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QHostAddress getAddress() const { return m_address; }
        quint16 getPort() const { return m_port; }
        const QString& getCallsign() const { return m_callsign; }
        const QString& getText() const { return m_text; }
        bool getBroadcast() const { return m_broadcast; }

        static MsgSendMessage* create(QHostAddress address, quint16 port, const QString& callsign, const QString& text, bool broadcast)
        {
            return new MsgSendMessage(address, port, callsign, text, broadcast);
        }

    private:
        QHostAddress m_address;
        quint16 m_port;
        QString m_callsign;
        QString m_text;
        bool m_broadcast;

        MsgSendMessage(QHostAddress address, quint16 port, const QString& callsign, const QString& text, bool broadcast) :
            Message(),
            m_address(address),
            m_port(port),
            m_callsign(callsign),
            m_text(text),
            m_broadcast(broadcast)
        { }
    };

    class MsgError : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        const QString& getError() const { return m_error; }

        static MsgError *create(const QString& error)
        {
            return new MsgError(error);
        }

    private:
        QString m_error;

        explicit MsgError(const QString& error) :
            Message(),
            m_error(error)
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
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }

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

    bool getSquelchOpen() const { return m_basebandSink && m_basebandSink->getSquelchOpen(); }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples)
    {
        if (m_basebandSink) {
            m_basebandSink->getMagSqLevels(avg, peak, nbSamples);
        } else {
            avg = 0.0; peak = 0.0; nbSamples = 1;
        }
    }
    void setMessageQueueToGUI(MessageQueue* queue) final {
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

    int m_basebandSampleRate;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    int m_clients; // Number of clients currently connected
    QNetworkReply *m_removeRequest;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const RemoteTCPSinkSettings& settings, const QStringList& settingsKeys, bool force = false, bool restartRequired = false);
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
    void removePublicListing(const QString& address, quint16 port);
    void updatePublicListing();

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
};

#endif /* INCLUDE_REMOTETCPSINK_H_ */
