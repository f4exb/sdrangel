///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_FEATURE_SID_H_
#define INCLUDE_FEATURE_SID_H_

#include <QThread>
#include <QNetworkRequest>
#include <QDateTime>

#include "feature/feature.h"
#include "util/message.h"

#include "sidsettings.h"

class WebAPIAdapterInterface;
class QNetworkAccessManager;
class QNetworkReply;
class SIDWorker;

namespace SWGSDRangel {
    class SWGDeviceState;
}

// There's a structure in winnt.h named SID
class SIDMain : public Feature
{
	Q_OBJECT
public:
    class MsgConfigureSID : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SIDSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureSID* create(const SIDSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureSID(settings, settingsKeys, force);
        }

    private:
        SIDSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureSID(const SIDSettings& settings, const QList<QString>& settingsKeys, bool force) :
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

    class MsgMeasurement : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QDateTime getDateTime() const { return m_dateTime; }
        const QStringList& getIds() const { return m_ids; }
        const QList<double>& getMeasurements() const { return m_measurements; }

        static MsgMeasurement* create(QDateTime dateTime, const QStringList& ids, const QList<double>& measurements) {
            return new MsgMeasurement(dateTime, ids, measurements);
        }

    private:
        QDateTime m_dateTime;
        QStringList m_ids;
        QList<double> m_measurements;

        MsgMeasurement(QDateTime dateTime, const QStringList& ids, const QList<double>& measurements) :
            Message(),
            m_dateTime(dateTime),
            m_ids(ids),
            m_measurements(measurements)
        {}
    };

    SIDMain(WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~SIDMain();
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

    static void webapiFormatFeatureSettings(
        SWGSDRangel::SWGFeatureSettings& response,
        const SIDSettings& settings);

    static void webapiUpdateFeatureSettings(
            SIDSettings& settings,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response);

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

private:
    QThread *m_thread;
    SIDWorker *m_worker;
    SIDSettings m_settings;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void start();
    void stop();
    void applySettings(const SIDSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const SIDSettings& settings, bool force);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_FEATURE_SID_H_
