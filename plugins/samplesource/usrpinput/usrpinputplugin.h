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

#ifndef PLUGINS_SAMPLESOURCE_USRPINPUT_USRPINPUTPLUGIN_H_
#define PLUGINS_SAMPLESOURCE_USRPINPUT_USRPINPUTPLUGIN_H_

#include <QObject>
#include "plugin/plugininterface.h"

class PluginAPI;

#define USRP_DEVICE_TYPE_ID "sdrangel.samplesource.usrp"

class USRPInputPlugin : public QObject, public PluginInterface {
    Q_OBJECT
    Q_INTERFACES(PluginInterface)
    Q_PLUGIN_METADATA(IID USRP_DEVICE_TYPE_ID)

public:
    explicit USRPInputPlugin(QObject* parent = 0);

    const PluginDescriptor& getPluginDescriptor() const;
    void initPlugin(PluginAPI* pluginAPI);

    virtual void enumOriginDevices(QStringList& listedHwIds, OriginDevices& originDevices);
    virtual SamplingDevices enumSampleSources(const OriginDevices& originDevices);
    virtual DeviceGUI* createSampleSourcePluginInstanceGUI(
            const QString& sourceId,
            QWidget **widget,
            DeviceUISet *deviceUISet);
    virtual DeviceSampleSource* createSampleSourcePluginInstance(const QString& sourceId, DeviceAPI *deviceAPI);
    virtual DeviceWebAPIAdapter* createDeviceWebAPIAdapter() const;

    virtual QString getDeviceTypeId() const { return m_deviceTypeID; }

    static const char* const m_deviceTypeID;

private:
    static const PluginDescriptor m_pluginDescriptor;
};


#endif /* PLUGINS_SAMPLESOURCE_USRPINPUT_USRPINPUTPLUGIN_H_ */
