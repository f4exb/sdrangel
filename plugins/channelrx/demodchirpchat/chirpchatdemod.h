///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// (C) 2015 John Greb                                                            //
// (C) 2020 Edouard Griffiths, F4EXB                                             //
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

#ifndef INCLUDE_CHIRPCHATDEMOD_H
#define INCLUDE_CHIRPCHATDEMOD_H

#include <vector>

#include <QNetworkRequest>

#include "dsp/basebandsamplesink.h"
#include "dsp/spectrumvis.h"
#include "channel/channelapi.h"
#include "util/message.h"
#include "util/udpsinkutil.h"

#include "chirpchatdemodbaseband.h"
#include "chirpchatdemoddecoder.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class QThread;
class ObjectPipe;

class ChirpChatDemod : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureChirpChatDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const ChirpChatDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureChirpChatDemod* create(const ChirpChatDemodSettings& settings, bool force)
        {
            return new MsgConfigureChirpChatDemod(settings, force);
        }

    private:
        ChirpChatDemodSettings m_settings;
        bool m_force;

        MsgConfigureChirpChatDemod(const ChirpChatDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgReportDecodeBytes : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QByteArray& getBytes() const { return m_bytes; }
        unsigned int getSyncWord() const { return m_syncWord; }
        float getSingalDb() const { return m_signalDb; }
        float getNoiseDb() const { return m_noiseDb; }
        unsigned int getPacketSize() const { return m_packetSize; }
        unsigned int getNbParityBits() const { return m_nbParityBits; }
        unsigned int getNbSymbols() const { return m_nbSymbols; }
        unsigned int getNbCodewords() const { return m_nbCodewords; }
        bool getHasCRC() const { return m_hasCRC; }
        bool getEarlyEOM() const { return m_earlyEOM; }
        int getHeaderParityStatus() const { return m_headerParityStatus; }
        bool getHeaderCRCStatus() const { return m_headerCRCStatus; }
        int getPayloadParityStatus() const { return m_payloadParityStatus; }
        bool getPayloadCRCStatus() const { return m_payloadCRCStatus; }

        static MsgReportDecodeBytes* create(const QByteArray& bytes) {
            return new MsgReportDecodeBytes(bytes);
        }
        void setSyncWord(unsigned int syncWord) {
            m_syncWord = syncWord;
        }
        void setSignalDb(float db) {
            m_signalDb = db;
        }
        void setNoiseDb(float db) {
            m_noiseDb = db;
        }
        void setPacketSize(unsigned int packetSize) {
            m_packetSize = packetSize;
        }
        void setNbParityBits(unsigned int nbParityBits) {
            m_nbParityBits = nbParityBits;
        }
        void setNbSymbols(unsigned int nbSymbols) {
            m_nbSymbols = nbSymbols;
        }
        void setNbCodewords(unsigned int nbCodewords) {
            m_nbCodewords = nbCodewords;
        }
        void setHasCRC(bool hasCRC) {
            m_hasCRC = hasCRC;
        }
        void setEarlyEOM(bool earlyEOM) {
            m_earlyEOM = earlyEOM;
        }
        void setHeaderParityStatus(int headerParityStatus) {
            m_headerParityStatus = headerParityStatus;
        }
        void setHeaderCRCStatus(bool headerCRCStatus) {
            m_headerCRCStatus = headerCRCStatus;
        }
        void setPayloadParityStatus(int payloadParityStatus) {
            m_payloadParityStatus = payloadParityStatus;
        }
        void setPayloadCRCStatus(bool payloadCRCStatus) {
            m_payloadCRCStatus = payloadCRCStatus;
        }

    private:
        QByteArray m_bytes;
        unsigned int m_syncWord;
        float m_signalDb;
        float m_noiseDb;
        unsigned int m_packetSize;
        unsigned int m_nbParityBits;
        unsigned int m_nbSymbols;
        unsigned int m_nbCodewords;
        bool m_hasCRC;
        bool m_earlyEOM;
        int m_headerParityStatus;
        bool m_headerCRCStatus;
        int m_payloadParityStatus;
        bool m_payloadCRCStatus;

        MsgReportDecodeBytes(const QByteArray& bytes) :
            Message(),
            m_bytes(bytes),
            m_syncWord(0),
            m_signalDb(0.0),
            m_noiseDb(0.0),
            m_packetSize(0),
            m_nbParityBits(0),
            m_nbSymbols(0),
            m_nbCodewords(0),
            m_hasCRC(false),
            m_earlyEOM(false),
            m_headerParityStatus(false),
            m_headerCRCStatus(false),
            m_payloadParityStatus(false),
            m_payloadCRCStatus(false)
        { }
    };

    class MsgReportDecodeString : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getString() const { return m_str; }
        unsigned int getSyncWord() const { return m_syncWord; }
        float getSingalDb() const { return m_signalDb; }
        float getNoiseDb() const { return m_noiseDb; }

        static MsgReportDecodeString* create(const QString& str)
        {
            return new MsgReportDecodeString(str);
        }
        void setSyncWord(unsigned int syncWord) {
            m_syncWord = syncWord;
        }
        void setSignalDb(float db) {
            m_signalDb = db;
        }
        void setNoiseDb(float db) {
            m_noiseDb = db;
        }

    private:
        QString m_str;
        unsigned int m_syncWord;
        float m_signalDb;
        float m_noiseDb;

        MsgReportDecodeString(const QString& str) :
            Message(),
            m_str(str),
            m_syncWord(0),
            m_signalDb(0.0),
            m_noiseDb(0.0)
        { }
    };

	ChirpChatDemod(DeviceAPI* deviceAPI);
	virtual ~ChirpChatDemod();
	virtual void destroy() { delete this; }
    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }
    SpectrumVis *getSpectrumVis() { return &m_spectrumVis; }

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
        const ChirpChatDemodSettings& settings);

    static void webapiUpdateChannelSettings(
            ChirpChatDemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    bool getDemodActive() const;
    double getCurrentNoiseLevel() const;
    double getTotalPower() const;

    uint32_t getNumberOfDeviceStreams() const;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
	DeviceAPI *m_deviceAPI;
    QThread *m_thread;
    ChirpChatDemodBaseband* m_basebandSink;
    ChirpChatDemodDecoder m_decoder;
    ChirpChatDemodSettings m_settings;
    SpectrumVis m_spectrumVis;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    float m_lastMsgSignalDb;
    float m_lastMsgNoiseDb;
    int m_lastMsgSyncWord;
    int m_lastMsgPacketLength;
    int m_lastMsgNbParityBits;
    bool m_lastMsgHasCRC;
    int m_lastMsgNbSymbols;
    int m_lastMsgNbCodewords;
    bool m_lastMsgEarlyEOM;
    bool m_lastMsgHeaderCRC;
    int m_lastMsgHeaderParityStatus;
    bool m_lastMsgPayloadCRC;
    int m_lastMsgPayloadParityStatus;
    QString m_lastMsgTimestamp;
    QString m_lastMsgString;
    QByteArray m_lastMsgBytes;
    UDPSinkUtil<uint8_t> m_udpSink;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

	virtual bool handleMessage(const Message& cmd);
    void applySettings(const ChirpChatDemodSettings& settings, bool force = false);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const ChirpChatDemodSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        QList<QString>& channelSettingsKeys,
        const ChirpChatDemodSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const ChirpChatDemodSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
};

#endif // INCLUDE_CHIRPCHATDEMOD_H
