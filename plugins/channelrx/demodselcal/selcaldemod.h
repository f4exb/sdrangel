///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_SELCALDEMOD_H
#define INCLUDE_SELCALDEMOD_H

#include <vector>

#include <QNetworkRequest>
#include <QHostInfo>
#include <QUdpSocket>
#include <QThread>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDateTime>
#include <QMutex>
#include "selcalmessages.h"

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "dsp/spectrumvis.h"
#include "util/message.h"

#include "selcaldemodbaseband.h"
#include "selcaldemodsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;

class SELCALDemod : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureSELCALDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SELCALDemodSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureSELCALDemod *create(const QStringList& settingsKeys, const SELCALDemodSettings& settings, bool force)
        {
            return new MsgConfigureSELCALDemod(settingsKeys, settings, force);
        }

    private:
        SELCALDemodSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureSELCALDemod(const QStringList& settingsKeys, const SELCALDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        {
        }
    };

    using MsgMessage = SELCALMessages::MsgCall;
    using MsgStatus = SELCALMessages::MsgStatus;

    SELCALDemod(DeviceAPI *deviceAPI);
    virtual ~SELCALDemod();

    virtual void destroy()
    {
        delete this;
    }

    virtual void setDeviceAPI(DeviceAPI *deviceAPI);

    virtual DeviceAPI *getDeviceAPI()
    {
        return m_deviceAPI;
    }

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

    int getChannelSampleRate() const override;
    int getAudioSampleRate() const override;
    virtual void setCenterFrequency(qint64 frequency);

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void)streamIndex;
        (void)sinkElseSource;
        return 0;
    }

    virtual int webapiSettingsGet(SWGSDRangel::SWGChannelSettings& response, QString& errorMessage);
    virtual int webapiWorkspaceGet(SWGSDRangel::SWGWorkspaceInfo& response, QString& errorMessage);
    virtual int webapiSettingsPutPatch(bool force, const QStringList& channelSettingsKeys, SWGSDRangel::SWGChannelSettings& response,
                                       QString& errorMessage);
    virtual int webapiReportGet(SWGSDRangel::SWGChannelReport& response, QString& errorMessage);
    static void webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const SELCALDemodSettings& settings);
    static void webapiUpdateChannelSettings(SELCALDemodSettings& settings, const QStringList& channelSettingsKeys,
                                            SWGSDRangel::SWGChannelSettings& response);

    double getMagSq() const { return m_basebandSink->getMagSq(); }
    SpectrumVis *getSpectrumVis() { return &m_spectrumVis; }
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) { m_basebandSink->getMagSqLevels(avg, peak, nbSamples); }

    uint32_t getNumberOfDeviceStreams() const;

    static const char *const m_channelIdURI;
    static const char *const m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    QThread m_thread;
    SELCALDemodBaseband *m_basebandSink;
    SpectrumVis m_spectrumVis;
    SELCALDemodSettings m_settings;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    qint64 m_centerFrequency;
    QUdpSocket m_udpSocket;
    QHostAddress m_udpHostAddress; // m_settings.m_udpAddress, resolved
    void resolveUDPAddress(const QString& address);
    void udpAddressResolved(const QHostInfo& info);
    QMutex m_reportMutex;
    SELCALMessages::Call m_lastCall;
    SELCALMessages::Status m_status;
    quint64 m_callCount = 0;
    QFile m_logFile;
    QTextStream m_logStream;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const SELCALDemodSettings& settings, bool force = false);
    void sendSampleRateToDemodAnalyzer();
    void webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const SELCALDemodSettings& settings, bool force);
    void webapiFormatChannelSettings(const QList<QString>& channelSettingsKeys, SWGSDRangel::SWGChannelSettings *swgChannelSettings,
                                     const SELCALDemodSettings& settings, bool force);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
};

#endif // INCLUDE_SELCALDEMOD_H
