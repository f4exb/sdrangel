///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2016, 2018 Ziga S <ziga.svetina@gmail.com>                      //
// Copyright (C) 2019 Davide Gerhard <rainbow@irh.it>                            //
// Copyright (C) 2020-2022 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
// Copyright (C) 2022 Jaroslav Škarvada <jskarvad@redhat.com>                    //
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
#include <algorithm>

#include "device/deviceenumerator.h"
#include "plugin/pluginmanager.h"

#ifndef LIB
#define LIB "lib"
#endif

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
const QString PluginManager::m_fileOutputHardwareID = "FileOutput";
const QString PluginManager::m_fileOutputDeviceTypeID = "sdrangel.samplesink.fileoutput";

const QString PluginManager::m_testMIMOHardwareID = "TestMI";
const QString PluginManager::m_testMIMODeviceTypeID = "sdrangel.samplemimo.testmi";

PluginManager::PluginManager(QObject* parent) :
	QObject(parent),
    m_pluginAPI(this),
    m_enableSoapy(false)
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
    QStringList filter;

    // NOTE: not the best solution but for now this is
    // on make install [PREFIX]/bin and [PREFIX]/lib/sdrangel
#if defined(ANDROID)
    PluginsPath = QStringList({applicationDirPath});
    // Qt5 add_library gives libsdrangel_plugins_antennatools.so
    // Qt6 qt_add_plugin gives libplugins__sdrangel_plugins_antennatools.so
    // Assuming PLUGINS_PREFIX=sdrangel_plugins_
    filter = QStringList({"lib*sdrangel_" + pluginsSubDir + "_*.so"});
#else
    filter = QStringList({"*"});
    PluginsPath << applicationDirPath + "/../" + LIB + "/sdrangel/" + pluginsSubDir;
    // on build
    PluginsPath << applicationDirPath + "/" + LIB + "/" + pluginsSubDir;
#ifdef __APPLE__
    // on SDRAngel.app
    PluginsPath << applicationDirPath + "/../Resources/lib/" + pluginsSubDir;
#elif defined(_WIN32) || defined(WIN32)
    PluginsPath << applicationDirPath + "/" + pluginsSubDir;
    // Give SoapySDR hint about modules location.
    // By default it searches in `$applicationDir/../lib/SoapySDR/`, which on Windows
    // is incorrect as both `bin` and `lib` dir are set to root application dir.
    qputenv("SOAPY_SDR_ROOT", applicationDirPath.toLocal8Bit());
#endif
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
        loadPluginsDir(d, filter);
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
    std::sort(m_plugins.begin(), m_plugins.end());

    for (Plugins::const_iterator it = m_plugins.begin(); it != m_plugins.end(); ++it)
    {
        it->pluginInterface->initPlugin(&m_pluginAPI);
    }

    DeviceEnumerator::instance()->enumerateAllDevices(this);
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

void PluginManager::registerFeature(const QString& featureIdURI, const QString& featureId, PluginInterface* plugin)
{
    qDebug() << "PluginManager::registerFeature "
            << plugin->getPluginDescriptor().displayedName.toStdString().c_str()
            << " with channel name " << featureIdURI;

	m_featureRegistrations.append(PluginAPI::FeatureRegistration(featureIdURI, featureId, plugin));
}

void PluginManager::loadPluginsDir(const QDir& dir, const QStringList& filter)
{
    QDir pluginsDir(dir);

    foreach (QString fileName, pluginsDir.entryList(filter, QDir::Files))
    {
        if (QLibrary::isLibrary(fileName))
        {
            if (!m_enableSoapy && fileName.contains("soapysdr"))
            {
                qInfo("PluginManager::loadPluginsDir: Soapy SDR disabled skipping %s", qPrintable(fileName));
                continue;
            }

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

void PluginManager::listFeatures(QList<QString>& list)
{
    list.clear();

    for (PluginAPI::FeatureRegistrations::iterator it = m_featureRegistrations.begin(); it != m_featureRegistrations.end(); ++it)
    {
        const PluginDescriptor& pluginDesciptor = it->m_plugin->getPluginDescriptor();
        list.append(pluginDesciptor.displayedName);
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

const PluginInterface *PluginManager::getFeaturePluginInterface(const QString& featureIdURI) const
{
    for (PluginAPI::FeatureRegistrations::const_iterator it = m_featureRegistrations.begin(); it != m_featureRegistrations.end(); ++it)
    {
        if (it->m_featureIdURI == featureIdURI) {
            return it->m_plugin;
        }
    }

    return nullptr;
}

QString PluginManager::uriToId(const QString& uri) const
{
    for (int i = 0; i < m_rxChannelRegistrations.size(); i++)
    {
        if (m_rxChannelRegistrations[i].m_channelIdURI == uri)
            return m_rxChannelRegistrations[i].m_channelId;
    }
    for (int i = 0; i < m_txChannelRegistrations.size(); i++)
    {
        if (m_txChannelRegistrations[i].m_channelIdURI == uri)
            return m_txChannelRegistrations[i].m_channelId;
    }
    for (int i = 0; i < m_mimoChannelRegistrations.size(); i++)
    {
        if (m_mimoChannelRegistrations[i].m_channelIdURI == uri)
            return m_mimoChannelRegistrations[i].m_channelId;
    }
    for (int i = 0; i < m_featureRegistrations.size(); i++)
    {
        if (m_featureRegistrations[i].m_featureIdURI == uri)
            return m_featureRegistrations[i].m_featureId;
    }
    return uri;
}
