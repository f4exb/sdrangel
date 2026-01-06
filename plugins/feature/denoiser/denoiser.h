///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#ifndef INCLUDE_FEATURE_DENOISER_H_
#define INCLUDE_FEATURE_DENOISER_H_

#include <QHash>
#include <QNetworkRequest>
#include <QRecursiveMutex>

#include "feature/feature.h"
#include "util/message.h"
#include "availablechannelorfeaturehandler.h"

#include "denoisersettings.h"

class WebAPIAdapterInterface;
class DenoiserWorker;
class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class ObjectPipe;

namespace SWGSDRangel {
    class SWGDeviceState;
}

class Denoiser : public Feature
{
	Q_OBJECT
public:
    class MsgConfigureDenoiser : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const DenoiserSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureDenoiser* create(const DenoiserSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureDenoiser(settings, settingsKeys, force);
        }

    private:
        DenoiserSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureDenoiser(const DenoiserSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgStartStop : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgStartStop* create(bool startStop) {
            return new MsgStartStop(startStop);
        }

    protected:
        bool m_startStop;

        MsgStartStop(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

    class MsgReportChannels : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        AvailableChannelOrFeatureList& getAvailableChannels() { return m_availableChannels; }
        const QStringList& getRenameFrom() const { return m_renameFrom; }
        const QStringList& getRenameTo() const { return m_renameTo; }

        static MsgReportChannels* create(const QStringList& renameFrom, const QStringList& renameTo) {
            return new MsgReportChannels(renameFrom, renameTo);
        }

    private:
        AvailableChannelOrFeatureList m_availableChannels;
        QStringList m_renameFrom;
        QStringList m_renameTo;

        MsgReportChannels(const QStringList& renameFrom, const QStringList& renameTo) :
            Message(),
            m_renameFrom(renameFrom),
            m_renameTo(renameTo)
        {}
    };

    class MsgSelectChannel : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        ChannelAPI *getChannel() { return m_channel; }
        static MsgSelectChannel* create(ChannelAPI *channel) {
            return new MsgSelectChannel(channel);
        }

    protected:
        ChannelAPI *m_channel;

        MsgSelectChannel(ChannelAPI *channel) :
            Message(),
            m_channel(channel)
        { }
    };

    class MsgReportSampleRate : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSampleRate() const { return m_sampleRate; }

        static MsgReportSampleRate* create(int sampleRate) {
            return new MsgReportSampleRate(sampleRate);
        }

    private:
        int m_sampleRate;

        MsgReportSampleRate(int sampleRate) :
            Message(),
            m_sampleRate(sampleRate)
        {}
    };

    Denoiser(WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~Denoiser();
    virtual void destroy() { delete this; }
    double getMagSqAvg() const;
    virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) const { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) const { title = m_settings.m_title; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int webapiRun(bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGFeatureSettings& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response,
            QString& errorMessage);

    virtual int webapiActionsPost(
            const QStringList& featureActionsKeys,
            SWGSDRangel::SWGFeatureActions& query,
            QString& errorMessage);

    static void webapiFormatFeatureSettings(
            SWGSDRangel::SWGFeatureSettings& response,
            const DenoiserSettings& settings);

    static void webapiUpdateFeatureSettings(
            DenoiserSettings& settings,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response);

    void getAvailableChannelsReport();

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

private:
    QThread *m_thread;
    QRecursiveMutex m_mutex;
    bool m_running;
    DenoiserWorker *m_worker;
    DenoiserSettings m_settings;
    AvailableChannelOrFeatureList m_availableChannels;
    AvailableChannelOrFeatureHandler m_availableChannelOrFeatureHandler;
    ChannelAPI *m_selectedChannel;
    ObjectPipe *m_dataPipe;
    int m_sampleRate;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void start();
    void stop();
    void applySettings(const DenoiserSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void notifyUpdate(const QStringList& renameFrom, const QStringList& renameTo);
    void setChannel(ChannelAPI *selectedChannel);
    void webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const DenoiserSettings& settings, bool force);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleChannelMessageQueue(MessageQueue *messageQueues);
    void channelsOrFeaturesChanged(const QStringList& renameFrom, const QStringList& renameTo);
    void handleDataPipeToBeDeleted(int reason, QObject *object);
};

#endif // INCLUDE_FEATURE_DENOISER_H_
