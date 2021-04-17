///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Swagger server adapter interface                                              //
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

#include <QDebug>

#include "mainbench.h"
#include "util/golay2312.h"

void MainBench::testGolay2312()
{
    qDebug() << "MainBench::testGolay2312: parity first";

    unsigned int msg = 04023; // {1, 0, 0,  0, 0, 0,  0, 1, 0,  0, 1, 1};
    unsigned int expectedCodeword = 035430000 + msg;
    Golay2312 golay2312;
    bool success = true;

    unsigned int codeword;
    golay2312.encodeParityFirst(msg, &codeword);

    if (codeword != expectedCodeword)
    {
        qDebug() << "MainBench::testGolay2312:"
            << "encoder mismatch: got:" << oct << codeword
            << "expected:" << oct << expectedCodeword;
        success = false;
    }

    unsigned int rxCodeword = codeword;
    bool decoded = golay2312.decodeParityFirst(&rxCodeword);

    if (!decoded)
    {
        qDebug() << "MainBench::testGolay2312:"
            << " unrecoverable error (no error)";
        success = false;
    }
    else if (rxCodeword != codeword)
    {
        qDebug() << "MainBench::testGolay2312:"
            << "decoder mismatch (no error): got:" << oct << rxCodeword
            << "expected:" << oct << codeword;
        success = false;
    }

    // flip one bit (4th)
    rxCodeword = codeword ^ 000020000;
    decoded = golay2312.decodeParityFirst(&rxCodeword);

    if (!decoded)
    {
        qDebug() << "MainBench::testGolay2312:"
            << " unrecoverable error (parity[1])";
        success = false;
    }
    else if (rxCodeword != codeword)
    {
        qDebug() << "MainBench::testGolay2312:"
            << "decoder mismatch (parity[1]): got:" << oct << rxCodeword
            << "expected:" << oct << codeword;
        success = false;
    }

    // flip two bits (1st, 5th)
    rxCodeword = codeword ^ 000120000;
    decoded = golay2312.decodeParityFirst(&rxCodeword);

    if (!decoded)
    {
        qDebug() << "MainBench::testGolay2312:"
            << " unrecoverable error (parity[1,3])";
        success = false;
    }
    else if (rxCodeword != codeword)
    {
        qDebug() << "MainBench::testGolay2312:"
            << "decoder mismatch (parity[1,3]): got:" << oct << rxCodeword
            << "expected:" << oct << codeword;
        success = false;
    }

    // Conclusion

    if (success) {
        qDebug() << "MainBench::testGolay2312: success";
    } else {
        qDebug() << "MainBench::testGolay2312: failed";
    }
}

