///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>         //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
// Copyright (C) 2021-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
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

#ifndef INCLUDE_FEATURE_SKYMAP_H_
#define INCLUDE_FEATURE_SKYMAP_H_

#include <QHash>
#include <QNetworkRequest>
#include <QDateTime>
#include <QRecursiveMutex>

#include "feature/feature.h"
#include "util/message.h"

#include "skymapsettings.h"

class WebAPIAdapterInterface;
class QNetworkAccessManager;
class QNetworkReply;

namespace SWGSDRangel {
    class SWGDeviceState;
}

class SkyMap : public Feature
{
    Q_OBJECT
public:

    class MsgConfigureSkyMap : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SkyMapSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureSkyMap* create(const SkyMapSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureSkyMap(settings, settingsKeys, force);
        }

    private:
        SkyMapSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureSkyMap(const SkyMapSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgFind : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QString getTarget() const { return m_target; }

        static MsgFind* create(const QString& target) {
            return new MsgFind(target);
        }

    private:
        QString m_target;

        MsgFind(const QString& target) :
            Message(),
            m_target(target)
        {}
    };

    class MsgSetDateTime : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QDateTime getDateTime() const { return m_dateTime; }

        static MsgSetDateTime* create(const QDateTime& dateTime) {
            return new MsgSetDateTime(dateTime);
        }

    private:
        QDateTime m_dateTime;

        MsgSetDateTime(const QDateTime& dateTime) :
            Message(),
            m_dateTime(dateTime)
        {}
    };

    struct ViewDetails {
        double m_ra;
        double m_dec;
        float m_azimuth;
        float m_elevation;
        float m_fov;
        float m_latitude;
        float m_longitude;
        QDateTime m_dateTime;

        ViewDetails() :
            m_ra(0.0),
            m_dec(0.0),
            m_azimuth(0.0f),
            m_elevation(0.0f),
            m_fov(0.0f),
            m_latitude(0.0f),
            m_longitude(0.0f)
        {
        }
    };

    class MsgReportViewDetails : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const ViewDetails& getViewDetails() { return m_viewDetails; }

        static MsgReportViewDetails* create(const ViewDetails& viewDetails) {
            return new MsgReportViewDetails(viewDetails);
        }

    private:
        ViewDetails m_viewDetails;

        MsgReportViewDetails(const ViewDetails& viewDetails) :
            Message(),
            m_viewDetails(viewDetails)
        {}
    };

    SkyMap(WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~SkyMap();
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
        const SkyMapSettings& settings);

    static void webapiUpdateFeatureSettings(
            SkyMapSettings& settings,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response);

    void setSkyMapDateTime(QDateTime skymapDateTime, QDateTime systemDateTime, double multiplier);
    QDateTime getSkyMapDateTime();

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

private:
    SkyMapSettings m_settings;
    ViewDetails m_viewDetails;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void applySettings(const SkyMapSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response);
    void webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const SkyMapSettings& settings, bool force);

    QDateTime m_skymapDateTime;
    QDateTime m_systemDateTime;
    double m_multiplier;
    QRecursiveMutex m_dateTimeMutex;

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_FEATURE_SKYMAP_H_
