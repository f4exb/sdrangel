///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                    //
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

#ifndef INCLUDE_PSK31DEMOD_H
#define INCLUDE_PSK31DEMOD_H

#include <QNetworkRequest>
#include <QUdpSocket>
#include <QThread>
#include <QFile>
#include <QTextStream>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "psk31demodbaseband.h"
#include "psk31demodsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;
class ScopeVis;

class PSK31Demod : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigurePSK31Demod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const PSK31DemodSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigurePSK31Demod* create(const QStringList& settingsKeys, const PSK31DemodSettings& settings, bool force)
        {
            return new MsgConfigurePSK31Demod(settingsKeys, settings, force);
        }

    private:
        PSK31DemodSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigurePSK31Demod(const QStringList& settingsKeys, const PSK31DemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    // Sent from Sink when character is decoded
    class MsgCharacter : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QString getCharacter() const { return m_character; }

        static MsgCharacter* create(const QString& character)
        {
            return new MsgCharacter(character);
        }

    private:
        QString m_character;

        MsgCharacter(const QString& character) :
            m_character(character)
        {}
    };

    class MsgDemodReport : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        float getFrequencyOffset() const { return m_frequencyOffset; }
        float getSNR() const { return m_snr; }
        bool isLocked() const { return m_locked; }

        static MsgDemodReport* create(float frequencyOffset, float snr, bool locked)
        {
            return new MsgDemodReport(frequencyOffset, snr, locked);
        }

    private:
        float m_frequencyOffset;
        float m_snr;
        bool m_locked;

        MsgDemodReport(float frequencyOffset, float snr, bool locked) :
            m_frequencyOffset(frequencyOffset),
            m_snr(snr),
            m_locked(locked)
        {}
    };

    PSK31Demod(DeviceAPI *deviceAPI);
    virtual ~PSK31Demod();
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
            const PSK31DemodSettings& settings);

    static void webapiUpdateChannelSettings(
            PSK31DemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    ScopeVis *getScopeSink();
    double getMagSq() const { return m_basebandSink->getMagSq(); }
    float getFrequencyOffset() const { return m_basebandSink->getFrequencyOffset(); }
    float getSNR() const { return m_basebandSink->getSNR(); }
    bool isLocked() const { return m_basebandSink->isLocked(); }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        m_basebandSink->getMagSqLevels(avg, peak, nbSamples);
    }

    uint32_t getNumberOfDeviceStreams() const;

    static const char * const m_channelIdURI;
    static const char * const m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    QThread m_thread;
    PSK31DemodBaseband* m_basebandSink;
    PSK31DemodSettings m_settings;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    qint64 m_centerFrequency;
    QUdpSocket m_udpSocket;
    QFile m_logFile;
    QTextStream m_logStream;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const PSK31DemodSettings& settings, bool force = false);
    void sendSampleRateToDemodAnalyzer();
    void webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const PSK31DemodSettings& settings, bool force);
    void webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const PSK31DemodSettings& settings,
        bool force
    );
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);

};

#endif // INCLUDE_PSK31DEMOD_H
