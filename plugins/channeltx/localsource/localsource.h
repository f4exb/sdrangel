///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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

#ifndef INCLUDE_LOCALSOURCE_H_
#define INCLUDE_LOCALSOURCE_H_

#include <QObject>
#include <QNetworkRequest>

#include "dsp/basebandsamplesource.h"
#include "util/message.h"
#include "channel/channelapi.h"
#include "localsourcesettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;

class DeviceAPI;
class DeviceSampleSink;
class LocalSourceBaseband;
class ObjectPipe;

class LocalSource : public BasebandSampleSource, public ChannelAPI {
public:
    class MsgConfigureLocalSource : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const LocalSourceSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureLocalSource* create(const LocalSourceSettings& settings, bool force)
        {
            return new MsgConfigureLocalSource(settings, force);
        }

    private:
        LocalSourceSettings m_settings;
        bool m_force;

        MsgConfigureLocalSource(const LocalSourceSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    LocalSource(DeviceAPI *deviceAPI);
    virtual ~LocalSource();
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
    virtual void getTitle(QString& title) { title = "Local Sink"; }
    virtual qint64 getCenterFrequency() const { return m_frequencyOffset; }
    virtual void setCenterFrequency(qint64) {}

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 0; }
    virtual int getNbSourceStreams() const { return 1; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return m_frequencyOffset;
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
        const LocalSourceSettings& settings);

    static void webapiUpdateChannelSettings(
            LocalSourceSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    void getLocalDevices(std::vector<uint32_t>& indexes);
    uint32_t getNumberOfDeviceStreams() const;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    QThread *m_thread;
    LocalSourceBaseband *m_basebandSource;
    LocalSourceSettings m_settings;

    uint64_t m_centerFrequency;
    int64_t m_frequencyOffset;
    uint32_t m_basebandSampleRate;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const LocalSourceSettings& settings, bool force = false);
    void propagateSampleRateAndFrequency(uint32_t index, uint32_t log2Interp);
    static void validateFilterChainHash(LocalSourceSettings& settings);
    void calculateFrequencyOffset(uint32_t log2Interp, uint32_t filterChainHash);
    DeviceSampleSink *getLocalDevice(uint32_t index);

    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const LocalSourceSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        QList<QString>& channelSettingsKeys,
        const LocalSourceSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const LocalSourceSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif /* INCLUDE_LOCALSOURCE_H_ */
