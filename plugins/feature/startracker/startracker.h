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

#ifndef INCLUDE_FEATURE_STARTRACKER_H_
#define INCLUDE_FEATURE_STARTRACKER_H_

#include <QThread>
#include <QNetworkRequest>
#include <QSet>

#include "feature/feature.h"
#include "util/message.h"
#include "util/fits.h"

#include "startrackersettings.h"

class WebAPIAdapterInterface;
class StarTrackerWorker;
class QNetworkAccessManager;
class QNetworkReply;
class Weather;

namespace SWGSDRangel {
    class SWGDeviceState;
}

class StarTracker : public Feature
{
	Q_OBJECT
public:
    class MsgConfigureStarTracker : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const StarTrackerSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureStarTracker* create(const StarTrackerSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureStarTracker(settings, settingsKeys, force);
        }

    private:
        StarTrackerSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureStarTracker(const StarTrackerSettings& settings, const QList<QString>& settingsKeys, bool force) :
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

    class MsgSetSolarFlux : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        float getFlux() const { return m_flux; }

        static MsgSetSolarFlux* create(float flux) {
            return new MsgSetSolarFlux(flux);
        }

    protected:
        float m_flux;

        MsgSetSolarFlux(float flux) :
            Message(),
            m_flux(flux)
        { }
    };

    class MsgReportAvailableSatelliteTrackers : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QList<StarTrackerSettings::AvailableFeature>& getFeatures() { return m_availableFeatures; }

        static MsgReportAvailableSatelliteTrackers* create() {
            return new MsgReportAvailableSatelliteTrackers();
        }

    private:
        QList<StarTrackerSettings::AvailableFeature> m_availableFeatures;

        MsgReportAvailableSatelliteTrackers() :
            Message()
        {}
    };

    StarTracker(WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~StarTracker();
    virtual void destroy() { delete this; }
    virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) const { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) const { title = m_settings.m_title; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int webapiRun(bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage) override;

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGFeatureSettings& response,
            QString& errorMessage) override;

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response,
            QString& errorMessage) override;

    virtual int webapiReportGet(
            SWGSDRangel::SWGFeatureReport& response,
            QString& errorMessage);

    virtual int webapiActionsPost(
            const QStringList& featureActionsKeys,
            SWGSDRangel::SWGFeatureActions& query,
            QString& errorMessage);

    static void webapiFormatFeatureSettings(
        SWGSDRangel::SWGFeatureSettings& response,
        const StarTrackerSettings& settings);

    static void webapiUpdateFeatureSettings(
            StarTrackerSettings& settings,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response);

    const FITS *getTempFITS(int index) const { return m_temps[index]; }
    const FITS *getSpectralIndexFITS() const { return m_spectralIndex; }
    bool calcSkyTemperature(double frequency, double beamwidth, double ra, double dec, double& temp) const;

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

private:
    QThread *m_thread;
    StarTrackerWorker *m_worker;
    StarTrackerSettings m_settings;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;
    QSet<ChannelAPI*> m_availableChannels;
    QHash<Feature*, StarTrackerSettings::AvailableFeature> m_satelliteTrackers;
    Weather *m_weather;
    float m_solarFlux;

    QList<FITS*> m_temps;
    FITS *m_spectralIndex;

    void start();
    void stop();
    void applySettings(const StarTrackerSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const StarTrackerSettings& settings, bool force);
    void webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response);
    double applyBeam(const FITS *fits, double beamwidth, double ra, double dec, int& imgX, int& imgY) const;
    void scanAvailableChannels();
    void scanAvailableFeatures();
    void notifyUpdateSatelliteTrackers();

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void weatherUpdated(float temperature, float pressure, float humidity);
    void handleChannelAdded(int deviceSetIndex, ChannelAPI *channel);
    void handleMessagePipeToBeDeleted(int reason, QObject* object);
    void handleFeatureAdded(int featureSetIndex, Feature *feature);
    void handleFeatureRemoved(int featureSetIndex, Feature *feature);
    void handleChannelMessageQueue(MessageQueue* messageQueue);
};

#endif // INCLUDE_FEATURE_STARTRACKER_H_
