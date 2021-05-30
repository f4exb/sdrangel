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

#ifndef DEVICES_PLUTOSDR_DEVICEPLUTOSDRSCAN_H_
#define DEVICES_PLUTOSDR_DEVICEPLUTOSDRSCAN_H_

#include <QString>

#include <string>
#include <vector>
#include <map>
#include <memory>

#include "plugin/plugininterface.h"
#include "export.h"

class DEVICES_API DevicePlutoSDRScan
{
public:
    struct DeviceScan
    {
        std::string m_name;
        std::string m_serial;
        std::string m_uri;
    };

    void scan();
    int getNbDevices() const { return m_scans.size(); }
    const std::string* getURIAt(unsigned int index) const;
    const std::string* getSerialAt(unsigned int index) const ;
    const std::string* getURIFromSerial(const std::string& serial) const;
    void getSerials(std::vector<std::string>& serials) const;
    void enumOriginDevices(const QString& hardwareId, PluginInterface::OriginDevices& originDevices);

private:
    std::vector<std::shared_ptr<DeviceScan>> m_scans;
    std::map<std::string, std::shared_ptr<DeviceScan>> m_serialMap;
    std::map<std::string, std::shared_ptr<DeviceScan>> m_urilMap;
};



#endif /* DEVICES_PLUTOSDR_DEVICEPLUTOSDRSCAN_H_ */
