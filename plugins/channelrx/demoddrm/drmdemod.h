///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_DRMDEMOD_H
#define INCLUDE_DRMDEMOD_H

#include <QThread>
#include <QNetworkRequest>

#include "channel/channelapi.h"
#include "dsp/basebandsamplesink.h"
#include "util/message.h"

#include "drmdemodbaseband.h"
#include "drmdemodsettings.h"

class DeviceAPI;
class QNetworkAccessManager;
class QNetworkReply;

namespace SWGSDRangel {
    class SWGDRMJournaline;
}

class DRMDemod : public BasebandSampleSink, public ChannelAPI
{
public:
    class MsgConfigureDRMDemod : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        const DRMDemodSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureDRMDemod* create(const DRMDemodSettings& settings, const QStringList& settingsKeys, bool force)
        {
            return new MsgConfigureDRMDemod(settings, settingsKeys, force);
        }

    private:
        DRMDemodSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureDRMDemod(const DRMDemodSettings& settings, const QStringList& settingsKeys, bool force) :
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        {}
    };

    explicit DRMDemod(DeviceAPI *deviceAPI);
    ~DRMDemod() override;

    void destroy() override { delete this; }
    void setDeviceAPI(DeviceAPI *deviceAPI) override;
    DeviceAPI *getDeviceAPI() override { return m_deviceAPI; }
    using BasebandSampleSink::feed;
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly) override;
    void start() override;
    void stop() override;
    void pushMessage(Message *message) override { m_inputMessageQueue.push(message); }
    QString getSinkName() override { return objectName(); }

    void getIdentifier(QString& id) override { id = objectName(); }
    QString getIdentifier() const override { return objectName(); }
    void getTitle(QString& title) override { title = m_settings.m_title; }
    qint64 getCenterFrequency() const override { return m_settings.m_inputFrequencyOffset; }
    void setCenterFrequency(qint64 frequency) override;
    QByteArray serialize() const override { return m_settings.serialize(); }
    bool deserialize(const QByteArray& data) override;

    int webapiSettingsGet(SWGSDRangel::SWGChannelSettings& response, QString& errorMessage) override;
    int webapiSettingsPutPatch(bool force, const QStringList& channelSettingsKeys, SWGSDRangel::SWGChannelSettings& response, QString& errorMessage) override;
    int webapiReportGet(SWGSDRangel::SWGChannelReport& response, QString& errorMessage) override;
    int webapiActionsPost(const QStringList& channelActionsKeys, SWGSDRangel::SWGChannelActions& query, QString& errorMessage) override;
    int webapiWorkspaceGet(SWGSDRangel::SWGWorkspaceInfo& response, QString& errorMessage) override;

    int getNbSinkStreams() const override { return 1; }
    int getNbSourceStreams() const override { return 0; }
    int getStreamIndex() const override { return m_settings.m_streamIndex; }
    qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const override;

    DRMDemodStatus getStatus() const { return m_basebandSink->getStatus(); }
    void getMagSqLevels(double& average, double& peak, int& samples) { m_basebandSink->getMagSqLevels(average, peak, samples); }

    uint32_t getNumberOfDeviceStreams() const;
    void requestReacquire();
    int getAudioSampleRate() const { return m_basebandSink->getAudioSampleRate(); }

    static const char * const m_channelIdURI;
    static const char * const m_channelId;

    static void webapiUpdateChannelSettings(DRMDemodSettings& settings, const QStringList& channelSettingsKeys, SWGSDRangel::SWGChannelSettings& response);
    static void webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const DRMDemodSettings& settings);
    static qint32 clampFrequencyOffset(qint64 frequency);
    static SWGSDRangel::SWGDRMJournaline *webapiFormatJournaline(const DRMDemodStatus& status, const DRMDemodStatus::JournalineObjects& objects);
    static QString journalineTypeName(int type);
    // A Broadcast Website ContentName as a path that stays within a directory, else empty.
    static QString websiteFilePath(const std::string& contentName);
    // Writes a website's files under directory, skipping those that cannot be
    // written: 200, or 400 (no directory), 404 (no files) or 500 (none
    // written) with a message.
    static int saveWebsiteFiles(const modemdrm::MOTWebsite* website, const QString& directory, QString& message);

protected:
    bool handleMessage(const Message& message) override;
    void handleIndexInDeviceSetChanged(int index);

private:
    void sendSampleRateToDemodAnalyzer();
    DeviceAPI *m_deviceAPI;
    QThread m_thread;
    DRMDemodBaseband *m_basebandSink;
    DRMDemodSettings m_settings;
    // The stream the channel is registered on, which a deserialize can leave
    // different from m_settings.m_streamIndex until settings are applied.
    int m_registeredStreamIndex = 0;
    int m_basebandSampleRate;
    qint64 m_centerFrequency;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void applySettings(const DRMDemodSettings& settings, const QStringList& settingsKeys, bool force);
    void webapiReverseSendSettings(const QStringList& channelSettingsKeys, const DRMDemodSettings& settings, bool force);
    void webapiFormatReverseSettings(const QStringList& channelSettingsKeys, SWGSDRangel::SWGChannelSettings *response, const DRMDemodSettings& settings, bool force);
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_DRMDEMOD_H
