///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FREEDVDEMOD_H
#define INCLUDE_FREEDVDEMOD_H

#include <vector>

#include <QNetworkRequest>

#include "dsp/basebandsamplesink.h"
#include "dsp/spectrumvis.h"
#include "channel/channelapi.h"
#include "util/message.h"
#include "util/doublebufferfifo.h"

#include "freedvdemodbaseband.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class QThread;
class ObjectPipe;

class FreeDVDemod : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureFreeDVDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const FreeDVDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureFreeDVDemod* create(const FreeDVDemodSettings& settings, bool force)
        {
            return new MsgConfigureFreeDVDemod(settings, force);
        }

    private:
        FreeDVDemodSettings m_settings;
        bool m_force;

        MsgConfigureFreeDVDemod(const FreeDVDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

	class MsgResyncFreeDVDemod : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
        static MsgResyncFreeDVDemod* create() {
            return new MsgResyncFreeDVDemod();
        }

	private:
		MsgResyncFreeDVDemod()
		{}
	};

    FreeDVDemod(DeviceAPI *deviceAPI);
	virtual ~FreeDVDemod();
	virtual void destroy() { delete this; }
    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }
    SpectrumVis *getSpectrumVis() { return &m_spectrumVis; }

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

    int getAudioSampleRate() const { return m_basebandSink->getAudioSampleRate(); }
    uint32_t getModemSampleRate() const { return m_basebandSink->getModemSampleRate(); }
    double getMagSq() const { return m_basebandSink->getMagSq(); }
	bool getAudioActive() const { return m_basebandSink->getAudioActive(); }
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) { m_basebandSink->getMagSqLevels(avg, peak, nbSamples); }
	void getSNRLevels(double& avg, double& peak, int& nbSamples) { m_basebandSink->getSNRLevels(avg, peak, nbSamples); }
	int getBER() const { return m_basebandSink->getBER(); }
	float getFrequencyOffset() const { return m_basebandSink->getFrequencyOffset(); }
	bool isSync() const { return m_basebandSink->isSync(); }
    void setMessageQueueToGUI(MessageQueue* queue) override {
        ChannelAPI::setMessageQueueToGUI(queue);
        m_basebandSink->setMessageQueueToGUI(queue);
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
        const FreeDVDemodSettings& settings);

    static void webapiUpdateChannelSettings(
            FreeDVDemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    uint32_t getNumberOfDeviceStreams() const;
    void setLevelMeter(QObject *levelMeter);

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
	DeviceAPI *m_deviceAPI;
    QThread *m_thread;
    FreeDVDemodBaseband *m_basebandSink;
    FreeDVDemodSettings m_settings;
    SpectrumVis m_spectrumVis;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

	virtual bool handleMessage(const Message& cmd);
	void applySettings(const FreeDVDemodSettings& settings, bool force = false);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const FreeDVDemodSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        QList<QString>& channelSettingsKeys,
        const FreeDVDemodSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const FreeDVDemodSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
};

#endif // INCLUDE_FREEDVDEMOD_H
