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

#include <QCoreApplication>
#include <QPluginLoader>
//#include <QComboBox>
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
const QString PluginManager::m_fileSourceHardwareID = "FileSource";
const QString PluginManager::m_fileSourceDeviceTypeID = "sdrangel.samplesource.filesource";

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
    PluginsPath << applicationDirPath + "/../Frameworks/" + pluginsSubDir;
#endif

    // NOTE: exit on the first folder found
    bool found = false;
    foreach (QString dir, PluginsPath)
      {
        QDir d(dir);
        if (d.isEmpty()) {
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

void PluginManager::loadPluginsDir(const QDir& dir)
{
	QDir pluginsDir(dir);

        foreach (QString fileName, pluginsDir.entryList(QDir::Files))
          {
            if (QLibrary::isLibrary(fileName))
              {
                qDebug("PluginManager::loadPluginsDir: fileName: %s", qPrintable(fileName));

                QPluginLoader* plugin = new QPluginLoader(pluginsDir.absoluteFilePath(fileName));
                if (!plugin->load())
                  {
                    qWarning("PluginManager::loadPluginsDir: %s", qPrintable(plugin->errorString()));
                    delete plugin;
                    continue;
                  }

                PluginInterface* instance = qobject_cast<PluginInterface*>(plugin->instance());
                if (instance == nullptr)
                  {
                    qWarning("PluginManager::loadPluginsDir: Unable to get main instance of plugin: %s", qPrintable(fileName) );
                    delete plugin;
                    continue;
                  }

                qInfo("PluginManager::loadPluginsDir: loaded plugin %s", qPrintable(fileName));
                m_plugins.append(Plugin(fileName, plugin, instance));
              }
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
