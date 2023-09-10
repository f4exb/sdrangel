///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_ANDROIDSDRDRIVERINPUTPLUGIN_H
#define INCLUDE_ANDROIDSDRDRIVERINPUTPLUGIN_H

#include <QObject>
#include "plugin/plugininterface.h"

#define ANDROIDSDRDRIVERINPUT_DEVICE_TYPE_ID "sdrangel.samplesource.androidsdrdriverinput"

class PluginAPI;

class AndroidSDRDriverInputPlugin : public QObject, public PluginInterface {
    Q_OBJECT
    Q_INTERFACES(PluginInterface)
    Q_PLUGIN_METADATA(IID ANDROIDSDRDRIVERINPUT_DEVICE_TYPE_ID)

public:
    explicit AndroidSDRDriverInputPlugin(QObject* parent = NULL);

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

private:
    static const PluginDescriptor m_pluginDescriptor;
};

#endif // INCLUDE_ANDROIDSDRDRIVERINPUTPLUGIN_H
