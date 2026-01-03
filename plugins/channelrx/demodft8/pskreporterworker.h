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

#ifndef INCLUDE_PSKREPORTERWORKER_H
#define INCLUDE_PSKREPORTERWORKER_H

#include <QObject>
#include <QList>
#include <QSet>
#include <QQueue>
#include <QUdpSocket>

#include "util/ft8message.h"
#include "util/message.h"
#include "util/messagequeue.h"


class PskReporterWorker : public QObject
{
    Q_OBJECT
public:
    PskReporterWorker();
    ~PskReporterWorker() = default;
    MessageQueue* getInputMessageQueue() { return &m_reportQueue; }
    void setMyCallsign(const QString& callsign) { m_myCallsign = callsign; }
    void setMyLocator(const QString& locator) { m_myLocator = locator; }
    void setDecoderInfo(const QString& decoderInfo) { m_decoderInfo = decoderInfo; }
    void setTestMode(bool isTestMode) { m_isTestMode = isTestMode; }

private:
    struct decoder_results {
        char     call[13];
        char     loc[7];
        int32_t  freq;
        int32_t  snr;
    };

    QString m_myCallsign;
    QString m_myLocator;
    QString m_decoderInfo;
    bool    m_isTestMode = false;
    MessageQueue m_reportQueue;
    QQueue<FT8Message> m_ft8MessageQueue;
    QSet<QString> m_reportedCalls;
    uint32_t m_reportSequenceNumber = 1;
    QDateTime m_lastReportTime;
    QUdpSocket* m_udpSocket;
    uint32_t m_identifier;

    char txInfoData[1500];

    static const char hostname[];
    static const char service[];
    static const char test_service[];
    static const char txMode[];
    static const unsigned char rxDescriptor[];
    static const unsigned char txDescriptor[];

    void processFT8Messages(const QList<FT8Message>& ft8Messages, qint64 baseFrequency);
    void sendMessageToPskReporter(uint32_t txPtr);
    bool handleMessage(const Message& message);

    inline uint16_t SwapEndian16(uint16_t val) {
        return (val<<8) | (val>>8);
    }


    inline uint32_t SwapEndian32(uint32_t val) {
        return (val<<24) | ((val<<8) & 0x00ff0000) | ((val>>8) & 0x0000ff00) | (val>>24);
    }

private slots:
    void handleInputMessages();

};


#endif // INCLUDE_PSKREPORTERWORKER_H
