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
    int deviceSetCount() const;
    QString describeType(const QString& type, const QString& kind);
    MCPDocs& docs() { return m_docs; }
    MCPDataFeed& dataFeed() { return m_dataFeed; }
    MCPCapture& capture() { return m_capture; }
    void setStreams(MCPStreams *streams) { m_streams = streams; }
    //!< The feature that owns this server, so it can refuse to delete or stop itself
    void setOwnerFeature(const QObject *feature) { m_ownerFeature = feature; }
    //!< Set by the server, on the main thread, before it waits for the HTTP threads to finish.
    //!< Every wait and sleep in the tools then ends at once with a tool error, since the main
    //!< thread they would otherwise be waiting on is the one waiting for them
    void setStopping(bool stopping);

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
    // Deletion is by index, and the index is resolved just before the delete is posted for
    // the main thread to do later. Two calls deleting on one device set unserialised can post
    // two indices that were right when read and wrong once the first delete has renumbered
    // the rest, taking a channel neither meant. Held from the lookup to the object being gone
    QMutex m_deletionMutex;
    // The channels listen and scan have added, so that the next of either on the same device
    // set can take them away again and cleanup can remove them all. Held by uid rather than
    // pointer: a channel removed from the GUI frees its address for the next one created,
    // which must not then be taken for one of these. They run without the dispatch lock, so
    // this has a lock of its own, which is never held across addChannelAndWait or a delete:
    // those take m_creationMutex or wait
    QMutex m_intentMutex;
    QSet<uint64_t> m_intentChannels;
    // The features listen added for a mode's output (AIS, Radiosonde, APRS), removed again once no
    // channel it added still feeds them. A feature that was already there is never among these
    QSet<uint64_t> m_intentFeatures;
    const QObject *m_ownerFeature;
    QAtomicInt m_stopping;
    QList<Tool> m_tools;
    // The definitions are loaded on first use and the parsed schemas cached as they are asked
    // for, from whichever thread asks. The settings tools run under the dispatch lock but
    // listen and scan do not, and they patch settings too, so these have a lock of their own,
    // taken by describeType and settingsSchema; the helpers they call expect it held
    QMutex m_schemaMutex;
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

    //!< Polls until the condition holds, at most timeoutMs. False on timeout; throws when the
    //!< server is stopping, as nothing waited for on the main thread will happen then
    bool waitFor(const std::function<bool()>& condition, int timeoutMs = m_waitTimeoutMs) const;
    //!< A sleep that a stop cuts short, with the same tool error
    void pause(int ms) const;
    static const int m_waitTimeoutMs = 5000;

    // Device set helpers
    int channelCount(int deviceSetIndex) const;
    int featureCount() const;
    int deviceSetDirection(int deviceSetIndex);
    QJsonObject selectDevice(int deviceSetIndex, const QJsonObject& args);
    QJsonObject getDeviceSettings(int deviceSetIndex);
    QJsonObject patchDeviceSettings(int deviceSetIndex, const QJsonObject& partial);
    int addChannelAndWait(int deviceSetIndex, const QString& channelType);
    int addFeatureAndWait(const QString& featureType);
    int addDeviceSetAndWait(int direction);
    //!< Identity of the current channels, features or device sets, to spot which one is new.
    //!< Channels and features are identified by UID throughout: a pointer to one is only ever
    //!< good until the next wait, since a deleted object's address is reused by the next one
    //!< created, and every tool here waits
    QSet<uint64_t> channelUids(int deviceSetIndex) const;
    QSet<uint64_t> featureUids() const;
    QSet<const void *> deviceSetPointers() const;
    void deleteChannelAndWait(int deviceSetIndex, int channelIndex);
    void deleteChannelLocked(int deviceSetIndex, int channelIndex); //!< With m_deletionMutex held
    //!< Removes a device set that listen or scan created and could not finish setting up, if it
    //!< is still the last one, which is the only one the API can remove
    void discardNewDeviceSet(int deviceSetIndex, uint64_t identity);
    //!< What tells one device set from another that has since taken its index: the engine's UID
    //!< together with the object's address. Zero when there is no such device set
    uint64_t deviceSetIdentity(int deviceSetIndex) const;
    friend class NewDeviceSetGuard;
    //!< Deletes a channel wherever it has moved to, for callers that held on to it across a wait
    void deleteChannelObjectAndWait(uint64_t channelUid);
    const void *featureAt(int featureIndex) const;
    //!< The UID of the feature or channel at an index, 0 when there is none
    uint64_t featureUidAt(int featureIndex) const;
    uint64_t channelUidAt(int deviceSetIndex, int channelIndex) const;
    void requireNotSelf(int featureIndex, const QString& action) const;
    QJsonObject getChannelSettings(int deviceSetIndex, int channelIndex);
    QJsonObject patchChannelSettings(int deviceSetIndex, int channelIndex, const QJsonObject& partial);
    //!< The property names and types of one settings definition. Used to tell a misspelled key from
    //!< a valid one, and to reject a value of the wrong type before it reaches the SWG object
    SettingsSchema settingsSchema(const QString& definition);
    QString resolveDefinition(const QString& name); //!< Call with m_schemaMutex held
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
    //!< doomed: uids of channels the caller is about to remove, which the reused set is not counted as carrying.
    //!< profileGain: give a device set this creates the fixed receiver gain profile; false when the gain is about to be measured
    QJsonObject pickReceiver(const QJsonObject& args, int minBaseband, const QSet<uint64_t>& doomed = QSet<uint64_t>(), bool profileGain = true);
    //!< The tune_gain tool: sweeps the gain and applies the best, with the table it measured
    QJsonObject tuneGain(const QJsonObject& args);
    //!< The index of a feature of this type, adding one if there is none. Returns whether it was added
    int ensureFeature(const QString& featureType, int& featureIndex);
    void trackIntentFeature(uint64_t featureUid);
    //!< Removes the features listen added that no channel it added still feeds, except one of keepType.
    //!< Returns the types of those removed
    QStringList reclaimIntentFeatures(const QString& keepType = QString());
    //!< Deletes a feature wherever it has been renumbered to
    void deleteFeatureObjectAndWait(uint64_t featureUid);
    //!< Whether listen or scan should measure the gain: a device set they created, or a retune to another band
    static bool gainWorthTuning(bool reused, double previousCentre, double centre);
    void trackIntentChannel(uint64_t channelUid);
    void untrackIntentChannel(uint64_t channelUid);
    QSet<uint64_t> intentChannels();
    //!< A channel's type id and whether listen or scan added it, for the notes the intent tools leave
    struct ChannelNote { uint64_t m_uid; int m_index; QString m_id; bool m_intent; };
    QList<ChannelNote> channelNotes(int deviceSetIndex, const QSet<uint64_t>& except = QSet<uint64_t>());
    //!< Removes the channels listen and scan added to a device set, except keep. Returns the ids of those removed
    QStringList reclaimIntentChannels(int deviceSetIndex, const QSet<uint64_t>& keep = QSet<uint64_t>());
    //!< The channel a previous listen added that a new one of this type can retune instead of replacing, or 0.
    //!< wanted: how many of the type the mode uses, 2 for a paired mode
    uint64_t reusableIntentChannel(int deviceSetIndex, const QString& channelType, int& channelIndex, int wanted = 1);
    //!< What a retune of a reused device set did to the channels listen and scan did not add
    struct RetuneOutcome
    {
        QStringList m_kept;     //!< Re-offset so that they stay on the frequency they had
        QStringList m_removed;  //!< Audio demodulators the new baseband could not hold, which would only have made noise
        QStringList m_stranded; //!< Left where they were: not audio, or the baseband unknown
    };
    RetuneOutcome retuneOtherChannels(int deviceSetIndex, double previousCentre, double centre, int baseband);
    //!< What listen and scan tell the caller about the rest of the device set once their own channels are in
    QStringList intentNotes(int deviceSetIndex, const QSet<uint64_t>& added, const QStringList& reclaimed,
        bool reused, double previousCentre, double centre, const RetuneOutcome& outcome);
    //!< Channel type ids for a direction, independent of how list_channel_types formats them
    QStringList channelTypeIds(int direction);
    //!< Where a channel is now. The intent tools sleep for many seconds without holding the
    //!< protocol mutex, and a concurrent request that deletes a lower numbered channel
    //!< renumbers everything above it, so their indices have to be resolved again before use.
    void relocateChannel(uint64_t channelUid, int& deviceSetIndex, int& channelIndex, const QString& what);

    void loadYamlDefinitions(); //!< Call with m_schemaMutex held
};

#endif // INCLUDE_FEATURE_MCPTOOLS_H_
