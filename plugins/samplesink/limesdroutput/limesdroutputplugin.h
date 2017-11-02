///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_SAMPLESOURCE_LIMESDROUTPUT_LIMESDROUTPUTPLUGIN_H_
#define PLUGINS_SAMPLESOURCE_LIMESDROUTPUT_LIMESDROUTPUTPLUGIN_H_

#include <QObject>
#include "plugin/plugininterface.h"

class PluginAPI;

#define LIMESDROUTPUT_DEVICE_TYPE_ID "sdrangel.samplesink.limesdr"

class LimeSDROutputPlugin : public QObject, public PluginInterface {
    Q_OBJECT
    Q_INTERFACES(PluginInterface)
    Q_PLUGIN_METADATA(IID LIMESDROUTPUT_DEVICE_TYPE_ID)

public:
    explicit LimeSDROutputPlugin(QObject* parent = 0);

    const PluginDescriptor& getPluginDescriptor() const;
    void initPlugin(PluginAPI* pluginAPI);

    virtual SamplingDevices enumSampleSinks();
    virtual PluginInstanceGUI* createSampleSinkPluginInstanceGUI(
            const QString& sinkId,
            QWidget **widget,
            DeviceUISet *deviceUISet);
    virtual DeviceSampleSink* createSampleSinkPluginInstanceOutput(const QString& sinkId, DeviceSinkAPI *deviceAPI);

    static const QString m_hardwareID;
    static const QString m_deviceTypeID;

private:
    static const PluginDescriptor m_pluginDescriptor;
    static bool findSerial(const char *lmsInfoStr, std::string& serial);
};


#endif /* PLUGINS_SAMPLESOURCE_LIMESDROUTPUT_LIMESDROUTPUTPLUGIN_H_ */
