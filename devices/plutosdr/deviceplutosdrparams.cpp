///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include "deviceplutosdrparams.h"

#include "deviceplutosdr.h"

DevicePlutoSDRParams::DevicePlutoSDRParams() :
    m_box(nullptr)
{
}

DevicePlutoSDRParams::~DevicePlutoSDRParams()
{
}

bool DevicePlutoSDRParams::open(const std::string& serial)
{
    m_box = DevicePlutoSDR::instance().getDeviceFromSerial(serial);
    return m_box != nullptr;
}

bool DevicePlutoSDRParams::openURI(const std::string& uri)
{
    m_box = DevicePlutoSDR::instance().getDeviceFromURI(uri);
    return m_box != nullptr;
}

void DevicePlutoSDRParams::close()
{
    delete m_box;
    m_box = nullptr;
}
