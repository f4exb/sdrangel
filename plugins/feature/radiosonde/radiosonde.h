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

#ifndef INCLUDE_FEATURE_RADIOSONDE_H_
#define INCLUDE_FEATURE_RADIOSONDE_H_

#include <QDateTime>
#include <QNetworkRequest>
#include <QSet>

#include "feature/feature.h"
#include "util/message.h"
#include "availablechannelorfeaturehandler.h"

#include "radiosondesettings.h"

class WebAPIAdapterInterface;
class QNetworkAccessManager;
class QNetworkReply;

namespace SWGSDRangel {
    class SWGDeviceState;
}

class Radiosonde : public Feature
{
	Q_OBJECT
public:
    class MsgConfigureRadiosonde : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RadiosondeSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureRadiosonde* create(const RadiosondeSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureRadiosonde(settings, settingsKeys, force);
        }

    private:
        RadiosondeSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureRadiosonde(const RadiosondeSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    // A radiosonde as the GUI knows it. The m_has flags matter because zero is a valid altitude,
    // rate and heading, and a sonde is heard well before its position decodes
    struct Sonde
    {
        QString m_serial;
        QString m_type;
        QString m_status;
        QString m_burstKillStatus;
        QString m_burstKillTimer;
        float m_latitude;
        float m_longitude;
        float m_altitude;
        float m_altitudeMax;
        float m_speed;
        float m_verticalRate;
        float m_heading;
        float m_pressure;
        float m_temperature;
        float m_humidity;
        qint64 m_frequency;
        int m_messages;
        QDateTime m_lastUpdate;
        bool m_hasPosition;
        bool m_hasAltitudeMax;
        bool m_hasPressure;
        bool m_hasTemperature;
        bool m_hasHumidity;
        bool m_hasFrequency;

        Sonde() :
            m_latitude(0.0f),
            m_longitude(0.0f),
            m_altitude(0.0f),
            m_altitudeMax(0.0f),
            m_speed(0.0f),
            m_verticalRate(0.0f),
            m_heading(0.0f),
            m_pressure(0.0f),
            m_temperature(0.0f),
            m_humidity(0.0f),
            m_frequency(0),
            m_messages(0),
            m_hasPosition(false),
            m_hasAltitudeMax(false),
            m_hasPressure(false),
            m_hasTemperature(false),
            m_hasHumidity(false),
            m_hasFrequency(false)
        { }
    };

    // Sent from the GUI, which holds the radiosonde table, so the report has something to serve
    class MsgReportRadiosondes : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QList<Sonde>& getRadiosondes() { return m_radiosondes; }

        static MsgReportRadiosondes* create() {
            return new MsgReportRadiosondes();
        }

    private:
        QList<Sonde> m_radiosondes;

        MsgReportRadiosondes() :
            Message()
        { }
    };

    Radiosonde(WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~Radiosonde();
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
        const RadiosondeSettings& settings);

    static void webapiUpdateFeatureSettings(
            RadiosondeSettings& settings,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response);

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

private:
    RadiosondeSettings m_settings;
    QList<Sonde> m_radiosondes;   //!< Latest snapshot pushed by the GUI
    QDateTime m_radiosondesUpdated;    //!< When that snapshot was taken
    AvailableChannelOrFeatureHandler m_availableChannelHandler;
    AvailableChannelOrFeatureList m_availableChannels;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void start();
    void stop();
    void applySettings(const RadiosondeSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const RadiosondeSettings& settings, bool force);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleChannelMessageQueue(MessageQueue* messageQueue);
};

#endif // INCLUDE_FEATURE_RADIOSONDE_H_
