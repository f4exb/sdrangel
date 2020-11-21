///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef _BLADERF2MIMO_BLADERF2MIMOPLUGIN_H
#define _BLADERF2MIMO_BLADERF2MIMOPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

class PluginAPI;

#define BLADERF2MIMO_DEVICE_TYPE_ID "sdrangel.samplemimo.bladerf2mimo"

class BladeRF2MIMOPlugin : public QObject, public PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID BLADERF2MIMO_DEVICE_TYPE_ID)

public:
	explicit BladeRF2MIMOPlugin(QObject* parent = nullptr);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	virtual void enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices);
	virtual SamplingDevices enumSampleMIMO(const OriginDevices& originDevices);

	virtual DeviceGUI* createSampleMIMOPluginInstanceGUI(
	        const QString& sourceId,
	        QWidget **widget,
	        DeviceUISet *deviceUISet);
	virtual DeviceSampleMIMO* createSampleMIMOPluginInstance(const QString& sourceId, DeviceAPI *deviceAPI);
    virtual DeviceWebAPIAdapter* createDeviceWebAPIAdapter() const;

private:
	static const PluginDescriptor m_pluginDescriptor;
};

#endif // _BLADERF2MIMO_BLADERF2MIMOPLUGIN_H
