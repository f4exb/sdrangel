///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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
#ifndef DEVICES_SPECTRAN_DEVICESPECTRAN_H
#define DEVICES_SPECTRAN_DEVICESPECTRAN_H

#include <aaroniartsaapi.h>

#include "plugin/plugininterface.h"
#include "export.h"

class AARTSAAPI_Handle;

class DEVICES_API DeviceSpectran
{
public:
    static DeviceSpectran& instance();
    static AARTSAAPI_Handle* getLibraryHandle();
    static void enumOriginDevices(const QString& hardwareId, PluginInterface::OriginDevices& originDevices);

protected:
    DeviceSpectran();
    DeviceSpectran(const DeviceSpectran&) {}
    DeviceSpectran& operator=(const DeviceSpectran& other) { (void) other; return *this; }
    ~DeviceSpectran();

private:
    static AARTSAAPI_Handle m_aartsaapiHandle;
    static bool m_successfullyInitialized;
};

#endif // DEVICES_SPECTRAN_DEVICESPECTRAN_H
