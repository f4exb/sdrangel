///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include "deviceperseusscan.h"
#include "perseus-sdr.h"
#include <sstream>
#include <QtGlobal>


void DevicePerseusScan::scan(int nbDevices)
{
	if (nbDevices == 0) {
		qInfo("DevicePerseusScan::scan: no Perseus devices");
		return;
	}

	perseus_descr *descr;
	eeprom_prodid prodid;

	for (int deviceIndex = 0; deviceIndex < nbDevices; deviceIndex++)
	{
		if ((descr = perseus_open(deviceIndex)) == 0) {
			qCritical("DevicePerseusScan::scan: open error: %s\n", perseus_errorstr());
			continue;
		}

//			if (perseus_firmware_download(descr, 0) < 0) {
//				qCritical("DevicePerseusScan::scan: firmware download error: %s", perseus_errorstr());
//				continue;
//			}
//			else
//			{
//				qInfo("DevicePerseusScan::scan: device #%d firmware downloaded", deviceIndex);
//			}

		if (perseus_get_product_id(descr,&prodid) < 0) {
			qCritical("DevicePerseusScan::scan: get product id error: %s", perseus_errorstr());
			perseus_close(descr);
			continue;
		}
		else
		{
			uint32_t sigA = (prodid.signature[5]<<16) + prodid.signature[4];
			uint32_t sigB = (prodid.signature[3]<<16) + prodid.signature[2];
			uint32_t sigC = (prodid.signature[1]<<16) + prodid.signature[0];
			std::stringstream ss;
			ss << prodid.sn << "-" << std::hex << sigA << "-" << sigB << "-" << sigC;
			m_scans.push_back({ss.str(), prodid.sn, deviceIndex});
			m_serialMap[m_scans.back().m_serial] = &m_scans.back();
			perseus_close(descr);
		}
	}
}

const std::string* DevicePerseusScan::getSerialAt(unsigned int index) const
{
    if (index < m_scans.size()) {
        return &(m_scans[index].m_serial);
    } else {
        return 0;
    }
}

uint16_t DevicePerseusScan::getSerialNumberAt(unsigned int index) const
{
    if (index < m_scans.size()) {
        return m_scans[index].m_serialNumber;
    } else {
        return 0;
    }
}

int DevicePerseusScan::getSequenceAt(unsigned int index) const
{
    if (index < m_scans.size()) {
        return m_scans[index].m_sequence;
    } else {
        return 0;
    }
}

int DevicePerseusScan::getSequenceFromSerial(const std::string& serial) const
{
    std::map<std::string, DeviceScan*>::const_iterator it = m_serialMap.find(serial);
    if (it == m_serialMap.end()) {
        return -1;
    } else {
        return ((it->second)->m_sequence);
    }
}

void DevicePerseusScan::getSerials(std::vector<std::string>& serials) const
{
    std::vector<DeviceScan>::const_iterator it = m_scans.begin();
    serials.clear();

    for (; it != m_scans.end(); ++it) {
        serials.push_back(it->m_serial);
    }
}



