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

#include <QStandardPaths>
#include <QDir>
#include <QDateTime>

#include "dsp/wavfilerecord.h"
#include "util/messagequeue.h"
#include "util/ft8message.h"

#include "ft8demodsettings.h"
#include "ft8demodworker.h"

FT8DemodWorker::FT8Callback::FT8Callback(const QDateTime& periodTS, FT8::Packing& packing) :
    m_packing(packing),
    m_periodTS(periodTS)
{
    m_msgReportFT8Messages = MsgReportFT8Messages::create();
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
    std::string msg = m_packing.unpack(a91, call1, call2, loc);

    cycle_mu.lock();

    if (cycle_already.count(msg) > 0)
    {
        // already decoded this message on this cycle
        cycle_mu.unlock();
        return 1; // 1 => already seen, don't subtract.
    }

    cycle_already[msg] = true;

    QList<FT8Message>& ft8Messages = m_msgReportFT8Messages->getFT8Messages();
    ft8Messages.push_back(FT8Message());
    FT8Message& ft8Message = ft8Messages.back();

    ft8Message.ts = m_periodTS;
    ft8Message.pass = pass;
    ft8Message.snr = (int) snr;
    ft8Message.nbCorrectBits = correct_bits;
    ft8Message.dt = off - 0.5;
    ft8Message.df = hz0;
    ft8Message.call1 = QString(call1.c_str());
    ft8Message.call2 = QString(call2.c_str());
    ft8Message.loc = QString(loc.c_str());
    ft8Message.decoderInfo = QString(comment);
    cycle_mu.unlock();

    qDebug("FT8DemodWorker::FT8Callback::hcb: %d %3d %3d %5.2f %6.1f %s [%s:%s:%s] (%s)",
        pass,
        (int)snr,
        correct_bits,
        off - 0.5,
        hz0,
        msg.c_str(),
        call1.c_str(),
        call2.c_str(),
        loc.c_str(),
        comment
    );

    return 2; // 2 => new decode, do subtract.
}

FT8DemodWorker::FT8DemodWorker() :
    m_recordSamples(false),
    m_nbDecoderThreads(6),
    m_decoderTimeBudget(0.5),
    m_lowFreq(200),
    m_highFreq(3000),
    m_reportingMessageQueue(nullptr)
{
    QString relPath = "sdrangel/ft8/save";
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
    dir.mkpath(relPath);
    m_samplesPath = dir.absolutePath() + "/" + relPath;
    qDebug("FT8DemodWorker::FT8DemodWorker: samples path: %s", qPrintable(m_samplesPath));
}

FT8DemodWorker::~FT8DemodWorker()
{}

void FT8DemodWorker::processBuffer(int16_t *buffer, QDateTime periodTS)
{
    qDebug("FT8DemodWorker::processBuffer: %s %d:%f [%d:%d]", qPrintable(periodTS.toString("yyyy-MM-dd HH:mm:ss")),
        m_nbDecoderThreads, m_decoderTimeBudget, m_lowFreq, m_highFreq);

    if (m_invalidSequence)
    {
        qDebug("FT8DemodWorker::processBuffer: invalid sequence");
        m_invalidSequence = false;
        return;
    }

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
        wavFileRecord->writeMono(buffer, 15*FT8DemodSettings::m_ft8SampleRate);
        wavFileRecord->stopRecording();
        delete wavFileRecord;
    }

    int hints[2] = { 2, 0 }; // CQ
    FT8Callback ft8Callback(periodTS, m_packing);
    m_ft8Decoder.getParams().nthreads = m_nbDecoderThreads;
    std::vector<float> samples(15*FT8DemodSettings::m_ft8SampleRate);

    std::transform(
        buffer,
        buffer + (15*FT8DemodSettings::m_ft8SampleRate),
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
    qDebug("FT8DemodWorker::processBuffer: done: %d messages", ft8Callback.getReportMessage()->getFT8Messages().size());

    if (m_reportingMessageQueue) {
        m_reportingMessageQueue->push(ft8Callback.getReportMessage());
    } else {
        delete ft8Callback.getReportMessage();
    }
}
