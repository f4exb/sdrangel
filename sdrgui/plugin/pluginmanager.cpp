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

#include <QApplication>
#include <QPluginLoader>
#include <QComboBox>
#include <QDebug>

#include <cstdio>

#include <plugin/plugininstancegui.h>
#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "settings/preset.h"
#include "util/message.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"

#include "plugin/pluginmanager.h"

const QString PluginManager::m_sdrDaemonHardwareID = "SDRdaemonSource";
const QString PluginManager::m_sdrDaemonDeviceTypeID = "sdrangel.samplesource.sdrdaemonsource";
const QString PluginManager::m_fileSourceHardwareID = "FileSource";
const QString PluginManager::m_fileSourceDeviceTypeID = "sdrangel.samplesource.filesource";
const QString PluginManager::m_fileSinkDeviceTypeID = "sdrangel.samplesink.filesink";

PluginManager::PluginManager(QObject* parent) :
	QObject(parent),
    m_pluginAPI(this)
{
}

PluginManager::~PluginManager()
{
//	freeAll();
}

void PluginManager::loadPlugins()
{
	QString applicationDirPath = QApplication::instance()->applicationDirPath();
	QString applicationLibPath = applicationDirPath + "/../lib";
	qDebug() << "PluginManager::loadPlugins: " << qPrintable(applicationDirPath) << ", " << qPrintable(applicationLibPath);

	QDir pluginsBinDir = QDir(applicationDirPath);
	QDir pluginsLibDir = QDir(applicationLibPath);

	loadPlugins(pluginsBinDir);
	loadPlugins(pluginsLibDir);

	qSort(m_plugins);

	for (Plugins::const_iterator it = m_plugins.begin(); it != m_plugins.end(); ++it)
	{
		it->pluginInterface->initPlugin(&m_pluginAPI);
	}

	updateSampleSourceDevices();
	updateSampleSinkDevices();
}

void PluginManager::registerRxChannel(const QString& channelName, PluginInterface* plugin)
{
    qDebug() << "PluginManager::registerRxChannel "
            << plugin->getPluginDescriptor().displayedName.toStdString().c_str()
            << " with channel name " << channelName;

	m_rxChannelRegistrations.append(PluginAPI::ChannelRegistration(channelName, plugin));
}

void PluginManager::registerTxChannel(const QString& channelName, PluginInterface* plugin)
{
    qDebug() << "PluginManager::registerTxChannel "
            << plugin->getPluginDescriptor().displayedName.toStdString().c_str()
            << " with channel name " << channelName;

	m_txChannelRegistrations.append(PluginAPI::ChannelRegistration(channelName, plugin));
}

void PluginManager::registerSampleSource(const QString& sourceName, PluginInterface* plugin)
{
	qDebug() << "PluginManager::registerSampleSource "
			<< plugin->getPluginDescriptor().displayedName.toStdString().c_str()
			<< " with source name " << sourceName.toStdString().c_str();

	m_sampleSourceRegistrations.append(SamplingDeviceRegistration(sourceName, plugin));
}

void PluginManager::registerSampleSink(const QString& sinkName, PluginInterface* plugin)
{
	qDebug() << "PluginManager::registerSampleSink "
			<< plugin->getPluginDescriptor().displayedName.toStdString().c_str()
			<< " with sink name " << sinkName.toStdString().c_str();

	m_sampleSinkRegistrations.append(SamplingDeviceRegistration(sinkName, plugin));
}

void PluginManager::updateSampleSourceDevices()
{
	m_sampleSourceDevices.clear();

	for(int i = 0; i < m_sampleSourceRegistrations.count(); ++i)
	{
		PluginInterface::SamplingDevices ssd = m_sampleSourceRegistrations[i].m_plugin->enumSampleSources();

		for(int j = 0; j < ssd.count(); ++j)
		{
			m_sampleSourceDevices.append(SamplingDevice(m_sampleSourceRegistrations[i].m_plugin,
					ssd[j].displayedName,
                    ssd[j].hardwareId,
					ssd[j].id,
					ssd[j].serial,
					ssd[j].sequence));
            qDebug("PluginManager::updateSampleSourceDevices: %s %s %s %s %d",
                    qPrintable(ssd[j].displayedName),
                    qPrintable(ssd[j].hardwareId),
                    qPrintable(ssd[j].id),
                    qPrintable(ssd[j].serial),
                    ssd[j].sequence);
		}
	}
}

void PluginManager::updateSampleSinkDevices()
{
	m_sampleSinkDevices.clear();

	for(int i = 0; i < m_sampleSinkRegistrations.count(); ++i)
	{
		PluginInterface::SamplingDevices ssd = m_sampleSinkRegistrations[i].m_plugin->enumSampleSinks();

		for(int j = 0; j < ssd.count(); ++j)
		{
			m_sampleSinkDevices.append(SamplingDevice(m_sampleSinkRegistrations[i].m_plugin,
					ssd[j].displayedName,
					ssd[j].hardwareId,
                    ssd[j].id,
					ssd[j].serial,
					ssd[j].sequence));
            qDebug("PluginManager::updateSampleSinkDevices: %s %s %s %s %d",
                    qPrintable(ssd[j].displayedName),
                    qPrintable(ssd[j].hardwareId),
                    qPrintable(ssd[j].id),
                    qPrintable(ssd[j].serial),
                    ssd[j].sequence);
		}
	}
}

void PluginManager::duplicateLocalSampleSourceDevices(uint deviceUID)
{
    if (deviceUID == 0) {
        return;
    }

    SamplingDevice *sdrDaemonSSD0 = 0;
    SamplingDevice *fileSourceSSD0 = 0;
    bool duplicateSDRDaemon = true;
    bool duplicateFileSource = true;

    for(int i = 0; i < m_sampleSourceDevices.count(); ++i)
    {
        if (m_sampleSourceDevices[i].m_deviceId == m_sdrDaemonDeviceTypeID) // SDRdaemon
        {
            if (m_sampleSourceDevices[i].m_deviceSequence == 0) { // reference to device 0
                sdrDaemonSSD0 = &m_sampleSourceDevices[i];
            }
            else if (m_sampleSourceDevices[i].m_deviceSequence == deviceUID) { // already there
                duplicateSDRDaemon = false;
            }
        }
        else if (m_sampleSourceDevices[i].m_deviceId == m_fileSourceDeviceTypeID) // File Source
        {
            if (m_sampleSourceDevices[i].m_deviceSequence == 0) { // reference to device 0
                fileSourceSSD0 = &m_sampleSourceDevices[i];
            }
            else if (m_sampleSourceDevices[i].m_deviceSequence == deviceUID) { // already there
                duplicateFileSource = false;
            }
        }
    }

    if (sdrDaemonSSD0 && duplicateSDRDaemon) // append item for a new instance
    {
        m_sampleSourceDevices.append(
            SamplingDevice(
                sdrDaemonSSD0->m_plugin,
                QString("SDRdaemonSource[%1]").arg(deviceUID),
                sdrDaemonSSD0->m_hadrwareId,
                sdrDaemonSSD0->m_deviceId,
                sdrDaemonSSD0->m_deviceSerial,
                deviceUID
            )
        );
    }

    if (fileSourceSSD0 && duplicateFileSource) // append item for a new instance
    {
        m_sampleSourceDevices.append(
            SamplingDevice(
                fileSourceSSD0->m_plugin,
                QString("FileSource[%1]").arg(deviceUID),
                fileSourceSSD0->m_hadrwareId,
                fileSourceSSD0->m_deviceId,
                fileSourceSSD0->m_deviceSerial,
                deviceUID
            )
        );
    }
}

void PluginManager::duplicateLocalSampleSinkDevices(uint deviceUID)
{
    if (deviceUID == 0) {
        return;
    }

    SamplingDevice *fileSinkSSD0 = 0;
    bool duplicateFileSink = true;

    for(int i = 0; i < m_sampleSinkDevices.count(); ++i)
    {
        if (m_sampleSinkDevices[i].m_deviceId == m_fileSinkDeviceTypeID) // File Sink
        {
            if (m_sampleSinkDevices[i].m_deviceSequence == 0) { // reference to device 0
            	fileSinkSSD0 = &m_sampleSinkDevices[i];
            }
            else if (m_sampleSinkDevices[i].m_deviceSequence == deviceUID) { // already there
                duplicateFileSink = false;
            }
        }
    }

    if (fileSinkSSD0 && duplicateFileSink) // append item for a new instance
    {
    	m_sampleSinkDevices.append(
            SamplingDevice(
            	fileSinkSSD0->m_plugin,
                QString("FileSink[%1]").arg(deviceUID),
                fileSinkSSD0->m_hadrwareId,
				fileSinkSSD0->m_deviceId,
				fileSinkSSD0->m_deviceSerial,
                deviceUID
            )
        );
    }
}

void PluginManager::fillSampleSourceSelector(QComboBox* comboBox, uint deviceUID)
{
	comboBox->clear();

	for(int i = 0; i < m_sampleSourceDevices.count(); i++)
	{
	    // For "local" devices show only ones that concern this device set
	    if ((m_sampleSourceDevices[i].m_deviceId == m_sdrDaemonDeviceTypeID)
	            || (m_sampleSourceDevices[i].m_deviceId == m_fileSourceDeviceTypeID))
	    {
	        if (deviceUID != m_sampleSourceDevices[i].m_deviceSequence) {
	            continue;
	        }
	    }

		comboBox->addItem(m_sampleSourceDevices[i].m_displayName, qVariantFromValue((void *) &m_sampleSourceDevices[i]));
	}
}

int PluginManager::getSampleSourceSelectorIndex(QComboBox* comboBox, DeviceSourceAPI *deviceSourceAPI)
{
    for (int i = 0; i < comboBox->count(); i++)
    {
        SamplingDevice *samplingDevice = (SamplingDevice*) (comboBox->itemData(i)).value<void *>();

        if ((samplingDevice->m_deviceId == deviceSourceAPI->getSampleSourceId()) &&
            (samplingDevice->m_deviceSerial == deviceSourceAPI->getSampleSourceSerial()) &&
            (samplingDevice->m_deviceSequence == deviceSourceAPI->getSampleSourceSequence()))
        {
            return i;
        }
    }

    return 0; // default to first item
}

void PluginManager::fillSampleSinkSelector(QComboBox* comboBox, uint deviceUID)
{
	comboBox->clear();

	for(int i = 0; i < m_sampleSinkDevices.count(); i++)
	{
	    // For "local" devices show only ones that concern this device set
	    if (m_sampleSinkDevices[i].m_deviceId == m_fileSinkDeviceTypeID)
	    {
	        if (deviceUID != m_sampleSinkDevices[i].m_deviceSequence) {
	            continue;
	        }
	    }

		comboBox->addItem(m_sampleSinkDevices[i].m_displayName, qVariantFromValue((void *) &m_sampleSinkDevices[i]));
	}
}

int PluginManager::getSampleSinkSelectorIndex(QComboBox* comboBox, DeviceSinkAPI *deviceSinkAPI)
{
    for (int i = 0; i < comboBox->count(); i++)
    {
        SamplingDevice *samplingDevice = (SamplingDevice*) (comboBox->itemData(i)).value<void *>();

        if ((samplingDevice->m_deviceId == deviceSinkAPI->getSampleSinkId()) &&
            (samplingDevice->m_deviceSerial == deviceSinkAPI->getSampleSinkSerial()) &&
            (samplingDevice->m_deviceSequence == deviceSinkAPI->getSampleSinkSequence()))
        {
            return i;
        }
    }

    return 0; // default to first item
}

int PluginManager::selectSampleSourceByIndex(int index, DeviceSourceAPI *deviceAPI)
{
	qDebug("PluginManager::selectSampleSourceByIndex: index: %d", index);

	if (m_sampleSourceDevices.count() == 0)
	{
		return -1;
	}

	if (index < 0)
	{
		return -1;
	}

	if (index >= m_sampleSourceDevices.count())
	{
		index = 0;
	}

    qDebug() << "PluginManager::selectSampleSourceByIndex: m_sampleSource at index " << index
            << " hid: " << m_sampleSourceDevices[index].m_hadrwareId.toStdString().c_str()
            << " id: " << m_sampleSourceDevices[index].m_deviceId.toStdString().c_str()
            << " ser: " << m_sampleSourceDevices[index].m_deviceSerial.toStdString().c_str()
            << " seq: " << m_sampleSourceDevices[index].m_deviceSequence;

    deviceAPI->stopAcquisition();

    deviceAPI->setSampleSourceSequence(m_sampleSourceDevices[index].m_deviceSequence);
    deviceAPI->setHardwareId(m_sampleSourceDevices[index].m_hadrwareId);
    deviceAPI->setSampleSourceId(m_sampleSourceDevices[index].m_deviceId);
    deviceAPI->setSampleSourceSerial(m_sampleSourceDevices[index].m_deviceSerial);
    deviceAPI->setSampleSourceDisplayName(m_sampleSourceDevices[index].m_displayName);
    deviceAPI->setSampleSourcePluginInterface(m_sampleSourceDevices[index].m_plugin);

	return index;
}

int PluginManager::selectSampleSourceBySerialOrSequence(const QString& sourceId, const QString& sourceSerial, uint32_t sourceSequence, DeviceSourceAPI *deviceAPI)
{
	qDebug("PluginManager::selectSampleSourceBySequence by sequence: id: %s ser: %s seq: %d", qPrintable(sourceId), qPrintable(sourceSerial), sourceSequence);

	int index = -1;
	int index_matchingSequence = -1;
	int index_firstOfKind = -1;

	for (int i = 0; i < m_sampleSourceDevices.count(); i++)
	{
		if (m_sampleSourceDevices[i].m_deviceId == sourceId)
		{
			index_firstOfKind = i;

			if (m_sampleSourceDevices[i].m_deviceSerial == sourceSerial)
			{
				index = i; // exact match
				break;
			}

			if (m_sampleSourceDevices[i].m_deviceSequence == sourceSequence)
			{
				index_matchingSequence = i;
			}
		}
	}

	if(index == -1) // no exact match
	{
		if (index_matchingSequence == -1) // no matching sequence
		{
			if (index_firstOfKind == -1) // no matching device type
			{
				if(m_sampleSourceDevices.count() > 0) // take first if any
				{
					index = 0;
				}
				else
				{
					return -1; // return if no device attached
				}
			}
			else
			{
				index = index_firstOfKind; // take first that matches device type
			}
		}
		else
		{
			index = index_matchingSequence; // take the one that matches the sequence in the device type
		}
	}

    qDebug() << "PluginManager::selectSampleSourceBySequence: m_sampleSource at index " << index
            << " hid: " << m_sampleSourceDevices[index].m_hadrwareId.toStdString().c_str()
            << " id: " << m_sampleSourceDevices[index].m_deviceId.toStdString().c_str()
            << " ser: " << m_sampleSourceDevices[index].m_deviceSerial.toStdString().c_str()
            << " seq: " << m_sampleSourceDevices[index].m_deviceSequence;

    deviceAPI->stopAcquisition();

	//	m_sampleSourcePluginGUI = pluginGUI;
    deviceAPI->setSampleSourceSequence(m_sampleSourceDevices[index].m_deviceSequence);
    deviceAPI->setHardwareId(m_sampleSourceDevices[index].m_hadrwareId);
    deviceAPI->setSampleSourceId(m_sampleSourceDevices[index].m_deviceId);
    deviceAPI->setSampleSourceSerial(m_sampleSourceDevices[index].m_deviceSerial);
    deviceAPI->setSampleSourceDisplayName(m_sampleSourceDevices[index].m_displayName);
    deviceAPI->setSampleSourcePluginInterface(m_sampleSourceDevices[index].m_plugin);

	return index;
}

int PluginManager::selectSampleSinkBySerialOrSequence(const QString& sinkId, const QString& sinkSerial, uint32_t sinkSequence, DeviceSinkAPI *deviceAPI)
{
	qDebug("PluginManager::selectSampleSinkBySerialOrSequence by sequence: id: %s ser: %s seq: %d", qPrintable(sinkId), qPrintable(sinkSerial), sinkSequence);

	int index = -1;
	int index_matchingSequence = -1;
	int index_firstOfKind = -1;

	for (int i = 0; i < m_sampleSinkDevices.count(); i++)
	{
		if (m_sampleSinkDevices[i].m_deviceId == sinkId)
		{
			index_firstOfKind = i;

			if (m_sampleSinkDevices[i].m_deviceSerial == sinkSerial)
			{
				index = i; // exact match
				break;
			}

			if (m_sampleSinkDevices[i].m_deviceSequence == sinkSequence)
			{
				index_matchingSequence = i;
			}
		}
	}

	if(index == -1) // no exact match
	{
		if (index_matchingSequence == -1) // no matching sequence
		{
			if (index_firstOfKind == -1) // no matching device type
			{
				if(m_sampleSinkDevices.count() > 0) // take first if any
				{
					index = 0;
				}
				else
				{
					return -1; // return if no device attached
				}
			}
			else
			{
				index = index_firstOfKind; // take first that matches device type
			}
		}
		else
		{
			index = index_matchingSequence; // take the one that matches the sequence in the device type
		}
	}

    qDebug() << "PluginManager::selectSampleSinkBySerialOrSequence: m_sampleSink at index " << index
            << " hid: " << m_sampleSinkDevices[index].m_hadrwareId.toStdString().c_str()
            << " id: " << m_sampleSinkDevices[index].m_deviceId.toStdString().c_str()
            << " ser: " << m_sampleSinkDevices[index].m_deviceSerial.toStdString().c_str()
            << " seq: " << m_sampleSinkDevices[index].m_deviceSequence;

    deviceAPI->stopGeneration();

	//	m_sampleSourcePluginGUI = pluginGUI;
    deviceAPI->setSampleSinkSequence(m_sampleSinkDevices[index].m_deviceSequence);
    deviceAPI->setHardwareId(m_sampleSinkDevices[index].m_hadrwareId);
    deviceAPI->setSampleSinkId(m_sampleSinkDevices[index].m_deviceId);
    deviceAPI->setSampleSinkSerial(m_sampleSinkDevices[index].m_deviceSerial);
    deviceAPI->setSampleSinkDisplayName(m_sampleSinkDevices[index].m_displayName);
    deviceAPI->setSampleSinkPluginInterface(m_sampleSinkDevices[index].m_plugin);

	return index;
}

void PluginManager::selectSampleSourceByDevice(void *devicePtr, DeviceSourceAPI *deviceAPI)
{
    SamplingDevice *sampleSourceDevice = (SamplingDevice *) devicePtr;

    qDebug() << "PluginManager::selectSampleSourceByDevice: "
            << " hid: " << sampleSourceDevice->m_hadrwareId.toStdString().c_str()
            << " id: " << sampleSourceDevice->m_deviceId.toStdString().c_str()
            << " ser: " << sampleSourceDevice->m_deviceSerial.toStdString().c_str()
            << " seq: " << sampleSourceDevice->m_deviceSequence;

    //  m_sampleSourcePluginGUI = pluginGUI;
    deviceAPI->setSampleSourceSequence(sampleSourceDevice->m_deviceSequence);
    deviceAPI->setHardwareId(sampleSourceDevice->m_hadrwareId);
    deviceAPI->setSampleSourceId(sampleSourceDevice->m_deviceId);
    deviceAPI->setSampleSourceSerial(sampleSourceDevice->m_deviceSerial);
    deviceAPI->setSampleSourceDisplayName(sampleSourceDevice->m_displayName);
    deviceAPI->setSampleSourcePluginInterface(sampleSourceDevice->m_plugin);
}

void PluginManager::selectSampleSinkByDevice(void *devicePtr, DeviceSinkAPI *deviceAPI)
{
    SamplingDevice *sampleSinkDevice = (SamplingDevice *) devicePtr;

    qDebug() << "PluginManager::selectSampleSinkByDevice: "
            << " hid: " << sampleSinkDevice->m_hadrwareId.toStdString().c_str()
            << " id: " << sampleSinkDevice->m_deviceId.toStdString().c_str()
            << " ser: " << sampleSinkDevice->m_deviceSerial.toStdString().c_str()
            << " seq: " << sampleSinkDevice->m_deviceSequence;

    //  m_sampleSourcePluginGUI = pluginGUI;
    deviceAPI->setSampleSinkSequence(sampleSinkDevice->m_deviceSequence);
    deviceAPI->setHardwareId(sampleSinkDevice->m_hadrwareId);
    deviceAPI->setSampleSinkId(sampleSinkDevice->m_deviceId);
    deviceAPI->setSampleSinkSerial(sampleSinkDevice->m_deviceSerial);
    deviceAPI->setSampleSinkDisplayName(sampleSinkDevice->m_displayName);
    deviceAPI->setSampleSinkPluginInterface(sampleSinkDevice->m_plugin);
}

void PluginManager::loadPlugins(const QDir& dir)
{
	QDir pluginsDir(dir);

	foreach (QString fileName, pluginsDir.entryList(QDir::Files))
	{
        if (fileName.endsWith(".so") || fileName.endsWith(".dll") || fileName.endsWith(".dylib"))
		{
			qDebug() << "PluginManager::loadPlugins: fileName: " << qPrintable(fileName);

			QPluginLoader* loader = new QPluginLoader(pluginsDir.absoluteFilePath(fileName));
			PluginInterface* plugin = qobject_cast<PluginInterface*>(loader->instance());

			if (loader->isLoaded())
			{
				qWarning("PluginManager::loadPlugins: loaded plugin %s", qPrintable(fileName));
			}
			else
			{
				qWarning() << "PluginManager::loadPlugins: " << qPrintable(loader->errorString());
			}

			if (plugin != 0)
			{
				m_plugins.append(Plugin(fileName, loader, plugin));
			}
			else
			{
				loader->unload();
			}

			delete loader; // Valgrind memcheck
		}
	}

	// recursive calls on subdirectories

	foreach (QString dirName, pluginsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
	{
		loadPlugins(pluginsDir.absoluteFilePath(dirName));
	}
}

PluginInterface* PluginManager::getPluginInterfaceAt(int index)
{
    if (index < m_sampleSourceDevices.size()) {
        return m_sampleSourceDevices[index].m_plugin;
    } else {
        return 0;
    }
}

void PluginManager::populateRxChannelComboBox(QComboBox *channels)
{
    for(PluginAPI::ChannelRegistrations::iterator it = m_rxChannelRegistrations.begin(); it != m_rxChannelRegistrations.end(); ++it)
    {
        const PluginDescriptor& pluginDescipror = it->m_plugin->getPluginDescriptor();
        channels->addItem(pluginDescipror.displayedName);
    }
}

void PluginManager::populateTxChannelComboBox(QComboBox *channels)
{
    for(PluginAPI::ChannelRegistrations::iterator it = m_txChannelRegistrations.begin(); it != m_txChannelRegistrations.end(); ++it)
    {
        const PluginDescriptor& pluginDescipror = it->m_plugin->getPluginDescriptor();
        channels->addItem(pluginDescipror.displayedName);
    }
}

void PluginManager::createRxChannelInstance(int channelPluginIndex, DeviceUISet *deviceUISet)
{
    if (channelPluginIndex < m_rxChannelRegistrations.size())
    {
        PluginInterface *pluginInterface = m_rxChannelRegistrations[channelPluginIndex].m_plugin;
        pluginInterface->createRxChannel(m_rxChannelRegistrations[channelPluginIndex].m_channelName, deviceUISet);
    }
}

void PluginManager::createTxChannelInstance(int channelPluginIndex, DeviceSinkAPI *deviceAPI)
{
    if (channelPluginIndex < m_txChannelRegistrations.size())
    {
        PluginInterface *pluginInterface = m_txChannelRegistrations[channelPluginIndex].m_plugin;
        pluginInterface->createTxChannel(m_txChannelRegistrations[channelPluginIndex].m_channelName, deviceAPI);
    }
}
