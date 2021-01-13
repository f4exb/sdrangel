///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_FEATURE_APRS_H_
#define INCLUDE_FEATURE_APRS_H_

#include <QThread>
#include <QHash>
#include <QNetworkRequest>
#include <QTimer>

#include "feature/feature.h"
#include "util/message.h"

#include "aprssettings.h"

class WebAPIAdapterInterface;
class APRSWorker;
class QNetworkAccessManager;
class QNetworkReply;

namespace SWGSDRangel {
    class SWGDeviceState;
}

class APRS : public Feature
{
    Q_OBJECT
public:
    class MsgConfigureAPRS : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const APRSSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureAPRS* create(const APRSSettings& settings, bool force) {
            return new MsgConfigureAPRS(settings, force);
        }

    private:
        APRSSettings m_settings;
        bool m_force;

        MsgConfigureAPRS(const APRSSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
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

    APRS(WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~APRS();
    virtual void destroy() { delete this; }
    virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) const { id = objectName(); }
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
        const APRSSettings& settings);

    static void webapiUpdateFeatureSettings(
            APRSSettings& settings,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response);

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

private:
    QThread m_thread;
    APRSWorker *m_worker;
    APRSSettings m_settings;
    QList<PipeEndPoint::AvailablePipeSource> m_availablePipes;
    QTimer m_updatePipesTimer;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void start();
    void stop();
    void applySettings(const APRSSettings& settings, bool force = false);
    void webapiReverseSendSettings(QList<QString>& featureSettingsKeys, const APRSSettings& settings, bool force);

private slots:
    void updatePipes();
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_FEATURE_APRS_H_
