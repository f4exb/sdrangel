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

#ifndef INCLUDE_FEATURE_VORLOCALIZER_H_
#define INCLUDE_FEATURE_VORLOCALIZER_H_

#include <QRecursiveMutex>
#include <QNetworkRequest>

#include "feature/feature.h"
#include "util/message.h"
#include "util/average.h"

#include "vorlocalizersettings.h"

class WebAPIAdapterInterface;
class VorLocalizerWorker;
class QNetworkAccessManager;
class QNetworkReply;
class QThread;

namespace SWGSDRangel {
    class SWGDeviceState;
}

class VORLocalizer : public Feature
{
	Q_OBJECT
public:
    class MsgConfigureVORLocalizer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const VORLocalizerSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureVORLocalizer* create(const VORLocalizerSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureVORLocalizer(settings, settingsKeys, force);
        }

    private:
        VORLocalizerSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureVORLocalizer(const VORLocalizerSettings& settings, const QList<QString>& settingsKeys, bool force) :
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

    class MsgAddVORChannel : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getNavId() const { return m_navId; }

        static MsgAddVORChannel* create(int navId) {
            return new MsgAddVORChannel(navId);
        }

    protected:
        int m_navId;

        MsgAddVORChannel(int navId) :
            Message(),
            m_navId(navId)
        { }
    };

    class MsgRemoveVORChannel : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getNavId() const { return m_navId; }

        static MsgRemoveVORChannel* create(int navId) {
            return new MsgRemoveVORChannel(navId);
        }

    protected:
        int m_navId;

        MsgRemoveVORChannel(int navId) :
            Message(),
            m_navId(navId)
        { }
    };

    class MsgRefreshChannels : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgRefreshChannels* create() {
            return new MsgRefreshChannels();
        }

    protected:
        MsgRefreshChannels() :
            Message()
        { }
    };

    VORLocalizer(WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~VORLocalizer();
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

    virtual int webapiReportGet(
            SWGSDRangel::SWGFeatureReport& response,
            QString& errorMessage);

    virtual int webapiActionsPost(
            const QStringList& featureActionsKeys,
            SWGSDRangel::SWGFeatureActions& query,
            QString& errorMessage);

    static void webapiFormatFeatureSettings(
        SWGSDRangel::SWGFeatureSettings& response,
        const VORLocalizerSettings& settings);

    static void webapiUpdateFeatureSettings(
            VORLocalizerSettings& settings,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response);

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

private:
    struct VORChannelReport
    {
        float m_radial; //!< current detected radial
        float m_refMag; //!< current reference signal magnitude
        float m_varMag; //!< current variable signal magnitude
        AverageUtil<float, double> m_radialAvg;
        AverageUtil<float, double> m_refMagAvg;
        AverageUtil<float, double> m_varMagAvg;
        bool m_validRadial;
        bool m_validRefMag;
        bool m_validVarMag;
        QString m_morseIdent; //!< identification morse code transcript

        VORChannelReport() = default;
        VORChannelReport(const VORChannelReport&) = default;
        VORChannelReport& operator=(const VORChannelReport&) = default;
    };

    QThread *m_thread;
    VorLocalizerWorker *m_worker;
    bool m_running;
    QRecursiveMutex m_mutex;
    VORLocalizerSettings m_settings;
    QHash<ChannelAPI*, VORLocalizerSettings::AvailableChannel> m_availableChannels;
    QHash<int, VORChannelReport> m_vorChannelReports;
    QHash<int, bool> m_vorSinglePlans;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void start();
    void stop();
    void applySettings(const VORLocalizerSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void updateChannels();
    void notifyUpdateChannels();
    void webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const VORLocalizerSettings& settings, bool force);
    void webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleChannelMessageQueue(MessageQueue* messageQueue);
    void handleMessagePipeToBeDeleted(int reason, QObject* object);
    void handleChannelAdded(int deviceSetIndex, ChannelAPI *channel);
};

#endif // INCLUDE_FEATURE_VORLOCALIZER_H_
