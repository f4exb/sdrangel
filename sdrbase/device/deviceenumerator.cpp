///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017, 2019-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2022-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#include <QGlobalStatic>

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

void DeviceEnumerator::addNonDiscoverableDevices(PluginManager *pluginManager, const DeviceUserArgs& deviceUserArgs)
{
    qDebug("DeviceEnumerator::addNonDiscoverableDevices: start");
    const QList<DeviceUserArgs::Args>& args = deviceUserArgs.getArgsByDevice();
    QList<DeviceUserArgs::Args>::const_iterator argsIt = args.begin();
    unsigned int rxIndex = m_rxEnumeration.size();
    unsigned int txIndex = m_txEnumeration.size();
    unsigned int mimoIndex = m_mimoEnumeration.size();

    for (; argsIt != args.end(); ++argsIt)
    {
        if (!argsIt->m_nonDiscoverable) { // this process is for non discoverable devices only
            continue;
        }

        // qDebug("DeviceEnumerator::addNonDiscoverableDevices: device: %s[%d]", qPrintable(argsIt->m_id), argsIt->m_sequence);
        QString serial = QString("%1-%2").arg(argsIt->m_id).arg(argsIt->m_sequence);

        PluginInterface *rxPlugin = getRxRegisteredPlugin(pluginManager, argsIt->m_id);

        if (rxPlugin && !isRxEnumerated(argsIt->m_id, argsIt->m_sequence))
        {
            int deviceNbItems = rxPlugin->getDefaultRxNbItems();
            QString deviceId = rxPlugin->getDeviceTypeId();

            for (int deviceIndex = 0; deviceIndex < deviceNbItems; deviceIndex++)
            {
                QString description = QString("%1[%2:%3] user defined").arg(argsIt->m_id).arg(argsIt->m_sequence).arg(deviceIndex);
                qDebug("DeviceEnumerator::addNonDiscoverableDevices: Rx: %s", qPrintable(description));
                PluginInterface::SamplingDevice ndDevice(
                    description,
                    argsIt->m_id,
                    deviceId, // id
                    serial,
                    argsIt->m_sequence,
                    rxPlugin->getSamplingDeviceType(),
                    PluginInterface::SamplingDevice::StreamSingleRx,
                    deviceNbItems, // deviceNbItems
                    deviceIndex    // deviceItemIndex
                );
                m_rxEnumeration.push_back(
                    DeviceEnumeration(
                        ndDevice,
                        rxPlugin,
                        rxIndex
                    )
                );
                rxIndex++;
            }
        }

        PluginInterface *txPlugin = getTxRegisteredPlugin(pluginManager, argsIt->m_id);

        if (txPlugin && !isTxEnumerated(argsIt->m_id, argsIt->m_sequence))
        {
            int deviceNbItems = txPlugin->getDefaultTxNbItems();
            QString deviceId = txPlugin->getDeviceTypeId();

            for (int deviceIndex = 0; deviceIndex < deviceNbItems; deviceIndex++)
            {
                QString description = QString("%1[%2:%3] user defined").arg(argsIt->m_id).arg(argsIt->m_sequence).arg(deviceIndex);
                qDebug("DeviceEnumerator::addNonDiscoverableDevices: Tx: %s", qPrintable(description));
                PluginInterface::SamplingDevice ndDevice(
                    description,
                    argsIt->m_id,
                    deviceId, // id
                    serial,
                    argsIt->m_sequence,
                    txPlugin->getSamplingDeviceType(),
                    PluginInterface::SamplingDevice::StreamSingleTx,
                    deviceNbItems, // deviceNbItems
                    deviceIndex    // deviceItemIndex
                );
                m_txEnumeration.push_back(
                    DeviceEnumeration(
                        ndDevice,
                        txPlugin,
                        txIndex
                    )
                );
                txIndex++;
            }
        }

        PluginInterface *mimoPlugin = getMIMORegisteredPlugin(pluginManager, argsIt->m_id);

        if (mimoPlugin && !isMIMOEnumerated(argsIt->m_id, argsIt->m_sequence))
        {
            int deviceNbItems = mimoPlugin->getDefaultMIMONbItems();
            QString deviceId = mimoPlugin->getDeviceTypeId();

            for (int deviceIndex = 0; deviceIndex < deviceNbItems; deviceIndex++)
            {
                QString description = QString("%1[%2:%3] user defined").arg(argsIt->m_id).arg(argsIt->m_sequence).arg(deviceIndex);
                qDebug("DeviceEnumerator::addNonDiscoverableDevices: MIMO: %s", qPrintable(description));
                PluginInterface::SamplingDevice ndDevice(
                    description,
                    argsIt->m_id,
                    deviceId, // id
                    serial,
                    argsIt->m_sequence,
                    mimoPlugin->getSamplingDeviceType(),
                    PluginInterface::SamplingDevice::StreamMIMO,
                    deviceNbItems, // deviceNbItems
                    deviceIndex    // deviceItemIndex
                );
                m_mimoEnumeration.push_back(
                    DeviceEnumeration(
                        ndDevice,
                        mimoPlugin,
                        mimoIndex
                    )
                );
                mimoIndex++;
            }
        }
    } // loop through user args
}

PluginInterface *DeviceEnumerator::getRxRegisteredPlugin(PluginManager *pluginManager, const QString& deviceHwId)
{
    PluginAPI::SamplingDeviceRegistrations& rxDeviceRegistrations = pluginManager->getSourceDeviceRegistrations();
    PluginInterface *rxPlugin = nullptr;

    for (int i = 0; i < rxDeviceRegistrations.count(); i++)
    {

        if (deviceHwId == rxDeviceRegistrations[i].m_deviceHardwareId)
        {
            rxPlugin = rxDeviceRegistrations[i].m_plugin;
            break;
        }
    }

    return rxPlugin;
}

bool DeviceEnumerator::isRxEnumerated(const QString& deviceHwId, int deviceSequence)
{
    std::vector<DeviceEnumeration>::const_iterator rxIt = m_rxEnumeration.begin();

    for (; rxIt != m_rxEnumeration.end(); ++rxIt)
    {
        if ((rxIt->m_samplingDevice.hardwareId == deviceHwId) && (rxIt->m_samplingDevice.sequence == deviceSequence)) {
            return true;
        }
    }

    return false;
}

PluginInterface *DeviceEnumerator::getTxRegisteredPlugin(PluginManager *pluginManager, const QString& deviceHwId)
{
    PluginAPI::SamplingDeviceRegistrations& txDeviceRegistrations = pluginManager->getSinkDeviceRegistrations();
    PluginInterface *txPlugin = nullptr;

    for (int i = 0; i < txDeviceRegistrations.count(); i++)
    {
        if (deviceHwId == txDeviceRegistrations[i].m_deviceHardwareId)
        {
            txPlugin = txDeviceRegistrations[i].m_plugin;
            break;
        }
    }

    return txPlugin;
}

bool DeviceEnumerator::isTxEnumerated(const QString& deviceHwId, int deviceSequence)
{
    std::vector<DeviceEnumeration>::const_iterator txIt = m_txEnumeration.begin();

    for (; txIt != m_txEnumeration.end(); ++txIt)
    {
        if ((txIt->m_samplingDevice.hardwareId == deviceHwId) && (txIt->m_samplingDevice.sequence == deviceSequence)) {
            return true;
        }
    }

    return false;
}

PluginInterface *DeviceEnumerator::getMIMORegisteredPlugin(PluginManager *pluginManager, const QString& deviceHwId)
{
    PluginAPI::SamplingDeviceRegistrations& mimoDeviceRegistrations = pluginManager->getMIMODeviceRegistrations();
    PluginInterface *mimoPlugin = nullptr;

    for (int i = 0; i < mimoDeviceRegistrations.count(); i++)
    {
        if (deviceHwId == mimoDeviceRegistrations[i].m_deviceHardwareId)
        {
            mimoPlugin = mimoDeviceRegistrations[i].m_plugin;
            break;
        }
    }

    return mimoPlugin;
}

bool DeviceEnumerator::isMIMOEnumerated(const QString& deviceHwId, int deviceSequence)
{
    std::vector<DeviceEnumeration>::const_iterator mimoIt = m_mimoEnumeration.begin();

    for (; mimoIt != m_mimoEnumeration.end(); ++mimoIt)
    {
        if ((mimoIt->m_samplingDevice.hardwareId == deviceHwId) && (mimoIt->m_samplingDevice.sequence == deviceSequence)) {
            return true;
        }
    }

    return false;
}

void DeviceEnumerator::enumerateDevices(PluginAPI::SamplingDeviceRegistrations& deviceRegistrations, DevicesEnumeration& enumeration, PluginInterface::SamplingDevice::StreamType type)
{
    DevicesEnumeration tempEnumeration;
    PluginInterface::OriginDevices originDevices;
    QStringList originDevicesHwIds;

    for (int i = 0; i < deviceRegistrations.count(); i++)
    {
        qDebug("DeviceEnumerator::enumerateDevices: %s", qPrintable(deviceRegistrations[i].m_deviceId));
        emit enumeratingDevices(deviceRegistrations[i].m_deviceId);
        deviceRegistrations[i].m_plugin->enumOriginDevices(originDevicesHwIds, originDevices);
        PluginInterface::SamplingDevices samplingDevices;
        if (type == PluginInterface::SamplingDevice::StreamSingleRx) {
            samplingDevices = deviceRegistrations[i].m_plugin->enumSampleSources(originDevices);
        } else if (type == PluginInterface::SamplingDevice::StreamSingleTx) {
            samplingDevices = deviceRegistrations[i].m_plugin->enumSampleSinks(originDevices);
        } else {
            samplingDevices = deviceRegistrations[i].m_plugin->enumSampleMIMO(originDevices);
        }

        for (int j = 0; j < samplingDevices.count(); j++)
        {
            tempEnumeration.push_back(
                DeviceEnumeration(
                    samplingDevices[j],
                    deviceRegistrations[i].m_plugin,
                    0   // Index will be set when adding to enumeration below
                )
            );
        }
    }

    // We don't remove devices that are no-longer found from the enumeration list, in case their
    // index is referenced in some other object (E.g. DeviceGUI)
    // Instead we mark as removed. If later re-added, then we re-use the same index
    for (DevicesEnumeration::iterator it = enumeration.begin(); it != enumeration.end(); ++it)
    {
        bool found = false;
        for (DevicesEnumeration::iterator it2 = tempEnumeration.begin(); it2 != tempEnumeration.end(); ++it2)
        {
            if (it->m_samplingDevice == it2->m_samplingDevice)
            {
                found = true;
                break;
            }
        }
        if (!found) {
            it->m_samplingDevice.removed = true;
        }
    }

    // Add new entries and update existing (in case re-added or sequence number has changed)
    int index = enumeration.size();
    for (DevicesEnumeration::iterator it = tempEnumeration.begin(); it != tempEnumeration.end(); ++it)
    {

        bool found = false;
        for (DevicesEnumeration::iterator it2 = enumeration.begin(); it2 != enumeration.end(); ++it2)
        {
            if (it->m_samplingDevice == it2->m_samplingDevice)
            {
                it2->m_samplingDevice.removed = false;
                it2->m_samplingDevice.displayedName = it->m_samplingDevice.displayedName;
                found = true;
                break;
            }

        }
        if (!found)
        {
            it->m_index = index++;
            enumeration.push_back(*it);
        }
    }
}

void DeviceEnumerator::enumerateRxDevices(PluginManager *pluginManager)
{
    enumerateDevices(pluginManager->getSourceDeviceRegistrations(), m_rxEnumeration, PluginInterface::SamplingDevice::StreamSingleRx);
}

void DeviceEnumerator::enumerateTxDevices(PluginManager *pluginManager)
{
    enumerateDevices(pluginManager->getSinkDeviceRegistrations(), m_txEnumeration, PluginInterface::SamplingDevice::StreamSingleTx);
}

void DeviceEnumerator::enumerateMIMODevices(PluginManager *pluginManager)
{
    enumerateDevices(pluginManager->getMIMODeviceRegistrations(), m_mimoEnumeration, PluginInterface::SamplingDevice::StreamMIMO);
}

void DeviceEnumerator::listRxDeviceNames(QList<QString>& list, std::vector<int>& indexes) const
{
    for (DevicesEnumeration::const_iterator it = m_rxEnumeration.begin(); it != m_rxEnumeration.end(); ++it)
    {
        if (((it->m_samplingDevice.claimed < 0) && (!it->m_samplingDevice.removed)) || (it->m_samplingDevice.type == PluginInterface::SamplingDevice::BuiltInDevice))
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
        if (((it->m_samplingDevice.claimed < 0) && (!it->m_samplingDevice.removed)) || (it->m_samplingDevice.type == PluginInterface::SamplingDevice::BuiltInDevice))
        {
            list.append(it->m_samplingDevice.displayedName);
            indexes.push_back(it->m_index);
        }
    }
}

void DeviceEnumerator::listMIMODeviceNames(QList<QString>& list, std::vector<int>& indexes) const
{
    for (DevicesEnumeration::const_iterator it = m_mimoEnumeration.begin(); it != m_mimoEnumeration.end(); ++it)
    {
        if (((it->m_samplingDevice.claimed < 0) && (!it->m_samplingDevice.removed)) || (it->m_samplingDevice.type == PluginInterface::SamplingDevice::BuiltInDevice))
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

void DeviceEnumerator::changeMIMOSelection(int tabIndex, int deviceIndex)
{
    for (DevicesEnumeration::iterator it = m_mimoEnumeration.begin(); it != m_mimoEnumeration.end(); ++it)
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

void DeviceEnumerator::removeMIMOSelection(int tabIndex)
{
    for (DevicesEnumeration::iterator it = m_mimoEnumeration.begin(); it != m_mimoEnumeration.end(); ++it)
    {
        if (it->m_samplingDevice.claimed == tabIndex) {
            it->m_samplingDevice.claimed = -1;
        }
    }
}

int DeviceEnumerator::getFileInputDeviceIndex() const
{
    for (DevicesEnumeration::const_iterator it = m_rxEnumeration.begin(); it != m_rxEnumeration.end(); ++it)
    {
        if (it->m_samplingDevice.id == PluginManager::getFileInputDeviceId()) {
            return it->m_index;
        }
    }

    return -1;
}

int DeviceEnumerator::getFileOutputDeviceIndex() const
{
    for (DevicesEnumeration::const_iterator it = m_txEnumeration.begin(); it != m_txEnumeration.end(); ++it)
    {
        if (it->m_samplingDevice.id == PluginManager::getFileOutputDeviceId()) {
            return it->m_index;
        }
    }

    return -1;
}

int DeviceEnumerator::getTestMIMODeviceIndex() const
{
    for (DevicesEnumeration::const_iterator it = m_mimoEnumeration.begin(); it != m_mimoEnumeration.end(); ++it)
    {
        if (it->m_samplingDevice.id == PluginManager::getTestMIMODeviceId()) {
            return it->m_index;
        }
    }

    return -1;
}

int DeviceEnumerator::getRxSamplingDeviceIndex(const QString& deviceId, int sequence, int deviceItemIndex)
{
    for (DevicesEnumeration::iterator it = m_rxEnumeration.begin(); it != m_rxEnumeration.end(); ++it)
    {
        if ((it->m_samplingDevice.id == deviceId)
         && (it->m_samplingDevice.sequence == sequence)
         && (it->m_samplingDevice.deviceItemIndex == deviceItemIndex))
        {
            return it->m_index;
        }
    }

    return -1;
}

int DeviceEnumerator::getTxSamplingDeviceIndex(const QString& deviceId, int sequence, int deviceItemIndex)
{
    for (DevicesEnumeration::iterator it = m_txEnumeration.begin(); it != m_txEnumeration.end(); ++it)
    {
        if ((it->m_samplingDevice.id == deviceId)
         && (it->m_samplingDevice.sequence == sequence)
         && (it->m_samplingDevice.deviceItemIndex == deviceItemIndex))
        {
            return it->m_index;
        }
    }

    return -1;
}

int DeviceEnumerator::getMIMOSamplingDeviceIndex(const QString& deviceId, int sequence)
{
    for (DevicesEnumeration::iterator it = m_mimoEnumeration.begin(); it != m_mimoEnumeration.end(); ++it)
    {
        if ((it->m_samplingDevice.id == deviceId) && (it->m_samplingDevice.sequence == sequence)) {
            return it->m_index;
        }
    }

    return -1;
}

int DeviceEnumerator::getBestRxSamplingDeviceIndex(const QString& deviceId, const QString& deviceSerial, int deviceSequence, int deviceItemIndex)
{
    return getBestSamplingDeviceIndex(m_rxEnumeration, deviceId, deviceSerial, deviceSequence, deviceItemIndex);
}

int DeviceEnumerator::getBestTxSamplingDeviceIndex(const QString& deviceId, const QString& deviceSerial, int deviceSequence, int deviceItemIndex)
{
    return getBestSamplingDeviceIndex(m_txEnumeration, deviceId, deviceSerial, deviceSequence, deviceItemIndex);
}

int DeviceEnumerator::getBestMIMOSamplingDeviceIndex(const QString& deviceId, const QString& deviceSerial, int deviceSequence)
{
    return getBestSamplingDeviceIndex(m_mimoEnumeration, deviceId, deviceSerial, deviceSequence, -1);
}

int DeviceEnumerator::getBestSamplingDeviceIndex(
    const DevicesEnumeration& devicesEnumeration,
    const QString& deviceId,
    const QString& deviceSerial,
    int deviceSequence,
    int deviceItemIndex
)
{
    DevicesEnumeration::const_iterator it = devicesEnumeration.begin();
    DevicesEnumeration::const_iterator itFirstOfKind = devicesEnumeration.end();
    DevicesEnumeration::const_iterator itMatchSequence = devicesEnumeration.end();

    for (; it != devicesEnumeration.end(); ++it)
    {
		if ((it->m_samplingDevice.id == deviceId) &&
            (
                ((deviceItemIndex < 0) || (deviceItemIndex > it->m_samplingDevice.deviceNbItems)) || // take first if item index is negative or out of range
                ((deviceItemIndex <= it->m_samplingDevice.deviceNbItems) && (deviceItemIndex == it->m_samplingDevice.deviceItemIndex)) // take exact item index if in range
            )
        )
		{
			if (itFirstOfKind == devicesEnumeration.end()) {
				itFirstOfKind = it;
			}

			if (deviceSerial.isNull() || deviceSerial.isEmpty())
			{
				if (it->m_samplingDevice.sequence == deviceSequence) {
					break;
				}
			}
			else
			{
				if (it->m_samplingDevice.serial == deviceSerial) {
					break;
				} else if(it->m_samplingDevice.sequence == deviceSequence) {
					itMatchSequence = it;
				}
			}
		}
    }

	if (it == devicesEnumeration.end()) // no exact match
	{
		if (itMatchSequence != devicesEnumeration.end()) // match sequence and device type ?
		{
			qDebug("DeviceEnumerator::getBestSamplingDeviceIndex: sequence matched: id: %s ser: %s seq: %d",
				qPrintable(itMatchSequence->m_samplingDevice.id),
                qPrintable(itMatchSequence->m_samplingDevice.serial),
                itMatchSequence->m_samplingDevice.sequence);
            return itMatchSequence - devicesEnumeration.begin();
		}
		else if (itFirstOfKind != devicesEnumeration.end()) // match just device type ?
		{
			qDebug("DeviceEnumerator::getBestSamplingDeviceIndex: first of kind matched: id: %s ser: %s seq: %d",
				qPrintable(itFirstOfKind->m_samplingDevice.id),
                qPrintable(itFirstOfKind->m_samplingDevice.serial),
                itFirstOfKind->m_samplingDevice.sequence);
            return itFirstOfKind - devicesEnumeration.begin();
		}
		else // definitely not found !
		{
			qDebug("DeviceEnumerator::getBestSamplingDeviceIndex: no match");
			return -1;
		}
	}
	else // exact match
	{
		qDebug("DeviceEnumerator::getBestSamplingDeviceIndex: serial matched (exact): id: %s ser: %s",
			qPrintable(it->m_samplingDevice.id), qPrintable(it->m_samplingDevice.serial));
        return it - devicesEnumeration.begin();
	}
}
