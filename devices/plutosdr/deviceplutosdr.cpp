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




