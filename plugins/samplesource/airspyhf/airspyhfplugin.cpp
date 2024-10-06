///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
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
#include <libairspyhf/airspyhf.h>

#include "plugin/pluginapi.h"
#include "airspyhfplugin.h"
#include "airspyhfwebapiadapter.h"
#ifdef SERVER_MODE
#include "airspyhfinput.h"
#else
#include "airspyhfgui.h"
#endif
#ifdef ANDROID
#include "util/android.h"
#endif

const PluginDescriptor AirspyHFPlugin::m_pluginDescriptor = {
    QStringLiteral("AirspyHF"),
	QStringLiteral("AirspyHF Input"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "AirspyHF";
static constexpr const char* const m_deviceTypeID = AIRSPYHF_DEVICE_TYPE_ID;
const int AirspyHFPlugin::m_maxDevices = 32;

AirspyHFPlugin::AirspyHFPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& AirspyHFPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void AirspyHFPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

void AirspyHFPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

	int nbDevices;
	uint64_t deviceSerials[m_maxDevices];

#ifdef ANDROID
    QStringList serialStrings = Android::listUSBDeviceSerials(0x03EB, 0x800C);
    nbDevices = 0;
    for (const auto serialString : serialStrings)
    {
        // Serials are of the form: "AIRSPYHF SN:12345678ABCDEF00"
        int idx = serialString.indexOf(':');
        if (idx != -1)
        {
            bool ok;
            uint64_t serialInt = serialString.mid(idx+1).toULongLong(&ok, 16);
            if (ok)
            {
                deviceSerials[nbDevices] = serialInt;
                nbDevices++;
            }
            else
            {
                qDebug() << "AirspyHFPlugin::enumOriginDevices: Couldn't convert " << serialString.mid(idx+1) << " to integer";
            }
        }
        else
        {
            qDebug() << "AirspyHFPlugin::enumOriginDevices: No : in serial " << serialString;
        }
    }
#else
	nbDevices = airspyhf_list_devices(deviceSerials, m_maxDevices);
#endif

    if (nbDevices < 0)
    {
        qCritical("AirspyHFPlugin::enumOriginDevices: failed to list Airspy HF devices");
    }

	for (int i = 0; i < nbDevices; i++)
	{
	    if (deviceSerials[i])
	    {
            QString serial_str = QString::number(deviceSerials[i], 16);
            QString displayableName(QString("AirspyHF[%1] %2").arg(i).arg(serial_str));

            originDevices.append(OriginDevice(
                displayableName,
                m_hardwareID,
                serial_str,
                i,
                1,
                0
            ));

            qDebug("AirspyHFPlugin::enumOriginDevices: enumerated Airspy HF device #%d", i);
	    }
	    else
	    {
            qDebug("AirspyHFPlugin::enumOriginDevices: finished to enumerate Airspy HF. %d devices found", i);
	        break; // finished
	    }
	}

    listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices AirspyHFPlugin::enumSampleSources(const OriginDevices& originDevices)
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
            qDebug("AirspyHFPlugin::enumSampleSources: enumerated Airspy HF device #%d", it->sequence);
        }
    }

	return result;
}

#ifdef SERVER_MODE
DeviceGUI* AirspyHFPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
    (void) sourceId;
    (void) widget;
    (void) deviceUISet;
    return 0;
}
#else
DeviceGUI* AirspyHFPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if (sourceId == m_deviceTypeID)
	{
	    AirspyHFGui* gui = new AirspyHFGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}
#endif

DeviceSampleSource *AirspyHFPlugin::createSampleSourcePluginInstance(const QString& sourceId, DeviceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        AirspyHFInput* input = new AirspyHFInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}

DeviceWebAPIAdapter *AirspyHFPlugin::createDeviceWebAPIAdapter() const
{
    return new AirspyHFWebAPIAdapter();
}
