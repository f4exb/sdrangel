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
#include "aaroniartsasdkhelper.h"
#include "devicespectran.h"

AARTSAAPI_Handle DeviceSpectran::m_aartsaapiHandle;
bool DeviceSpectran::m_successfullyInitialized = false;

DeviceSpectran& DeviceSpectran::instance()
{
    static DeviceSpectran instance;
    return instance;
}

AARTSAAPI_Handle* DeviceSpectran::getLibraryHandle()
{
    if (!m_successfullyInitialized)
    {
        qWarning("DeviceSpectran::getLibraryHandle: AARTSAAPI library not initialized");
        return nullptr;
    }

    return &m_aartsaapiHandle;
}

DeviceSpectran::DeviceSpectran()
{
    qDebug("DeviceSpectran::DeviceSpectran: Initializing AARTSAAPI library");

    // Check if the AARTSAAPI library has already been initialized
    if (m_successfullyInitialized)
    {
        qWarning("DeviceSpectran::DeviceSpectran: AARTSAAPI library already initialized");
        return;
    }

    // Initialize the AARTSAAPI library handle
    AARTSAAPI_Result res;

	if (LoadRTSAAPI_with_searchpath() != 0)
	{
		qWarning("DeviceSpectran::DeviceSpectran: Load RTSSAPI failed");
		return;
	}

    if ((res = AARTSAAPI_Init_With_Path(AARTSAAPI_MEMORY_MEDIUM, CFG_AARONIA_XML_LOOKUP_DIRECTORY)) != AARTSAAPI_OK)
    {
        qWarning("DeviceSpectran::DeviceSpectran: AARTSAAPI_Init_With_Path failed with error %d", res);
        AARTSAAPI_Shutdown();
        return;
    }

    if ((res = AARTSAAPI_Open(&m_aartsaapiHandle)) != AARTSAAPI_OK)
    {
        qWarning("DeviceSpectran::DeviceSpectran: AARTSAAPI_Open failed with error %d", res);
        AARTSAAPI_Close(&m_aartsaapiHandle);
        AARTSAAPI_Shutdown();
    }

    m_successfullyInitialized = true;
}

DeviceSpectran::~DeviceSpectran()
{
    qDebug("DeviceSpectran::~DeviceSpectran: Closing AARTSAAPI library");

    if (!m_successfullyInitialized)
    {
        qWarning("DeviceSpectran::~DeviceSpectran: AARTSAAPI library not initialized");
        return;
    }

    AARTSAAPI_Result res = AARTSAAPI_Close(&m_aartsaapiHandle);

    if (res != AARTSAAPI_OK) {
        qWarning("DeviceSpectran::~DeviceSpectran: AARTSAAPI_Close failed with error %d", res);
    }

    // Do not call AARTSAAPI_Shutdown here as it is called by the main application
    // However this should be handled by the plugin manager when the application exits since the library
    // is initialized at enumeration time
    // AARTSAAPI_Shutdown();
    m_successfullyInitialized = false;
}

void DeviceSpectran::enumOriginDevices(const QString& hardwareId, PluginInterface::OriginDevices& originDevices)
{
    instance();

    if (!m_successfullyInitialized)
    {
        qWarning("DeviceSpectran::enumOriginDevices: AARTSAAPI library not initialized");
        return;
    }

    AARTSAAPI_Result res;

    if ((res = AARTSAAPI_RescanDevices(&m_aartsaapiHandle, 2000)) != AARTSAAPI_OK)
    {
        qWarning("DeviceSpectran::enumOriginDevices: AARTSAAPI_RescanDevices failed with error %d", res);
        return;
    }

    // Initialize device info structure with the structure size
    AARTSAAPI_DeviceInfo dinfo = { sizeof(AARTSAAPI_DeviceInfo) };

    // Loop over Spectran V6 devices, starting from zero until an error occurs
    // or we run out of devices
    int i = 0;

    while (AARTSAAPI_EnumDevice(&m_aartsaapiHandle, L"spectranv6", i, &dinfo) == AARTSAAPI_OK)
    {
        QString serial = QString::fromWCharArray(dinfo.serialNumber);
		QString displayableName(QString("SpectranV6[%1] %2").arg(i).arg(serial));

        originDevices.append(PluginInterface::OriginDevice(
            displayableName,
            hardwareId,
            serial,
            i, // sequence
            2, // Nb Rx
            1  // Nb Tx
        ));
        i++;
    }

    // Loop over Spectran V6 Eco devices, starting from zero until an error occurs
    // or we run out of devices
    i = 0;

    while (AARTSAAPI_EnumDevice(&m_aartsaapiHandle, L"spectranv6eco", i, &dinfo) == AARTSAAPI_OK)
    {
        QString serial = QString::fromWCharArray(dinfo.serialNumber);
		QString displayableName(QString("SpectranV6Eco[%1] %2").arg(i).arg(serial));

        originDevices.append(PluginInterface::OriginDevice(
            displayableName,
            hardwareId,
            serial,
            i, // sequence
            2, // Nb Rx
            1  // Nb Tx
        ));
        i++;
    }
}
