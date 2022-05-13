///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_UDPSINK_UDPSOURCE_H_
#define PLUGINS_CHANNELTX_UDPSINK_UDPSOURCE_H_

#include <QObject>
#include <QNetworkRequest>

#include "dsp/basebandsamplesource.h"
#include "dsp/spectrumvis.h"
#include "channel/channelapi.h"
#include "dsp/basebandsamplesink.h"
#include "util/message.h"

#include "udpsourcesettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;
class UDPSourceBaseband;
class ObjectPipe;

class UDPSource : public BasebandSampleSource, public ChannelAPI {
public:
    class MsgConfigureUDPSource : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const UDPSourceSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureUDPSource* create(const UDPSourceSettings& settings, bool force)
        {
            return new MsgConfigureUDPSource(settings, force);
        }

    private:
        UDPSourceSettings m_settings;
        bool m_force;

        MsgConfigureUDPSource(const UDPSourceSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        {
        }
    };

    /**
    * |<------ Baseband from device (before device soft interpolation) -------------------------->|
    * |<- Channel SR ------->|<- Channel SR ------->|<- Channel SR ------->|<- Channel SR ------->|
    * |             ^-------------------------------|
    * |             |        Source CF
    * |      | Source SR   |
    */
    class MsgConfigureChannelizer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSourceSampleRate() const { return m_sourceSampleRate; }
        int getSourceCenterFrequency() const { return m_sourceCenterFrequency; }

        static MsgConfigureChannelizer* create(int sourceSampleRate, int sourceCenterFrequency)
        {
            return new MsgConfigureChannelizer(sourceSampleRate, sourceCenterFrequency);
        }

    private:
        int m_sourceSampleRate;
        int m_sourceCenterFrequency;

        MsgConfigureChannelizer(int sourceSampleRate, int sourceCenterFrequency) :
            Message(),
            m_sourceSampleRate(sourceSampleRate),
            m_sourceCenterFrequency(sourceCenterFrequency)
        { }
    };

    UDPSource(DeviceAPI *deviceAPI);
    virtual ~UDPSource();
    virtual void destroy() { delete this; }
    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }

    virtual void start();
    virtual void stop();
    virtual void pull(SampleVector::iterator& begin, unsigned int nbSamples);
    virtual void pushMessage(Message *msg) { m_inputMessageQueue.push(msg); }
    virtual QString getSourceName() { return objectName(); }

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
        const UDPSourceSettings& settings);

    static void webapiUpdateChannelSettings(
            UDPSourceSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    SpectrumVis *getSpectrumVis() { return &m_spectrumVis; }
    double getMagSq() const;
    double getInMagSq() const;
    int32_t getBufferGauge() const;
    bool getSquelchOpen() const;
    void setSpectrum(bool enabled);
    void resetReadIndex();
    void setLevelMeter(QObject *levelMeter);
    uint32_t getNumberOfDeviceStreams() const;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private slots:
    void networkManagerFinished(QNetworkReply *reply);

private:
    DeviceAPI* m_deviceAPI;
    QThread *m_thread;
    UDPSourceBaseband* m_basebandSource;
    UDPSourceSettings m_settings;
    SpectrumVis m_spectrumVis;

    SampleVector m_sampleBuffer;
    QMutex m_settingsMutex;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    virtual bool handleMessage(const Message& cmd);

    void applySettings(const UDPSourceSettings& settings, bool force = false);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const UDPSourceSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        QList<QString>& channelSettingsKeys,
        const UDPSourceSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const UDPSourceSettings& settings,
        bool force
    );

};

#endif /* PLUGINS_CHANNELTX_UDPSINK_UDPSOURCE_H_ */
