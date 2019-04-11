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

#include "devicesoapysdrshared.h"

MESSAGE_CLASS_DEFINITION(DeviceSoapySDRShared::MsgReportBuddyChange, Message)
MESSAGE_CLASS_DEFINITION(DeviceSoapySDRShared::MsgReportDeviceArgsChange, Message)

const float  DeviceSoapySDRShared::m_sampleFifoLengthInSeconds = 0.25;
const int    DeviceSoapySDRShared::m_sampleFifoMinSize = 75000; // 300 kS/s knee
const int    DeviceSoapySDRShared::m_sampleFifoMinSize32 = 150000; // Fixed for interpolation by 32

DeviceSoapySDRShared::DeviceSoapySDRShared() :
    m_device(0),
    m_deviceParams(0),
    m_channel(-1),
    m_source(0),
    m_sink(0)
{}

DeviceSoapySDRShared::~DeviceSoapySDRShared()
{}
