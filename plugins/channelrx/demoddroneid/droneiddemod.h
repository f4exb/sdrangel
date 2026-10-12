///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_DRONEIDDEMOD_H
#define INCLUDE_DRONEIDDEMOD_H

#include <QNetworkRequest>
#include <QUdpSocket>
#include <QThread>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QElapsedTimer>
#include <QHash>

#include <atomic>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "droneiddemodbaseband.h"
#include "droneiddemodsettings.h"

namespace SWGSDRangel {
    class SWGDroneIDDemodSettings;
}

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class DroneIDMessage;

class DroneIDDemod : public BasebandSampleSink, public ChannelAPI {

public:
    class MsgConfigureDroneIDDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const DroneIDDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }

        static MsgConfigureDroneIDDemod* create(const QStringList& settingsKeys, const DroneIDDemodSettings& settings, bool force)
        {
            return new MsgConfigureDroneIDDemod(settingsKeys, settings, force);
        }

    private:
        DroneIDDemodSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureDroneIDDemod(const QStringList& settingsKeys, const DroneIDDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    // Result of attempting to decode a burst
    class MsgBurstResult : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getDetected() const { return m_detected; }         //!< Whether DroneID sync sequences were found
        bool getDecoded() const { return m_decoded; }           //!< Whether turbo decoded data passed CRC
        QByteArray getMessage() const { return m_message; }    //!< Decoded message
        QDateTime getDateTime() const { return m_dateTime; }
        float getSNR() const { return m_snr; }
        float getCFO() const { return m_cfo; }
        float getEVM() const { return m_evm; }
        float getZCMetric() const { return m_zcMetric; }
        int getIterations() const { return m_iterations; }
        bool getLegacy() const { return m_legacy; }
        qint64 getFrequency() const { return m_frequency; }    //!< Channel frequency in Hz the burst was received on

        static MsgBurstResult* create(bool detected, bool decoded, const QByteArray& message, QDateTime dateTime,
            float snr, float cfo, float evm, float zcMetric, int iterations, bool legacy, qint64 frequency)
        {
            return new MsgBurstResult(detected, decoded, message, dateTime, snr, cfo, evm, zcMetric, iterations, legacy, frequency);
        }

    private:
        bool m_detected;
        bool m_decoded;
        QByteArray m_message;
        QDateTime m_dateTime;
        float m_snr;
        float m_cfo;
        float m_evm;
        float m_zcMetric;
        int m_iterations;
        bool m_legacy;
        qint64 m_frequency;

        MsgBurstResult(bool detected, bool decoded, const QByteArray& message, QDateTime dateTime,
                float snr, float cfo, float evm, float zcMetric, int iterations, bool legacy, qint64 frequency) :
            Message(),
            m_detected(detected),
            m_decoded(decoded),
            m_message(message),
            m_dateTime(dateTime),
            m_snr(snr),
            m_cfo(cfo),
            m_evm(evm),
            m_zcMetric(zcMetric),
            m_iterations(iterations),
            m_legacy(legacy),
            m_frequency(frequency)
        { }
    };

    DroneIDDemod(DeviceAPI *deviceAPI);
    virtual ~DroneIDDemod();
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
    virtual const QString& getURI() const { return getName(); }
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
        return 0;
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
            const DroneIDDemodSettings& settings);

    static void webapiUpdateChannelSettings(
            DroneIDDemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    double getMagSq() const { return m_basebandSink->getMagSq(); }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        m_basebandSink->getMagSqLevels(avg, peak, nbSamples);
    }

    int getBurstsDetected() const { return m_basebandSink->getBurstsDetected(); }
    int getBurstsDropped() const { return m_basebandSink->getBurstsDropped(); }
    int getBurstsDecoded() const { return m_burstsDecoded; }
    int getDronesTracked() const { return m_dronesTracked; }
    bool isScanLocked() const { return m_scanLocked; }  //!< Whether scan is paused on a channel with DroneID bursts

    uint32_t getNumberOfDeviceStreams() const;

    static const char * const m_channelIdURI;
    static const char * const m_channelId;

private:
    static constexpr int m_scanTimerInterval = 100;     // How often scan state is checked, in ms
    // Bursts detected shortly after retuning may be from samples received on the previous frequency,
    // so are ignored when deciding whether to remain on a channel
    static constexpr qint64 m_scanSettleTime = 250;     // ms

    struct Drone {
        QDateTime m_lastSeen;   // Message time, which may be from a file
        QDateTime m_lastUpdate; // Wall clock time message was received
        bool m_onMap;
        bool m_propellersSpinning;  // Whether propeller animation has been started on 3D map

        Drone() : m_onMap(false), m_propellersSpinning(false) {}
    };

    DeviceAPI *m_deviceAPI;
    QThread m_thread;
    DroneIDDemodBaseband* m_basebandSink;
    DroneIDDemodSettings m_settings;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    qint64 m_centerFrequency;
    QUdpSocket m_udpSocket;
    QFile m_logFile;
    QTextStream m_logStream;
    std::atomic<int> m_burstsDecoded;   // Atomic, as read by web API thread
    std::atomic<int> m_dronesTracked;
    QHash<QString, Drone> m_drones;  // Indexed by serial number
    QTimer m_mapTimer;

    QTimer m_scanTimer;
    QElapsedTimer m_scanClock;
    QList<qint64> m_scanFrequencies;    // Frequencies in band being scanned
    QList<qint64> m_scanHistory;        // Frequencies bursts have been detected on, most recent first
    QList<qint64> m_scanOrder;          // Order frequencies are visited in this pass
    int m_scanPosition;                 // Index in to m_scanOrder of next frequency to tune to
    qint64 m_scanFrequency;             // Frequency currently tuned to, or 0 if not on a scanned frequency
    qint64 m_scanTuneTime;              // m_scanClock time of last retune
    qint64 m_scanBurstTime;             // m_scanClock time of last burst detected, or -1 if none
    std::atomic<bool> m_scanLocked;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const DroneIDDemodSettings& settings, bool force = false);
    void sendSampleRateToDemodAnalyzer();
    void handleBurstResult(const MsgBurstResult& result);
    void logMessage(const MsgBurstResult& result, const DroneIDMessage *msg);
    void sendToMap(const DroneIDMessage *msg, const QDateTime& dateTime, Drone& drone);
    void sendMapItem(const QString& name, double latitude, double longitude, float altitude, int altitudeReference,
        const QString& image, float imageRotation, const QString& label, const QString& text, const QDateTime& dateTime,
        const QString& model = QString(), const QString& animation = QString(), bool stopAnimation = false);
    void clearFromMap(const QString& serial);
    static void setString(QString *(SWGSDRangel::SWGDroneIDDemodSettings::*get)(), void (SWGSDRangel::SWGDroneIDDemodSettings::*set)(QString*),
        SWGSDRangel::SWGDroneIDDemodSettings *swg, const QString& value);
    void startScan();
    void stopScan();
    void scanNewPass();
    void scanTune(qint64 frequency);
    qint64 scanFrequencyMatch(qint64 frequency) const;
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(const QStringList& channelSettingsKeys, const DroneIDDemodSettings& settings, bool force);
    void webapiFormatChannelSettings(
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const DroneIDDemodSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
    void removeOldDrones();
    void scanTimeout();
};

#endif // INCLUDE_DRONEIDDEMOD_H
