///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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

#ifndef INCLUDE_FREQTRACKER_H
#define INCLUDE_FREQTRACKER_H

#include <vector>

#include <QNetworkRequest>
#include <QMutex>

#include "dsp/basebandsamplesink.h"
#include "dsp/spectrumvis.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "freqtrackerbaseband.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class ObjectPipe;

class FreqTracker : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureFreqTracker : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const FreqTrackerSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureFreqTracker* create(const FreqTrackerSettings& settings, bool force)
        {
            return new MsgConfigureFreqTracker(settings, force);
        }

    private:
        FreqTrackerSettings m_settings;
        bool m_force;

        MsgConfigureFreqTracker(const FreqTrackerSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    FreqTracker(DeviceAPI *deviceAPI);
	virtual ~FreqTracker();
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
        const FreqTrackerSettings& settings);

    static void webapiUpdateChannelSettings(
            FreqTrackerSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    SpectrumVis *getSpectrumVis() { return &m_spectrumVis; }
    uint32_t getSampleRate() const { return m_basebandSink->getSampleRate(); }
	double getMagSq() const { return m_basebandSink->getMagSq(); }
	bool getSquelchOpen() const { return m_basebandSink->getSquelchOpen(); }
	bool getPllLocked() const { return m_basebandSink->getPllLocked(); }
	Real getFrequency() const { return m_basebandSink->getFrequency(); };
    Real getAvgDeltaFreq() const { return m_basebandSink->getAvgDeltaFreq(); }
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) { m_basebandSink->getMagSqLevels(avg, peak, nbSamples); }

    void propagateMessageQueue(MessageQueue *messageQueueToInput) {
        m_basebandSink->setMessageQueueToInput(messageQueueToInput);
    }

    uint32_t getNumberOfDeviceStreams() const;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
    DeviceAPI* m_deviceAPI;
    QThread *m_thread;
    FreqTrackerBaseband* m_basebandSink;
    FreqTrackerSettings m_settings;
    SpectrumVis m_spectrumVis;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    static const int m_udpBlockSize;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

	virtual bool handleMessage(const Message& cmd);
    void applySettings(const FreqTrackerSettings& settings, bool force = false);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const FreqTrackerSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        QList<QString>& channelSettingsKeys,
        const FreqTrackerSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const FreqTrackerSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
};

#endif // INCLUDE_FREQTRACKER_H
