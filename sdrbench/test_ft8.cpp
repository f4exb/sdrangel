///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB.                                  //
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

#include <iostream>
#include <fstream>

#include "mainbench.h"
#include "dsp/wavfilerecord.h"

#ifdef LINUX
#include "ft8/ft8.h"
#include "ft8/unpack.h"

#include <QMutex>
#endif

#ifndef LINUX
void MainBench::testFT8()
{
    qDebug("Implemented in Linux only");
}
#else

class TestFT8Callback : public FT8::CallbackInterface
{
public:
    virtual int hcb(
        int *a91,
        float hz0,
        float off,
        const char *comment,
        float snr,
        int pass,
        int correct_bits
    );
    const std::map<std::string, bool>& getMsgMap() {
        return cycle_already;
    }
private:
    QMutex cycle_mu;
    std::map<std::string, bool> cycle_already;
    FT8::Packing packing;
};


int TestFT8Callback::hcb(
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
    std::string msg = packing.unpack(a91, call1, call2, loc);

    cycle_mu.lock();

    if (cycle_already.count(msg) > 0)
    {
        // already decoded this message on this cycle
        cycle_mu.unlock();
        return 1; // 1 => already seen, don't subtract.
    }

    cycle_already[msg] = true;

    cycle_mu.unlock();

    qDebug("TestFT8Callback::hcb: %d %3d %3d %5.2f %6.1f %s [%s:%s:%s] (%s)",
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
    fflush(stdout);

    return 2; // 2 => new decode, do subtract.
}

void MainBench::testFT8(const QString& wavFile)
{
    qDebug("MainBench::testFT8: start");
    int hints[2] = { 2, 0 }; // CQ
    double budget = 2.5; // compute for this many seconds per cycle
    TestFT8Callback testft8Callback;

    std::ifstream wfile;

#ifdef Q_OS_WIN
	wfile.open(m_settings.m_fileName.toStdWString().c_str(), std::ios::binary | std::ios::ate);
#else
	wfile.open(wavFile.toStdString().c_str(), std::ios::binary | std::ios::ate);
#endif
    WavFileRecord::Header header;
    wfile.seekg(0, std::ios_base::beg);
    bool headerOK = WavFileRecord::readHeader(wfile, header, false);

    if (!headerOK)
    {
        qDebug("MainBench::testFT8: test file is not a wave file");
        return;
    }

    if (header.m_sampleRate != 12000)
    {
        qDebug("MainBench::testFT8: wave file sample rate is not 12000 S/s");
        return;
    }

    if (header.m_bitsPerSample != 16)
    {
        qDebug("MainBench::testFT8: sample size is not 16 bits");
        return;
    }

    if (header.m_audioFormat != 1)
    {
        qDebug("MainBench::testFT8: wav file format is not PCM");
        return;
    }

    if (header.m_dataHeader.m_size != 360000)
    {
        qDebug("MainBench::testFT8: wave file size is not 15s at 12000 S/s");
        return;
    }

    const int bufsize = 1000;
    int16_t buffer[bufsize];
    std::vector<float> samples;
    uint32_t remainder = header.m_dataHeader.m_size;

    while (remainder != 0)
    {
        wfile.read((char *) buffer, bufsize*2);

        for (int i = 0; i < bufsize; i++) {
            samples.push_back(buffer[i] / 32768.0f);
        }

        remainder -= bufsize*2;
    }

    wfile.close();

    FT8::FT8Decoder decoder;

    decoder.entry(
        samples.data(),
        samples.size(),
        0.5 * header.m_sampleRate,
        header.m_sampleRate,
        150,
        3600, // 2900,
        hints,
        hints,
        budget,
        budget,
        &testft8Callback,
        0,
        (struct FT8::cdecode *) nullptr
    );
    qDebug("MainBench::testFT8: done");
    const std::map<std::string, bool>& msgMap = testft8Callback.getMsgMap();

    if (msgMap.size() != 15)
    {
        qDebug("MainBench::testFT8: failed: invlid size: %lu expected 15", msgMap.size());
        return;
    }

    QStringList messages = {
        "CQ DF5SF JN39",
        "CQ DL1SVA JO64",
        "CQ DL7CO JO42",
        "CQ F4BAL JO10",
        "CQ LA1XJA JO49",
        "CQ ON7VG JO21",
        "CQ OZ1BJF JO55",
        "CQ S51TA JN75",
        "HA3PT SQ8AA -18",
        "JA2KFQ EI4KF -17",
        "LY3PW DF2FE R-13",
        "N9GQA DG9NAY JN58",
        "OK1HEH OH8NW 73  ",
        "UN6T EA1FQ IN53",
        "W5SUM G8OO -18"
    };

    for (const auto &msg : messages)
    {
        if (msgMap.count(msg.toStdString()) != 1)
        {
            qDebug("MainBench::testFT8: failed: key: %s", qPrintable(msg));
            return;
        }
    }

    qDebug("MainBench::testFT8: success");
}
#endif
