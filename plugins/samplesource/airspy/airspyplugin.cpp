///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                            //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include <QtPlugin>
#include <libairspy/airspy.h>

#ifdef SERVER_MODE
#include "airspyinput.h"
#else
#include "airspygui.h"
#endif
#include "airspywebapiadapter.h"
#include "airspyplugin.h"

#include "plugin/pluginapi.h"

#ifdef ANDROID
#include "util/android.h"
#endif

const int AirspyPlugin::m_maxDevices = 32;

const PluginDescriptor AirspyPlugin::m_pluginDescriptor = {
    QStringLiteral("Airspy"),
	QStringLiteral("Airspy Input"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "Airspy";
static constexpr const char* const m_deviceTypeID = AIRSPY_DEVICE_TYPE_ID;

AirspyPlugin::AirspyPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& AirspyPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void AirspyPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

void AirspyPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

#ifdef ANDROID

    QStringList serialStrings = Android::listUSBDeviceSerials(0x1d50, 0x60a1);
    int deviceNo = 0;
    for (const auto serialString : serialStrings)
    {
        QString displayableName(QString("Airspy[%1] %2").arg(deviceNo).arg(serialString));

        originDevices.append(OriginDevice(
            displayableName,
            m_hardwareID,
            serialString,
            deviceNo,
            1,
            0
        ));
        deviceNo++;
    }

    listedHwIds.append(m_hardwareID);

#else

	airspy_read_partid_serialno_t read_partid_serialno;
	struct airspy_device *devinfo;
	uint32_t serial_msb = 0;
	uint32_t serial_lsb = 0;
	airspy_error rc;
	int i;

	rc = (airspy_error) airspy_init();

	if (rc != AIRSPY_SUCCESS)
	{
		qCritical("AirspyPlugin::enumOriginDevices: failed to initiate Airspy library: %s", airspy_error_name(rc));
	}

	for (i=0; i < m_maxDevices; i++)
	{
		rc = (airspy_error) airspy_open(&devinfo);

		if (rc == AIRSPY_SUCCESS)
		{
			qDebug("AirspyPlugin::enumOriginDevices: try to enumerate Airspy device #%d", i);

			rc = (airspy_error) airspy_board_partid_serialno_read(devinfo, &read_partid_serialno);

			if (rc != AIRSPY_SUCCESS)
			{
				qDebug("AirspyPlugin::enumOriginDevices: failed to read serial no: %s", airspy_error_name(rc));
				airspy_close(devinfo);
				continue; // next
			}

			if ((read_partid_serialno.serial_no[2] != serial_msb) && (read_partid_serialno.serial_no[3] != serial_lsb))
			{
				serial_msb = read_partid_serialno.serial_no[2];
				serial_lsb = read_partid_serialno.serial_no[3];

				QString serial_str = QString::number(serial_msb, 16) + QString::number(serial_lsb, 16);
				//uint64_t serial_num = (((uint64_t) serial_msb)<<32) + serial_lsb;
				QString displayableName(QString("Airspy[%1] %2").arg(i).arg(serial_str));

                originDevices.append(OriginDevice(
                    displayableName,
                    m_hardwareID,
                    serial_str,
                    i,
                    1,
                    0
                ));

				qDebug("AirspyPlugin::enumOriginDevices: enumerated Airspy device #%d", i);
			}

			airspy_close(devinfo);
		}
		else
		{
			qDebug("AirspyPlugin::enumOriginDevices: enumerated %d Airspy devices %s", i, airspy_error_name(rc));
			break; // finished
		}
	}

	rc = (airspy_error) airspy_exit();
	qDebug("AirspyPlugin::enumOriginDevices: airspy_exit: %s", airspy_error_name(rc));

    listedHwIds.append(m_hardwareID);

#endif
}

PluginInterface::SamplingDevices AirspyPlugin::enumSampleSources(const OriginDevices& originDevices)
{
	SamplingDevices result;

	for (OriginDevices::const_iterator it = originDevices.begin(); it != originDevices.end(); ++it)
    {
        if (it->hardwareId == m_hardwareID)
        {
            result.append(SamplingDevice(
                it->displayableName,
                m_hardwareID,
                m_deviceTypeID,
                it->serial,
                it->sequence,
                PluginInterface::SamplingDevice::PhysicalDevice,
                PluginInterface::SamplingDevice::StreamSingleRx,
                1,
                0
            ));
            qDebug("AirspyPlugin::enumSampleSources: enumerated Airspy device #%d", it->sequence);
        }
    }

	return result;
}

#ifdef SERVER_MODE
DeviceGUI* AirspyPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    (void) sourceId;
    (void) widget;
    (void) deviceUISet;
    return nullptr;
}
#else
DeviceGUI* AirspyPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if (sourceId == m_deviceTypeID)
	{
		AirspyGui* gui = new AirspyGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return nullptr;
	}
}
#endif

DeviceSampleSource *AirspyPlugin::createSampleSourcePluginInstance(const QString& sourceId, DeviceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        AirspyInput* input = new AirspyInput(deviceAPI);
        return input;
    }
    else
    {
        return nullptr;
    }
}

DeviceWebAPIAdapter *AirspyPlugin::createDeviceWebAPIAdapter() const
{
    return new AirspyWebAPIAdapter();
}
