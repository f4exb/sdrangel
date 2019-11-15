///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include "devicebladerf2shared.h"

MESSAGE_CLASS_DEFINITION(DeviceBladeRF2Shared::MsgReportBuddyChange, Message)

const unsigned int DeviceBladeRF2Shared::m_sampleFifoMinRate = 48000;

DeviceBladeRF2Shared::DeviceBladeRF2Shared() :
    m_dev(0),
    m_channel(-1),
    m_source(0),
    m_sink(0)
{}

DeviceBladeRF2Shared::~DeviceBladeRF2Shared()
{}



