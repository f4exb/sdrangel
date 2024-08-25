///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2015-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
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

#ifndef INCLUDE_WDSPRX_H
#define INCLUDE_WDSPRX_H

#include <vector>

#include <QRecursiveMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesink.h"
#include "dsp/spectrumvis.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "wdsprxsettings.h"
#include "wdsprxbaseband.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;
class ObjectPipe;

class WDSPRx : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureWDSPRx : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const WDSPRxSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureWDSPRx* create(const WDSPRxSettings& settings, bool force)
        {
            return new MsgConfigureWDSPRx(settings, force);
        }

    private:
        WDSPRxSettings m_settings;
        bool m_force;

        MsgConfigureWDSPRx(const WDSPRxSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

	WDSPRx(DeviceAPI *deviceAPI);
	virtual ~WDSPRx();
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
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return m_settings.m_inputFrequencyOffset;
    }

    void setMessageQueueToGUI(MessageQueue* queue) final;
    uint32_t getAudioSampleRate() const { return m_running ? m_basebandSink->getAudioSampleRate() : 0; }
    uint32_t getChannelSampleRate() const { return m_running ? m_basebandSink->getChannelSampleRate() : 0; }
    double getMagSq() const { return m_running ? m_basebandSink->getMagSq() : 0.0; }
	bool getAudioActive() const { return m_running && m_basebandSink->getAudioActive(); }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples)
    {
        if (m_running) {
            m_basebandSink->getMagSqLevels(avg, peak, nbSamples);
        } else {
            avg = 0.0; peak = 0.0; nbSamples = 1;
        }
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
        const WDSPRxSettings& settings);

    static void webapiUpdateChannelSettings(
            WDSPRxSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    uint32_t getNumberOfDeviceStreams() const;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
	DeviceAPI *m_deviceAPI;
    QThread *m_thread;
    WDSPRxBaseband* m_basebandSink;
    QRecursiveMutex m_mutex;
    bool m_running;
    WDSPRxSettings m_settings;
    SpectrumVis m_spectrumVis;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

	virtual bool handleMessage(const Message& cmd);
	void applySettings(const WDSPRxSettings& settings, bool force = false);
    void sendSampleRateToDemodAnalyzer();
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const WDSPRxSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        QList<QString>& channelSettingsKeys,
        const WDSPRxSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const WDSPRxSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
};

#endif // INCLUDE_WDSPRX_H
