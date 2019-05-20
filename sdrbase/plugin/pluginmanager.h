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

#ifndef INCLUDE_PLUGINMANAGER_H
#define INCLUDE_PLUGINMANAGER_H

#include <stdint.h>
#include <QObject>
#include <QDir>
#include <QList>
#include <QString>

#include "plugin/plugininterface.h"
#include "plugin/pluginapi.h"
#include "export.h"

class QComboBox;
class QPluginLoader;
class Preset;
class Message;
class MessageQueue;
class DeviceAPI;

class SDRBASE_API PluginManager : public QObject {
	Q_OBJECT

public:
	struct Plugin
	{
		QString filename;
		PluginInterface* pluginInterface;

		Plugin(const QString& _filename, PluginInterface* _plugin) :
			filename(_filename),
			pluginInterface(_plugin)
		{ }
	};

	typedef QList<Plugin> Plugins;

	explicit PluginManager(QObject* parent = 0);
	~PluginManager();

	PluginAPI *getPluginAPI() { return &m_pluginAPI; }
	void loadPlugins(const QString& pluginsSubDir);
	void loadPluginsPart(const QString& pluginsSubDir);
	void loadPluginsFinal();
	const Plugins& getPlugins() const { return m_plugins; }

	// Callbacks from the plugins
	void registerRxChannel(const QString& channelIdURI, const QString& channelId, PluginInterface* plugin);
	void registerSampleSource(const QString& sourceName, PluginInterface* plugin);
	void registerTxChannel(const QString& channelIdURI, const QString& channelId, PluginInterface* plugin);
	void registerSampleSink(const QString& sinkName, PluginInterface* plugin);
    void registerSampleMIMO(const QString& mimoName, PluginInterface* plugin);

	PluginAPI::SamplingDeviceRegistrations& getSourceDeviceRegistrations() { return m_sampleSourceRegistrations; }
	PluginAPI::SamplingDeviceRegistrations& getSinkDeviceRegistrations() { return m_sampleSinkRegistrations; }
    PluginAPI::SamplingDeviceRegistrations& getMIMODeviceRegistrations() { return m_sampleMIMORegistrations; }
	PluginAPI::ChannelRegistrations *getRxChannelRegistrations() { return &m_rxChannelRegistrations; }
	PluginAPI::ChannelRegistrations *getTxChannelRegistrations() { return &m_txChannelRegistrations; }

    void createRxChannelInstance(int channelPluginIndex, DeviceUISet *deviceUISet, DeviceAPI *deviceAPI);
    void listRxChannels(QList<QString>& list);

	void createTxChannelInstance(int channelPluginIndex, DeviceUISet *deviceUISet, DeviceAPI *deviceAPI);
	void listTxChannels(QList<QString>& list);

	static const QString& getFileSourceDeviceId() { return m_fileSourceDeviceTypeID; }
	static const QString& getFileSinkDeviceId() { return m_fileSinkDeviceTypeID; }
	static const QString& getTestMIMODeviceId() { return m_testMIMODeviceTypeID; }

private:
	struct SamplingDevice { //!< This is the device registration
		PluginInterface* m_plugin;
		QString m_displayName;
		QString m_hadrwareId;
		QString m_deviceId;
		QString m_deviceSerial;
		uint32_t m_deviceSequence;

		SamplingDevice(PluginInterface* plugin,
				const QString& displayName,
				const QString& hadrwareId,
				const QString& deviceId,
				const QString& deviceSerial,
				int deviceSequence) :
			m_plugin(plugin),
			m_displayName(displayName),
			m_hadrwareId(hadrwareId),
			m_deviceId(deviceId),
			m_deviceSerial(deviceSerial),
			m_deviceSequence(deviceSequence)
		{ }
	};

	typedef QList<SamplingDevice> SamplingDevices;

	PluginAPI m_pluginAPI;
	Plugins m_plugins;

	PluginAPI::ChannelRegistrations m_rxChannelRegistrations;           //!< Channel plugins register here
	PluginAPI::SamplingDeviceRegistrations m_sampleSourceRegistrations; //!< Input source plugins (one per device kind) register here

	PluginAPI::ChannelRegistrations m_txChannelRegistrations;         //!< Channel plugins register here
	PluginAPI::SamplingDeviceRegistrations m_sampleSinkRegistrations; //!< Output sink plugins (one per device kind) register here

	PluginAPI::SamplingDeviceRegistrations m_sampleMIMORegistrations; //!< MIMO sink plugins (one per device kind) register here

	// "Local" sample source device IDs
    static const QString m_localInputHardwareID;     //!< Local input hardware ID
    static const QString m_localInputDeviceTypeID;   //!< Local input plugin ID
    static const QString m_remoteInputHardwareID;    //!< Remote input hardware ID
    static const QString m_remoteInputDeviceTypeID;  //!< Remote input plugin ID
    static const QString m_fileSourceHardwareID;     //!< FileSource source hardware ID
    static const QString m_fileSourceDeviceTypeID;   //!< FileSource source plugin ID

    // "Local" sample sink device IDs
    static const QString m_localOutputHardwareID;    //!< Local output hardware ID
    static const QString m_localOutputDeviceTypeID;  //!< Local output plugin ID
    static const QString m_remoteOutputHardwareID;   //!< Remote output hardware ID
    static const QString m_remoteOutputDeviceTypeID; //!< Remote output plugin ID
    static const QString m_fileSinkHardwareID;       //!< FileSource source hardware ID
    static const QString m_fileSinkDeviceTypeID;     //!< FileSink sink plugin ID

    // "Local" sample MIMO device IDs
    static const QString m_testMIMOHardwareID;       //!< Test MIMO hardware ID
    static const QString m_testMIMODeviceTypeID;     //!< Test MIMO plugin ID

	void loadPluginsDir(const QDir& dir);
};

static inline bool operator<(const PluginManager::Plugin& a, const PluginManager::Plugin& b)
{
	return a.pluginInterface->getPluginDescriptor().displayedName < b.pluginInterface->getPluginDescriptor().displayedName;
}

#endif // INCLUDE_PLUGINMANAGER_H
