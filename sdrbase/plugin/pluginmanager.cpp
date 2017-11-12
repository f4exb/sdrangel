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

#include <QCoreApplication>
#include <QPluginLoader>
//#include <QComboBox>
#include <QDebug>

#include <cstdio>

#include <plugin/plugininstancegui.h>
#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "device/deviceenumerator.h"
#include "settings/preset.h"
#include "util/message.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"

#include "plugin/pluginmanager.h"

const QString PluginManager::m_sdrDaemonSourceHardwareID = "SDRdaemonSource";
const QString PluginManager::m_sdrDaemonSourceDeviceTypeID = "sdrangel.samplesource.sdrdaemonsource";
const QString PluginManager::m_fileSourceHardwareID = "FileSource";
const QString PluginManager::m_fileSourceDeviceTypeID = "sdrangel.samplesource.filesource";

const QString PluginManager::m_sdrDaemonSinkHardwareID = "SDRdaemonSink";
const QString PluginManager::m_sdrDaemonSinkDeviceTypeID = "sdrangel.samplesink.sdrdaemonsink";
const QString PluginManager::m_fileSinkHardwareID = "FileSink";
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
	QString applicationDirPath = QCoreApplication::instance()->applicationDirPath();
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

	DeviceEnumerator::instance()->enumerateRxDevices(this);
	DeviceEnumerator::instance()->enumerateTxDevices(this);
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

	m_sampleSourceRegistrations.append(PluginAPI::SamplingDeviceRegistration(sourceName, plugin));
}

void PluginManager::registerSampleSink(const QString& sinkName, PluginInterface* plugin)
{
	qDebug() << "PluginManager::registerSampleSink "
			<< plugin->getPluginDescriptor().displayedName.toStdString().c_str()
			<< " with sink name " << sinkName.toStdString().c_str();

	m_sampleSinkRegistrations.append(PluginAPI::SamplingDeviceRegistration(sinkName, plugin));
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
				qInfo("PluginManager::loadPlugins: loaded plugin %s", qPrintable(fileName));
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

void PluginManager::listTxChannels(QList<QString>& list)
{
    list.clear();

    for(PluginAPI::ChannelRegistrations::iterator it = m_txChannelRegistrations.begin(); it != m_txChannelRegistrations.end(); ++it)
    {
        const PluginDescriptor& pluginDescipror = it->m_plugin->getPluginDescriptor();
        list.append(pluginDescipror.displayedName);
    }
}

void PluginManager::listRxChannels(QList<QString>& list)
{
    list.clear();

    for(PluginAPI::ChannelRegistrations::iterator it = m_rxChannelRegistrations.begin(); it != m_rxChannelRegistrations.end(); ++it)
    {
        const PluginDescriptor& pluginDescipror = it->m_plugin->getPluginDescriptor();
        list.append(pluginDescipror.displayedName);
    }
}

void PluginManager::createRxChannelInstance(int channelPluginIndex, DeviceUISet *deviceUISet, DeviceSourceAPI *deviceAPI)
{
    if (channelPluginIndex < m_rxChannelRegistrations.size())
    {
        PluginInterface *pluginInterface = m_rxChannelRegistrations[channelPluginIndex].m_plugin;
        BasebandSampleSink *rxChannel = pluginInterface->createRxChannel(
                m_rxChannelRegistrations[channelPluginIndex].m_channelId, deviceAPI);
        pluginInterface->createRxChannelGUI(m_rxChannelRegistrations[channelPluginIndex].m_channelId, deviceUISet, rxChannel);
    }
}

void PluginManager::createTxChannelInstance(int channelPluginIndex, DeviceUISet *deviceUISet, DeviceSinkAPI *deviceAPI)
{
    if (channelPluginIndex < m_txChannelRegistrations.size())
    {
        PluginInterface *pluginInterface = m_txChannelRegistrations[channelPluginIndex].m_plugin;
        BasebandSampleSource *txChannel = pluginInterface->createTxChannel(
                m_txChannelRegistrations[channelPluginIndex].m_channelId, deviceAPI);
        pluginInterface->createTxChannelGUI(m_txChannelRegistrations[channelPluginIndex].m_channelId, deviceUISet, txChannel);
    }
}
