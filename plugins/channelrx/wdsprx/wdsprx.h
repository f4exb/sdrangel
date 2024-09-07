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

	explicit WDSPRx(DeviceAPI *deviceAPI);
	~WDSPRx() final;
	void destroy() final { delete this; }
    void setDeviceAPI(DeviceAPI *deviceAPI) final;
    DeviceAPI *getDeviceAPI() final { return m_deviceAPI; }
    SpectrumVis *getSpectrumVis() { return &m_spectrumVis; }

    using BasebandSampleSink::feed;
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po) final;
	void start() final;
	void stop() final;
    void pushMessage(Message *msg) final { m_inputMessageQueue.push(msg); }
    QString getSinkName() final { return objectName(); }

    void getIdentifier(QString& id) final { id = objectName(); }
    QString getIdentifier() const final { return objectName(); }
    void getTitle(QString& title) final { title = m_settings.m_title; }
    qint64 getCenterFrequency() const final { return m_settings.m_inputFrequencyOffset; }
    void setCenterFrequency(qint64 frequency) final;

    QByteArray serialize() const final;
    bool deserialize(const QByteArray& data) final;

    int getNbSinkStreams() const final { return 1; }
    int getNbSourceStreams() const final { return 0; }
    int getStreamIndex() const final { return m_settings.m_streamIndex; }

    qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const final
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

    int webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage) final;

    int webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage) final;

    int webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage) final;

    int webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage) final;

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

	bool handleMessage(const Message& cmd) final;
	void applySettings(const WDSPRxSettings& settings, bool force = false);
    void sendSampleRateToDemodAnalyzer() const;
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const WDSPRxSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        const QList<QString>& channelSettingsKeys,
        const WDSPRxSettings& settings,
        bool force
    ) const;
    void webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const WDSPRxSettings& settings,
        bool force
    ) const;

private slots:
    void networkManagerFinished(QNetworkReply *reply) const;
    void handleIndexInDeviceSetChanged(int index);
};

#endif // INCLUDE_WDSPRX_H
