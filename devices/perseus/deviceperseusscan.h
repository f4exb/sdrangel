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

#ifndef DEVICES_PERSEUS_DEVICEPERSEUSSCAN_H_
#define DEVICES_PERSEUS_DEVICEPERSEUSSCAN_H_

#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include <QString>

#include "export.h"

class DEVICES_API DevicePerseusScan
{
public:
    struct DeviceScan
    {
        std::string m_serial;
        uint16_t    m_serialNumber;
        int         m_sequence;
    };

    bool scan(int nbDevices); //!< false if one device had its firmware not yet downloaded
    void clear();
    int getNbActiveDevices() const { return m_scans.size(); }
    const std::string* getSerialAt(unsigned int index) const;
    uint16_t getSerialNumberAt(unsigned int index) const ;
    int getSequenceAt(unsigned int index) const ;
    int getSequenceFromSerial(const std::string& serial) const;
    void getSerials(std::vector<std::string>& serials) const;

private:
    std::vector<DeviceScan> m_scans;
    std::map<std::string, DeviceScan*> m_serialMap;
};


#endif /* DEVICES_PERSEUS_DEVICEPERSEUSSCAN_H_ */
