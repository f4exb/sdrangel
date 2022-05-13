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

#ifndef PLUGINS_CHANNELTX_MODAIS_AISMOD_H_
#define PLUGINS_CHANNELTX_MODAIS_AISMOD_H_

#include <vector>

#include <QMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesource.h"
#include "dsp/spectrumvis.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "aismodsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class QUdpSocket;
class DeviceAPI;
class AISModBaseband;
class ScopeVis;
class ObjectPipe;

class AISMod : public BasebandSampleSource, public ChannelAPI {
public:
    class MsgConfigureAISMod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const AISModSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureAISMod* create(const AISModSettings& settings, bool force)
        {
            return new MsgConfigureAISMod(settings, force);
        }

    private:
        AISModSettings m_settings;
        bool m_force;

        MsgConfigureAISMod(const AISModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgReportData : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgReportData* create(const QString& data) {
            return new MsgReportData(data);
        }
        const QString& getData() const { return m_data; }

    private:
        QString m_data;

        MsgReportData(const QString& data) :
            Message(),
            m_data(data)
        {}
    };

    class MsgTx : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgTx* create() {
            return new MsgTx();
        }

   private:
        MsgTx() :
            Message()
        { }
    };

    class MsgEncode : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgEncode* create() {
            return new MsgEncode();
        }

   private:
        MsgEncode() :
            Message()
        { }
    };

    class MsgTXPacketBytes : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgTXPacketBytes* create(const QByteArray& data) {
            return new MsgTXPacketBytes(data);
        }

        QByteArray m_data;

   private:

        MsgTXPacketBytes(const QByteArray& data) :
            Message(),
            m_data(data)
        { }
    };

    class MsgTXPacketData : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgTXPacketData* create(const QString& data) {
            return new MsgTXPacketData(data);
        }

        QString m_data;

   private:

        MsgTXPacketData(const QString& data) :
            Message(),
            m_data(data)
        { }
    };

    //=================================================================

    AISMod(DeviceAPI *deviceAPI);
    virtual ~AISMod();
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
        const AISModSettings& settings);

    static void webapiUpdateChannelSettings(
            AISModSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    SpectrumVis *getSpectrumVis() { return &m_spectrumVis; }
    ScopeVis *getScopeSink();
    double getMagSq() const;
    void setLevelMeter(QObject *levelMeter);
    uint32_t getNumberOfDeviceStreams() const;
    void encode();

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:

    DeviceAPI* m_deviceAPI;
    QThread *m_thread;
    AISModBaseband* m_basebandSource;
    AISModSettings m_settings;
    SpectrumVis m_spectrumVis;

    QMutex m_settingsMutex;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;
    QUdpSocket *m_udpSocket;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const AISModSettings& settings, bool force = false);
    void sendSampleRateToDemodAnalyzer();
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const AISModSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        QList<QString>& channelSettingsKeys,
        const AISModSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const AISModSettings& settings,
        bool force
    );
    void openUDP(const AISModSettings& settings);
    void closeUDP();
    static int degToMinFracs(float decimal);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void udpRx();
};


#endif /* PLUGINS_CHANNELTX_MODAIS_AISMOD_H_ */
