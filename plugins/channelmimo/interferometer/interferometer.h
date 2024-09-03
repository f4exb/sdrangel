///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#ifndef INCLUDE_INTERFEROMETER_H
#define INCLUDE_INTERFEROMETER_H

#include <vector>
#include <QNetworkRequest>

#include "dsp/mimochannel.h"
#include "dsp/spectrumvis.h"
#include "dsp/scopevis.h"
#include "channel/channelapi.h"
#include "util/messagequeue.h"
#include "util/message.h"

#include "interferometersettings.h"

class QThread;
class DeviceAPI;
class InterferometerBaseband;
class QNetworkReply;
class QNetworkAccessManager;
class ObjectPipe;
class DevieSampleSource;
class Interferometer: public MIMOChannel, public ChannelAPI
{
public:
    class MsgConfigureInterferometer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const InterferometerSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureInterferometer* create(const InterferometerSettings& settings, const QList<QString>& settingsKeys, bool force)
        {
            return new MsgConfigureInterferometer(settings, settingsKeys, force);
        }

    private:
        InterferometerSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureInterferometer(const InterferometerSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
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

    class MsgReportDevices : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QList<int>& getDeviceSetIndexes() { return m_deviceSetIndexes; }

        static MsgReportDevices* create() {
            return new MsgReportDevices();
        }

    private:
        QList<int> m_deviceSetIndexes;

        MsgReportDevices() :
            Message()
        { }
    };

    explicit Interferometer(DeviceAPI *deviceAPI);
	~Interferometer() final;
	void destroy() final { delete this; }
    void setDeviceAPI(DeviceAPI *deviceAPI) final;
    DeviceAPI *getDeviceAPI() final { return m_deviceAPI; }

	void startSinks() final; //!< thread start()
	void stopSinks() final;  //!< thread exit() and wait()
    void startSources() final { /* not for MIMMO*/ }
    void stopSources() final { /* not for MIMMO*/ }
	void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, unsigned int sinkIndex) final;
    void pull(SampleVector::iterator& begin, unsigned int nbSamples, unsigned int sourceIndex) final;
    void pushMessage(Message *msg) final { m_inputMessageQueue.push(msg); }
    QString getMIMOName() final { return objectName(); }

    void getIdentifier(QString& id) final { id = objectName(); }
    QString getIdentifier() const final { return objectName(); }
    void getTitle(QString& title) final { title = "Interferometer"; }
    qint64 getCenterFrequency() const final { return m_frequencyOffset; }
    void setCenterFrequency(qint64) final { /* not for MIMMO*/ }
    uint32_t getDeviceSampleRate() const { return m_deviceSampleRate; }

    QByteArray serialize() const final;
    bool deserialize(const QByteArray& data) final;

    int getNbSinkStreams() const final { return 2; }
    int getNbSourceStreams() const final { return 0; }
    int getStreamIndex() const final { return -1; }

    qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const final
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return m_frequencyOffset;
    }

    SpectrumVis *getSpectrumVis() { return &m_spectrumVis; }
    ScopeVis *getScopeVis() { return &m_scopeSink; }
    void applyChannelSettings(uint32_t log2Decim, uint32_t filterChainHash);
    const QList<int>& getDeviceSetList() { return m_localInputDeviceIndexes; }

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
        const InterferometerSettings& settings);

    static void webapiUpdateChannelSettings(
        InterferometerSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response);

    static const char* const m_channelIdURI;
    static const char* const m_channelId;
    static const int m_fftSize;

private:
    DeviceAPI *m_deviceAPI;
    SpectrumVis m_spectrumVis;
    ScopeVis m_scopeSink;
    QThread *m_thread;
    InterferometerBaseband* m_basebandSink;
    QMutex m_mutex;
    bool m_running;
    InterferometerSettings m_settings;
    MessageQueue *m_guiMessageQueue;  //!< Input message queue to the GUI

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    uint64_t m_centerFrequency;
    int64_t m_frequencyOffset;
    uint32_t m_deviceSampleRate;

    QList<int> m_localInputDeviceIndexes;

	bool handleMessage(const Message& cmd) final; //!< Processing of a message. Returns true if message has actually been processed
    void applySettings(const InterferometerSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    static void validateFilterChainHash(InterferometerSettings& settings);
    void calculateFrequencyOffset(uint32_t log2Decim, uint32_t filterChainHash);
    void updateDeviceSetList();
    DeviceSampleSource *getLocalDevice(int deviceSetIndex);
    void propagateSampleRateAndFrequency(int index, uint32_t log2Decim);
    void webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const InterferometerSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        const QList<QString>& channelSettingsKeys,
        const InterferometerSettings& settings,
        bool force
    ) const;
    void webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const InterferometerSettings& settings,
        bool force
    ) const;

private slots:
    void handleInputMessages();
    void networkManagerFinished(QNetworkReply *reply) const;
};

#endif // INCLUDE_INTERFEROMETER_H
