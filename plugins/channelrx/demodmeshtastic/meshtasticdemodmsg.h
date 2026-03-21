///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#ifndef INCLUDE_MESHTASTICDEMODMSG_H
#define INCLUDE_MESHTASTICDEMODMSG_H

#include <vector>

#include <QObject>
#include <QPair>
#include <QVector>
#include "util/message.h"

#include "meshtasticdemodsettings.h"

namespace MeshtasticDemodMsg
{
    class MsgDecodeSymbols : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const std::vector<unsigned short>& getSymbols() const { return m_symbols; }
        const std::vector<std::vector<float>>& getMagnitudes() const { return m_magnitudes; }
        const std::vector<std::vector<float>>& getDechirpedSpectrum() const { return m_dechirpedSpectrum; }
        uint32_t getFrameId() const { return m_frameId; }
        unsigned int getSyncWord() const { return m_syncWord; }
        float getSingalDb() const { return m_signalDb; }
        float getNoiseDb() const { return m_noiseDb; }

        void pushBackSymbol(unsigned short symbol) {
            m_symbols.push_back(symbol);
        }
        void popSymbol() {
            m_symbols.pop_back();
        }
        void setSyncWord(unsigned char syncWord) {
            m_syncWord = syncWord;
        }
        void setSignalDb(float db) {
            m_signalDb = db;
        }
        void setNoiseDb(float db) {
            m_noiseDb = db;
        }
        void setFrameId(uint32_t frameId) {
            m_frameId = frameId;
        }

        void pushBackMagnitudes(const std::vector<float>& magnitudes) {
            m_magnitudes.push_back(magnitudes);
        }
        void pushBackDechirpedSpectrumLine(const std::vector<float>& spectrumLine) {
            m_dechirpedSpectrum.push_back(spectrumLine);
        }
        void dropFront(unsigned int count)
        {
            const unsigned int symbolsDrop = std::min<unsigned int>(count, static_cast<unsigned int>(m_symbols.size()));
            m_symbols.erase(m_symbols.begin(), m_symbols.begin() + symbolsDrop);

            const unsigned int magnitudesDrop = std::min<unsigned int>(count, static_cast<unsigned int>(m_magnitudes.size()));
            m_magnitudes.erase(m_magnitudes.begin(), m_magnitudes.begin() + magnitudesDrop);

            const unsigned int spectrumDrop = std::min<unsigned int>(count, static_cast<unsigned int>(m_dechirpedSpectrum.size()));
            m_dechirpedSpectrum.erase(m_dechirpedSpectrum.begin(), m_dechirpedSpectrum.begin() + spectrumDrop);
        }

        static MsgDecodeSymbols* create() {
            return new MsgDecodeSymbols();
        }
        static MsgDecodeSymbols* create(const std::vector<unsigned short> symbols) {
            return new MsgDecodeSymbols(symbols);
        }

    private:
        std::vector<unsigned short> m_symbols;
        std::vector<std::vector<float>> m_magnitudes;
        std::vector<std::vector<float>> m_dechirpedSpectrum;
        uint32_t m_frameId;
        unsigned int m_syncWord;
        float m_signalDb;
        float m_noiseDb;

        MsgDecodeSymbols() : //!< create an empty message
            Message(),
            m_frameId(0),
            m_syncWord(0),
            m_signalDb(0.0),
            m_noiseDb(0.0)
        {}
        MsgDecodeSymbols(const std::vector<unsigned short> symbols) : //!< create a message with symbols copy
            Message(),
            m_frameId(0),
            m_syncWord(0),
            m_signalDb(0.0),
            m_noiseDb(0.0)
        { m_symbols = symbols; }
    };

    class MsgLoRaHeaderProbe : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        uint32_t getFrameId() const { return m_frameId; }
        const std::vector<unsigned short>& getSymbols() const { return m_symbols; }
        unsigned int getPayloadNbSymbolBits() const { return m_payloadNbSymbolBits; }
        unsigned int getHeaderNbSymbolBits() const { return m_headerNbSymbolBits; }
        unsigned int getSpreadFactor() const { return m_spreadFactor; }
        unsigned int getBandwidth() const { return m_bandwidth; }
        bool getHasHeader() const { return m_hasHeader; }
        bool getHasCRC() const { return m_hasCRC; }

        static MsgLoRaHeaderProbe* create(
            uint32_t frameId,
            const std::vector<unsigned short>& symbols,
            unsigned int payloadNbSymbolBits,
            unsigned int headerNbSymbolBits,
            unsigned int spreadFactor,
            unsigned int bandwidth,
            bool hasHeader,
            bool hasCRC
        ) {
            return new MsgLoRaHeaderProbe(
                frameId,
                symbols,
                payloadNbSymbolBits,
                headerNbSymbolBits,
                spreadFactor,
                bandwidth,
                hasHeader,
                hasCRC
            );
        }

    private:
        uint32_t m_frameId;
        std::vector<unsigned short> m_symbols;
        unsigned int m_payloadNbSymbolBits;
        unsigned int m_headerNbSymbolBits;
        unsigned int m_spreadFactor;
        unsigned int m_bandwidth;
        bool m_hasHeader;
        bool m_hasCRC;

        MsgLoRaHeaderProbe(
            uint32_t frameId,
            const std::vector<unsigned short>& symbols,
            unsigned int payloadNbSymbolBits,
            unsigned int headerNbSymbolBits,
            unsigned int spreadFactor,
            unsigned int bandwidth,
            bool hasHeader,
            bool hasCRC
        ) :
            Message(),
            m_frameId(frameId),
            m_symbols(symbols),
            m_payloadNbSymbolBits(payloadNbSymbolBits),
            m_headerNbSymbolBits(headerNbSymbolBits),
            m_spreadFactor(spreadFactor),
            m_bandwidth(bandwidth),
            m_hasHeader(hasHeader),
            m_hasCRC(hasCRC)
        {}
    };

    class MsgLoRaHeaderFeedback : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        uint32_t getFrameId() const { return m_frameId; }
        bool isValid() const { return m_valid; }
        bool getHasCRC() const { return m_hasCRC; }
        unsigned int getNbParityBits() const { return m_nbParityBits; }
        unsigned int getPacketLength() const { return m_packetLength; }
        bool getLdro() const { return m_ldro; }
        unsigned int getExpectedSymbols() const { return m_expectedSymbols; }
        int getHeaderParityStatus() const { return m_headerParityStatus; }
        bool getHeaderCRCStatus() const { return m_headerCRCStatus; }

        static MsgLoRaHeaderFeedback* create(
            uint32_t frameId,
            bool valid,
            bool hasCRC,
            unsigned int nbParityBits,
            unsigned int packetLength,
            bool ldro,
            unsigned int expectedSymbols,
            int headerParityStatus,
            bool headerCRCStatus
        ) {
            return new MsgLoRaHeaderFeedback(
                frameId,
                valid,
                hasCRC,
                nbParityBits,
                packetLength,
                ldro,
                expectedSymbols,
                headerParityStatus,
                headerCRCStatus
            );
        }

    private:
        uint32_t m_frameId;
        bool m_valid;
        bool m_hasCRC;
        unsigned int m_nbParityBits;
        unsigned int m_packetLength;
        bool m_ldro;
        unsigned int m_expectedSymbols;
        int m_headerParityStatus;
        bool m_headerCRCStatus;

        MsgLoRaHeaderFeedback(
            uint32_t frameId,
            bool valid,
            bool hasCRC,
            unsigned int nbParityBits,
            unsigned int packetLength,
            bool ldro,
            unsigned int expectedSymbols,
            int headerParityStatus,
            bool headerCRCStatus
        ) :
            Message(),
            m_frameId(frameId),
            m_valid(valid),
            m_hasCRC(hasCRC),
            m_nbParityBits(nbParityBits),
            m_packetLength(packetLength),
            m_ldro(ldro),
            m_expectedSymbols(expectedSymbols),
            m_headerParityStatus(headerParityStatus),
            m_headerCRCStatus(headerCRCStatus)
        {}
    };

    class MsgReportDecodeBytes : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QByteArray& getBytes() const { return m_bytes; }
        uint32_t getFrameId() const { return m_frameId; }
        unsigned int getSyncWord() const { return m_syncWord; }
        float getSingalDb() const { return m_signalDb; }
        float getNoiseDb() const { return m_noiseDb; }
        const QString& getMsgTimestamp() const { return m_msgTimestamp; }
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
        int getPipelineId() const { return m_pipelineId; }
        const QString& getPipelineName() const { return m_pipelineName; }
        const QString& getPipelinePreset() const { return m_pipelinePreset; }
        const std::vector<std::vector<float>>& getDechirpedSpectrum() const { return m_dechirpedSpectrum; }

        static MsgReportDecodeBytes* create(const QByteArray& bytes) {
            return new MsgReportDecodeBytes(bytes);
        }
        void setSyncWord(unsigned int syncWord) {
            m_syncWord = syncWord;
        }
        void setFrameId(uint32_t frameId) {
            m_frameId = frameId;
        }
        void setSignalDb(float db) {
            m_signalDb = db;
        }
        void setNoiseDb(float db) {
            m_noiseDb = db;
        }
        void setMsgTimestamp(const QString& ts) {
            m_msgTimestamp = ts;
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
        void setPipelineMetadata(int pipelineId, const QString& pipelineName, const QString& pipelinePreset) {
            m_pipelineId = pipelineId;
            m_pipelineName = pipelineName;
            m_pipelinePreset = pipelinePreset;
        }
        void setDechirpedSpectrum(const std::vector<std::vector<float>>& dechirpedSpectrum) {
            m_dechirpedSpectrum = dechirpedSpectrum;
        }

    private:
        QByteArray m_bytes;
        uint32_t m_frameId;
        unsigned int m_syncWord;
        float m_signalDb;
        float m_noiseDb;
        QString m_msgTimestamp;
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
        int m_pipelineId;
        QString m_pipelineName;
        QString m_pipelinePreset;
        std::vector<std::vector<float>> m_dechirpedSpectrum;

        MsgReportDecodeBytes(const QByteArray& bytes) :
            Message(),
            m_bytes(bytes),
            m_frameId(0),
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
            m_payloadParityStatus((int) MeshtasticDemodSettings::ParityUndefined),
            m_payloadCRCStatus(false),
            m_pipelineId(-1)
        { }
    };

    class MsgReportDecodeString : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getString() const { return m_str; }
        uint32_t getFrameId() const { return m_frameId; }
        unsigned int getSyncWord() const { return m_syncWord; }
        float getSingalDb() const { return m_signalDb; }
        float getNoiseDb() const { return m_noiseDb; }
        const QString& getMsgTimestamp() const { return m_msgTimestamp; }
        int getPipelineId() const { return m_pipelineId; }
        const QString& getPipelineName() const { return m_pipelineName; }
        const QString& getPipelinePreset() const { return m_pipelinePreset; }
        const QVector<QPair<QString, QString>>& getStructuredFields() const { return m_structuredFields; }
        bool hasStructuredFields() const { return !m_structuredFields.isEmpty(); }

        static MsgReportDecodeString* create(const QString& str)
        {
            return new MsgReportDecodeString(str);
        }
        void setSyncWord(unsigned int syncWord) {
            m_syncWord = syncWord;
        }
        void setFrameId(uint32_t frameId) {
            m_frameId = frameId;
        }
        void setSignalDb(float db) {
            m_signalDb = db;
        }
        void setNoiseDb(float db) {
            m_noiseDb = db;
        }
        void setMsgTimestamp(const QString& ts) {
            m_msgTimestamp = ts;
        }
        void setPipelineMetadata(int pipelineId, const QString& pipelineName, const QString& pipelinePreset) {
            m_pipelineId = pipelineId;
            m_pipelineName = pipelineName;
            m_pipelinePreset = pipelinePreset;
        }
        void setStructuredFields(const QVector<QPair<QString, QString>>& fields) {
            m_structuredFields = fields;
        }
        void addStructuredField(const QString& path, const QString& value) {
            m_structuredFields.append(qMakePair(path, value));
        }

    private:
        QString m_str;
        uint32_t m_frameId;
        unsigned int m_syncWord;
        float m_signalDb;
        float m_noiseDb;
        QString m_msgTimestamp;
        int m_pipelineId;
        QString m_pipelineName;
        QString m_pipelinePreset;
        QVector<QPair<QString, QString>> m_structuredFields;

        MsgReportDecodeString(const QString& str) :
            Message(),
            m_str(str),
            m_frameId(0),
            m_syncWord(0),
            m_signalDb(0.0),
            m_noiseDb(0.0),
            m_pipelineId(-1)
        { }
    };

    class MsgReportDecodeFT : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getMessage() const { return m_message; }
        const QString& getCall1() const { return m_call1; }
        const QString& getCall2() const { return m_call2; }
        const QString& getLoc() const { return m_loc; }
        bool isReply() const { return m_reply; }
        bool isFreeText() const { return m_freeText; }
        unsigned int getSyncWord() const { return m_syncWord; }
        float getSingalDb() const { return m_signalDb; }
        float getNoiseDb() const { return m_noiseDb; }
        const QString& getMsgTimestamp() const { return m_msgTimestamp; }
        int getPayloadParityStatus() const { return m_payloadParityStatus; }
        bool getPayloadCRCStatus() const { return m_payloadCRCStatus; }
        int getPipelineId() const { return m_pipelineId; }
        const QString& getPipelineName() const { return m_pipelineName; }
        const QString& getPipelinePreset() const { return m_pipelinePreset; }

        static MsgReportDecodeFT* create()
        {
            return new MsgReportDecodeFT();
        }
        void setMessage(const QString& message) {
            m_message = message;
        }
        void setCall1(const QString& call1) {
            m_call1 = call1;
        }
        void setCall2(const QString& call2) {
            m_call2 = call2;
        }
        void setLoc(const QString& loc) {
            m_loc = loc;
        }
        void setReply(bool reply) {
            m_reply = reply;
        }
        void setFreeText(bool freeText) {
            m_freeText = freeText;
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
        void setMsgTimestamp(const QString& ts) {
            m_msgTimestamp = ts;
        }
        void setPayloadParityStatus(int payloadParityStatus) {
            m_payloadParityStatus = payloadParityStatus;
        }
        void setPayloadCRCStatus(bool payloadCRCStatus) {
            m_payloadCRCStatus = payloadCRCStatus;
        }
        void setPipelineMetadata(int pipelineId, const QString& pipelineName, const QString& pipelinePreset) {
            m_pipelineId = pipelineId;
            m_pipelineName = pipelineName;
            m_pipelinePreset = pipelinePreset;
        }

    private:
        QString m_message;
        QString m_call1;
        QString m_call2;
        QString m_loc;
        bool m_reply;
        bool m_freeText;
        unsigned int m_syncWord;
        float m_signalDb;
        float m_noiseDb;
        QString m_msgTimestamp;
        int m_payloadParityStatus;
        bool m_payloadCRCStatus;
        int m_pipelineId;
        QString m_pipelineName;
        QString m_pipelinePreset;

        MsgReportDecodeFT() :
            Message(),
            m_reply(false),
            m_freeText(false),
            m_syncWord(0),
            m_signalDb(0.0),
            m_noiseDb(0.0),
            m_payloadParityStatus((int) MeshtasticDemodSettings::ParityUndefined),
            m_payloadCRCStatus(false),
            m_pipelineId(-1)
        { }
    };
}

#endif // INCLUDE_MESHTASTICDEMODMSG_H
