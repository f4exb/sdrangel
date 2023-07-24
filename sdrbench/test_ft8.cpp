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

#include <QMutex>

#ifndef HAS_FT8
void MainBench::testFT8(const QString& wavFile, const QString& argsStr)
{
    (void) wavFile;
    (void) argsStr;
    qWarning("MainBench::testFT8: this version has no FT8 support");
}
#else

#include "ft8/ft8.h"
#include "ft8/unpack.h"

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
    virtual QString get_name();
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
    std::string type;
    std::string msg = packing.unpack(a91, call1, call2, loc, type);

    cycle_mu.lock();

    if (cycle_already.count(msg) > 0)
    {
        // already decoded this message on this cycle
        cycle_mu.unlock();
        return 1; // 1 => already seen, don't subtract.
    }

    cycle_already[msg] = true;

    cycle_mu.unlock();

    qDebug("TestFT8Callback::hcb: %3s %d %3d %3d %5.2f %6.1f %s [%s:%s:%s] (%s)",
        type.c_str(),
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

QString TestFT8Callback::get_name()
{
    return "test";
}

void MainBench::testFT8(const QString& wavFile, const QString& argsStr)
{
    int nthreads = 8;    // number of threads (default)
    double budget = 2.5; // compute for this many seconds per cycle (default)
    // 3,0.5 combinaion may be enough

    QStringList argElements = argsStr.split(','); // comma separated list of arguments

    for (int i = 0; i < argElements.size(); i++)
    {
        const QString& argStr = argElements.at(i);
        bool ok;

        if (i == 0) // first is the number of threads (integer)
        {
            int nthreads_x = argStr.toInt(&ok);

            if (ok) {
                nthreads = nthreads_x;
            }
        }

        if (i == 1) // second is the time budget in seconds (double)
        {
            double budget_x = argStr.toDouble(&ok);

            if (ok) {
                budget = budget_x;
            }
        }
    }

    qDebug("MainBench::testFT8: start nthreads: %d budget: %fs", nthreads, budget);
    int hints[2] = { 2, 0 }; // CQ
    TestFT8Callback testft8Callback;

    std::ifstream wfile;

#ifdef Q_OS_WIN
	wfile.open(wavFile.toStdWString().c_str(), std::ios::binary | std::ios::ate);
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
    decoder.getParams().nthreads = nthreads;
    decoder.getParams().use_osd = 0;

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

    decoder.wait(budget + 1.0); // add one second to budget to force quit threads
    const std::map<std::string, bool>& msgMap = testft8Callback.getMsgMap();
    qDebug("MainBench::testFT8: done %lu decodes", msgMap.size());

    if (msgMap.size() != 15)
    {
        qDebug("MainBench::testFT8: failed: invalid size: %lu expected 15", msgMap.size());
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
