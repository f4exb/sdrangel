///////////////////////////////////////////////////////////////////////////////////
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

#ifndef INCLUDE_IEEE_802_15_4_DEMOD_H
#define INCLUDE_IEEE_802_15_4_DEMOD_H

#include <QNetworkRequest>
#include <QUdpSocket>
#include <QThread>
#include <QDateTime>
#include <QFile>
#include <QTextStream>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "ieee_802_15_4_demodbaseband.h"
#include "ieee_802_15_4_demodsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class ScopeVis;

class IEEE_802_15_4_Demod : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureIEEE_802_15_4_Demod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const IEEE_802_15_4_DemodSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureIEEE_802_15_4_Demod* create(const QStringList& settingsKeys, const IEEE_802_15_4_DemodSettings& settings, bool force)
        {
            return new MsgConfigureIEEE_802_15_4_Demod(settingsKeys, settings, force);
        }

    private:
        IEEE_802_15_4_DemodSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureIEEE_802_15_4_Demod(const QStringList& settingsKeys, const IEEE_802_15_4_DemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    //! A received frame
    class MsgFrame : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QByteArray& getPSDU() const { return m_psdu; }
        QDateTime getDateTime() const { return m_dateTime; }
        bool getFCSOK() const { return m_fcsOK; }
        bool getCoherent() const { return m_coherent; }
        float getFrequencyOffset() const { return m_frequencyOffset; }
        float getLQI() const { return m_lqi; }
        float getPowerDB() const { return m_powerDB; }
        float getSNR() const { return m_snr; }

        static MsgFrame* create(const QByteArray& psdu, const QDateTime& dateTime, bool fcsOK, bool coherent,
                                float frequencyOffset, float lqi, float powerDB, float snr)
        {
            return new MsgFrame(psdu, dateTime, fcsOK, coherent, frequencyOffset, lqi, powerDB, snr);
        }

    private:
        QByteArray m_psdu;          //!< MAC frame including FCS
        QDateTime m_dateTime;
        bool m_fcsOK;
        bool m_coherent;            //!< Received by coherent rather than non-coherent detector
        float m_frequencyOffset;    //!< Hz
        float m_lqi;                //!< 0-1
        float m_powerDB;
        float m_snr;                //!< dB

        MsgFrame(const QByteArray& psdu, const QDateTime& dateTime, bool fcsOK, bool coherent,
                 float frequencyOffset, float lqi, float powerDB, float snr) :
            Message(),
            m_psdu(psdu),
            m_dateTime(dateTime),
            m_fcsOK(fcsOK),
            m_coherent(coherent),
            m_frequencyOffset(frequencyOffset),
            m_lqi(lqi),
            m_powerDB(powerDB),
            m_snr(snr)
        { }
    };

    IEEE_802_15_4_Demod(DeviceAPI *deviceAPI);
    virtual ~IEEE_802_15_4_Demod();
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
    int getAudioSampleRate() const override { return 0; }
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
            const IEEE_802_15_4_DemodSettings& settings);

    static void webapiUpdateChannelSettings(
            IEEE_802_15_4_DemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    ScopeVis *getScopeSink();
    double getMagSq() const { return m_basebandSink->getMagSq(); }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        m_basebandSink->getMagSqLevels(avg, peak, nbSamples);
    }

    uint32_t getNumberOfDeviceStreams() const;

    //! 802.15.4 channel number for a frequency, or -1 if not a channel centre frequency
    static int getChannelNumber(qint64 frequency);
    //! Encapsulate frame in ZEP v2, for Wireshark
    static QByteArray encodeZEP(const QByteArray& psdu, const QDateTime& dateTime, int channel, int lqi, quint32 sequenceNumber);

    static const char * const m_channelIdURI;
    static const char * const m_channelId;

private:
    static constexpr quint32 m_pcapLinkTypeIEEE802154WithFCS = 195; //!< PCAP link type for IEEE 802.15.4 frames including FCS

    DeviceAPI *m_deviceAPI;
    QThread m_thread;
    IEEE_802_15_4_DemodBaseband* m_basebandSink;
    IEEE_802_15_4_DemodSettings m_settings;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    qint64 m_centerFrequency;
    QUdpSocket m_udpSocket;
    quint32 m_zepSequenceNumber;
    QFile m_logFile;
    QTextStream m_logStream;
    bool m_logPCAP;
    quint64 m_framesOK;
    quint64 m_framesBad;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const IEEE_802_15_4_DemodSettings& settings, bool force = false);
    void openLog(const QString& filename);
    void logFrame(const MsgFrame& frame);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const IEEE_802_15_4_DemodSettings& settings, bool force);
    void webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const IEEE_802_15_4_DemodSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
};

#endif // INCLUDE_IEEE_802_15_4_DEMOD_H
