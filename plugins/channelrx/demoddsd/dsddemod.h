///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_DSDDEMOD_H
#define INCLUDE_DSDDEMOD_H

#include <vector>

#include <QHash>
#include <QList>
#include <QNetworkRequest>
#include <QJsonObject>
#include <QQueue>
#include <QRecursiveMutex>
#include <QTimer>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "dsddemodsettings.h"
#include "dsddemodsink.h"
#include "dsddemodbaseband.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DownChannelizer;
class ObjectPipe;

class DSDDemod : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureDSDDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const DSDDemodSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureDSDDemod* create(const QStringList& settingsKeys, const DSDDemodSettings& settings, bool force)
        {
            return new MsgConfigureDSDDemod(settingsKeys, settings, force);
        }

    private:
        DSDDemodSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureDSDDemod(const QStringList& settingsKeys, const DSDDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgQueryAvailableAMBEFeatures : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgQueryAvailableAMBEFeatures* create() {
            return new MsgQueryAvailableAMBEFeatures();
        }

    protected:
        MsgQueryAvailableAMBEFeatures() :
            Message()
        { }
    };

    class MsgReportAvailableAMBEFeatures : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QList<DSDDemodSettings::AvailableAMBEFeature>& getFeatures() { return m_availableFeatures; }

        static MsgReportAvailableAMBEFeatures* create() {
            return new MsgReportAvailableAMBEFeatures();
        }

    private:
        QList<DSDDemodSettings::AvailableAMBEFeature> m_availableFeatures;

        MsgReportAvailableAMBEFeatures() :
            Message()
        {}
    };

    DSDDemod(DeviceAPI *deviceAPI);
	virtual ~DSDDemod();
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

    virtual int webapiActionsPost(
            const QStringList& channelActionsKeys,
            SWGSDRangel::SWGChannelActions& query,
            QString& errorMessage);

    static void webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const DSDDemodSettings& settings);

    static void webapiUpdateChannelSettings(
            DSDDemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    uint32_t getNumberOfDeviceStreams() const;
	void setScopeXYSink(BasebandSampleSink* sampleSink);
	void configureMyPosition(float myLatitude, float myLongitude) { if (m_running) { m_basebandSink->configureMyPosition(myLatitude, myLongitude); } }
	double getMagSq() { return m_running ? m_basebandSink->getMagSq() : 0.0; }
    //! Demodulator and decoder status (a copy, safe from any thread)
    DSDDemodSink::Report getReport() const { return m_running ? m_basebandSink->getReport() : DSDDemodSink::Report(); }
    //! The operator's decision on the Angel61 file offer awaiting confirmation (the report's angel61FilePending)
    void angel61FileDecision(bool accept) { if (m_running) { m_basebandSink->setAngel61FileDecision(accept); } }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples)
    {
        if (m_running) {
            m_basebandSink->getMagSqLevels(avg, peak, nbSamples);
        } else {
            avg = 0.0; peak = 0.0; nbSamples = 1;
        }
    }

    int getAudioSampleRate() const override { return m_running ? m_basebandSink->getAudioSampleRate() : 0; }
    bool isRunning() { return m_running; }
    QString contactReportStatus() const { return m_contactStatus; }

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

    //! What a DSE Modulator of the instance makes of the IP datagrams heard (3.5)
    struct Angel61Tunnel
    {
        bool m_listening = true;           //!< False when its Rx selector names another demodulator
        quint64 m_datagrams = 0;           //!< Delivered to the host, from any demodulator it listens to
        quint64 m_bytes = 0;
        quint64 m_refused = 0;             //!< Heard and refused: not the peer, not valid, host would not take it
        QString m_status = QStringLiteral("Tunnel: off");
    };

    //! The DSE Modulators of the instance, in channel order: none if there is none
    QList<Angel61Tunnel> getAngel61Tunnels();

private:
	DeviceAPI *m_deviceAPI;
    QThread *m_thread;
    DSDDemodBaseband *m_basebandSink;
    QRecursiveMutex m_mutex;
    bool m_running;
	DSDDemodSettings m_settings;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    qint64 m_centerFrequency;
    QHash<Feature*, DSDDemodSettings::AvailableAMBEFeature> m_availableAMBEFeatures;
    Feature *m_connectedAMBEFeature;   //!< The feature the sink was given, whatever the settings now say (identity only)
    std::shared_ptr<MbeEncoderPort> m_connectedAMBEPort;   //!< Its port, which the sink calls
    BasebandSampleSink *m_scopeXYSink;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;
    QNetworkAccessManager *m_contactNetwork;
    QTimer m_contactTimer;
    QQueue<QJsonObject> m_pendingContacts;
    bool m_contactInFlight = false;
    qint64 m_contactNextAttemptMs = 0;
    int m_contactFailures = 0;
    QString m_contactStatus = QStringLiteral("Off");
    QHash<const QObject*, Angel61Tunnel> m_angel61Tunnels; //!< By modulator (identity only): what each last said

    static const int m_udpBlockSize;

	virtual bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const DSDDemodSettings& settings, bool force = false);
    void scanAvailableAMBEFeatures();
    void notifyUpdateAMBEFeatures();
    void sendSampleRateToDemodAnalyzer();
    void pumpContacts();
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const DSDDemodSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        const QList<QString>& channelSettingsKeys,
        const DSDDemodSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const DSDDemodSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
    void handleFeatureAdded(int featureSetIndex, Feature *feature);
    void handleFeatureRemoved(int featureSetIndex, Feature *feature);
    //! The sink's AMBE feature (none: nullptr), through the port the feature hands out
    void connectAMBEFeature(Feature *feature);
};

#endif // INCLUDE_DSDDEMOD_H
