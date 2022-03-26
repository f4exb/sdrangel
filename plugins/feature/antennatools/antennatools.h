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

#ifndef INCLUDE_FEATURE_ANTENNATOOLS_H_
#define INCLUDE_FEATURE_ANTENNATOOLS_H_

#include <QThread>
#include <QHash>
#include <QNetworkRequest>
#include <QTimer>

#include "feature/feature.h"
#include "util/message.h"

#include "antennatoolssettings.h"

class WebAPIAdapterInterface;
class QNetworkAccessManager;
class QNetworkReply;

namespace SWGSDRangel {
    class SWGDeviceState;
}

class AntennaTools : public Feature
{
    Q_OBJECT
public:
    class MsgConfigureAntennaTools : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const AntennaToolsSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureAntennaTools* create(const AntennaToolsSettings& settings, bool force) {
            return new MsgConfigureAntennaTools(settings, force);
        }

    private:
        AntennaToolsSettings m_settings;
        bool m_force;

        MsgConfigureAntennaTools(const AntennaToolsSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    AntennaTools(WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~AntennaTools();
    virtual void destroy() { delete this; }
    virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) const { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) const { title = m_settings.m_title; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

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
        const AntennaToolsSettings& settings);

    static void webapiUpdateFeatureSettings(
            AntennaToolsSettings& settings,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response);

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

private:
    QThread m_thread;
    AntennaToolsSettings m_settings;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void applySettings(const AntennaToolsSettings& settings, bool force = false);
    void webapiReverseSendSettings(QList<QString>& featureSettingsKeys, const AntennaToolsSettings& settings, bool force);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_FEATURE_ANTENNATOOLS_H_
