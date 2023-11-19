///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany     //
// written by Christian Daniel                                                       //
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>              //
// Copyright (C) 2020 Jon Beniston, M7RCE <jon@beniston.com>                         //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#ifndef INCLUDE_PLUGINAPI_H
#define INCLUDE_PLUGINAPI_H

#include <QObject>
#include <QList>

#include "export.h"
#include "plugin/plugininterface.h"

class QString;

class PluginManager;
class MessageQueue;

class SDRBASE_API PluginAPI : public QObject {
	Q_OBJECT

public:
    struct SamplingDeviceRegistration //!< This is the device registration
    {
        QString m_deviceHardwareId;
        QString m_deviceId;
        PluginInterface* m_plugin;
        SamplingDeviceRegistration(const QString& hardwareId, const QString& deviceId, PluginInterface* plugin) :
            m_deviceHardwareId(hardwareId),
            m_deviceId(deviceId),
            m_plugin(plugin)
        { }
    };

    typedef QList<SamplingDeviceRegistration> SamplingDeviceRegistrations;

    struct ChannelRegistration
    {
        QString m_channelIdURI;       //!< Channel type ID in URI form
        QString m_channelId;          //!< Channel type ID in short form from object name
        PluginInterface* m_plugin;
        ChannelRegistration(const QString& channelIdURI, const QString& channelId, PluginInterface* plugin) :
            m_channelIdURI(channelIdURI),
            m_channelId(channelId),
            m_plugin(plugin)
        { }
    };

    typedef QList<ChannelRegistration> ChannelRegistrations;

    struct FeatureRegistration
    {
        QString m_featureIdURI; //!< Feature type ID in URI form
        QString m_featureId;    //!< Feature type ID in short form from object name
        PluginInterface *m_plugin;
        FeatureRegistration(const QString& featureIdURI, const QString& featureId, PluginInterface* plugin) :
            m_featureIdURI(featureIdURI),
            m_featureId(featureId),
            m_plugin(plugin)
        { }
    };

    typedef QList<FeatureRegistration> FeatureRegistrations;

	// Rx Channel stuff
	void registerRxChannel(const QString& channelIdURI, const QString& channelId, PluginInterface* plugin);
	ChannelRegistrations *getRxChannelRegistrations();

	// Tx Channel stuff
	void registerTxChannel(const QString& channelIdURI, const QString& channelId, PluginInterface* plugin);
	ChannelRegistrations *getTxChannelRegistrations();

    // MIMO Channel stuff
	void registerMIMOChannel(const QString& channelIdURI, const QString& channelId, PluginInterface* plugin);
	ChannelRegistrations *getMIMOChannelRegistrations();

	// Sample Source stuff
	void registerSampleSource(const QString& sourceName, PluginInterface* plugin);

	// Sample Sink stuff
	void registerSampleSink(const QString& sinkName, PluginInterface* plugin);

	// Sample MIMO stuff
	void registerSampleMIMO(const QString& sinkName, PluginInterface* plugin);

    // Feature stuff
    void registerFeature(const QString& featureIdURI, const QString& featureId, PluginInterface* plugin);
    FeatureRegistrations *getFeatureRegistrations();

protected:
	PluginManager* m_pluginManager;

	PluginAPI(PluginManager* pluginManager);
	~PluginAPI();

	friend class PluginManager;
};

#endif // INCLUDE_PLUGINAPI_H
