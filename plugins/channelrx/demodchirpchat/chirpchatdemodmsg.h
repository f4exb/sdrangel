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

#ifndef INCLUDE_CHIRPCHATDEMODMSG_H
#define INCLUDE_CHIRPCHATDEMODMSG_H

#include <QObject>
#include "util/message.h"

#include "chirpchatdemodsettings.h"

namespace ChirpChatDemodMsg
{
    class MsgDecodeSymbols : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const std::vector<unsigned short>& getSymbols() const { return m_symbols; }
        const std::vector<std::vector<float>>& getMagnitudes() const { return m_magnitudes; }
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

        void pushBackMagnitudes(const std::vector<float>& magnitudes) {
            m_magnitudes.push_back(magnitudes);
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
        unsigned int m_syncWord;
        float m_signalDb;
        float m_noiseDb;

        MsgDecodeSymbols() : //!< create an empty message
            Message(),
            m_syncWord(0),
            m_signalDb(0.0),
            m_noiseDb(0.0)
        {}
        MsgDecodeSymbols(const std::vector<unsigned short> symbols) : //!< create a message with symbols copy
            Message(),
            m_syncWord(0),
            m_signalDb(0.0),
            m_noiseDb(0.0)
        { m_symbols = symbols; }
    };

    class MsgReportDecodeBytes : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QByteArray& getBytes() const { return m_bytes; }
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

    private:
        QByteArray m_bytes;
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
            m_payloadParityStatus((int) ChirpChatDemodSettings::ParityUndefined),
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
        const QString& getMsgTimestamp() const { return m_msgTimestamp; }

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
        void setMsgTimestamp(const QString& ts) {
            m_msgTimestamp = ts;
        }

    private:
        QString m_str;
        unsigned int m_syncWord;
        float m_signalDb;
        float m_noiseDb;
        QString m_msgTimestamp;

        MsgReportDecodeString(const QString& str) :
            Message(),
            m_str(str),
            m_syncWord(0),
            m_signalDb(0.0),
            m_noiseDb(0.0)
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

        MsgReportDecodeFT() :
            Message(),
            m_reply(false),
            m_freeText(false),
            m_syncWord(0),
            m_signalDb(0.0),
            m_noiseDb(0.0),
            m_payloadParityStatus((int) ChirpChatDemodSettings::ParityUndefined),
            m_payloadCRCStatus(false)
        { }
    };
}

#endif // INCLUDE_CHIRPCHATDEMODMSG_H
