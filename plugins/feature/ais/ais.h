///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>         //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
// Copyright (C) 2021-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#ifndef INCLUDE_FEATURE_AIS_H_
#define INCLUDE_FEATURE_AIS_H_

#include <QDateTime>
#include <QNetworkRequest>
#include <QSet>

#include "feature/feature.h"
#include "util/message.h"
#include "availablechannelorfeaturehandler.h"

#include "aissettings.h"

class WebAPIAdapterInterface;
class QNetworkAccessManager;
class QNetworkReply;

namespace SWGSDRangel {
    class SWGDeviceState;
}

class AIS : public Feature
{
	Q_OBJECT
public:
    class MsgConfigureAIS : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const AISSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureAIS* create(const AISSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureAIS(settings, settingsKeys, force);
        }

    private:
        AISSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureAIS(const AISSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    // A vessel as the GUI knows it. Every field except the MMSI is optional, and m_has* says
    // which have actually been received: zero is a valid position, course, speed and heading
    struct Vessel
    {
        QString m_mmsi;
        QString m_name;
        QString m_callsign;
        QString m_imo;
        QString m_country;
        QString m_type;
        QString m_shipType;
        QString m_status;
        QString m_destination;
        float m_latitude;
        float m_longitude;
        float m_course;
        float m_speed;
        int m_heading;
        int m_length;
        int m_messages;
        QDateTime m_positionUpdate;
        QDateTime m_lastUpdate;
        bool m_hasPosition;
        bool m_hasCourse;
        bool m_hasSpeed;
        bool m_hasHeading;
        bool m_hasLength;

        Vessel() :
            m_latitude(0.0f),
            m_longitude(0.0f),
            m_course(0.0f),
            m_speed(0.0f),
            m_heading(0),
            m_length(0),
            m_messages(0),
            m_hasPosition(false),
            m_hasCourse(false),
            m_hasSpeed(false),
            m_hasHeading(false),
            m_hasLength(false)
        { }
    };

    // Sent from the GUI, which holds the vessel table, so that the report has something to serve
    class MsgReportVessels : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QList<Vessel>& getVessels() { return m_vessels; }

        static MsgReportVessels* create() {
            return new MsgReportVessels();
        }

    private:
        QList<Vessel> m_vessels;

        MsgReportVessels() :
            Message()
        { }
    };

    AIS(WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~AIS();
    virtual void destroy() { delete this; }
    virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) const { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) const { title = m_settings.m_title; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int webapiReportGet(
            SWGSDRangel::SWGFeatureReport& response,
            QString& errorMessage);

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGFeatureSettings& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response,
            QString& errorMessage);

    void webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response);

    static void webapiFormatFeatureSettings(
        SWGSDRangel::SWGFeatureSettings& response,
        const AISSettings& settings);

    static void webapiUpdateFeatureSettings(
            AISSettings& settings,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response);

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

private:
    AISSettings m_settings;
    QList<Vessel> m_vessels;      //!< Latest snapshot pushed by the GUI
    QDateTime m_vesselsUpdated;   //!< When that snapshot was taken
    AvailableChannelOrFeatureHandler m_availableChannelHandler;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void start();
    void stop();
    void applySettings(const AISSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const AISSettings& settings, bool force);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleChannelMessageQueue(MessageQueue* messageQueue);
};

#endif // INCLUDE_FEATURE_AIS_H_
