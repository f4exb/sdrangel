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

#ifndef INCLUDE_FEATURE_SIMPLEPTT_H_
#define INCLUDE_FEATURE_SIMPLEPTT_H_

#include <QRecursiveMutex>
#include <QNetworkRequest>

#include "feature/feature.h"
#include "util/message.h"

#include "simplepttsettings.h"

class WebAPIAdapterInterface;
class SimplePTTWorker;
class QNetworkAccessManager;
class QNetworkReply;
class QThread;

namespace SWGSDRangel {
    class SWGDeviceState;
}

class SimplePTT : public Feature
{
	Q_OBJECT
public:
    class MsgConfigureSimplePTT : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SimplePTTSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureSimplePTT* create(const SimplePTTSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureSimplePTT(settings, settingsKeys, force);
        }

    private:
        SimplePTTSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureSimplePTT(const SimplePTTSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgPTT : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getTx() const { return m_tx; }

        static MsgPTT* create(bool tx) {
            return new MsgPTT(tx);
        }

    private:
        bool m_tx;

        MsgPTT(bool tx) :
            Message(),
            m_tx(tx)
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

    SimplePTT(WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~SimplePTT();
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
        const SimplePTTSettings& settings);

    static void webapiUpdateFeatureSettings(
            SimplePTTSettings& settings,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response);

    void getAudioPeak(float& peak);

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

private:
    QThread *m_thread;
    QRecursiveMutex m_mutex;
    bool m_running;
    SimplePTTWorker *m_worker;
    SimplePTTSettings m_settings;
    bool m_ptt;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void start();
    void stop();
    void applySettings(const SimplePTTSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response);
    void webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const SimplePTTSettings& settings, bool force);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_FEATURE_SIMPLEPTT_H_
