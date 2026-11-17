///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#ifndef INCLUDE_RADIOPAGINGDEMOD_H
#define INCLUDE_RADIOPAGINGDEMOD_H

#include <atomic>

#include <QFile>
#include <QNetworkRequest>
#include <QTextStream>
#include <QThread>
#include <QUdpSocket>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "radiopagingdemodbaseband.h"
#include "radiopagingdemodsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class DeviceAPI;
class ScopeVis;

// Radio paging demodulator: POCSAG, FLEX and GSC, all speeds and polarities at the same
// time. Every decoded message is published as a MessageRecord on the "messages" pipe
class RadioPagingDemod : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureRadioPagingDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RadioPagingDemodSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureRadioPagingDemod* create(const QStringList& settingsKeys, const RadioPagingDemodSettings& settings, bool force) {
            return new MsgConfigureRadioPagingDemod(settingsKeys, settings, force);
        }

    private:
        RadioPagingDemodSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureRadioPagingDemod(const QStringList& settingsKeys, const RadioPagingDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    RadioPagingDemod(DeviceAPI *deviceAPI);
    virtual ~RadioPagingDemod();
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
    int getChannelSampleRate() const override;
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
            const RadioPagingDemodSettings& settings);

    static void webapiUpdateChannelSettings(
            RadioPagingDemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    double getMagSq() const { return m_basebandSink->getMagSq(); }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        m_basebandSink->getMagSqLevels(avg, peak, nbSamples);
    }

    RadioPagingDemodSink::Status getStatus() const { return m_basebandSink->getStatus(); }
    //!< Forget what the receivers of a protocol remember (0 POCSAG, 1 FLEX, 2 GSC, -1 all)
    void resetProtocol(int protocol);
    ScopeVis *getScopeSink() { return m_basebandSink->getScopeSink(); }
    //! The scope is fed while it is shown
    void setScopeEnabled(bool enabled) { m_basebandSink->setScopeEnabled(enabled); }

    uint32_t getNumberOfDeviceStreams() const;

    static const char * const m_channelIdURI;
    static const char * const m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    QThread m_thread;
    RadioPagingDemodBaseband *m_basebandSink;
    RadioPagingDemodSettings m_settings;
    int m_basebandSampleRate;           //!< stored from device message used when starting baseband sink
    qint64 m_centerFrequency;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;
    QUdpSocket m_udpSocket;
    QFile m_logFile;
    QTextStream m_logStream;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const RadioPagingDemodSettings& settings, bool force = false);
    void openLog(const RadioPagingDemodSettings& settings);
    void outputRecord(const MessageRecord& record);
    void sendSampleRateToDemodAnalyzer();
    void publishRecord(MessageRecord record);
    void announce(const QString& event);
    QTimer *m_announceTimer = nullptr;
    QString m_lastName;                         //!< The channel name announced last
    std::atomic<qint64> m_samplesFed;           //!< Samples fed by the device (its thread)
    qint64 m_samplesSeen;                       //!< ... when the input was last checked
    int m_inputIdleChecks;                      //!< Checks in a row without samples
    std::atomic<bool> m_inputActive;            //!< Samples came since the input last stopped
    QTimer *m_inputTimer;
    void checkInput();
    void webapiReverseSendSettings(const QStringList& channelSettingsKeys, const RadioPagingDemodSettings& settings, bool force);
    void webapiFormatChannelSettings(
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings *swgChannelSettings,
            const RadioPagingDemodSettings& settings,
            bool force);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
};

#endif // INCLUDE_RADIOPAGINGDEMOD_H
