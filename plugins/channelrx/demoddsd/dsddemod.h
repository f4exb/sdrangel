///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <QNetworkRequest>
#include <QMutex>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "dsddemodsettings.h"
#include "dsddecoder.h"
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
        bool getForce() const { return m_force; }

        static MsgConfigureDSDDemod* create(const DSDDemodSettings& settings, bool force)
        {
            return new MsgConfigureDSDDemod(settings, force);
        }

    private:
        DSDDemodSettings m_settings;
        bool m_force;

        MsgConfigureDSDDemod(const DSDDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
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
    virtual void setCenterFrequency(qint64 frequency);

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }

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
        const DSDDemodSettings& settings);

    static void webapiUpdateChannelSettings(
            DSDDemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    uint32_t getNumberOfDeviceStreams() const;
	void setScopeXYSink(BasebandSampleSink* sampleSink) { m_basebandSink->setScopeXYSink(sampleSink); }
	void configureMyPosition(float myLatitude, float myLongitude) { m_basebandSink->configureMyPosition(myLatitude, myLongitude); }
	double getMagSq() { return m_basebandSink->getMagSq(); }
	bool getSquelchOpen() const { return m_basebandSink->getSquelchOpen(); }
	const DSDDecoder& getDecoder() const { return m_basebandSink->getDecoder(); }
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) { m_basebandSink->getMagSqLevels(avg, peak, nbSamples); }
    const char *updateAndGetStatusText() { return m_basebandSink->updateAndGetStatusText(); }
    int getAudioSampleRate() const { return m_basebandSink->getAudioSampleRate(); }

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
	DeviceAPI *m_deviceAPI;
    QThread *m_thread;
    DSDDemodBaseband *m_basebandSink;
    QMutex m_mutex;
    bool m_running;
	DSDDemodSettings m_settings;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    QHash<Feature*, DSDDemodSettings::AvailableAMBEFeature> m_availableAMBEFeatures;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    static const int m_udpBlockSize;

	virtual bool handleMessage(const Message& cmd);
    void applySettings(const DSDDemodSettings& settings, bool force = false);
    void scanAvailableAMBEFeatures();
    void notifyUpdateAMBEFeatures();
    void sendSampleRateToDemodAnalyzer();
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const DSDDemodSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        QList<QString>& channelSettingsKeys,
        const DSDDemodSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const DSDDemodSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
    void handleFeatureAdded(int featureSetIndex, Feature *feature);
    void handleFeatureRemoved(int featureSetIndex, Feature *feature);
};

#endif // INCLUDE_DSDDEMOD_H
