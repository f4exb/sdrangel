///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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
#include <regex>
#include <random>

#include <QTextStream>

#include "mainbench.h"

#ifndef HAS_FT8
void MainBench::testFT8(const QString& wavFile, const QString& argsStr)
{
    (void) wavFile;
    (void) argsStr;
    qWarning("MainBench::testFT8: this version has no FT8 support");
}
#else

#include "ft8/ft8.h"
#include "ft8/packing.h"

class TestFT8Protocols
{
    public:
        static void testMsg1(const QStringList& argElements, bool runLDPC = false);
        static void testMsg00(const QStringList& argElements, bool runLDPC = false);
        static void testOnesZeroes(const QStringList& argElements);
        static void testSoftDecode(const QStringList& argElements);

    private:
        static bool testLDPC(int a77[]);
        static bool compareBits(int a[], int r[], int nbBits = 174);
        static void debugIntArray(int a[], int length);
};


void MainBench::testFT8Protocols(const QString& argsStr)
{
    QStringList argElements = argsStr.split(','); // comma separated list of arguments

    if (argElements.size() == 0)
    {
        qWarning("MainBench::testFT8Protocols: no arguments");
        return;
    }

    QString& testType = argElements[0];

    if (testType == "msg1") {
        TestFT8Protocols::testMsg1(argElements); // type 1 message test
    } else if (testType == "msg00") {
        TestFT8Protocols::testMsg00(argElements); // type 0.0 message test
    } else if (testType == "msg1L") {
        TestFT8Protocols::testMsg1(argElements, true); // type 1 message test with LDPC encoding/decoding test
    } else if (testType == "msg00L") {
        TestFT8Protocols::testMsg00(argElements, true); // type 0.0 message test with LDPC encoding/decoding test
    } else if (testType == "zeroones") {
        TestFT8Protocols::testOnesZeroes(argElements);
    } else if (testType == "softdec") {
        TestFT8Protocols::testSoftDecode(argElements);
    } else {
        qWarning("MainBench::testFT8Protocols: unrecognized test type");
    }
}


void TestFT8Protocols::testMsg1(const QStringList& argElements, bool runLDPC)
{
    if (argElements.size() < 4)
    {
        qWarning("TestFT8Protocols::testMsg1: missing callsigns and locator/report in argument");
        return;
    }

    int c28_1, c28_2, g15;

    if (!FT8::Packing::packcall_std(c28_1, argElements[1].toStdString()))
    {
        qWarning("TestFT8Protocols::testMsg1: callsign %s is not a standard callsign", qPrintable(argElements[1]));
        return;
    }

    if (!FT8::Packing::packcall_std(c28_2, argElements[2].toStdString()))
    {
        qWarning("TestFT8Protocols::testMsg1: callsign %s is not a standard callsign", qPrintable(argElements[2]));
        return;
    }


    std::string locstr;
    int reply;

    if (argElements[3].startsWith("R+") || argElements[3].startsWith("R-"))
    {
        reply = 1;
        locstr = argElements[3].mid(1).toStdString();
    }
    else
    {
        reply = 0;
        locstr = argElements[3].toStdString();
    }

    if (!FT8::Packing::packgrid(g15, locstr))
    {
        qWarning("TestFT8Protocols::testMsg1: locator or report %s is not valid", locstr.c_str());
        return;
    }

    qDebug("TestFT8Protocols::testMsg1: c28_1: %d c28_2: %d g15: %d", c28_1, c28_2, g15);

    int a77[77];
    std::fill(a77, a77 + 77, 0);
    FT8::Packing::pack1(a77, c28_1, c28_2, g15, reply);
    FT8::Packing packing;

    std::string call1, call2, loc;
    std::string msg = packing.unpack_1(a77, call1, call2, loc);
    qInfo("TestFT8Protocols::testMsg1: msg: %s, call1: %s, call2: %s, loc: %s", msg.c_str(), call1.c_str(), call2.c_str(), loc.c_str());

    if (runLDPC)
    {
        if (testLDPC(a77)) {
            qInfo("TestFT8Protocols::testMsg1: LDPC test succeeded");
        } else {
            qWarning("TestFT8Protocols::testMsg1: LDPC test failed");
        }
    }
}

void TestFT8Protocols::testMsg00(const QStringList& argElements, bool runLDPC)
{
    if (argElements.size() < 2)
    {
        qWarning("TestFT8Protocols::testMsg00: missing free text in argument");
        return;
    }

    int a77[77];
    std::fill(a77, a77 + 77, 0);

    if (!FT8::Packing::packfree(a77, argElements[1].toStdString()))
    {
        qWarning("TestFT8Protocols::testMsg00: message %s is not valid", qPrintable(argElements[1]));
        return;
    }

    std::string call1, call2, loc;
    std::string msg = FT8::Packing::unpack_0_0(a77, call1, call2, loc);
    qInfo("TestFT8Protocols::testMsg00: msg: %s, call1: %s", msg.c_str(), call1.c_str());

    if (runLDPC)
    {
        if (testLDPC(a77)) {
            qInfo("TestFT8Protocols::testMsg00: LDPC test succeeded");
        } else {
            qWarning("TestFT8Protocols::testMsg00: LDPC test failed");
        }
    }
}

bool TestFT8Protocols::testLDPC(int a77[])
{
    int a174[174], r174[174];
    FT8::FT8::encode(a174, a77);
    FT8::FT8Params ft8Params;
    float ll174[174];
    FT8::FT8Params params;
    std::string comments;

    std::transform(
        a174,
        a174+174,
        ll174,
        [](const int& s) -> float { return s == 1 ? -1.0 : 1.0; }
    );

    if (FT8::FT8::decode(ll174, r174, params, 0, comments) == 0)
    {
        qInfo("TestFT8Protocols::testLDPC(: LDPC or CRC check failed");
        return false;
    }
    else
    {
        return compareBits(a174, r174);
    }
}

bool TestFT8Protocols::compareBits(int a[], int r[], int nbBits)
{
    for (int i=0; i < nbBits; i++)
    {
        if (a[i] != r[i])
        {
            qDebug("TestFT8Protocols::compareBits: failed at index %d: %d != %d", i, a[i], r[i]);
            return false;
        }
    }

    return true;
}

void TestFT8Protocols::debugIntArray(int a[], int length)
{
    QString s;
    QTextStream os(&s);

    for (int i=0; i < length; i++) {
        os << a[i] << " ";
    }

    qDebug("TestFT8Protocols::debugIntArray: %s", qPrintable(s));
}

void TestFT8Protocols::testOnesZeroes(const QStringList& argElements)
{
    if (argElements.size() < 3)
    {
        qWarning("TestFT8Protocols::testOnesZeroes: not enough elements");
        return;
    }

    int nbBits, bitIndex;
    bool intOK;

    nbBits = argElements[1].toInt(&intOK);

    if (!intOK)
    {
        qWarning("TestFT8Protocols::testOnesZeroes: first argument is not numeric: %s", qPrintable(argElements[1]));
        return;
    }

    bitIndex = argElements[2].toInt(&intOK);

    if (!intOK)
    {
        qWarning("TestFT8Protocols::testOnesZeroes: second argument is not numeric: %s", qPrintable(argElements[2]));
        return;
    }

    if (nbBits < 2)
    {
        qWarning("TestFT8Protocols::testOnesZeroes: nbBits too small: %d", nbBits);
        return;
    }

    bitIndex = bitIndex > nbBits - 1 ? nbBits - 1 : bitIndex;

    int *ones = new int[1<<nbBits];
    int *zeroes = new int[1<<nbBits];
    FT8::FT8::set_ones_zeroes(ones, zeroes, nbBits, bitIndex);
    QString s;
    QTextStream os(&s);

    for (int i = 0; i < (1<<(nbBits-1)); i++) {
        os << i << ": " << zeroes[i] << ", " << ones[i] << "\n";
    }

    qInfo("TestFT8Protocols::testOnesZeroes: (%d,%d) index: zeroes, ones:\n%s", nbBits, bitIndex, qPrintable(s));
}

void TestFT8Protocols::testSoftDecode(const QStringList& argElements)
{
    if (argElements.size() < 3)
    {
        qWarning("TestFT8Protocols::testSoftDecode: not enough elements");
        return;
    }

    bool intOK;
    int nbBits = argElements[1].toInt(&intOK);

    if (!intOK)
    {
        qWarning("TestFT8Protocols::testSoftDecode: first argument is not numeric: %s", qPrintable(argElements[1]));
        return;
    }

    if ((nbBits < 2) || (nbBits > 12))
    {
        qWarning("TestFT8Protocols::testSoftDecode: bits peer symbols invalid: %d", nbBits);
        return;
    }

    int symbolSize = 1<<nbBits;
    std::vector<float> magSymbols(symbolSize);
    std::vector<std::vector<float>> mags;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0, 0.01);

    for (int i = 2; i < argElements.size(); i++)
    {
        int symbol = argElements[i].toInt(&intOK);

        if (!intOK)
        {
            qWarning("TestFT8Protocols::testSoftDecode: symbol is not numeric: %s", qPrintable(argElements[i]));
            return;
        }

        for (auto& m : magSymbols) {
            m = 0.01 + dist(gen);
        }

        symbol = symbol % symbolSize;
        symbol = symbol ^(symbol >> 1); // Gray code
        magSymbols[symbol] += 0.015;
        mags.push_back(magSymbols);
    }

    QString s;
    QTextStream os(&s);

    qDebug("TestFT8Protocols::testSoftDecode: mags:");

    for (const auto& magrow : mags)
    {
        for (const auto& mag : magrow) {
            os << mag << " ";
        }
        qDebug("TestFT8Protocols::testSoftDecode: %s", qPrintable(s));
        s.clear();
    }

    float *lls = new float[mags.size()*nbBits];
    std::fill(lls, lls+mags.size()*nbBits, 0.0);
    FT8::FT8Params params;
    FT8::FT8::soft_decode_mags(params, mags, nbBits, lls);

    for (unsigned int si = 0; si < mags.size(); si++)
    {
        for (int biti = 0; biti < nbBits; biti++) {
            os << " " << lls[nbBits*si + biti];
        }
        os << "  ";
    }

    // for (unsigned int i = 0; i < mags.size()*nbBits; i++) {
    //     os << " " << lls[i];
    // }

    qInfo("TestFT8Protocols::testSoftDecode: lls: %s", qPrintable(s));
    delete[] lls;
}

#endif
