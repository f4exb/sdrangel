///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2019 Edouard Griffiths, F4EXB                              //
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

#ifndef INCLUDE_IEEE_802_15_4_MOD_H
#define INCLUDE_IEEE_802_15_4_MOD_H

#include <vector>
#include <iostream>
#include <fstream>

#include <QMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesource.h"
#include "dsp/spectrumvis.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "ieee_802_15_4_modsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class QUdpSocket;
class DeviceAPI;
class IEEE_802_15_4_ModBaseband;
class ScopeVis;
class ObjectPipe;

class IEEE_802_15_4_Mod : public BasebandSampleSource, public ChannelAPI {
public:
    class MsgConfigureIEEE_802_15_4_Mod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const IEEE_802_15_4_ModSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureIEEE_802_15_4_Mod* create(const IEEE_802_15_4_ModSettings& settings, bool force)
        {
            return new MsgConfigureIEEE_802_15_4_Mod(settings, force);
        }

    private:
        IEEE_802_15_4_ModSettings m_settings;
        bool m_force;

        MsgConfigureIEEE_802_15_4_Mod(const IEEE_802_15_4_ModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgTxHexString : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgTxHexString* create(QString data)
        {
            return new MsgTxHexString(data);
        }

        QString m_data;

   private:

        MsgTxHexString(QString data) :
            Message(),
            m_data(data)
        { }
    };

    //=================================================================

    IEEE_802_15_4_Mod(DeviceAPI *deviceAPI);
    ~IEEE_802_15_4_Mod();
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

    virtual int webapiActionsPost(
            const QStringList& channelActionsKeys,
            SWGSDRangel::SWGChannelActions& query,
            QString& errorMessage);

    static void webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const IEEE_802_15_4_ModSettings& settings);

    static void webapiUpdateChannelSettings(
            IEEE_802_15_4_ModSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    SpectrumVis *getSpectrumVis() { return &m_spectrumVis; }
    ScopeVis *getScopeSink();
    double getMagSq() const;
    void setLevelMeter(QObject *levelMeter);
    uint32_t getNumberOfDeviceStreams() const;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:

    DeviceAPI* m_deviceAPI;
    QThread *m_thread;
    IEEE_802_15_4_ModBaseband* m_basebandSource;
    IEEE_802_15_4_ModSettings m_settings;
    SpectrumVis m_spectrumVis;

    SampleVector m_sampleBuffer;
    QMutex m_settingsMutex;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;
    // QUdpSocket *m_udpSocket;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const IEEE_802_15_4_ModSettings& settings, bool force = false);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const IEEE_802_15_4_ModSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        QList<QString>& channelSettingsKeys,
        const IEEE_802_15_4_ModSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const IEEE_802_15_4_ModSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};


#endif /* INCLUDE_IEEE_802_15_4_MOD_H */
