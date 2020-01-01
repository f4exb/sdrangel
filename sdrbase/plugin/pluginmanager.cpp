///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2019 Edouard Griffiths, F4EXB                              //
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

#include <QCoreApplication>
#include <QPluginLoader>
#include <QDebug>

#include <cstdio>

#include <plugin/plugininstancegui.h>
#include "device/deviceenumerator.h"
#include "settings/preset.h"
#include "util/message.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"

#include "plugin/pluginmanager.h"

const QString PluginManager::m_localInputHardwareID = "LocalInput";
const QString PluginManager::m_localInputDeviceTypeID = "sdrangel.samplesource.localinput";
const QString PluginManager::m_remoteInputHardwareID = "RemoteInput";
const QString PluginManager::m_remoteInputDeviceTypeID = "sdrangel.samplesource.remoteinput";
const QString PluginManager::m_fileInputHardwareID = "FileInput";
const QString PluginManager::m_fileInputDeviceTypeID = "sdrangel.samplesource.fileinput";

const QString PluginManager::m_localOutputHardwareID = "LocalOutput";
const QString PluginManager::m_localOutputDeviceTypeID = "sdrangel.samplesource.localoutput";
const QString PluginManager::m_remoteOutputHardwareID = "RemoteOutput";
const QString PluginManager::m_remoteOutputDeviceTypeID = "sdrangel.samplesink.remoteoutput";
const QString PluginManager::m_fileSinkHardwareID = "FileSink";
const QString PluginManager::m_fileSinkDeviceTypeID = "sdrangel.samplesink.filesink";

PluginManager::PluginManager(QObject* parent) :
	QObject(parent),
    m_pluginAPI(this)
{
}

PluginManager::~PluginManager()
{
  //  freeAll();
}

void PluginManager::loadPlugins(const QString& pluginsSubDir)
{
    loadPluginsPart(pluginsSubDir);
    loadPluginsFinal();
}

void PluginManager::loadPluginsPart(const QString& pluginsSubDir)
{
    QString applicationDirPath = QCoreApplication::instance()->applicationDirPath();
    QStringList PluginsPath;

    // NOTE: not the best solution but for now this is
    // on make install [PREFIX]/bin and [PREFIX]/lib/sdrangel
    PluginsPath << applicationDirPath + "/../lib/sdrangel/" + pluginsSubDir;
    // on build
    PluginsPath << applicationDirPath + "/lib/" + pluginsSubDir;
#ifdef __APPLE__
    // on SDRAngel.app
    PluginsPath << applicationDirPath + "/../Resources/lib/" + pluginsSubDir;
#elif defined(_WIN32) || defined(WIN32)
    PluginsPath << applicationDirPath + "/" + pluginsSubDir;
#endif

    // NOTE: exit on the first folder found
    bool found = false;
    foreach (QString dir, PluginsPath)
    {
        QDir d(dir);
        if (d.entryList(QDir::Files).count() == 0) {
            qDebug("PluginManager::loadPluginsPart folder %s is empty", qPrintable(dir));
            continue;
        }

        found = true;
        loadPluginsDir(d);
        break;
    }

    if (!found)
    {
        qCritical("No plugins found. Exit immediately.");
        exit(EXIT_FAILURE);
    }
}

void PluginManager::loadPluginsFinal()
{
    qSort(m_plugins);

    for (Plugins::const_iterator it = m_plugins.begin(); it != m_plugins.end(); ++it)
    {
        it->pluginInterface->initPlugin(&m_pluginAPI);
    }

    DeviceEnumerator::instance()->enumerateRxDevices(this);
    DeviceEnumerator::instance()->enumerateTxDevices(this);
    DeviceEnumerator::instance()->enumerateMIMODevices(this);
}

void PluginManager::loadPluginsNonDiscoverable(const DeviceUserArgs& deviceUserArgs)
{
    DeviceEnumerator::instance()->addNonDiscoverableDevices(this, deviceUserArgs);
}

void PluginManager::registerRxChannel(const QString& channelIdURI, const QString& channelId, PluginInterface* plugin)
{
    qDebug() << "PluginManager::registerRxChannel "
            << plugin->getPluginDescriptor().displayedName.toStdString().c_str()
            << " with channel name " << channelIdURI;

	m_rxChannelRegistrations.append(PluginAPI::ChannelRegistration(channelIdURI, channelId, plugin));
}

void PluginManager::registerTxChannel(const QString& channelIdURI, const QString& channelId, PluginInterface* plugin)
{
    qDebug() << "PluginManager::registerTxChannel "
            << plugin->getPluginDescriptor().displayedName.toStdString().c_str()
            << " with channel name " << channelIdURI;

	m_txChannelRegistrations.append(PluginAPI::ChannelRegistration(channelIdURI, channelId, plugin));
}

void PluginManager::registerMIMOChannel(const QString& channelIdURI, const QString& channelId, PluginInterface* plugin)
{
    qDebug() << "PluginManager::registerMIMOChannel "
            << plugin->getPluginDescriptor().displayedName.toStdString().c_str()
            << " with channel name " << channelIdURI;

	m_mimoChannelRegistrations.append(PluginAPI::ChannelRegistration(channelIdURI, channelId, plugin));
}

void PluginManager::registerSampleSource(const QString& sourceName, PluginInterface* plugin)
{
	qDebug() << "PluginManager::registerSampleSource "
			<< plugin->getPluginDescriptor().displayedName.toStdString().c_str()
			<< " with source name " << sourceName.toStdString().c_str()
            << " and hardware id " << plugin->getPluginDescriptor().hardwareId;

	m_sampleSourceRegistrations.append(PluginAPI::SamplingDeviceRegistration(
        plugin->getPluginDescriptor().hardwareId,
        sourceName,
        plugin
    ));
}

void PluginManager::registerSampleSink(const QString& sinkName, PluginInterface* plugin)
{
	qDebug() << "PluginManager::registerSampleSink "
			<< plugin->getPluginDescriptor().displayedName.toStdString().c_str()
			<< " with sink name " << sinkName.toStdString().c_str()
            << " and hardware id " << plugin->getPluginDescriptor().hardwareId;

	m_sampleSinkRegistrations.append(PluginAPI::SamplingDeviceRegistration(
        plugin->getPluginDescriptor().hardwareId,
        sinkName,
        plugin
    ));
}

void PluginManager::registerSampleMIMO(const QString& mimoName, PluginInterface* plugin)
{
	qDebug() << "PluginManager::registerSampleMIMO "
			<< plugin->getPluginDescriptor().displayedName.toStdString().c_str()
			<< " with MIMO name " << mimoName.toStdString().c_str()
            << " and hardware id " << plugin->getPluginDescriptor().hardwareId;

	m_sampleMIMORegistrations.append(PluginAPI::SamplingDeviceRegistration(
        plugin->getPluginDescriptor().hardwareId,
        mimoName,
        plugin
    ));
}

void PluginManager::loadPluginsDir(const QDir& dir)
{
    QDir pluginsDir(dir);

    foreach (QString fileName, pluginsDir.entryList(QDir::Files))
    {
        if (QLibrary::isLibrary(fileName))
        {
            qDebug("PluginManager::loadPluginsDir: fileName: %s", qPrintable(fileName));

            QPluginLoader* pluginLoader = new QPluginLoader(pluginsDir.absoluteFilePath(fileName));
            if (!pluginLoader->load())
            {
                qWarning("PluginManager::loadPluginsDir: %s", qPrintable(pluginLoader->errorString()));
                delete pluginLoader;
                continue;
            }

            PluginInterface* instance = qobject_cast<PluginInterface*>(pluginLoader->instance());
            if (instance == nullptr)
            {
                qWarning("PluginManager::loadPluginsDir: Unable to get main instance of plugin: %s", qPrintable(fileName) );
                delete pluginLoader;
                continue;
            }

            delete(pluginLoader);

            qInfo("PluginManager::loadPluginsDir: loaded plugin %s", qPrintable(fileName));
            m_plugins.append(Plugin(fileName, instance));
       }
    }
}

void PluginManager::listTxChannels(QList<QString>& list)
{
    list.clear();

    for (PluginAPI::ChannelRegistrations::iterator it = m_txChannelRegistrations.begin(); it != m_txChannelRegistrations.end(); ++it)
    {
        const PluginDescriptor& pluginDescipror = it->m_plugin->getPluginDescriptor();
        list.append(pluginDescipror.displayedName);
    }
}

void PluginManager::listRxChannels(QList<QString>& list)
{
    list.clear();

    for (PluginAPI::ChannelRegistrations::iterator it = m_rxChannelRegistrations.begin(); it != m_rxChannelRegistrations.end(); ++it)
    {
        const PluginDescriptor& pluginDesciptor = it->m_plugin->getPluginDescriptor();
        list.append(pluginDesciptor.displayedName);
    }
}

void PluginManager::listMIMOChannels(QList<QString>& list)
{
    list.clear();

    for (PluginAPI::ChannelRegistrations::iterator it = m_mimoChannelRegistrations.begin(); it != m_mimoChannelRegistrations.end(); ++it)
    {
        const PluginDescriptor& pluginDesciptor = it->m_plugin->getPluginDescriptor();
        list.append(pluginDesciptor.displayedName);
    }
}

void PluginManager::createRxChannelInstance(int channelPluginIndex, DeviceUISet *deviceUISet, DeviceAPI *deviceAPI)
{
    if (channelPluginIndex < m_rxChannelRegistrations.size())
    {
        PluginInterface *pluginInterface = m_rxChannelRegistrations[channelPluginIndex].m_plugin;
        BasebandSampleSink *rxChannel = pluginInterface->createRxChannelBS(deviceAPI);
        pluginInterface->createRxChannelGUI(deviceUISet, rxChannel);
    }
}

void PluginManager::createTxChannelInstance(int channelPluginIndex, DeviceUISet *deviceUISet, DeviceAPI *deviceAPI)
{
    if (channelPluginIndex < m_txChannelRegistrations.size())
    {
        PluginInterface *pluginInterface = m_txChannelRegistrations[channelPluginIndex].m_plugin;
        BasebandSampleSource *txChannel = pluginInterface->createTxChannelBS(deviceAPI);
        pluginInterface->createTxChannelGUI(deviceUISet, txChannel);
    }
}

void PluginManager::createMIMOChannelInstance(int channelPluginIndex, DeviceUISet *deviceUISet, DeviceAPI *deviceAPI)
{
    if (channelPluginIndex < m_mimoChannelRegistrations.size())
    {
        PluginInterface *pluginInterface = m_mimoChannelRegistrations[channelPluginIndex].m_plugin;
        MIMOChannel *mimoChannel = pluginInterface->createMIMOChannelBS(deviceAPI);
        pluginInterface->createMIMOChannelGUI(deviceUISet, mimoChannel);
    }
}

const PluginInterface *PluginManager::getChannelPluginInterface(const QString& channelIdURI) const
{
    for (PluginAPI::ChannelRegistrations::const_iterator it = m_rxChannelRegistrations.begin(); it != m_rxChannelRegistrations.end(); ++it)
    {
        if (it->m_channelIdURI == channelIdURI) {
            return it->m_plugin;
        }
    }

    for (PluginAPI::ChannelRegistrations::const_iterator it = m_txChannelRegistrations.begin(); it != m_txChannelRegistrations.end(); ++it)
    {
        if (it->m_channelIdURI == channelIdURI) {
            return it->m_plugin;
        }
    }

    return nullptr;
}

const PluginInterface *PluginManager::getDevicePluginInterface(const QString& deviceId) const
{
    for (PluginAPI::SamplingDeviceRegistrations::const_iterator it = m_sampleSourceRegistrations.begin(); it != m_sampleSourceRegistrations.end(); ++it)
    {
        if (it->m_deviceId == deviceId) {
            return it->m_plugin;
        }
    }

    for (PluginAPI::SamplingDeviceRegistrations::const_iterator it = m_sampleSinkRegistrations.begin(); it != m_sampleSinkRegistrations.end(); ++it)
    {
        if (it->m_deviceId == deviceId) {
            return it->m_plugin;
        }
    }

    for (PluginAPI::SamplingDeviceRegistrations::const_iterator it = m_sampleMIMORegistrations.begin(); it != m_sampleMIMORegistrations.end(); ++it)
    {
        if (it->m_deviceId == deviceId) {
            return it->m_plugin;
        }
    }

    return nullptr;
}