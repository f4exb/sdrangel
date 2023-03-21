///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB.                             //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_ILSDEMOD_H
#define INCLUDE_ILSDEMOD_H

#include <QNetworkRequest>
#include <QUdpSocket>
#include <QThread>
#include <QFile>
#include <QTextStream>

#include "dsp/basebandsamplesink.h"
#include "dsp/spectrumvis.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "ilsdemodbaseband.h"
#include "ilsdemodsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;
class ScopeVis;

class ILSDemod : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureILSDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const ILSDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureILSDemod* create(const ILSDemodSettings& settings, bool force)
        {
            return new MsgConfigureILSDemod(settings, force);
        }

    private:
        ILSDemodSettings m_settings;
        bool m_force;

        MsgConfigureILSDemod(const ILSDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    // Sent from Sink when an estimate is made of the angle
    class MsgAngleEstimate : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        Real getPowerCarrier() const { return m_powerCarrier; }
        Real getPower90() const { return m_power90; }
        Real getPower150() const { return m_power150; }
        Real getModDepth90() const { return m_modDepth90; }
        Real getModDepth150() const { return m_modDepth150; }
        Real getSDM() const { return m_sdm; }
        Real getDDM() const { return m_ddm; }
        Real getAngle() const { return m_angle; }

        static MsgAngleEstimate* create(Real powerCarrier, Real power90, Real power150, Real modDepth90, Real modDepth150, Real sdm, Real ddm, Real angle)
        {
            return new MsgAngleEstimate(powerCarrier, power90, power150, modDepth90, modDepth150, sdm, ddm, angle);
        }

    private:
        Real m_powerCarrier;
        Real m_power90;
        Real m_power150;
        Real m_modDepth90;
        Real m_modDepth150;
        Real m_sdm;
        Real m_ddm;
        Real m_angle;

        MsgAngleEstimate(Real powerCarrier, Real power90, Real power150, Real modDepth90, Real modDepth150, Real sdm, Real ddm, Real angle) :
            m_powerCarrier(powerCarrier),
            m_power90(power90),
            m_power150(power150),
            m_modDepth90(modDepth90),
            m_modDepth150(modDepth150),
            m_sdm(sdm),
            m_ddm(ddm),
            m_angle(angle)
        {}
    };

    ILSDemod(DeviceAPI *deviceAPI);
    virtual ~ILSDemod();
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
    virtual void setCenterFrequency(qint64 frequency);

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }

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
            const ILSDemodSettings& settings);

    static void webapiUpdateChannelSettings(
            ILSDemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    SpectrumVis *getSpectrumVis() { return &m_spectrumVis; }
    ScopeVis *getScopeSink();
    uint32_t getAudioSampleRate() const { return m_running ? m_basebandSink->getAudioSampleRate() : 0; }
    bool getSquelchOpen() const { return m_running ? m_basebandSink->getSquelchOpen() : false; }
    double getMagSq() const { return m_basebandSink->getMagSq(); }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        m_basebandSink->getMagSqLevels(avg, peak, nbSamples);
    }
/*    void setMessageQueueToGUI(MessageQueue* queue) override {
        ChannelAPI::setMessageQueueToGUI(queue);
        m_basebandSink->setMessageQueueToGUI(queue);
    }*/

    uint32_t getNumberOfDeviceStreams() const;

    static const char * const m_channelIdURI;
    static const char * const m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    QThread m_thread;
    ILSDemodBaseband* m_basebandSink;
    bool m_running;
    ILSDemodSettings m_settings;
    SpectrumVis m_spectrumVis;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    qint64 m_centerFrequency;
    QUdpSocket m_udpSocket;
    QFile m_logFile;
    QTextStream m_logStream;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const ILSDemodSettings& settings, bool force = false);
    void sendSampleRateToDemodAnalyzer();
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const ILSDemodSettings& settings, bool force);
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const ILSDemodSettings& settings,
        bool force
    );
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);

};

#endif // INCLUDE_ILSDEMOD_H

