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

#ifndef SDRBASE_DEVICE_DEVICEENUMERATOR_H_
#define SDRBASE_DEVICE_DEVICEENUMERATOR_H_

#include <vector>

#include "plugin/plugininterface.h"

class PluginManager;

class DeviceEnumerator
{
public:
    DeviceEnumerator();
    ~DeviceEnumerator();

    static DeviceEnumerator *instance();

    void enumerateRxDevices(PluginManager *pluginManager);
    void enumerateTxDevices(PluginManager *pluginManager);
    void listRxDeviceNames(QList<QString>& list, std::vector<int>& indexes) const;
    void listTxDeviceNames(QList<QString>& list, std::vector<int>& indexes) const;
    void changeRxSelection(int tabIndex, int deviceIndex);
    void changeTxSelection(int tabIndex, int deviceIndex);
    void removeRxSelection(int tabIndex);
    void removeTxSelection(int tabIndex);
    PluginInterface::SamplingDevice getRxSamplingDevice(int deviceIndex) const { return m_rxEnumeration[deviceIndex].m_samplingDevice; }
    PluginInterface::SamplingDevice getTxSamplingDevice(int deviceIndex) const { return m_txEnumeration[deviceIndex].m_samplingDevice; }
    PluginInterface *getRxPluginInterface(int deviceIndex) { return m_rxEnumeration[deviceIndex].m_pluginInterface; }
    PluginInterface *getTxPluginInterface(int deviceIndex) { return m_txEnumeration[deviceIndex].m_pluginInterface; }
    int getFileSourceDeviceIndex() const;
    int getFileSinkDeviceIndex() const;
    int getRxSamplingDeviceIndex(const QString& deviceId, int sequence);
    int getTxSamplingDeviceIndex(const QString& deviceId, int sequence);

private:
    struct DeviceEnumeration
    {
        PluginInterface::SamplingDevice m_samplingDevice;
        PluginInterface *m_pluginInterface;
        int m_index;

        DeviceEnumeration(const PluginInterface::SamplingDevice& samplingDevice, PluginInterface *pluginInterface, int index) :
            m_samplingDevice(samplingDevice),
            m_pluginInterface(pluginInterface),
            m_index(index)
        {}
    };

    typedef std::vector<DeviceEnumeration> DevicesEnumeration;

    DevicesEnumeration m_rxEnumeration;
    DevicesEnumeration m_txEnumeration;
};

#endif /* SDRBASE_DEVICE_DEVICEENUMERATOR_H_ */
