///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_SAMPLESOURCE_SDRPLAY_SDRPLAYPLUGIN_H_
#define PLUGINS_SAMPLESOURCE_SDRPLAY_SDRPLAYPLUGIN_H_

#include <QObject>
#include "plugin/plugininterface.h"

class PluginAPI;

#define SDRPLAY_DEVICE_TYPE_ID "sdrangel.samplesource.sdrplay"

class SDRPlayPlugin : public QObject, public PluginInterface {
    Q_OBJECT
    Q_INTERFACES(PluginInterface)
    Q_PLUGIN_METADATA(IID SDRPLAY_DEVICE_TYPE_ID)

public:
    explicit SDRPlayPlugin(QObject* parent = NULL);

    const PluginDescriptor& getPluginDescriptor() const;
    void initPlugin(PluginAPI* pluginAPI);

    virtual SamplingDevices enumSampleSources();
    virtual PluginInstanceGUI* createSampleSourcePluginInstanceGUI(
            const QString& sourceId,
            QWidget **widget,
            DeviceUISet *deviceUISet);
    virtual DeviceSampleSource* createSampleSourcePluginInstanceInput(const QString& sourceId, DeviceSourceAPI *deviceAPI);

    static const QString m_hardwareID;
    static const QString m_deviceTypeID;

private:
    static const PluginDescriptor m_pluginDescriptor;
};

#endif /* PLUGINS_SAMPLESOURCE_SDRPLAY_SDRPLAYPLUGIN_H_ */
