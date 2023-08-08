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

#include "devicejupitersdrparams.h"

#include "devicejupitersdr.h"

DeviceJupiterSDRParams::DeviceJupiterSDRParams() :
    m_box(nullptr)
{
}

DeviceJupiterSDRParams::~DeviceJupiterSDRParams()
{
}

bool DeviceJupiterSDRParams::open(const std::string& serial)
{
    m_box = DeviceJupiterSDR::instance().getDeviceFromSerial(serial);
    return m_box != nullptr;
}

bool DeviceJupiterSDRParams::openURI(const std::string& uri)
{
    m_box = DeviceJupiterSDR::instance().getDeviceFromURI(uri);
    return m_box != nullptr;
}

void DeviceJupiterSDRParams::close()
{
    delete m_box;
    m_box = nullptr;
}
