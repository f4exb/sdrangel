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

#ifndef INCLUDE_LOCALSINK_H_
#define INCLUDE_LOCALSINK_H_

#include <QMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesink.h"
#include "dsp/spectrumvis.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "localsinksettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;

class DeviceAPI;
class DeviceSampleSource;
class LocalSinkBaseband;
class ObjectPipe;

class LocalSink : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureLocalSink : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const LocalSinkSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureLocalSink* create(const LocalSinkSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureLocalSink(settings, settingsKeys, force);
        }

    private:
        LocalSinkSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureLocalSink(const LocalSinkSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgReportDevices : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QList<int>& getDeviceSetIndexes() { return m_deviceSetIndexes; }

        static MsgReportDevices* create() {
            return new MsgReportDevices();
        }

    private:
        QList<int> m_deviceSetIndexes;

        MsgReportDevices() :
            Message()
        { }
    };

    LocalSink(DeviceAPI *deviceAPI);
    virtual ~LocalSink();
    virtual void destroy() { delete this; }
    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }
    SpectrumVis *getSpectrumVis() { return &m_spectrumVis; }

    using BasebandSampleSink::feed;
    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
    virtual void start();
    virtual void stop();
    virtual void pushMessage(Message *msg) { m_inputMessageQueue.push(msg); }
    virtual QString getSinkName() { return objectName(); }

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) { title = "Local Sink"; }
    virtual qint64 getCenterFrequency() const { return m_frequencyOffset; }
    virtual void setCenterFrequency(qint64) {}

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }

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
        const LocalSinkSettings& settings);

    static void webapiUpdateChannelSettings(
            LocalSinkSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    uint32_t getNumberOfDeviceStreams() const;
    const QList<int>& getDeviceSetList() { return m_localInputDeviceIndexes; }

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    QThread *m_thread;
    LocalSinkBaseband *m_basebandSink;
    bool m_running;
    LocalSinkSettings m_settings;
    QList<int> m_localInputDeviceIndexes;
    SpectrumVis m_spectrumVis;

    uint64_t m_centerFrequency;
    int64_t m_frequencyOffset;
    uint32_t m_basebandSampleRate;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const LocalSinkSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void propagateSampleRateAndFrequency(int index, uint32_t log2Decim);
    static void validateFilterChainHash(LocalSinkSettings& settings);
    void calculateFrequencyOffset(uint32_t log2Decim, uint32_t filterChainHash);
    void updateDeviceSetList();
    DeviceSampleSource *getLocalDevice(int index);
    void startProcessing();
    void stopProcessing();

    void webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const LocalSinkSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        const QList<QString>& channelSettingsKeys,
        const LocalSinkSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const LocalSinkSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
};

#endif /* INCLUDE_LOCALSINK_H_ */
