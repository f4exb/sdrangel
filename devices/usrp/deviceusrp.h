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

#ifndef DEVICES_USRP_DEVICEUSRP_H_
#define DEVICES_USRP_DEVICEUSRP_H_

#include <QString>

#include <uhd/usrp/multi_usrp.hpp>

#include "plugin/plugininterface.h"
#include "export.h"

class DEVICES_API DeviceUSRP
{
public:

    /** Enumeration of USRP hardware devices */
    static void enumOriginDevices(const QString& hardwareId, PluginInterface::OriginDevices& originDevices);

    /** Wait for ref clock and LO to lock */
    static void waitForLock(uhd::usrp::multi_usrp::sptr usrp, const QString& clockSource, int channel);
};

#endif /* DEVICES_USRP_DEVICEUSRP_H_ */
