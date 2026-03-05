///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2016-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#ifndef INCLUDE_MESHTASTICDEMODDECODER_H
#define INCLUDE_MESHTASTICDEMODDECODER_H

#include <vector>

#include <QObject>

#include "util/messagequeue.h"
#include "meshtasticdemodsettings.h"

class MeshtasticDemodDecoder : public QObject
{
    Q_OBJECT
public:
    MeshtasticDemodDecoder();
    ~MeshtasticDemodDecoder();

    void setCodingScheme(MeshtasticDemodSettings::CodingScheme codingScheme) { m_codingScheme = codingScheme; }
    void setNbSymbolBits(unsigned int spreadFactor, unsigned int deBits);
    void setLoRaParityBits(unsigned int parityBits) { m_nbParityBits = parityBits; }
    void setLoRaHasHeader(bool hasHeader) { m_hasHeader = hasHeader; }
    void setLoRaHasCRC(bool hasCRC) { m_hasCRC = hasCRC; }
    void setLoRaPacketLength(unsigned int packetLength) { m_packetLength = packetLength; }
    void setLoRaBandwidth(unsigned int bandwidth) { m_loRaBandwidth = bandwidth; }
    void setPipelineMetadata(int pipelineId, const QString& pipelineName, const QString& pipelinePreset)
    {
        m_pipelineId = pipelineId;
        m_pipelineName = pipelineName;
        m_pipelinePreset = pipelinePreset;
    }
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setOutputMessageQueue(MessageQueue *messageQueue) { m_outputMessageQueue = messageQueue; }
    void setHeaderFeedbackMessageQueue(MessageQueue *messageQueue) { m_headerFeedbackMessageQueue = messageQueue; }

private:
    bool handleMessage(const Message& cmd);
    void decodeSymbols(const std::vector<unsigned short>& symbols, QString& str);      //!< For ASCII and TTY
    void decodeSymbols(const std::vector<unsigned short>& symbols, QByteArray& bytes); //!< For raw bytes (original LoRa)
    void decodeSymbols( //!< For FT coding scheme
        const std::vector<std::vector<float>>& mags, // vector of symbols magnitudes
        int nbSymbolBits, //!< number of bits per symbol
        std::string& msg,     //!< formatted message
        std::string& call1,   //!< 1st callsign or shorthand
        std::string& call2,   //!< 2nd callsign
        std::string& loc,     //!< locator, report or shorthand
        bool& reply       //!< true if message is a reply report
    );
    unsigned int getNbParityBits() const { return m_nbParityBits; }
    unsigned int getPacketLength() const { return m_packetLength; }
    bool getHasCRC() const { return m_hasCRC; }
    unsigned int getNbSymbols() const { return m_nbSymbols; }
    unsigned int getNbCodewords() const { return m_nbCodewords; }
    bool getEarlyEOM() const { return m_earlyEOM; }
    int getHeaderParityStatus() const { return m_headerParityStatus; }
    bool getHeaderCRCStatus() const { return m_headerCRCStatus; }
    int getPayloadParityStatus() const { return m_payloadParityStatus; }
    bool getPayloadCRCStatus() const { return m_payloadCRCStatus; }

    MeshtasticDemodSettings::CodingScheme m_codingScheme;
    unsigned int m_spreadFactor;
    unsigned int m_deBits;
    unsigned int m_nbSymbolBits;
    // LoRa attributes
    unsigned int m_nbParityBits; //!< 1 to 4 Hamming FEC bits for 4 payload bits
    bool m_hasCRC;
    bool m_hasHeader;
    unsigned int m_packetLength;
    unsigned int m_loRaBandwidth;
    unsigned int m_nbSymbols;    //!< Number of encoded symbols: this is only dependent of nbSymbolBits, nbParityBits, packetLength, hasHeader and hasCRC
    unsigned int m_nbCodewords;  //!< Number of encoded codewords: this is only dependent of nbSymbolBits, nbParityBits, packetLength, hasHeader and hasCRC
    bool m_earlyEOM;
    int m_headerParityStatus;
    bool m_headerCRCStatus;
    int m_payloadParityStatus;
    bool m_payloadCRCStatus;
    int m_pipelineId;
    QString m_pipelineName;
    QString m_pipelinePreset;
    MessageQueue m_inputMessageQueue;
    MessageQueue *m_outputMessageQueue;
    MessageQueue *m_headerFeedbackMessageQueue;

private slots:
    void handleInputMessages();
};

#endif // INCLUDE_MESHTASTICDEMODDECODER_H
