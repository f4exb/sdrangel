///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_ADSBDEMOD_H
#define INCLUDE_ADSBDEMOD_H

#include <QNetworkRequest>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "adsbdemodbaseband.h"
#include "adsbdemodsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;
class ADSBDemodWorker;
class ADSBDemodTracker;

class ADSBDemod : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureADSBDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const ADSBDemodSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureADSBDemod* create(const ADSBDemodSettings& settings, const QStringList& settingsKeys, bool force)
        {
            return new MsgConfigureADSBDemod(settings, settingsKeys, force);
        }

    private:
        ADSBDemodSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureADSBDemod(const ADSBDemodSettings& settings, const QStringList& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgAircraftReport : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        struct AircraftReport {
            QString m_icao;
            QString m_callsign;
            float m_latitude;
            float m_longitude;
            int m_altitude;
            int m_groundSpeed;
            // Which of the above have been received. Zero is a valid position, altitude and
            // speed, so a reader cannot otherwise tell a value apart from one not yet known
            bool m_positionValid;
            bool m_altitudeValid;
            bool m_groundSpeedValid;

            // Extended state from ADS-B and Mode-S frames. Each has its own validity flag
            // so an unknown value is never reported as a real zero.
            QString m_flight;
            int m_pressureAltitude;        bool m_pressureAltitudeValid;
            float m_heading;               bool m_headingValid;
            float m_track;                 bool m_trackValid;
            int m_verticalRate;            bool m_verticalRateValid;
            QString m_emitterCategory;
            QString m_status;
            int m_squawk;
            bool m_spi;
            bool m_onSurface;              bool m_onSurfaceValid;
            int m_selAltitude;             bool m_selAltitudeValid;
            int m_selHeading;              bool m_selHeadingValid;
            int m_trueAirspeed;            bool m_trueAirspeedValid;
            int m_indicatedAirspeed;       bool m_indicatedAirspeedValid;
            float m_mach;                  bool m_machValid;
            float m_roll;                  bool m_rollValid;
            float m_turnRate;              bool m_turnRateValid;
            float m_range;
            float m_azimuth;
            float m_elevation;
            int m_adsbFrameCount;
            int m_modesFrameCount;
            float m_minCorrelation;
            float m_maxCorrelation;
            float m_correlation;

            AircraftReport() :
                m_latitude(0.0f), m_longitude(0.0f), m_altitude(0), m_groundSpeed(0),
                m_positionValid(false), m_altitudeValid(false), m_groundSpeedValid(false),
                m_pressureAltitude(0), m_pressureAltitudeValid(false),
                m_heading(0.0f), m_headingValid(false), m_track(0.0f), m_trackValid(false),
                m_verticalRate(0), m_verticalRateValid(false),
                m_squawk(-1), m_spi(false), m_onSurface(false), m_onSurfaceValid(false),
                m_selAltitude(0), m_selAltitudeValid(false),
                m_selHeading(0), m_selHeadingValid(false),
                m_trueAirspeed(0), m_trueAirspeedValid(false),
                m_indicatedAirspeed(0), m_indicatedAirspeedValid(false),
                m_mach(0.0f), m_machValid(false),
                m_roll(0.0f), m_rollValid(false), m_turnRate(0.0f), m_turnRateValid(false),
                m_range(0.0f), m_azimuth(0.0f), m_elevation(0.0f),
                m_adsbFrameCount(0), m_modesFrameCount(0),
                m_minCorrelation(0.0f), m_maxCorrelation(0.0f), m_correlation(0.0f)
            {}
        };

        QList<AircraftReport>& getReport() { return m_report; }

        static MsgAircraftReport* create()
        {
            return new MsgAircraftReport();
        }

    private:
        QList<AircraftReport> m_report;

        MsgAircraftReport() :
            Message()
        { }
    };

    class MsgResetStats : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgResetStats* create()
        {
            return new MsgResetStats();
        }

    private:
        MsgResetStats() :
            Message()
        { }
    };

    ADSBDemod(DeviceAPI *deviceAPI);
    virtual ~ADSBDemod();
    virtual void destroy() { delete this; }
    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }

    using BasebandSampleSink::feed;
    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
    virtual void start();
    virtual void stop();
    virtual void pushMessage(Message *msg) { m_inputMessageQueue.push(msg); }
    virtual QString getSinkName() { return objectName(); }

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }
    int getChannelSampleRate() const override;
    virtual void setCenterFrequency(qint64 frequency);

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return m_settings.m_inputFrequencyOffset;
    }

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiWorkspaceGet(
            SWGSDRangel::SWGWorkspaceInfo& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage);

    static void webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const ADSBDemodSettings& settings);

    static void webapiUpdateChannelSettings(
            ADSBDemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    void getMagSqLevels(double& avg, double& peak, int& nbSamples) { m_basebandSink->getMagSqLevels(avg, peak, nbSamples); }
    void setMessageQueueToGUI(MessageQueue* queue) final;

    ADSBDemodTracker *getTracker() { return m_tracker; }

    void setTarget(const QString& name, float targetAzimuth, float targetElevation, float targetRange);
    void clearTarget() { m_targetAzElValid = false; }

    uint32_t getNumberOfDeviceStreams() const;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
    DeviceAPI* m_deviceAPI;
    QThread *m_thread;
    ADSBDemodWorker *m_worker;
    ADSBDemodTracker *m_tracker;
    QThread *m_workerThread;    //!< Runs the tracker and the export worker
    ADSBDemodBaseband* m_basebandSink;
    ADSBDemodSettings m_settings;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    qint64 m_centerFrequency;

    bool m_targetAzElValid;
    float m_targetAzimuth;
    float m_targetElevation;
    float m_targetRange;
    QString m_targetName;
    QList<MsgAircraftReport::AircraftReport> m_aircraftReport;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

	virtual bool handleMessage(const Message& cmd); //!< Processing of a message. Returns true if message has actually been processed
    void applySettings(const ADSBDemodSettings& settings, const QStringList& settingsKeys, bool force = false);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const ADSBDemodSettings& settings, bool force);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
};

#endif // INCLUDE_ADSBDEMOD_H
