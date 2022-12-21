///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_DEVICE_DEVICEENUMERATOR_H_
#define SDRBASE_DEVICE_DEVICEENUMERATOR_H_

#include <vector>

#include "plugin/plugininterface.h"
#include "plugin/pluginmanager.h"
#include "device/deviceuserargs.h"
#include "export.h"

class PluginManager;

class SDRBASE_API DeviceEnumerator
{
public:
    DeviceEnumerator();
    ~DeviceEnumerator();

    static DeviceEnumerator *instance();

    void enumerateRxDevices(PluginManager *pluginManager);
    void enumerateTxDevices(PluginManager *pluginManager);
    void enumerateMIMODevices(PluginManager *pluginManager);
    void addNonDiscoverableDevices(PluginManager *pluginManager, const DeviceUserArgs& deviceUserArgs);
    void listRxDeviceNames(QList<QString>& list, std::vector<int>& indexes) const;
    void listTxDeviceNames(QList<QString>& list, std::vector<int>& indexes) const;
    void listMIMODeviceNames(QList<QString>& list, std::vector<int>& indexes) const;
    void changeRxSelection(int tabIndex, int deviceIndex);
    void changeTxSelection(int tabIndex, int deviceIndex);
    void changeMIMOSelection(int tabIndex, int deviceIndex);
    void removeRxSelection(int tabIndex);
    void removeTxSelection(int tabIndex);
    void removeMIMOSelection(int tabIndex);
    int getNbRxSamplingDevices() const { return m_rxEnumeration.size(); }
    int getNbTxSamplingDevices() const { return m_txEnumeration.size(); }
    int getNbMIMOSamplingDevices() const { return m_mimoEnumeration.size(); }
    const PluginInterface::SamplingDevice* getRxSamplingDevice(int deviceIndex) const { return &m_rxEnumeration[deviceIndex].m_samplingDevice; }
    const PluginInterface::SamplingDevice* getTxSamplingDevice(int deviceIndex) const { return &m_txEnumeration[deviceIndex].m_samplingDevice; }
    const PluginInterface::SamplingDevice* getMIMOSamplingDevice(int deviceIndex) const { return &m_mimoEnumeration[deviceIndex].m_samplingDevice; }
    PluginInterface *getRxPluginInterface(int deviceIndex) { return m_rxEnumeration[deviceIndex].m_pluginInterface; }
    PluginInterface *getTxPluginInterface(int deviceIndex) { return m_txEnumeration[deviceIndex].m_pluginInterface; }
    PluginInterface *getMIMOPluginInterface(int deviceIndex) { return m_mimoEnumeration[deviceIndex].m_pluginInterface; }
    int getFileInputDeviceIndex() const;  //!< Get Rx default device
    int getTestMIMODeviceIndex() const;   //!< Get MIMO default device
    int getFileOutputDeviceIndex() const;   //!< Get Tx default device
    int getRxSamplingDeviceIndex(const QString& deviceId, int sequence, int deviceItemIndex);
    int getTxSamplingDeviceIndex(const QString& deviceId, int sequence, int deviceItemIndex);
    int getMIMOSamplingDeviceIndex(const QString& deviceId, int sequence);
    int getBestRxSamplingDeviceIndex(const QString& deviceId, const QString& serial, int sequence, int deviceItemIndex);
    int getBestTxSamplingDeviceIndex(const QString& deviceId, const QString& serial, int sequence, int deviceItemIndex);
    int getBestMIMOSamplingDeviceIndex(const QString& deviceId, const QString& serial, int sequence);

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
    DevicesEnumeration m_mimoEnumeration;

    void enumerateDevices(PluginAPI::SamplingDeviceRegistrations& deviceRegistrations, DevicesEnumeration& enumeration, PluginInterface::SamplingDevice::StreamType type);
    PluginInterface *getRxRegisteredPlugin(PluginManager *pluginManager, const QString& deviceHwId);
    PluginInterface *getTxRegisteredPlugin(PluginManager *pluginManager, const QString& deviceHwId);
    PluginInterface *getMIMORegisteredPlugin(PluginManager *pluginManager, const QString& deviceHwId);
    bool isRxEnumerated(const QString& deviceHwId, int deviceSequence);
    bool isTxEnumerated(const QString& deviceHwId, int deviceSequence);
    bool isMIMOEnumerated(const QString& deviceHwId, int deviceSequence);
    int getBestSamplingDeviceIndex(
        const DevicesEnumeration& devicesEnumeration,
        const QString& deviceId,
        const QString& serial,
        int sequence,
        int deviceItemIndex
    );
};

#endif /* SDRBASE_DEVICE_DEVICEENUMERATOR_H_ */
