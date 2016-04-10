///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB.                                  //
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

#include "dmr_voice.h"
#include "dsd_decoder.h"

namespace DSDPlus
{

DSDDMRVoice::DSDDMRVoice(DSDDecoder *dsdDecoder) :
        m_dsdDecoder(dsdDecoder)
{
}

DSDDMRVoice::~DSDDMRVoice()
{
}

void DSDDMRVoice::init()
{
    mutecurrentslot = 0;
    msMode = 0;
    dibit_p = m_dsdDecoder->m_state.dibit_buf_p - 144;
    m_slotIndex = 0;
    m_majorBlock = 0;
}

void DSDDMRVoice::process()
{

}

} // namespace dsdplus
