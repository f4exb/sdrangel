///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FT8DEMODWORKER_H
#define INCLUDE_FT8DEMODWORKER_H

#include <QObject>
#include <QSet>

#include "ft8.h"
#include "unpack.h"

class QDateTime;
class MessageQueue;
class MsgReportFT8Messages;
class ChannelAPI;

class FT8DemodWorker : public QObject
{
    Q_OBJECT
public:
    FT8DemodWorker();
    ~FT8DemodWorker();

    void processBuffer(int16_t *buffer, QDateTime periodTS);
    void setRecordSamples(bool recordSamples) { m_recordSamples = recordSamples; }
    void setLogMessages(bool logMessages) { m_logMessages = logMessages; }
    void setNbDecoderThreads(int nbDecoderThreads) { m_nbDecoderThreads = nbDecoderThreads; }
    void setDecoderTimeBudget(float decoderTimeBudget) { m_decoderTimeBudget = decoderTimeBudget; }
    void setUseOSD(bool useOSD) { m_useOSD = useOSD; }
    void setOSDDepth(int osdDepth) { m_osdDepth = osdDepth; }
    void setOSDLDPCThreshold(int osdLDPCThreshold) { m_osdLDPCThreshold = osdLDPCThreshold; }
    void setVerifyOSD(bool verifyOSD) { m_verifyOSD = verifyOSD; }
    void setLowFrequency(int lowFreq) { m_lowFreq = lowFreq; }
    void setHighFrequency(int highFreq) { m_highFreq = highFreq; }
    void setReportingMessageQueue(MessageQueue *messageQueue) { m_reportingMessageQueue = messageQueue; }
    void invalidateSequence() { m_invalidSequence = true; }
    void setBaseFrequency(qint64 baseFrequency) { m_baseFrequency = baseFrequency; }
    void setChannel(ChannelAPI *channel) { m_channel = channel; }

private:
    class FT8Callback : public FT8::CallbackInterface
    {
    public:
        FT8Callback
            (const QDateTime& periodTS,
            qint64 baseFrequency,
            FT8::Packing& packing,
            const QString& name
        );
        virtual int hcb(
            int *a91,
            float hz0,
            float off,
            const char *comment,
            float snr,
            int pass,
            int correct_bits
        );
        virtual QString get_name();
        const std::map<std::string, bool>& getMsgMap() { return cycle_already; }
        MsgReportFT8Messages *getReportMessage() { return m_msgReportFT8Messages; }
        void setValidCallsigns(const QSet<QString> *validCallsigns) { m_validCallsigns = validCallsigns; }

    private:
        QMutex cycle_mu;
        std::map<std::string, bool> cycle_already;
        FT8::Packing& m_packing;
        MsgReportFT8Messages *m_msgReportFT8Messages;
        const QDateTime& m_periodTS;
        qint64 m_baseFrequency;
        QString m_name;
        const QSet<QString> *m_validCallsigns;
    };

    QString m_samplesPath;
    QString m_logsPath;
    bool m_recordSamples;
    bool m_logMessages;
    int m_nbDecoderThreads;
    float m_decoderTimeBudget;
    bool m_useOSD;
    int m_osdDepth;
    int m_osdLDPCThreshold;
    bool m_verifyOSD;
    int m_lowFreq;
    int m_highFreq;
    bool m_invalidSequence;
    qint64 m_baseFrequency;
    FT8::FT8Decoder m_ft8Decoder;
    FT8::Packing m_packing;
    MessageQueue *m_reportingMessageQueue;
    ChannelAPI *m_channel;
    QSet<QString> m_validCallsigns;
};

#endif // INCLUDE_FT8DEMODWORKER_H
