///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
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

#ifndef INCLUDE_BEAMSTEERINGCWMOD_H
#define INCLUDE_BEAMSTEERINGCWMOD_H

#include <vector>
#include <QNetworkRequest>

#include "dsp/mimochannel.h"
#include "channel/channelapi.h"
#include "util/messagequeue.h"
#include "util/message.h"

#include "beamsteeringcwmodsettings.h"

class QThread;
class DeviceAPI;
class BeamSteeringCWModBaseband;
class QNetworkReply;
class QNetworkAccessManager;
class BasebandSampleSink;
class ObjectPipe;

class BeamSteeringCWMod: public MIMOChannel, public ChannelAPI
{
public:
    class MsgConfigureBeamSteeringCWMod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const BeamSteeringCWModSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureBeamSteeringCWMod* create(const BeamSteeringCWModSettings& settings, bool force)
        {
            return new MsgConfigureBeamSteeringCWMod(settings, force);
        }

    private:
        BeamSteeringCWModSettings m_settings;
        bool m_force;

        MsgConfigureBeamSteeringCWMod(const BeamSteeringCWModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgBasebandNotification : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgBasebandNotification* create(int sampleRate, qint64 centerFrequency) {
            return new MsgBasebandNotification(sampleRate, centerFrequency);
        }

        int getSampleRate() const { return m_sampleRate; }
        qint64 getCenterFrequency() const { return m_centerFrequency; }

    private:

        MsgBasebandNotification(int sampleRate, qint64 centerFrequency) :
            Message(),
            m_sampleRate(sampleRate),
            m_centerFrequency(centerFrequency)
        { }

        int m_sampleRate;
        qint64 m_centerFrequency;
    };

    explicit BeamSteeringCWMod(DeviceAPI *deviceAPI);
	~BeamSteeringCWMod() final;
	void destroy() final { delete this; }
    void setDeviceAPI(DeviceAPI *deviceAPI) final;
    DeviceAPI *getDeviceAPI() final { return m_deviceAPI; }

    void startSinks() final { /* Not used for MIMO */ }
	void stopSinks() final { /* Not used for MIMO */ }
    void startSources( /* Not used for MIMO */ ) final; //!< thread start()
    void stopSources( /* Not used for MIMO */ ) final; //!< thread exit() and wait()
	void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, unsigned int sinkIndex) final;
    void pull(SampleVector::iterator& begin, unsigned int nbSamples, unsigned int sourceIndex) final;
    void pushMessage(Message *msg) final { m_inputMessageQueue.push(msg); }
    QString getMIMOName() final { return objectName(); }

    void getIdentifier(QString& id) final { id = objectName(); }
    QString getIdentifier() const final { return objectName(); }
    void getTitle(QString& title) final { title = "BeamSteeringCWMod"; }
    qint64 getCenterFrequency() const final { return m_frequencyOffset; }
    void setCenterFrequency(qint64) final { /* Not used for MIMO */ }
    uint32_t getBasebandSampleRate() const { return m_basebandSampleRate; }

    QByteArray serialize() const final;
    bool deserialize(const QByteArray& data) final;

    int getNbSinkStreams() const final { return 0; }
    int getNbSourceStreams() const final { return 2; }
    int getStreamIndex() const final { return -1; }

    qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const final
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return m_frequencyOffset;
    }

    int webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage) final;

    int webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage) final;

    int webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& query,
        QString& errorMessage) final;

    static void webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const BeamSteeringCWModSettings& settings);

    static void webapiUpdateChannelSettings(
        BeamSteeringCWModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response);

    static const char* const m_channelIdURI;
    static const char* const m_channelId;
    static const int m_fftSize;

private:
    DeviceAPI *m_deviceAPI;
    QThread *m_thread;
    BeamSteeringCWModBaseband* m_basebandSource;
    QMutex m_mutex;
    bool m_running;
    BeamSteeringCWModSettings m_settings;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    int64_t m_frequencyOffset;
    uint32_t m_basebandSampleRate;

   	bool handleMessage(const Message& cmd) final; //!< Processing of a message. Returns true if message has actually been processed
    void applySettings(const BeamSteeringCWModSettings& settings, bool force = false);
    static void validateFilterChainHash(BeamSteeringCWModSettings& settings);
    void calculateFrequencyOffset();
    void webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const BeamSteeringCWModSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        const QList<QString>& channelSettingsKeys,
        const BeamSteeringCWModSettings& settings,
        bool force
    ) const;
    void webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const BeamSteeringCWModSettings& settings,
        bool force
    ) const;

private slots:
    void handleInputMessages();
    void networkManagerFinished(QNetworkReply *reply) const;
};

#endif // INCLUDE_BEAMSTEERINGCWSOURCE_H
