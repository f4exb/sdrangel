///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include "deviceplutosdr.h"

const uint64_t DevicePlutoSDR::loLowLimitFreq  =   70000000UL; // 70 MHz: take AD9364 specs
const uint64_t DevicePlutoSDR::loHighLimitFreq = 6000000000UL; //  6 GHz: take AD9364 specs

const uint32_t DevicePlutoSDR::srLowLimitFreq  =   200000; // 200 kS/s
const uint32_t DevicePlutoSDR::srHighLimitFreq = 20000000; // 20 MS/s: take AD9363 speces

const uint32_t DevicePlutoSDR::bbLPRxLowLimitFreq  =   200000; // 200 kHz
const uint32_t DevicePlutoSDR::bbLPRxHighLimitFreq = 14000000; // 14 MHz
const uint32_t DevicePlutoSDR::bbLPTxLowLimitFreq  =   625000; // 625 kHz
const uint32_t DevicePlutoSDR::bbLPTxHighLimitFreq = 16000000; // 16 MHz

DevicePlutoSDR::DevicePlutoSDR()
{
}

DevicePlutoSDR::~DevicePlutoSDR()
{
}

DevicePlutoSDR& DevicePlutoSDR::instance()
{
    static DevicePlutoSDR inst;
    return inst;
}

DevicePlutoSDRBox* DevicePlutoSDR::getDeviceFromURI(const std::string& uri)
{
    return new DevicePlutoSDRBox(uri);
}

DevicePlutoSDRBox* DevicePlutoSDR::getDeviceFromSerial(const std::string& serial)
{
    const std::string *uri = m_scan.getURIFromSerial(serial);

    if (uri) {
        return new DevicePlutoSDRBox(*uri);
    } else {
        return 0;
    }
}




