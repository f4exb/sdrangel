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

#include "mainbench.h"
#ifdef LINUX
#include "ft8/ft8.h"
#include "ft8/util.h"
#include "ft8/unpack.h"

#include <QMutex>
#endif

#ifndef LINUX
void MainBench::testFT8()
{
    qDebug("Implemented in Linux only");
}
#else

QMutex cycle_mu;
std::map<std::string, bool> cycle_already;

int hcb(
    int *a91,
    float hz0,
    float off,
    const char *comment,
    float snr,
    int pass,
    int correct_bits)
{
    std::string msg = FT8::unpack(a91);

    cycle_mu.lock();

    if (cycle_already.count(msg) > 0)
    {
        // already decoded this message on this cycle
        cycle_mu.unlock();
        return 1; // 1 => already seen, don't subtract.
    }

    cycle_already[msg] = true;

    cycle_mu.unlock();

    printf("%d %3d %3d %5.2f %6.1f %s, %s\n",
           pass,
           (int)snr,
           correct_bits,
           off - 0.5,
           hz0,
           msg.c_str(),
           comment);
    fflush(stdout);

    return 2; // 2 => new decode, do subtract.
}

void MainBench::testFT8()
{
    qDebug("MainBench::testFT8: start");
    int hints[2] = { 2, 0 }; // CQ
    double budget = 2.5; // compute for this many seconds per cycle

    int rate;
    std::vector<float> s = FT8::readwav("/home/f4exb/.local/share/WSJT-X/save/230105_091630.wav", rate); // FIXME: download file
    FT8::entry(
        s.data(),
        s.size(),
        0.5 * rate,
        rate,
        150,
        3600, // 2900,
        hints,
        hints,
        budget,
        budget,
        hcb,
        0,
        (struct FT8::cdecode *) 0
    );
    qDebug("MainBench::testFT8: end");
}
#endif
