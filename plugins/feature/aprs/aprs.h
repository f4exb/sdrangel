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

#ifndef INCLUDE_FEATURE_APRS_H_
#define INCLUDE_FEATURE_APRS_H_

#include <QThread>
#include <QHash>
#include <QDateTime>
#include <QNetworkRequest>

#include "feature/feature.h"
#include "util/message.h"
#include "availablechannelorfeaturehandler.h"

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
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureAPRS* create(const APRSSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureAPRS(settings, settingsKeys, force);
        }

    private:
        APRSSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureAPRS(const APRSSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
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

    class MsgQueryAvailableChannels : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgQueryAvailableChannels* create() {
            return new MsgQueryAvailableChannels();
        }

    protected:
        MsgQueryAvailableChannels() :
            Message()
        { }
    };

    class MsgReportAvailableChannels : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        AvailableChannelOrFeatureList& getChannels() { return m_availableChannels; }

        static MsgReportAvailableChannels* create() {
            return new MsgReportAvailableChannels();
        }

    private:
        AvailableChannelOrFeatureList m_availableChannels;

        MsgReportAvailableChannels() :
            Message()
        {}
    };

    // A station or object as the GUI knows it. A station is heard long before it reports a
    // position, and zero is a valid latitude, course and speed, so each value has its own flag
    struct Station
    {
        QString m_callsign;
        QString m_reportingStation;
        QString m_symbol;
        QString m_status;
        QString m_comment;
        QString m_antennaDirectivity;
        QString m_telemetryProjectName;
        float m_latitude;
        float m_longitude;
        float m_altitude;
        float m_course;
        float m_speed;
        float m_powerWatts;
        float m_antennaHeight;
        float m_antennaGain;
        float m_radioRange;
        int m_packets;
        QDateTime m_lastPacket;
        bool m_isObject;
        bool m_hasWeather;
        bool m_hasTelemetry;
        bool m_hasPosition;
        bool m_hasAltitude;
        bool m_hasCourseAndSpeed;
        bool m_hasStationDetails;
        bool m_hasRadioRange;

        Station() :
            m_latitude(0.0f),
            m_longitude(0.0f),
            m_altitude(0.0f),
            m_course(0.0f),
            m_speed(0.0f),
            m_powerWatts(0.0f),
            m_antennaHeight(0.0f),
            m_antennaGain(0.0f),
            m_radioRange(0.0f),
            m_packets(0),
            m_isObject(false),
            m_hasWeather(false),
            m_hasTelemetry(false),
            m_hasPosition(false),
            m_hasAltitude(false),
            m_hasCourseAndSpeed(false),
            m_hasStationDetails(false),
            m_hasRadioRange(false)
        { }
    };

    // Sent from the GUI, which holds the station hash, so the report has something to serve
    class MsgReportStations : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QList<Station>& getStations() { return m_stations; }
        int getPacketCount() const { return m_packetCount; }
        void setPacketCount(int count) { m_packetCount = count; }

        static MsgReportStations* create() {
            return new MsgReportStations();
        }

    private:
        QList<Station> m_stations;
        int m_packetCount;

        MsgReportStations() :
            Message(),
            m_packetCount(0)
        { }
    };

    APRS(WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~APRS();
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
        const APRSSettings& settings);

    static void webapiUpdateFeatureSettings(
            APRSSettings& settings,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response);

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

private:
    QThread *m_thread;
    QList<Station> m_stations;      //!< Latest snapshot pushed by the GUI
    QDateTime m_stationsUpdated;    //!< When that snapshot was taken
    int m_packetCount;              //!< Packets across all stations in that snapshot
    APRSWorker *m_worker;
    APRSSettings m_settings;
    AvailableChannelOrFeatureHandler m_availableChannelHandler;
    AvailableChannelOrFeatureList m_availableChannels;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void start();
    void stop();
    void applySettings(const APRSSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const APRSSettings& settings, bool force);
    void notifyUpdateChannels();

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleChannelMessageQueue(MessageQueue* messageQueue);
    void channelsChanged();
};

#endif // INCLUDE_FEATURE_APRS_H_
