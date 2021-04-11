///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <algorithm>
#include "datvudpstream.h"

const int DATVUDPStream::m_tsBlocksPerFrame = 7; // The usual value

DATVUDPStream::DATVUDPStream(int tsBlockSize) :
    m_active(false),
    m_address(QHostAddress::LocalHost),
    m_port(8882),
    m_tsBlockSize(tsBlockSize),
    m_tsBlockIndex(0),
    m_dataBytes(0),
    m_totalBytes(0),
    m_fifoSignalCount(0)
{
    m_tsBuffer = new char[m_tsBlocksPerFrame*m_tsBlockSize];
}

DATVUDPStream::~DATVUDPStream()
{
    delete[] m_tsBuffer;
}

void DATVUDPStream::pushData(const char *chrData, int nbTSBlocks)
{
    if (!m_active) {
        return;
    }

    for (int i = 0; i < nbTSBlocks; i++)
    {
        std::copy(chrData + i*m_tsBlockSize, chrData + (i+1)*m_tsBlockSize, m_tsBuffer + m_tsBlockIndex*m_tsBlockSize);

        if (m_tsBlockIndex < m_tsBlocksPerFrame - 1)
        {
            m_tsBlockIndex++;
        }
        else
        {
            m_udpSocket.writeDatagram(m_tsBuffer, m_tsBlocksPerFrame*m_tsBlockSize, m_address, m_port);
            m_dataBytes += m_tsBlocksPerFrame*m_tsBlockSize;
            m_totalBytes += m_tsBlocksPerFrame*m_tsBlockSize;

            if (++m_fifoSignalCount == 10)
            {
                emit fifoData(m_dataBytes, 0, m_totalBytes);
                m_fifoSignalCount = 0;
            }

            m_dataBytes = 0;
            m_tsBlockIndex = 0;
        }
    }
}

void DATVUDPStream::resetTotalReceived()
{
    m_totalBytes = 0;
    emit fifoData(m_dataBytes, 0, m_totalBytes);
}
