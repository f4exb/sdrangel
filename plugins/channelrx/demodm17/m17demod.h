///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef INCLUDE_M17DEMOD_H
#define INCLUDE_M17DEMOD_H

#include <vector>

#include <QNetworkRequest>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "m17demodsettings.h"
#include "m17demodbaseband.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DownChannelizer;
class ObjectPipe;

class M17Demod : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureM17Demod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const M17DemodSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureM17Demod* create(const M17DemodSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureM17Demod(settings, settingsKeys, force);
        }

    private:
        M17DemodSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureM17Demod(const M17DemodSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgReportSMS : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getSource() const { return m_source; }
        const QString& getDest() const { return m_dest; }
        const QString& getSMS() const { return m_sms; }

        static MsgReportSMS* create(const QString& source, const QString& dest, const QString& sms) {
            return new MsgReportSMS(source, dest, sms);
        }

    private:
        QString m_source;
        QString m_dest;
        QString m_sms;

        MsgReportSMS(const QString& source, const QString& dest, const QString& sms) :
            Message(),
            m_source(source),
            m_dest(dest),
            m_sms(sms)
        { }
    };

    class MsgReportAPRS : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getSource() const { return m_source; }
        const QString& getDest() const { return m_dest; }
        const QString& getFrom() const { return m_from; }
        const QString& getTo() const { return m_to; }
        const QString& getVia() const { return m_via; }
        const QString& getType() const { return m_type; }
        const QString& getPID() const { return m_pid; }
        const QString& getData() const { return m_data; }
        QByteArray& getPacket() { return m_packet; }

        static MsgReportAPRS* create(
            const QString& source,
            const QString& dest,
            const QString& from,
            const QString& to,
            const QString& via,
            const QString& type,
            const QString& pid,
            const QString& data
        )
        {
            return new MsgReportAPRS(source, dest, from, to, via, type, pid, data);
        }

    private:
        QString m_source;
        QString m_dest;
        QString m_from;
        QString m_to;
        QString m_via;
        QString m_type;
        QString m_pid;
        QString m_data;
        QByteArray m_packet;

        MsgReportAPRS(
            const QString& source,
            const QString& dest,
            const QString& from,
            const QString& to,
            const QString& via,
            const QString& type,
            const QString& pid,
            const QString& data
        ) :
            Message(),
            m_source(source),
            m_dest(dest),
            m_from(from),
            m_to(to),
            m_via(via),
            m_type(type),
            m_pid(pid),
            m_data(data)
        { }
    };

    M17Demod(DeviceAPI *deviceAPI);
	virtual ~M17Demod();
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
        const M17DemodSettings& settings);

    static void webapiUpdateChannelSettings(
            M17DemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    uint32_t getNumberOfDeviceStreams() const;
	void setScopeXYSink(BasebandSampleSink* sampleSink) { m_basebandSink->setScopeXYSink(sampleSink); }
	void configureMyPosition(float myLatitude, float myLongitude) { m_basebandSink->configureMyPosition(myLatitude, myLongitude); }
	double getMagSq() { return m_basebandSink->getMagSq(); }
	bool getSquelchOpen() const { return m_basebandSink->getSquelchOpen(); }
    void getMagSqLevels(double& avg, double& peak, int& nbSamples) { m_basebandSink->getMagSqLevels(avg, peak, nbSamples); }
    int getAudioSampleRate() const { return m_basebandSink->getAudioSampleRate(); }

    void getDiagnostics(
        bool& dcd,
        float& evm,
        float& deviation,
        float& offset,
        int& status,
        int& sync_word_type,
        float& clock,
        int& sampleIndex,
        int& syncIndex,
        int& clockIndex,
        int& viterbiCost
    ) const
    {
        m_basebandSink->getDiagnostics(
            dcd,
            evm,
            deviation,
            offset,
            status,
            sync_word_type,
            clock,
            sampleIndex,
            syncIndex,
            clockIndex,
            viterbiCost
        );
    }

    void getBERT(uint32_t& bertErrors, uint32_t& bertBits) {
        m_basebandSink->getBERT(bertErrors, bertBits);
    }

    void resetPRBS() { m_basebandSink->resetPRBS(); }

    uint32_t getLSFCount() const { return m_basebandSink->getLSFCount(); }
    const QString& getSrcCall() const { return m_basebandSink->getSrcCall(); }
    const QString& getDestcCall() const { return m_basebandSink->getDestcCall(); }
    const QString& getTypeInfo() const { return m_basebandSink->getTypeInfo(); }
    const std::array<uint8_t, 14>& getMeta() const { return m_basebandSink->getMeta(); }
    bool getHasGNSS() const { return m_basebandSink->getHasGNSS(); }
    bool getStreamElsePacket() const { return m_basebandSink->getStreamElsePacket(); }
    uint16_t getCRC() const { return m_basebandSink->getCRC(); }
    int getStdPacketProtocol() const { return m_basebandSink->getStdPacketProtocol(); }

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
	DeviceAPI *m_deviceAPI;
    QThread *m_thread;
    M17DemodBaseband *m_basebandSink;
	M17DemodSettings m_settings;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    static const int m_udpBlockSize;

	virtual bool handleMessage(const Message& cmd);
    void applySettings(const M17DemodSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void sendSampleRateToDemodAnalyzer();
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const M17DemodSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        const QList<QString>& channelSettingsKeys,
        const M17DemodSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const M17DemodSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
};

#endif // INCLUDE_M17DEMOD_H
