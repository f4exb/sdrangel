///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include "dsddecoder.h"

#include <QtGlobal>
#include "audio/audiofifo.h"


DSDDecoder::DSDDecoder()
{
    m_decoder.setQuiet();
    m_decoder.setUpsampling(6); // force upsampling of audio to 48k
    m_decoder.setStereo(true);  // force copy to L+R channels
    m_decoder.setDecodeMode(DSDcc::DSDDecoder::DSDDecodeAuto, true); // Initialize with auto-detect
    m_decoder.setUvQuality(3); // This is gr-dsd default
    m_decoder.enableCosineFiltering(false);
    m_decoder.setDataRate(DSDcc::DSDDecoder::DSDRate4800);
}

DSDDecoder::~DSDDecoder()
{
}

void DSDDecoder::set48k(bool to48k)
{
    m_decoder.setUpsampling(to48k ? 6 : 0);
}

void DSDDecoder::setUpsampling(int upsampling)
{
    m_decoder.setUpsampling(upsampling);
}

void DSDDecoder::setBaudRate(int baudRate)
{
    if (baudRate == 2400)
    {
        m_decoder.setDataRate(DSDcc::DSDDecoder::DSDRate2400);
    }
    else if (baudRate == 4800)
    {
        m_decoder.setDataRate(DSDcc::DSDDecoder::DSDRate4800);
    }
    else if (baudRate == 9600)
    {
        m_decoder.setDataRate(DSDcc::DSDDecoder::DSDRate9600);
    }
    else // default 4800 bauds
    {
        m_decoder.setDataRate(DSDcc::DSDDecoder::DSDRate4800);
    }

    // when setting baud rate activate detection of all possible modes for this rate
    // because on the other hand when a mode is selected then the baud rate is automatically changed
    m_decoder.setDecodeMode(DSDcc::DSDDecoder::DSDDecodeAuto, true);
}
