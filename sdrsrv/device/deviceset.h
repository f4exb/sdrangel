///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRSRV_DEVICE_DEVICESET_H_
#define SDRSRV_DEVICE_DEVICESET_H_

#include <QTimer>

class DeviceAPI;
class DSPDeviceSourceEngine;
class DSPDeviceSinkEngine;
class DSPDeviceMIMOEngine;
class PluginAPI;
class ChannelAPI;
class Preset;

class DeviceSet
{
public:
    DeviceAPI *m_deviceAPI;
    DSPDeviceSourceEngine *m_deviceSourceEngine;
    DSPDeviceSinkEngine *m_deviceSinkEngine;
    DSPDeviceMIMOEngine *m_deviceMIMOEngine;

    DeviceSet(int tabIndex);
    ~DeviceSet();

    int getNumberOfRxChannels() const { return m_rxChannelInstanceRegistrations.size(); }
    int getNumberOfTxChannels() const { return m_txChannelInstanceRegistrations.size(); }
    void addRxChannel(int selectedChannelIndex, PluginAPI *pluginAPI);
    void addTxChannel(int selectedChannelIndex, PluginAPI *pluginAPI);
    void deleteRxChannel(int channelIndex);
    void deleteTxChannel(int channelIndex);
    void registerRxChannelInstance(const QString& channelName, ChannelAPI* channelAPI);
    void registerTxChannelInstance(const QString& channelName, ChannelAPI* channelAPI);
    void removeRxChannelInstance(ChannelAPI* channelAPI);
    void removeTxChannelInstance(ChannelAPI* channelAPI);
    void freeRxChannels();
    void freeTxChannels();
    void loadRxChannelSettings(const Preset* preset, PluginAPI *pluginAPI);
    void saveRxChannelSettings(Preset* preset);
    void loadTxChannelSettings(const Preset* preset, PluginAPI *pluginAPI);
    void saveTxChannelSettings(Preset* preset);

private:
    struct ChannelInstanceRegistration
    {
        QString m_channelName;
        ChannelAPI *m_channelSinkAPI;
        ChannelAPI *m_channelSourceAPI;

        ChannelInstanceRegistration() :
            m_channelName(),
            m_channelSinkAPI(nullptr),
            m_channelSourceAPI(nullptr)
        { }

        ChannelInstanceRegistration(const QString& channelName, ChannelAPI* channelAPI);

        bool operator<(const ChannelInstanceRegistration& other) const;
    };

    typedef QList<ChannelInstanceRegistration> ChannelInstanceRegistrations;

    ChannelInstanceRegistrations m_rxChannelInstanceRegistrations;
    ChannelInstanceRegistrations m_txChannelInstanceRegistrations;
    int m_deviceTabIndex;

    void renameRxChannelInstances();
    void renameTxChannelInstances();
    /** Use this function to support possible older identifiers in presets */
    bool compareRxChannelURIs(const QString& registerdChannelURI, const QString& xChannelURI);
};

#endif /* SDRSRV_DEVICE_DEVICESET_H_ */
