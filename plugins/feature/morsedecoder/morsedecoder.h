///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#ifndef INCLUDE_FEATURE_MORSEDECODER_H_
#define INCLUDE_FEATURE_MORSEDECODER_H_

#include <QHash>
#include <QNetworkRequest>
#include <QRecursiveMutex>
#include <QFile>
#include <QTextStream>
#include <QUdpSocket>

#include "feature/feature.h"
#include "util/message.h"
#include "availablechannelorfeaturehandler.h"
#include "dsp/scopevis.h"

#include "morsedecodersettings.h"

class WebAPIAdapterInterface;
class MorseDecoderWorker;
class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class ObjectPipe;

namespace SWGSDRangel {
    class SWGDeviceState;
}

class MorseDecoder : public Feature
{
	Q_OBJECT
public:
    class MsgConfigureMorseDecoder : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const MorseDecoderSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureMorseDecoder* create(const MorseDecoderSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureMorseDecoder(settings, settingsKeys, force);
        }

    private:
        MorseDecoderSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureMorseDecoder(const MorseDecoderSettings& settings, const QList<QString>& settingsKeys, bool force) :
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

    class MsgReportText : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QString getText() const { return m_text; }

        static MsgReportText* create(const QString& text) {
            return new MsgReportText(text);
        }

        float m_estimatedPitchHz;
        float m_estimatedSpeedWPM;
        float m_signalThreshold;
        float m_costFunction;

    private:
        QString m_text;

        MsgReportText(const QString& text) :
            m_text(text)
        {}
    };

    MorseDecoder(WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~MorseDecoder();
    virtual void destroy() { delete this; }
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
        const MorseDecoderSettings& settings);

    static void webapiUpdateFeatureSettings(
            MorseDecoderSettings& settings,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response);

    ScopeVis *getScopeVis() { return &m_scopeVis; }
    void getAvailableChannelsReport();

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

private:
    QThread *m_thread;
    QRecursiveMutex m_mutex;
    bool m_running;
    MorseDecoderWorker *m_worker;
    MorseDecoderSettings m_settings;
    ScopeVis m_scopeVis;
    AvailableChannelOrFeatureList m_availableChannels;
    AvailableChannelOrFeatureHandler m_availableChannelOrFeatureHandler;
    ChannelAPI *m_selectedChannel;
    ObjectPipe *m_dataPipe;
    int m_sampleRate;
    QFile m_logFile;
    QTextStream m_logStream;

    QUdpSocket m_udpSocket;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void start();
    void stop();
    void applySettings(const MorseDecoderSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void notifyUpdate(const QStringList& renameFrom, const QStringList& renameTo);
    void setChannel(ChannelAPI *selectedChannel);
    void webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const MorseDecoderSettings& settings, bool force);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleChannelMessageQueue(MessageQueue *messageQueues);
    void channelsOrFeaturesChanged(const QStringList& renameFrom, const QStringList& renameTo);
    void handleDataPipeToBeDeleted(int reason, QObject *object);
};

#endif // INCLUDE_FEATURE_MORSEDECODER_H_
