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

#ifndef DEVICES_PERSEUS_DEVICEPERSEUS_H_
#define DEVICES_PERSEUS_DEVICEPERSEUS_H_

#include "deviceperseusscan.h"

#include "export.h"

class DEVICES_API DevicePerseus
{
public:
    static DevicePerseus& instance();
    void scan();
    void getSerials(std::vector<std::string>& serials) const { m_scan.getSerials(serials); }
    int getSequenceFromSerial(const std::string& serial) const { return m_scan.getSequenceFromSerial(serial); }

protected:
    DevicePerseus();
    DevicePerseus(const DevicePerseus&) : m_nbDevices(0) {}
    DevicePerseus& operator=(const DevicePerseus& other) { (void) other; return *this; }
    ~DevicePerseus();

private:
    bool internal_scan();
    int m_nbDevices;
    DevicePerseusScan m_scan;
};


#endif /* DEVICES_PERSEUS_DEVICEPERSEUS_H_ */
