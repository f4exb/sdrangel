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

#include <QtPlugin>
#include <aaroniartsaapi.h>

#include "plugin/pluginapi.h"
#include "spectran/devicespectran.h"

#ifdef SERVER_MODE
#include "spectranmiso.h"
#else
#include "spectranmisogui.h"
#endif

#include "spectranmisoplugin.h"
#include "spectranmisowebapiadapter.h"

const PluginDescriptor SpectranMISOPlugin::m_pluginDescriptor = {
    QStringLiteral("SpectranMISO"),
	QStringLiteral("Spectran MISO"),
    QStringLiteral("7.23.0"),
	QStringLiteral("(c) Edouard Griffiths, F4EXB"),
	QStringLiteral("https://github.com/f4exb/sdrangel"),
	true,
	QStringLiteral("https://github.com/f4exb/sdrangel")
};

static constexpr const char* const m_hardwareID = "SpectranMISO";
static constexpr const char* const m_deviceTypeID = SPECTRANMISO_DEVICE_TYPE_ID;

SpectranMISOPlugin::SpectranMISOPlugin(QObject* parent) :
	QObject(parent)
{
}

const PluginDescriptor& SpectranMISOPlugin::getPluginDescriptor() const
{
	return m_pluginDescriptor;
}

void SpectranMISOPlugin::initPlugin(PluginAPI* pluginAPI)
{
	pluginAPI->registerSampleMIMO(m_deviceTypeID, this);
}

void SpectranMISOPlugin::enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices)
{
    if (listedHwIds.contains(m_hardwareID)) { // check if it was done
        return;
    }

    DeviceSpectran::instance().enumOriginDevices(m_hardwareID, originDevices);
    listedHwIds.append(m_hardwareID);
}

PluginInterface::SamplingDevices SpectranMISOPlugin::enumSampleMIMO(const OriginDevices& originDevices)
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
                    PluginInterface::SamplingDevice::StreamMIMO,
                    1,    // MIMO is always considered as a single device
                    0)
            );
            qDebug("SpectranMISOPlugin::enumSampleMIMO: enumerated Spectran device #%d serial: %s",
                it->sequence, it->serial.toStdString().c_str());
        }
    }

	return result;
}

#ifdef SERVER_MODE
DeviceGUI* SpectranMISOPlugin::createSampleMIMOPluginInstanceGUI(
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
DeviceGUI* SpectranMISOPlugin::createSampleMIMOPluginInstanceGUI(
        const QString& sourceId,
        QWidget **widget,
        DeviceUISet *deviceUISet)
{
	if (sourceId == m_deviceTypeID) {
		SpectranMISOGui* gui = new SpectranMISOGui(deviceUISet);
		*widget = gui;
		return gui;
	} else {
		return nullptr;
	}
}
#endif

DeviceSampleMIMO *SpectranMISOPlugin::createSampleMIMOPluginInstance(const QString& mimoId, DeviceAPI *deviceAPI)
{
    if (mimoId == m_deviceTypeID)
    {
        SpectranMISO* input = new SpectranMISO(deviceAPI);
        return input;
    }
    else
    {
        return nullptr;
    }
}

DeviceWebAPIAdapter *SpectranMISOPlugin::createDeviceWebAPIAdapter() const
{
    return new SpectranMISOWebAPIAdapter();
}
