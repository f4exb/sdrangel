///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_MODCHIRPCHAT_CHIRPCHATMOD_H_
#define PLUGINS_CHANNELTX_MODCHIRPCHAT_CHIRPCHATMOD_H_

#include <vector>
#include <iostream>
#include <fstream>

#include <QMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesource.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "chirpchatmodsettings.h"
#include "chirpchatmodencoder.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class QUdpSocket;
class DeviceAPI;
class CWKeyer;
class ChirpChatModBaseband;
class ObjectPipe;

class ChirpChatMod : public BasebandSampleSource, public ChannelAPI {
public:
    class MsgConfigureChirpChatMod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const ChirpChatModSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureChirpChatMod* create(const ChirpChatModSettings& settings, bool force)
        {
            return new MsgConfigureChirpChatMod(settings, force);
        }

    private:
        ChirpChatModSettings m_settings;
        bool m_force;

        MsgConfigureChirpChatMod(const ChirpChatModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgReportPayloadTime : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        float getPayloadTimeMs() const { return m_timeMs; }
        static MsgReportPayloadTime* create(float timeMs) {
            return new MsgReportPayloadTime(timeMs);
        }

    private:
        float m_timeMs; //!< time in milliseconds

        MsgReportPayloadTime(float timeMs) :
            Message(),
            m_timeMs(timeMs)
        {}
    };

    //=================================================================

    ChirpChatMod(DeviceAPI *deviceAPI);
    virtual ~ChirpChatMod();
    virtual void destroy() { delete this; }
    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }

    virtual void start();
    virtual void stop();
    virtual void pull(SampleVector::iterator& begin, unsigned int nbSamples);
    virtual void pushMessage(Message *msg) { m_inputMessageQueue.push(msg); }
    virtual QString getSourceName() { return objectName(); }

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
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

    virtual int webapiReportGet(
                SWGSDRangel::SWGChannelReport& response,
                QString& errorMessage);

    static void webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const ChirpChatModSettings& settings);

    static void webapiUpdateChannelSettings(
            ChirpChatModSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    double getMagSq() const;
    CWKeyer *getCWKeyer();
    void setLevelMeter(QObject *levelMeter);
    uint32_t getNumberOfDeviceStreams() const;
    bool getModulatorActive() const;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
    DeviceAPI* m_deviceAPI;
    QThread *m_thread;
    ChirpChatModBaseband* m_basebandSource;
    ChirpChatModEncoder m_encoder; // TODO: check if it needs to be on its own thread
    ChirpChatModSettings m_settings;
    float m_currentPayloadTime;

    SampleVector m_sampleBuffer;
    QMutex m_settingsMutex;

    int m_sampleRate;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;
    QUdpSocket *m_udpSocket;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const ChirpChatModSettings& settings, bool force = false);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const ChirpChatModSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        QList<QString>& channelSettingsKeys,
        const ChirpChatModSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const ChirpChatModSettings& settings,
        bool force
    );
    void openUDP(const ChirpChatModSettings& settings);
    void closeUDP();

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void udpRx();
};


#endif /* PLUGINS_CHANNELTX_MODCHIRPCHAT_CHIRPCHATMOD_H_ */
