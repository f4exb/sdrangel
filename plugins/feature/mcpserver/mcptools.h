///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_FEATURE_MCPTOOLS_H_
#define INCLUDE_FEATURE_MCPTOOLS_H_

#include <functional>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QMap>
#include <QString>
#include <QMutex>
#include <QSet>
#include <QStringList>

#include <QAtomicInt>
#include <QObject>

#include "mcperror.h"
#include "mcpdocs.h"
#include "mcpdatafeed.h"
#include "mcpcapture.h"

//!< A settings definition's properties, read from the swagger YAML that describe_settings serves.
//!< The current value of a setting cannot serve this purpose: generated SWG objects omit empty
//!< strings, so a blank setting is indistinguishable from a key that does not exist
struct SettingsSchema
{
    bool m_known = false;           //!< False when the definition was not found, so nothing can be judged
    QMap<QString, QString> m_types; //!< Property name -> YAML type: integer, number, string, array or object
};

class WebAPIAdapterInterface;
class MCPStreams;
class DeviceAPI;
class ChannelAPI;
class Feature;

namespace SWGSDRangel {
    class SWGObject;
    class SWGErrorResponse;
}

// Counts MainCore structural change signals so that tool handlers running on HTTP threads
// can wait for asynchronous operations (which complete on the main thread) to finish.
// Lives in the main thread; the counters are read from other threads.
class MCPEventCounter : public QObject
{
    Q_OBJECT
public:
    MCPEventCounter();

    QAtomicInt m_deviceSetAdded;
    QAtomicInt m_deviceSetRemoved;
    QAtomicInt m_deviceChanged;
    QAtomicInt m_channelAdded;
    QAtomicInt m_channelRemoved;
    QAtomicInt m_featureAdded;
    QAtomicInt m_featureRemoved;

private slots:
    void onDeviceSetAdded(int index, DeviceAPI *device);
    void onDeviceSetRemoved(int index);
    void onDeviceChanged(int index);
    void onChannelAdded(int deviceSetIndex, ChannelAPI *channel);
    void onChannelRemoved(int deviceSetIndex, ChannelAPI *channel);
    void onFeatureAdded(int featureSetIndex, Feature *feature);
    void onFeatureRemoved(int featureSetIndex, Feature *feature);
};

// The set of MCP tools that control SDRangel. Everything goes through the
// in-process Web API adapter, so what the tools can do matches the REST API,
// plus waiting for asynchronous operations to complete.
class MCPTools
{
public:
    struct Tool
    {
        QString name;
        QString description;
        QJsonObject inputSchema;
        std::function<QJsonValue(const QJsonObject& args)> handler;
    };

    explicit MCPTools(WebAPIAdapterInterface *webAPIAdapterInterface);

    QJsonArray listTools() const;
    QJsonObject callTool(const QString& name, const QJsonObject& arguments);

    // Data also served as resources
    QJsonObject getInstanceSummary();
    QJsonObject getAvailableDevices(int direction = -1); //!< -1 for all directions
    QJsonObject getAvailableChannels(int direction = -1);
    QJsonObject getAvailableFeatures();
    QJsonObject getPresets();
    QJsonObject getConfigurations();
    QJsonObject getDeviceSet(int deviceSetIndex);
    QString describeType(const QString& type, const QString& kind);
    MCPDocs& docs() { return m_docs; }
    MCPDataFeed& dataFeed() { return m_dataFeed; }
    MCPCapture& capture() { return m_capture; }
    void setStreams(MCPStreams *streams) { m_streams = streams; }
    //!< The feature that owns this server, so it can refuse to delete or stop itself
    void setOwnerFeature(const QObject *feature) { m_ownerFeature = feature; }

private:
    WebAPIAdapterInterface *m_adapter;
    MCPEventCounter m_events;
    MCPDocs m_docs;
    MCPDataFeed m_dataFeed;
    MCPCapture m_capture;
    MCPStreams *m_streams;

    // Creating an object and working out which one is new must not overlap with another
    // creation, or two callers snapshot the same state and both claim the same object.
    // The capture tools run without the dispatch lock, so this is reachable.
    QMutex m_creationMutex;
    // The channel each device set's last listen created, so the next one can take it away
    // again. listen runs without the dispatch lock, so this has a lock of its own, which is
    // never held across addChannelAndWait: that takes m_creationMutex
    QMutex m_listenMutex;
    QMap<int, const void *> m_listenChannels;
    const QObject *m_ownerFeature;
    QList<Tool> m_tools;
    QMap<QString, QString> m_yamlDefinitions; //!< Swagger definition name -> YAML text
    QMap<QString, QString> m_yamlDefinitionsByLower; //!< Lower cased name -> the name as written
    QMap<QString, SettingsSchema> m_schemaCache; //!< Parsed form of the above, by definition name
    bool m_yamlLoaded;

    void add(const QString& name, const QString& description, const QJsonObject& inputSchema,
        std::function<QJsonValue(const QJsonObject& args)> handler);

    void registerInstanceTools();
    void registerDeviceSetTools();
    void registerChannelTools();
    void registerFeatureTools();
    void registerPresetTools();
    void registerWorkspaceTools();
    void registerCaptureTools();
    void registerIntentTools();

    // Device set helpers
    int deviceSetCount() const;
    int channelCount(int deviceSetIndex) const;
    int featureCount() const;
    int deviceSetDirection(int deviceSetIndex);
    QJsonObject selectDevice(int deviceSetIndex, const QJsonObject& args);
    QJsonObject getDeviceSettings(int deviceSetIndex);
    QJsonObject patchDeviceSettings(int deviceSetIndex, const QJsonObject& partial);
    int addChannelAndWait(int deviceSetIndex, const QString& channelType);
    int addFeatureAndWait(const QString& featureType);
    int addDeviceSetAndWait(int direction);
    //!< Identity of the current channels, features or device sets, to spot which one is new
    QSet<const void *> channelPointers(int deviceSetIndex) const;
    QSet<const void *> featurePointers() const;
    QSet<const void *> deviceSetPointers() const;
    void deleteChannelAndWait(int deviceSetIndex, int channelIndex);
    //!< Deletes a channel wherever it has moved to, for callers that held on to it across a wait
    void deleteChannelObjectAndWait(const void *channel);
    const void *featureAt(int featureIndex) const;
    const void *channelAt(int deviceSetIndex, int channelIndex) const;
    void requireNotSelf(int featureIndex, const QString& action) const;
    QJsonObject getChannelSettings(int deviceSetIndex, int channelIndex);
    QJsonObject patchChannelSettings(int deviceSetIndex, int channelIndex, const QJsonObject& partial);
    //!< The property names and types of one settings definition. Used to tell a misspelled key from
    //!< a valid one, and to reject a value of the wrong type before it reaches the SWG object
    SettingsSchema settingsSchema(const QString& definition);
    QString resolveDefinition(const QString& name);
    QJsonObject getSpectrumSettings(int deviceSetIndex);
    //!< unknownKeys, when given, receives the patch keys the schema does not define
    QJsonObject patchSpectrumSettings(int deviceSetIndex, const QJsonObject& partial, QStringList *unknownKeys = nullptr);
    //!< Explains an rdsReport that is absent because RDS decoding is switched off, rather than
    //!< because nothing has been decoded yet
    void addRdsHint(QJsonObject& report, const QString& channelType, int deviceSetIndex, int channelIndex);
    QJsonObject getFeatureSettings(int featureIndex);
    QJsonObject patchFeatureSettings(int featureIndex, const QJsonObject& partial);
    QJsonObject deviceState(int deviceSetIndex, int subsystemIndex, int run); //!< run: 1 start, 0 stop, -1 query
    //!< Empty unless the device set's baseband is too narrow for this channel type
    QString basebandWarning(int deviceSetIndex, const QString& channelType);
    //!< The same for every channel a device set holds
    QStringList basebandWarnings(int deviceSetIndex);
    QJsonObject featureState(int featureIndex, int run);
    QJsonObject presetIdentifier(const QJsonObject& args, int deviceSetIndex, bool forSave);

    // Shared by the intent level tools
    QJsonObject channelReport(int deviceSetIndex, int channelIndex);
    void postChannelAction(int deviceSetIndex, int channelIndex, const QJsonObject& actions);
    QJsonObject pickReceiver(const QJsonObject& args, int minBaseband);
    //!< Channel type ids for a direction, independent of how list_channel_types formats them
    QStringList channelTypeIds(int direction);
    //!< Where a channel is now. The intent tools sleep for many seconds without holding the
    //!< protocol mutex, and a concurrent request that deletes a lower numbered channel
    //!< renumbers everything above it, so their indices have to be resolved again before use.
    void relocateChannel(const void *channel, int& deviceSetIndex, int& channelIndex, const QString& what);

    void loadYamlDefinitions();
};

#endif // INCLUDE_FEATURE_MCPTOOLS_H_
