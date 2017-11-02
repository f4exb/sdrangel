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

#include <QGlobalStatic>

#include "plugin/pluginmanager.h"
#include "deviceenumerator.h"

Q_GLOBAL_STATIC(DeviceEnumerator, deviceEnumerator)
DeviceEnumerator *DeviceEnumerator::instance()
{
    return deviceEnumerator;
}

DeviceEnumerator::DeviceEnumerator()
{}

DeviceEnumerator::~DeviceEnumerator()
{}

void DeviceEnumerator::enumerateRxDevices(PluginManager *pluginManager)
{
    m_rxEnumeration.clear();
    PluginAPI::SamplingDeviceRegistrations& rxDeviceRegistrations = pluginManager->getSourceDeviceRegistrations();
    int index = 0;

    for (int i = 0; i < rxDeviceRegistrations.count(); i++)
    {
        PluginInterface::SamplingDevices samplingDevices = rxDeviceRegistrations[i].m_plugin->enumSampleSources();

        for (int j = 0; j < samplingDevices.count(); j++)
        {
            m_rxEnumeration.push_back(
                DeviceEnumeration(
                    samplingDevices[j],
                    rxDeviceRegistrations[i].m_plugin,
                    index
                )
            );
            index++;
        }
    }
}

void DeviceEnumerator::enumerateTxDevices(PluginManager *pluginManager)
{
    m_txEnumeration.clear();
    PluginAPI::SamplingDeviceRegistrations& txDeviceRegistrations = pluginManager->getSinkDeviceRegistrations();
    int index = 0;

    for (int i = 0; i < txDeviceRegistrations.count(); i++)
    {
        PluginInterface::SamplingDevices samplingDevices = txDeviceRegistrations[i].m_plugin->enumSampleSinks();

        for (int j = 0; j < samplingDevices.count(); j++)
        {
            m_txEnumeration.push_back(
                DeviceEnumeration(
                    samplingDevices[j],
                    txDeviceRegistrations[i].m_plugin,
                    index
                )
            );
            index++;
        }
    }
}

void DeviceEnumerator::listRxDeviceNames(QList<QString>& list, std::vector<int>& indexes) const
{
    for (DevicesEnumeration::const_iterator it = m_rxEnumeration.begin(); it != m_rxEnumeration.end(); ++it)
    {
        if ((it->m_samplingDevice.claimed < 0) || (it->m_samplingDevice.type == PluginInterface::SamplingDevice::BuiltInDevice))
        {
            list.append(it->m_samplingDevice.displayedName);
            indexes.push_back(it->m_index);
        }
    }
}

void DeviceEnumerator::listTxDeviceNames(QList<QString>& list, std::vector<int>& indexes) const
{
    for (DevicesEnumeration::const_iterator it = m_txEnumeration.begin(); it != m_txEnumeration.end(); ++it)
    {
        if ((it->m_samplingDevice.claimed < 0) || (it->m_samplingDevice.type == PluginInterface::SamplingDevice::BuiltInDevice))
        {
            list.append(it->m_samplingDevice.displayedName);
            indexes.push_back(it->m_index);
        }
    }
}

void DeviceEnumerator::changeRxSelection(int tabIndex, int deviceIndex)
{
    for (DevicesEnumeration::iterator it = m_rxEnumeration.begin(); it != m_rxEnumeration.end(); ++it)
    {
        if (it->m_samplingDevice.claimed == tabIndex) {
            it->m_samplingDevice.claimed = -1;
        }
        if (it->m_index == deviceIndex) {
            it->m_samplingDevice.claimed = tabIndex;
        }
    }
}

void DeviceEnumerator::changeTxSelection(int tabIndex, int deviceIndex)
{
    for (DevicesEnumeration::iterator it = m_txEnumeration.begin(); it != m_txEnumeration.end(); ++it)
    {
        if (it->m_samplingDevice.claimed == tabIndex) {
            it->m_samplingDevice.claimed = -1;
        }
        if (it->m_index == deviceIndex) {
            it->m_samplingDevice.claimed = tabIndex;
        }
    }
}

void DeviceEnumerator::removeRxSelection(int tabIndex)
{
    for (DevicesEnumeration::iterator it = m_rxEnumeration.begin(); it != m_rxEnumeration.end(); ++it)
    {
        if (it->m_samplingDevice.claimed == tabIndex) {
            it->m_samplingDevice.claimed = -1;
        }
    }
}

void DeviceEnumerator::removeTxSelection(int tabIndex)
{
    for (DevicesEnumeration::iterator it = m_txEnumeration.begin(); it != m_txEnumeration.end(); ++it)
    {
        if (it->m_samplingDevice.claimed == tabIndex) {
            it->m_samplingDevice.claimed = -1;
        }
    }
}

int DeviceEnumerator::getFileSourceDeviceIndex() const
{
    for (DevicesEnumeration::const_iterator it = m_rxEnumeration.begin(); it != m_rxEnumeration.end(); ++it)
    {
        if (it->m_samplingDevice.id == PluginManager::getFileSourceDeviceId()) {
            return it->m_index;
        }
    }

    return -1;
}

int DeviceEnumerator::getFileSinkDeviceIndex() const
{
    for (DevicesEnumeration::const_iterator it = m_txEnumeration.begin(); it != m_txEnumeration.end(); ++it)
    {
        if (it->m_samplingDevice.id == PluginManager::getFileSinkDeviceId()) {
            return it->m_index;
        }
    }

    return -1;
}

int DeviceEnumerator::getRxSamplingDeviceIndex(const QString& deviceId, int sequence)
{
    for (DevicesEnumeration::iterator it = m_rxEnumeration.begin(); it != m_rxEnumeration.end(); ++it)
    {
        if ((it->m_samplingDevice.id == deviceId) && (it->m_samplingDevice.sequence == sequence)) {
            return it->m_index;
        }
    }

    return -1;
}

int DeviceEnumerator::getTxSamplingDeviceIndex(const QString& deviceId, int sequence)
{
    for (DevicesEnumeration::iterator it = m_txEnumeration.begin(); it != m_txEnumeration.end(); ++it)
    {
        if ((it->m_samplingDevice.id == deviceId) && (it->m_samplingDevice.sequence == sequence)) {
            return it->m_index;
        }
    }

    return -1;
}

