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

#ifndef INCLUDE_AARONIARTSAOUTPUTPLUGIN_H
#define INCLUDE_AARONIARTSAOUTPUTPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

#define AARONIARTSAOUTPUT_DEVICE_TYPE_ID "sdrangel.samplesink.aaroniartsaoutput"

class PluginAPI;

class AaroniaRTSAOutputPlugin : public QObject, public PluginInterface {
	Q_OBJECT
	Q_INTERFACES(PluginInterface)
	Q_PLUGIN_METADATA(IID AARONIARTSAOUTPUT_DEVICE_TYPE_ID)

public:
	explicit AaroniaRTSAOutputPlugin(QObject* parent = nullptr);

	const PluginDescriptor& getPluginDescriptor() const;
	void initPlugin(PluginAPI* pluginAPI);

	virtual void enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices);
	virtual SamplingDevices enumSampleSinks(const OriginDevices& originDevices);
	virtual DeviceGUI* createSampleSinkPluginInstanceGUI(
	        const QString& sinkId,
	        QWidget **widget,
	        DeviceUISet *deviceUISet);
	virtual DeviceSampleSink* createSampleSinkPluginInstance(const QString& sinkId, DeviceAPI *deviceAPI);
    virtual DeviceWebAPIAdapter* createDeviceWebAPIAdapter() const;

private:
	static const PluginDescriptor m_pluginDescriptor;
};

#endif // INCLUDE_AARONIARTSAOUTPUTPLUGIN_H
