///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include <QDebug>

#include <cstdio>
#include <cstring>
#include <cmath>
#include <regex>
#include <thread>

#include <uhd/types/device_addr.hpp>

#include "deviceusrpparam.h"
#include "deviceusrp.h"

void DeviceUSRP::enumOriginDevices(const QString& hardwareId, PluginInterface::OriginDevices& originDevices)
{
    try
    {
        uhd::device_addr_t hint; // Discover all devices
        uhd::device_addrs_t dev_addrs = uhd::device::find(hint);

        if (dev_addrs.size() <= 0)
        {
            qDebug("DeviceUSRP::enumOriginDevices: Could not find any USRP device");
            return;
        }

        for(unsigned i = 0; i != dev_addrs.size(); i++)
        {
            QString id = QString::fromStdString(dev_addrs[i].to_string());
            QString name = QString::fromStdString(dev_addrs[i].get("name", "N/A"));
            QString serial = QString::fromStdString(dev_addrs[i].get("serial", "N/A"));
            QString displayedName(QString("%1[%2:$1] %3").arg(name).arg(i).arg(serial));

            qDebug() << "DeviceUSRP::enumOriginDevices: found USRP device " << displayedName;

            DeviceUSRPParams usrpParams;
            usrpParams.open(id, true);
            usrpParams.close();

            originDevices.append(PluginInterface::OriginDevice(
                    displayedName,
                    hardwareId,
                    id,
                    (int)i,
                    usrpParams.m_nbRxChannels,
                    usrpParams.m_nbTxChannels
                ));
        }
    }
    catch (const std::exception& e)
    {
        qDebug() << "DeviceUSRP::enumOriginDevices: exception: " << e.what();
    }
}

void DeviceUSRP::waitForLock(uhd::usrp::multi_usrp::sptr usrp, const QString& clockSource, int channel)
{
    int tries;
    const int maxTries = 100;

    // Wait for Ref lock
    std::vector<std::string> sensor_names;
    sensor_names = usrp->get_tx_sensor_names(channel);
    if (clockSource == "external")
    {
        if (std::find(sensor_names.begin(), sensor_names.end(), "ref_locked") != sensor_names.end())
        {
            for (tries = 0; !usrp->get_mboard_sensor("ref_locked", 0).to_bool() && (tries < maxTries); tries++)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (tries == maxTries)
                qCritical("USRPInput::acquireChannel: Failed to lock ref");
        }
    }
    // Wait for LO lock
    if (std::find(sensor_names.begin(), sensor_names.end(), "lo_locked") != sensor_names.end())
    {
        for (tries = 0; !usrp->get_tx_sensor("lo_locked", channel).to_bool() && (tries < maxTries); tries++)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (tries == maxTries)
            qCritical("USRPInput::acquireChannel: Failed to lock LO");
    }
}
