///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon source channel (Tx). Samples buffer                                 //
//                                                                               //
// SDRdaemon is a detached SDR front end that handles the interface with a       //
// physical device and sends or receives the I/Q samples stream to or from a     //
// SDRangel instance via UDP. It is controlled via a Web REST API.               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "channel/sdrdaemonchannelsourcebuffer.h"

SDRDaemonChannelSourceBuffer::SDRDaemonChannelSourceBuffer(uint32_t nbSamples)
{
    m_buffer = new Sample[nbSamples];
    m_wp = nbSamples/2;
    m_rp = 0;
    m_size = nbSamples;
}

SDRDaemonChannelSourceBuffer::~SDRDaemonChannelSourceBuffer()
{
    delete[] m_buffer;
}

void SDRDaemonChannelSourceBuffer::resize(uint32_t nbSamples)
{
    if (nbSamples > m_size)
    {
        delete[] m_buffer;
        m_buffer = new Sample[nbSamples];
    }

    m_wp = nbSamples/2;
    m_rp = 0;
    m_size = nbSamples;
}

void SDRDaemonChannelSourceBuffer::write(Sample *begin, uint32_t nbSamples)
{
    if (m_wp + nbSamples < m_size)
    {
        std::copy(begin, begin+nbSamples, &m_buffer[m_wp]);
        m_wp += nbSamples;
    }
    else // wrap
    {
        int first = m_size - m_wp;
        std::copy(begin, begin+first, &m_buffer[m_wp]);
        int second = nbSamples - first;
        std::copy(begin+first, begin+nbSamples, m_buffer);
        m_wp = second;
    }
}

void SDRDaemonChannelSourceBuffer::readOne(Sample& sample)
{
    sample = m_buffer[m_rp];
    m_rp++; 

    if (m_rp == m_size) { // wrap
        m_rp = 0;
    }
}

int SDRDaemonChannelSourceBuffer::getRWBalancePercent()
{
    if (m_wp > m_rp) {
        return (((m_wp - m_rp) - (m_size/2))*100)/m_size;
    } else {
        return (((m_size/2) - (m_rp - m_wp))*100)/m_size;
    }
}