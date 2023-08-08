///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023 Benjamin Menkuec, DJ4LF                                    //
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

#include "devicejupitersdr.h"

const uint64_t DeviceJupiterSDR::rxLOLowLimitFreq  =   30000000UL; // 30 MHz
const uint64_t DeviceJupiterSDR::rxLOHighLimitFreq = 6000000000UL; //  6 GHz

const uint64_t DeviceJupiterSDR::txLOLowLimitFreq  =   30000000UL; // 30 MHz
const uint64_t DeviceJupiterSDR::txLOHighLimitFreq = 6000000000UL; //  6 GHz

const uint32_t DeviceJupiterSDR::srLowLimitFreq  =         12000U; // 12 KSPS
const uint32_t DeviceJupiterSDR::srHighLimitFreq =     61440000UL; // 61.44 MSPS

const uint32_t DeviceJupiterSDR::bbLPRxLowLimitFreq  =    12000U; // 12 kHz
const uint32_t DeviceJupiterSDR::bbLPRxHighLimitFreq = 40000000U; // 40 MHz
const uint32_t DeviceJupiterSDR::bbLPTxLowLimitFreq  =    12000U; // 12 kHz
const uint32_t DeviceJupiterSDR::bbLPTxHighLimitFreq = 40000000U; // 40 MHz

DeviceJupiterSDR::DeviceJupiterSDR()
{
}

DeviceJupiterSDR::~DeviceJupiterSDR()
{
}

DeviceJupiterSDR& DeviceJupiterSDR::instance()
{
    static DeviceJupiterSDR inst;
    return inst;
}

DeviceJupiterSDRBox* DeviceJupiterSDR::getDeviceFromURI(const std::string& uri)
{
    return new DeviceJupiterSDRBox(uri);
}

DeviceJupiterSDRBox* DeviceJupiterSDR::getDeviceFromSerial(const std::string& serial)
{
    const std::string *uri = m_scan.getURIFromSerial(serial);

    if (uri) {
        return new DeviceJupiterSDRBox(*uri);
    } else {
        return 0;
    }
}




