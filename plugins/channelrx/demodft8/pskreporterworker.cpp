///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Edouard Griffiths, F4EXB <f4exb06@gmail.com>                   //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////

#include <QHostInfo>
#include <QRandomGenerator>
#include "util/ft8message.h"
#include "pskreporterworker.h"

const char PskReporterWorker::hostname[] = "report.pskreporter.info";
const char PskReporterWorker::service[] = "4739";
const char PskReporterWorker::test_service[] = "14739";
const char PskReporterWorker::txMode[] = "FT8";
const unsigned char PskReporterWorker::rxDescriptor[] = {
        0x00, 0x03,              // Template Set ID
        0x00, 0x24,              // Length
        0x99, 0x92,              // Link ID
        0x00, 0x03,              // Field Count
        0x00, 0x00,              // Scope Field Count
        0x80, 0x02,              // Receiver Callsign ID
        0xFF, 0xFF,              // Variable field length
        0x00, 0x00, 0x76, 0x8F,  // Enterprise number
        0x80, 0x04,              // Receiver Locator ID
        0xFF, 0xFF,              // Variable field length
        0x00, 0x00, 0x76, 0x8F,  // Enterprise number
        0x80, 0x08,              // Receiver Decoder Software ID
        0xFF, 0xFF,              // Variable field length
        0x00, 0x00, 0x76, 0x8F,  // Enterprise number
        0x00, 0x00               // Padding
}; // "PSKREPORTER_RX"
const unsigned char PskReporterWorker::txDescriptor[] = {
        0x00, 0x02,              // Template Set ID
        0x00, 0x3C,              // Length
        0x99, 0x93,              // Link ID
        0x00, 0x07,              // Field Count
        0x80, 0x01,              // Sender Callsign ID
        0xFF, 0xFF,              // Variable field length
        0x00, 0x00, 0x76, 0x8F,  // Enterprise number
        0x80, 0x05,              // Sender Frequency ID
        0x00, 0x04,              // Fixed length (4)
        0x00, 0x00, 0x76, 0x8F,  // Enterprise number
        0x80, 0x06,              // Sender SNR ID
        0x00, 0x01,              // Fixed length (1)
        0x00, 0x00, 0x76, 0x8F,  // Enterprise number
        0x80, 0x0A,              // Sender Mode ID
        0xFF, 0xFF,              // Variable field length
        0x00, 0x00, 0x76, 0x8F,  // Enterprise number
        0x80, 0x03,              // Sender Locator ID
        0xFF, 0xFF,              // Variable field length
        0x00, 0x00, 0x76, 0x8F,  // Enterprise number
        0x80, 0x0B,              // Information Source ID
        0x00, 0x01,              // Fixed length (1)
        0x00, 0x00, 0x76, 0x8F,  // Enterprise number
        0x00, 0x96,              // DateTimeSeconds ID
        0x00, 0x04               // Field Length
}; // "PSKREPORTER_TX"

PskReporterWorker::PskReporterWorker() :
    m_udpSocket(new QUdpSocket(this))
{
    connect(&m_reportQueue, &MessageQueue::messageEnqueued,
            this, &PskReporterWorker::handleInputMessages);
    m_lastReportTime = QDateTime::currentDateTimeUtc();
    m_identifier = QRandomGenerator::global()->generate(); // random number for the identifier for this session
}

void PskReporterWorker::processFT8Messages(const QList<FT8Message>& ft8Messages, qint64 baseFrequency)
{
    m_ft8MessageQueue.append(ft8Messages); // Queue messages for processing
    qDebug("PskReporterWorker::processFT8Messages: queued %d messages", ft8Messages.size());

    // Avoid reporting too frequently - 5 minutes interval is the recommended value
    if (m_lastReportTime.secsTo(QDateTime::currentDateTimeUtc()) < 5*60) {
        return;
    }

    m_lastReportTime = QDateTime::currentDateTimeUtc();
    uint32_t txPtr = 4;
    std::fill(std::begin(txInfoData), std::end(txInfoData), 0);

    while (!m_ft8MessageQueue.isEmpty())
    {
        FT8Message msg = m_ft8MessageQueue.dequeue();

        if (m_reportedCalls.contains(msg.call2)) {
            continue;
        }

        m_reportedCalls.insert(msg.call2);

        if (txPtr > 1200)
        {
            sendMessageToPskReporter(txPtr);
            txPtr = 4;
            std::fill(std::begin(txInfoData), std::end(txInfoData), 0);
        }

        // Station callsign
        *(uint8_t  *)&txInfoData[txPtr] = (uint8_t) msg.call2.size();
        txPtr += 1;
        memcpy(&txInfoData[txPtr], msg.call2.toStdString().c_str(), msg.call2.size());
        txPtr += msg.call2.size();

        // Station frequency
        uint32_t freq = static_cast<uint32_t>(baseFrequency + msg.df);
        freq = SwapEndian32(freq);
        memcpy(&txInfoData[txPtr], &freq, 4);
        txPtr +=4;

        // Station SNR
        int8_t snr = static_cast<int8_t>(msg.snr);
        memcpy(&txInfoData[txPtr], &snr, 1);
        txPtr +=1;

        // Station Mode
        *(uint8_t  *)&txInfoData[txPtr] = (uint8_t)strlen(txMode);
        txPtr += 1;
        strncpy((char *)&txInfoData[txPtr], txMode, strlen(txMode));
        txPtr += strlen(txMode);

        // Station locator
        *(uint8_t  *)&txInfoData[txPtr] = (uint8_t) msg.loc.size();
        txPtr += 1;
        memcpy(&txInfoData[txPtr], msg.loc.toStdString().c_str(), msg.loc.size());
        txPtr += msg.loc.size();

        /* Station Info -- Static length (1) */
        *(uint8_t  *)&txInfoData[txPtr] = (uint8_t)1;
        txPtr += 1;

        // Message timestamp -- Static length (4)
        uint32_t timestamp = static_cast<uint32_t>(msg.ts.toSecsSinceEpoch());
        timestamp = SwapEndian32(timestamp);
        memcpy(&txInfoData[txPtr], &timestamp, 4);
        txPtr +=4;
    }

    sendMessageToPskReporter(txPtr);
    m_reportedCalls.clear();
    m_reportSequenceNumber++;
}

void PskReporterWorker::sendMessageToPskReporter(uint32_t txPtr)
{
    // Implement PSK Reporter message sending here
    if (txInfoData[4] == 0) {
        return; // No data to send
    }

    // Prepare and send the UDP message to PSK Reporter server
    const uint32_t headerSize = 16;
    char headerData[headerSize] = {0};
    uint32_t hPtr = 0;

    *(uint16_t *)&headerData[hPtr] = SwapEndian16(0x000A);
    hPtr += 2;
    hPtr += 2;  // Skip the size block, adjust later

    uint32_t timestamp = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    *(uint32_t *)&headerData[hPtr] = SwapEndian32(timestamp);
    hPtr += 4;

    *(uint32_t *)&headerData[hPtr] = SwapEndian32(m_reportSequenceNumber);
    hPtr += 4;

    *(uint32_t *)&headerData[hPtr] = SwapEndian32(m_identifier);
    hPtr += 4;

    char rxInfoData[256] = {0};
    uint32_t rxPtr = 0;

    *(uint16_t *)&rxInfoData[rxPtr] = SwapEndian16(0x9992);
    rxPtr += 2;
    rxPtr += 2;  // Skip the size block, adjust later

    // Receiver callsign
    *(uint8_t  *)&rxInfoData[rxPtr] = (uint8_t) m_myCallsign.size();
    rxPtr += 1;
    memcpy(&rxInfoData[rxPtr], m_myCallsign.toStdString().c_str(), m_myCallsign.size());
    rxPtr += m_myCallsign.size();

    // Receiver locator
    *(uint8_t  *)&rxInfoData[rxPtr] = (uint8_t) m_myLocator.size();
    rxPtr += 1;
    memcpy(&rxInfoData[rxPtr], m_myLocator.toStdString().c_str(), m_myLocator.size());
    rxPtr += m_myLocator.size();

    // Receiver decoder software
    *(uint8_t  *)&rxInfoData[rxPtr] = (uint8_t) m_decoderInfo.size();
    rxPtr += 1;
    memcpy(&rxInfoData[rxPtr], m_decoderInfo.toStdString().c_str(), m_decoderInfo.size());
    rxPtr += m_decoderInfo.size();

    // Padding to 4-byte boundary
    if ((rxPtr % 4) > 0)
        rxPtr += (4 - (rxPtr % 4));

    // Padding to 4-byte boundary
    if ((txPtr % 4) > 0)
        txPtr += (4 - (txPtr % 4));

    *(uint16_t *)&txInfoData[0] = SwapEndian16(0x9993);

    /* Adjust the block sizes */
    uint32_t fullBlockSize  = headerSize + sizeof(rxDescriptor) + sizeof(txDescriptor) + rxPtr + txPtr;
    *(uint16_t *)&rxInfoData[2] = SwapEndian16(rxPtr);
    *(uint16_t *)&txInfoData[2] = SwapEndian16(txPtr);
    *(uint16_t *)&headerData[2] = SwapEndian16(fullBlockSize);

    /* Assemble the block to send over UDP */
    char *fullBlockData = new char[fullBlockSize];
    uint32_t ptrBlock = 0;
    memcpy(&fullBlockData[ptrBlock], headerData,   headerSize);           ptrBlock += headerSize;
    memcpy(&fullBlockData[ptrBlock], rxDescriptor, sizeof(rxDescriptor)); ptrBlock += sizeof(rxDescriptor);
    memcpy(&fullBlockData[ptrBlock], txDescriptor, sizeof(txDescriptor)); ptrBlock += sizeof(txDescriptor);
    memcpy(&fullBlockData[ptrBlock], rxInfoData,   rxPtr);                ptrBlock += rxPtr;
    memcpy(&fullBlockData[ptrBlock], txInfoData,   txPtr);                ptrBlock += txPtr;

    /* Send via UDP */
    const char* servicePort = m_isTestMode ? test_service : service;
    QHostAddress hostAddress;

    // Resolve hostname
    QHostInfo hostInfo = QHostInfo::fromName(hostname);

    if (!hostInfo.addresses().isEmpty()) {
        hostAddress = hostInfo.addresses().first();

        // Send the datagram
        qint64 bytesSent = m_udpSocket->writeDatagram(fullBlockData, fullBlockSize, hostAddress, QString(servicePort).toUInt());

        if (bytesSent == -1) {
            qWarning("PskReporterWorker::sendMessageToPskReporter: Failed to send UDP datagram: %s",
                     qPrintable(m_udpSocket->errorString()));
        } else {
            qDebug("PskReporterWorker::sendMessageToPskReporter: Sent %lld bytes to %s:%s",
                   bytesSent, hostname, servicePort);
        }
    } else {
        qWarning("PskReporterWorker::sendMessageToPskReporter: Failed to resolve hostname: %s", hostname);
    }

    delete[] fullBlockData;
}

bool PskReporterWorker::handleMessage(const Message& message)
{
    if (MsgReportFT8Messages::match(message))
    {
        const MsgReportFT8Messages& ft8Msg = static_cast<const MsgReportFT8Messages&>(message);
        const QList<FT8Message>& ft8Messages = ft8Msg.getFT8Messages();
        qint64 baseFrequency = ft8Msg.getBaseFrequency();
        // Process FT8 messages for PSK Reporter here
        processFT8Messages(ft8Messages, baseFrequency);

        return true;
    }

    return false;
}

void PskReporterWorker::handleInputMessages()
{
    Message* message = m_reportQueue.pop();

    if (message == nullptr) {
        return;
    }

    if (!handleMessage(*message)) {
        delete message;
    }
}
