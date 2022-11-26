///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_FEATURE_PERTESTER_H_
#define INCLUDE_FEATURE_PERTESTER_H_

#include <QThread>
#include <QNetworkRequest>
#include <QTimer>

#include "feature/feature.h"
#include "util/message.h"

#include "pertestersettings.h"

class WebAPIAdapterInterface;
class PERTesterWorker;
class QNetworkAccessManager;
class QNetworkReply;

namespace SWGSDRangel {
    class SWGDeviceState;
}

class PERTester : public Feature
{
	Q_OBJECT
public:
    class MsgConfigurePERTester : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const PERTesterSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigurePERTester* create(const PERTesterSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigurePERTester(settings, settingsKeys, force);
        }

    private:
        PERTesterSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigurePERTester(const PERTesterSettings& settings, const QList<QString>& settingsKeys, bool force) :
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

    class MsgResetStats : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgResetStats* create() {
            return new MsgResetStats();
        }

    protected:
        MsgResetStats() :
            Message()
        { }
    };

    class MsgReportWorker : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QString getMessage() { return m_message; }

        static MsgReportWorker* create(QString message) {
            return new MsgReportWorker(message);
        }

    private:
        QString m_message;

        MsgReportWorker(QString message) :
            Message(),
            m_message(message)
        {}
    };

    PERTester(WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~PERTester();
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
        const PERTesterSettings& settings);

    static void webapiUpdateFeatureSettings(
            PERTesterSettings& settings,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response);

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

private:
    QThread *m_thread;
    PERTesterWorker *m_worker;
    PERTesterSettings m_settings;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void start();
    void stop();
    void applySettings(const PERTesterSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const PERTesterSettings& settings, bool force);
    void webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_FEATURE_PERTESTER_H_
