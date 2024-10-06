///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                            //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
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
#include "perseus-sdr.h"

#include "plugin/pluginapi.h"
#include "perseus/deviceperseus.h"
#include "perseusplugin.h"
#include "perseuswebapiadapter.h"

#ifdef SERVER_MODE
#include "perseusinput.h"
#else
#include "perseusgui.h"
#endif

const PluginDescriptor PerseusPlugin::m_pluginDescriptor = {
    QStringLiteral("Perseus"),
	QStringLiteral("Perseus Input"),
    QStringLiteral("7.22.1"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "Perseus";
static constexpr const char* const m_deviceTypeID = PERSEUS_DEVICE_TYPE_ID;
const int PerseusPlugin::m_maxDevices = 32;

PerseusPlugin::PerseusPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& PerseusPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void PerseusPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleSource(m_deviceTypeID, this);
}

void PerseusPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

    DevicePerseus::instance().scan();
    std::vector<std::string> serials;
    DevicePerseus::instance().getSerials(serials);

    std::vector<std::string>::const_iterator it = serials.begin();
    int i;

	for (i = 0; it != serials.end(); ++it, ++i)
	{
	    QString serial_str = QString::fromLocal8Bit(it->c_str());
	    QString displayableName(QString("Perseus[%1] %2").arg(i).arg(serial_str));

        originDevices.append(OriginDevice(
            displayableName,
            m_hardwareID,
            serial_str,
            i, // sequence
            1, // Nb Rx
            0  // Nb Tx
        ));

        qDebug("PerseusPlugin::enumOriginDevices: enumerated Perseus device #%d", i);
	}

    listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices PerseusPlugin::enumSampleSources(const OriginDevices& originDevices)
{
	SamplingDevices result;

	for (OriginDevices::const_iterator it = originDevices.begin(); it != originDevices.end(); ++it)
    {
        if (it->hardwareId == m_hardwareID)
        {
            result.append(SamplingDevice(
                it->displayableName,
                it->hardwareId,
                m_deviceTypeID,
                it->serial,
                it->sequence,
                PluginInterface::SamplingDevice::PhysicalDevice,
                PluginInterface::SamplingDevice::StreamSingleRx,
                1,
                0
            ));
            qDebug("PerseusPlugin::enumSampleSources: enumerated Perseus device #%d", it->sequence);
        }
    }

	return result;
}

#ifdef SERVER_MODE
DeviceGUI* PerseusPlugin::createSampleSourcePluginInstanceGUI(
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
DeviceGUI* PerseusPlugin::createSampleSourcePluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if (sourceId == m_deviceTypeID)
	{
	    PerseusGui* gui = new PerseusGui(deviceUISet);
		*widget = gui;
		return gui;
	}
	else
	{
		return 0;
	}
}
#endif

DeviceSampleSource *PerseusPlugin::createSampleSourcePluginInstance(const QString& sourceId, DeviceAPI *deviceAPI)
{
    if (sourceId == m_deviceTypeID)
    {
        PerseusInput* input = new PerseusInput(deviceAPI);
        return input;
    }
    else
    {
        return 0;
    }
}

DeviceWebAPIAdapter *PerseusPlugin::createDeviceWebAPIAdapter() const
{
    return new PerseusWebAPIAdapter();
}
