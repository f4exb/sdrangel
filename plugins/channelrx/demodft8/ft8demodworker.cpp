///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019, 2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
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

#include <QStandardPaths>
#include <QDir>
#include <QDateTime>

#include <array>
#include <algorithm>
#include <cmath>

#include "channel/channelapi.h"
#include "dsp/wavfilerecord.h"
#include "util/messagequeue.h"
#include "util/ft8message.h"
#include "util/maidenhead.h"
#include "maincore.h"
#include "SWGMapItem.h"

#include "ft8demodsettings.h"
#include "ft8demodworker.h"
#include "libldpc.h"
#include "osd.h"

namespace {

// 87 data symbols, 4 sync blocks of 4 symbols each, total 103 symbols per frame
constexpr int kFT4SymbolSamples = 576; // Number of samples per symbol at 12 kHz sample rate
constexpr int kFT4TotalSymbols = 103; // Number of symbols in a complete FT4 frame (including sync and data symbols and excluding the 2 ramp symbols)
constexpr int kFT4DataSymbols = 87; // Number of data symbols in a FT4 frame
constexpr int kFT4FrameSamples = kFT4TotalSymbols * kFT4SymbolSamples; // Total number of samples in a FT4 frame
const float kFT4ToneSpacing = FT8DemodSettings::m_ft8SampleRate / static_cast<float>(kFT4SymbolSamples); // Tone spacing in Hz

// Costas arrays for FT4 sync detection, indexed by block and symbol index within block
const std::array<std::array<int, 4>, 4> kFT4SyncTones = {{
    {{0, 1, 3, 2}},
    {{1, 0, 2, 3}},
    {{2, 3, 1, 0}},
    {{3, 2, 0, 1}}
}};

const std::array<int, 77> kFT4Rvec = {{
    0,1,0,0,1,0,1,0,0,1,0,1,1,1,1,0,1,0,0,0,1,0,0,1,1,0,1,1,0,
    1,0,0,1,0,1,1,0,0,0,0,1,0,0,0,1,0,1,0,0,1,1,1,1,0,0,1,0,1,
    0,1,0,1,0,1,1,0,1,1,1,1,1,0,0,0,1,0,1
}};

struct FT4Candidate {
    int start;
    float tone0;
    float sync;
    float noise;
    float score;
};

float goertzelPower(const int16_t *samples, int sampleCount, float sampleRate, float frequency)
{
    const float omega = 2.0f * static_cast<float>(M_PI) * frequency / sampleRate;
    const float coeff = 2.0f * std::cos(omega);
    float q0 = 0.0f;
    float q1 = 0.0f;
    float q2 = 0.0f;

    for (int i = 0; i < sampleCount; i++)
    {
        q0 = coeff * q1 - q2 + (samples[i] / 32768.0f);
        q2 = q1;
        q1 = q0;
    }

    return q1 * q1 + q2 * q2 - coeff * q1 * q2;
}

float symbolTonePower(const int16_t *buffer, int start, int symbolIndex, float tone0, int tone)
{
    const int symbolStart = start + symbolIndex * kFT4SymbolSamples;
    const float frequency = tone0 + tone * kFT4ToneSpacing;
    return goertzelPower(&buffer[symbolStart], kFT4SymbolSamples, FT8DemodSettings::m_ft8SampleRate, frequency);
}

void collectFT4SyncMetrics(const int16_t *buffer, int start, float tone0, float& sync, float& noise)
{
    sync = 0.0f;
    noise = 0.0f;

    for (int block = 0; block < 4; block++)
    {
        const int symbolOffset = block * 33;

        for (int index = 0; index < 4; index++)
        {
            const int symbolIndex = symbolOffset + index;
            const int expectedTone = kFT4SyncTones[block][index];

            for (int tone = 0; tone < 4; tone++)
            {
                const float power = symbolTonePower(buffer, start, symbolIndex, tone0, tone);

                if (tone == expectedTone) {
                    sync += power;
                } else {
                    noise += power;
                }
            }
        }
    }
}

void buildFT4BitMetrics(const int16_t *buffer, int start, float tone0, std::array<float, 174>& bitMetrics)
{
    int bitIndex = 0;

    for (int symbolIndex = 0; symbolIndex < kFT4TotalSymbols; symbolIndex++)
    {
        const bool inSync = (symbolIndex < 4)
                         || (symbolIndex >= 33 && symbolIndex < 37)
                         || (symbolIndex >= 66 && symbolIndex < 70)
                         || (symbolIndex >= 99);

        if (inSync) {
            continue;
        }

        std::array<float, 4> tonePower;

        for (int tone = 0; tone < 4; tone++) {
            tonePower[tone] = symbolTonePower(buffer, start, symbolIndex, tone0, tone);
        }

        const float b0Zero = std::max(tonePower[0], tonePower[1]);
        const float b0One = std::max(tonePower[2], tonePower[3]);
        const float b1Zero = std::max(tonePower[0], tonePower[3]);
        const float b1One = std::max(tonePower[1], tonePower[2]);

        bitMetrics[bitIndex++] = b0Zero - b0One;
        bitMetrics[bitIndex++] = b1Zero - b1One;
    }
}

} // namespace

FT8DemodWorker::FT8Callback::FT8Callback(
    const QDateTime& periodTS,
    qint64 baseFrequency,
    FT8::Packing& packing,
    const QString& name
) :
    m_packing(packing),
    m_periodTS(periodTS),
    m_name(name),
    m_validCallsigns(nullptr)
{
    m_msgReportFT8Messages = MsgReportFT8Messages::create();
    m_msgReportFT8Messages->setBaseFrequency(baseFrequency);
}

int FT8DemodWorker::FT8Callback::hcb(
    int *a91,
    float hz0,
    float off,
    const char *comment,
    float snr,
    int pass,
    int correct_bits
)
{
    std::string call1;
    std::string call2;
    std::string loc;
    std::string type;
    std::string msg = m_packing.unpack(a91, call1, call2, loc, type);

    cycle_mu.lock();

    if (cycle_already.count(msg) > 0)
    {
        // already decoded this message on this cycle
        cycle_mu.unlock();
        return 1; // 1 => already seen, don't subtract.
    }

    cycle_already[msg] = true;
    QString call2Str(call2.c_str());
    QString info(comment);

    if (m_validCallsigns && info.startsWith("OSD") && !m_validCallsigns->contains(call2Str))
    {
        cycle_mu.unlock();
        return 2; // leave without reporting. Set as new decode.
    }

    QList<FT8Message>& ft8Messages = m_msgReportFT8Messages->getFT8Messages();
    FT8Message baseMessage{
        m_periodTS,
        QString(type.c_str()),
        pass,
        (int) snr,
        correct_bits,
        off - 0.5f,
        hz0,
        QString(call1.c_str()).simplified(),
        call2Str,
        QString(loc.c_str()).simplified(),
        info
    };

    // DXpedition packs two messages in one with the two callees in the first call area separated by a semicolon
    if (type == "0.1")
    {
        QStringList callees = QString(call1.c_str()).simplified().split(";");

        for (int i = 0; i < 2; i++)
        {
            baseMessage.call1 = callees[i];

            if (i == 0) { // first is with RR73 greetings in locator area
                baseMessage.loc = "RR73";
            }

            ft8Messages.push_back(baseMessage);
        }
    }
    else
    {
        ft8Messages.push_back(baseMessage);
    }

    cycle_mu.unlock();

    return 2; // 2 => new decode, do subtract.
}

QString FT8DemodWorker::FT8Callback::get_name()
{
    return m_name;
}

FT8DemodWorker::FT8DemodWorker() :
    m_recordSamples(false),
    m_logMessages(false),
    m_enablePskReporter(false),
    m_nbDecoderThreads(6),
    m_decoderTimeBudget(0.5),
    m_decoderMode(FT8DemodSettings::DecoderModeFT8),
    m_useOSD(false),
    m_osdDepth(0),
    m_osdLDPCThreshold(70),
    m_verifyOSD(false),
    m_lowFreq(200),
    m_highFreq(3000),
    m_unsupportedModeWarningPending(true),
    m_invalidSequence(true),
    m_baseFrequency(0),
    m_guiReportingMessageQueue(nullptr),
    m_pskReportingMessageQueue(nullptr),
    m_channel(nullptr)
{
    QString relPath = "ft8/save";
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    dir.mkpath(relPath);
    m_samplesPath = dir.absolutePath() + "/" + relPath;
    qDebug("FT8DemodWorker::FT8DemodWorker: samples path: %s", qPrintable(m_samplesPath));
    relPath = "ft8/logs";
    m_logsPath = dir.absolutePath() + "/" + relPath;
    qDebug("FT8DemodWorker::FT8DemodWorker: logs path: %s", qPrintable(m_logsPath));

    QDir samplesDir(m_samplesPath);
    if (!samplesDir.exists()) {
        samplesDir.mkpath(".");
    }

    QDir logsDir(m_logsPath);
    if (!logsDir.exists()) {
        logsDir.mkpath(".");
    }
}

FT8DemodWorker::~FT8DemodWorker()
{}

void FT8DemodWorker::setDecoderMode(int decoderMode)
{
    m_decoderMode = decoderMode;
    m_unsupportedModeWarningPending = true;
}

bool FT8DemodWorker::processFT4Experimental(int16_t *buffer, int frameSampleCount, FT8Callback& ft8Callback)
{
    std::vector<float> samples(frameSampleCount);
    std::transform(
        buffer,
        buffer + frameSampleCount,
        samples.begin(),
        [](const int16_t& s) -> float { return s / 32768.0f; }
    );

    int hints[2] = { 2, 0 }; // CQ
    const int previousMessageCount = ft8Callback.getReportMessage()->getFT8Messages().size();
    m_ft4Decoder.getParams().nthreads = m_nbDecoderThreads;
    m_ft4Decoder.getParams().ldpc_iters = m_ft8Decoder.getParams().ldpc_iters;
    m_ft4Decoder.getParams().use_osd = m_useOSD ? 1 : 0;
    m_ft4Decoder.getParams().osd_depth = m_osdDepth;
    m_ft4Decoder.getParams().osd_ldpc_thresh = m_osdLDPCThreshold;
    m_ft4Decoder.entry(
        samples.data(),
        samples.size(),
        0.5 * FT8DemodSettings::m_ft8SampleRate,
        FT8DemodSettings::m_ft8SampleRate,
        m_lowFreq,
        m_highFreq,
        hints,
        hints,
        m_decoderTimeBudget,
        m_decoderTimeBudget,
        &ft8Callback,
        0,
        (struct FT8::cdecode *) nullptr
    );
    m_ft4Decoder.wait(m_decoderTimeBudget + 1.0);
    const int currentMessageCount = ft8Callback.getReportMessage()->getFT8Messages().size();
    return currentMessageCount > previousMessageCount;
}

void FT8DemodWorker::processBuffer(int16_t *buffer, QDateTime periodTS)
{
    const int frameSampleCount = FT8DemodSettings::getDecoderFrameSamples(m_decoderMode);
    const QString decoderMode = FT8DemodSettings::getDecoderModeString(m_decoderMode);

    qDebug("FT8DemodWorker::processBuffer: %6.3f %s mode:%s %d:%f [%d:%d]",
        m_baseFrequency / 1000000.0,
        qPrintable(periodTS.toString("yyyy-MM-dd HH:mm:ss")),
        qPrintable(decoderMode),
        m_nbDecoderThreads,
        m_decoderTimeBudget,
        m_lowFreq,
        m_highFreq
    );

    if (m_invalidSequence)
    {
        qDebug("FT8DemodWorker::processBuffer: invalid sequence");
        m_invalidSequence = false;
        return;
    }

    QString channelReference = "d0c0"; // default

    if (m_channel) {
        channelReference = tr("d%1c%2").arg(m_channel->getDeviceSetIndex()).arg(m_channel->getIndexInDeviceSet());
    }

    int hints[2] = { 2, 0 }; // CQ
    FT8Callback ft8Callback(periodTS, m_baseFrequency, m_packing, channelReference);
    ft8Callback.setValidCallsigns((m_useOSD && m_verifyOSD) ? &m_validCallsigns : nullptr);

    if (m_decoderMode == FT8DemodSettings::DecoderModeFT8)
    {
        m_ft8Decoder.getParams().nthreads = m_nbDecoderThreads;
        m_ft8Decoder.getParams().use_osd = m_useOSD ? 1 : 0;
        m_ft8Decoder.getParams().osd_depth = m_osdDepth;
        m_ft8Decoder.getParams().osd_ldpc_thresh = m_osdLDPCThreshold;
        std::vector<float> samples(frameSampleCount);

        std::transform(
            buffer,
            buffer + frameSampleCount,
            samples.begin(),
            [](const int16_t& s) -> float { return s / 32768.0f; }
        );

        m_ft8Decoder.entry(
            samples.data(),
            samples.size(),
            0.5 * FT8DemodSettings::m_ft8SampleRate,
            FT8DemodSettings::m_ft8SampleRate,
            m_lowFreq,
            m_highFreq,
            hints,
            hints,
            m_decoderTimeBudget,
            m_decoderTimeBudget,
            &ft8Callback,
            0,
            (struct FT8::cdecode *) nullptr
        );

        m_ft8Decoder.wait(m_decoderTimeBudget + 1.0); // add one second to budget to force quit threads
    }
    else if (m_decoderMode == FT8DemodSettings::DecoderModeFT4)
    {
        const bool decoded = processFT4Experimental(buffer, frameSampleCount, ft8Callback);

        if (!decoded && m_unsupportedModeWarningPending)
        {
            qWarning("FT8DemodWorker::processBuffer: FT4 experimental decoder did not find valid messages");
            m_unsupportedModeWarningPending = false;
        }
    }
    else
    {
        if (m_unsupportedModeWarningPending)
        {
            qWarning("FT8DemodWorker::processBuffer: %s decoding is not supported", qPrintable(decoderMode));
            m_unsupportedModeWarningPending = false;
        }
    }

    qDebug("FT8DemodWorker::processBuffer: done: at %6.3f %d messages",
        m_baseFrequency / 1000000.0, (int)ft8Callback.getReportMessage()->getFT8Messages().size());

    if (m_guiReportingMessageQueue) {
        m_guiReportingMessageQueue->push(new MsgReportFT8Messages(*ft8Callback.getReportMessage()));
    }

    if (m_enablePskReporter && m_pskReportingMessageQueue) {
        m_pskReportingMessageQueue->push(new MsgReportFT8Messages(*ft8Callback.getReportMessage()));
    }

    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes((const QObject*) m_channel, "mapitems", mapPipes);
    const QList<FT8Message>& ft8Messages = ft8Callback.getReportMessage()->getFT8Messages();
    std::ofstream logFile;
    double baseFrequencyMHz = m_baseFrequency/1000000.0;

    for (const auto& ft8Message : ft8Messages)
    {
        if (m_logMessages)
        {
            if (!logFile.is_open())
            {
                QString logFileName(tr("%1_%2.txt").arg(periodTS.toString("yyyyMMdd")).arg(channelReference));
                QFileInfo lfi(QDir(m_logsPath), logFileName);
                QString logFilePath = lfi.absoluteFilePath();

                if (lfi.exists()) {
                    logFile.open(logFilePath.toStdString(), std::ios::app);
                } else {
                    logFile.open(logFilePath.toStdString());
                }
            }

            if (ft8Message.call1 == "UNK") {
                continue;
            }

            QString logMessage = QString("%1 %2 Rx %3 %4 %5 %6 %7 %8 %9")
                .arg(periodTS.toString("yyyyMMdd_HHmmss"))
                .arg(baseFrequencyMHz, 9, 'f', 3)
                .arg(decoderMode)
                .arg(ft8Message.snr, 6)
                .arg(ft8Message.dt, 4, 'f', 1)
                .arg(ft8Message.df, 4, 'f', 0)
                .arg(ft8Message.call1)
                .arg(ft8Message.call2)
                .arg(ft8Message.loc);
            logMessage.remove(0, 2);
            logFile << logMessage.toStdString() << std::endl;
        }
        else
        {
            if (logFile.is_open()) {
                logFile.close();
            }
        }

        if (mapPipes.size() > 0)
        {
            // If message contains a Maidenhead locator, display caller on Map feature
            float latitude, longitude;

            if ((ft8Message.loc.size() == 4) && (ft8Message.loc != "RR73") && Maidenhead::fromMaidenhead(ft8Message.loc, latitude, longitude))
            {
                QString text = QString("%1\nMode: %2\nFrequency: %3 Hz\nLocator: %4\nSNR: %5\nLast heard: %6")
                                    .arg(ft8Message.call2)
                                    .arg(decoderMode)
                                    .arg(baseFrequencyMHz*1000000 + ft8Message.df)
                                    .arg(ft8Message.loc)
                                    .arg(ft8Message.snr)
                                    .arg(periodTS.toString("dd MMM yyyy HH:mm:ss"));

                for (const auto& pipe : mapPipes)
                {
                    MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
                    SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
                    swgMapItem->setName(new QString(ft8Message.call2));
                    swgMapItem->setLatitude(latitude);
                    swgMapItem->setLongitude(longitude);
                    swgMapItem->setAltitude(0);
                    swgMapItem->setAltitudeReference(1); // CLAMP_TO_GROUND
                    swgMapItem->setPositionDateTime(new QString(QDateTime::currentDateTime().toString(Qt::ISODateWithMs)));
                    swgMapItem->setImageRotation(0);
                    swgMapItem->setText(new QString(text));
                    swgMapItem->setImage(new QString("antenna.png"));
                    swgMapItem->setModel(new QString("antenna.glb"));
                    swgMapItem->setModelAltitudeOffset(0.0);
                    swgMapItem->setLabel(new QString(ft8Message.call2));
                    swgMapItem->setLabelAltitudeOffset(4.5);
                    swgMapItem->setFixedPosition(false);
                    swgMapItem->setOrientation(0);
                    swgMapItem->setHeading(0);
                    MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create((const QObject*) m_channel, swgMapItem);
                    messageQueue->push(msg);
                }
            }
        }

        if (m_verifyOSD && !ft8Message.decoderInfo.startsWith("OSD"))
        {
            if ((ft8Message.type == "1") || (ft8Message.type == "2"))
            {
                if (!ft8Message.call2.startsWith("<")) {
                    m_validCallsigns.insert(ft8Message.call2);
                }

                if (!ft8Message.call1.startsWith("CQ") && !ft8Message.call1.startsWith("<")) {
                    m_validCallsigns.insert(ft8Message.call1);
                }
            }
        }
    }

    delete ft8Callback.getReportMessage();

    if (m_recordSamples)
    {
        WavFileRecord *wavFileRecord = new WavFileRecord(FT8DemodSettings::m_ft8SampleRate);
        QFileInfo wfi(QDir(m_samplesPath), periodTS.toString("yyyyMMdd_HHmmss"));
        QString wpath = wfi.absoluteFilePath();
        qDebug("FT8DemodWorker::processBuffer: WAV file: %s.wav", qPrintable(wpath));
        wavFileRecord->setFileName(wpath);
        wavFileRecord->setFileBaseIsFileName(true);
        wavFileRecord->setMono(true);
        wavFileRecord->startRecording();
        wavFileRecord->writeMono(buffer, frameSampleCount);
        wavFileRecord->stopRecording();
        delete wavFileRecord;
    }
}
