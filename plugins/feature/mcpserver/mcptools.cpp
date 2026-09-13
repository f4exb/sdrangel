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

#include <algorithm>
#include <cmath>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QThread>

#include "SWGErrorResponse.h"
#include "SWGSuccessResponse.h"
#include "SWGInstanceSummaryResponse.h"
#include "SWGInstanceDevicesResponse.h"
#include "SWGInstanceChannelsResponse.h"
#include "SWGInstanceFeaturesResponse.h"
#include "SWGDeviceSetList.h"
#include "SWGDeviceSet.h"
#include "SWGDeviceListItem.h"
#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGDeviceActions.h"
#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGChannelActions.h"
#include "SWGChannelsDetail.h"
#include "SWGFeatureSet.h"
#include "SWGFeatureSettings.h"
#include "SWGFeatureReport.h"
#include "SWGFeatureActions.h"
#include "SWGPresets.h"
#include "SWGPresetGroup.h"
#include "SWGPresetItem.h"
#include "SWGPresetTransfer.h"
#include "SWGPresetIdentifier.h"
#include "SWGConfigurations.h"
#include "SWGConfigurationIdentifier.h"
#include "SWGWorkspaceInfo.h"
#include "SWGWorkspaceActions.h"
#include "SWGGLSpectrum.h"
#include "SWGGLSpectrumReport.h"
#include "SWGSpectrumActions.h"
#include "SWGGLSpectrumData.h"
#include "SWGAudioDevices.h"
#include "SWGLocationInformation.h"

#include "maincore.h"
#include "device/deviceset.h"
#include "channel/channelapi.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "feature/featureset.h"
#include "webapi/webapiadapterinterface.h"
#include "webapi/webapiutils.h"

#include "mcpstreams.h"
#include "mcptools.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

const int waitTimeoutMs = 5000;

QJsonObject toJson(SWGSDRangel::SWGObject& object)
{
    QJsonObject *obj = object.asJsonObject();
    QJsonObject result = *obj;
    delete obj;
    return result;
}

QString compact(const QJsonValue& value)
{
    if (value.isObject()) {
        return QString(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    } else if (value.isArray()) {
        return QString(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    } else if (value.isString()) {
        return value.toString();
    } else {
        return QString(QJsonDocument(QJsonArray({value})).toJson(QJsonDocument::Compact));
    }
}

// Throws if the Web API call failed
void check(int httpRC, SWGSDRangel::SWGErrorResponse& error, const QString& what)
{
    if (httpRC / 100 != 2)
    {
        QString message = error.getMessage() ? *error.getMessage() : QString();
        throw MCPToolError(QString("%1 failed (HTTP %2)%3").arg(what).arg(httpRC).arg(message.isEmpty() ? "" : ": " + message));
    }
}

bool waitFor(const std::function<bool()>& condition, int timeoutMs = waitTimeoutMs)
{
    QElapsedTimer timer;
    timer.start();

    while (!condition())
    {
        if (timer.elapsed() > timeoutMs) {
            return false;
        }

        QThread::msleep(20);
    }

    return true;
}

// Argument access. Accepts numbers given as strings, as models sometimes do.
bool hasArg(const QJsonObject& args, const QString& key)
{
    return args.contains(key) && !args[key].isNull() && !args[key].isUndefined();
}

int argInt(const QJsonObject& args, const QString& key, bool required = true, int defaultValue = 0)
{
    if (!hasArg(args, key))
    {
        if (required) {
            throw MCPToolError(QString("Missing required argument: %1").arg(key));
        }

        return defaultValue;
    }

    QJsonValue value = args[key];

    if (value.isDouble()) {
        return (int) value.toDouble();
    }

    bool ok;
    int result = value.toString().trimmed().toInt(&ok);

    if (!ok)
    {
        double d = value.toString().trimmed().toDouble(&ok);

        if (ok) {
            return (int) d;
        }

        throw MCPToolError(QString("Argument %1 must be an integer").arg(key));
    }

    return result;
}

qint64 argInt64(const QJsonObject& args, const QString& key, bool required = true, qint64 defaultValue = 0)
{
    if (!hasArg(args, key))
    {
        if (required) {
            throw MCPToolError(QString("Missing required argument: %1").arg(key));
        }

        return defaultValue;
    }

    QJsonValue value = args[key];

    if (value.isDouble()) {
        return (qint64) value.toDouble();
    }

    bool ok;
    double d = value.toString().trimmed().toDouble(&ok);

    if (!ok) {
        throw MCPToolError(QString("Argument %1 must be a number").arg(key));
    }

    return (qint64) d;
}

double argDouble(const QJsonObject& args, const QString& key, bool required = true, double defaultValue = 0.0)
{
    if (!hasArg(args, key))
    {
        if (required) {
            throw MCPToolError(QString("Missing required argument: %1").arg(key));
        }

        return defaultValue;
    }

    QJsonValue value = args[key];

    if (value.isDouble()) {
        return value.toDouble();
    }

    bool ok;
    double d = value.toString().trimmed().toDouble(&ok);

    if (!ok) {
        throw MCPToolError(QString("Argument %1 must be a number").arg(key));
    }

    return d;
}

QString argString(const QJsonObject& args, const QString& key, bool required = true, const QString& defaultValue = QString())
{
    if (!hasArg(args, key))
    {
        if (required) {
            throw MCPToolError(QString("Missing required argument: %1").arg(key));
        }

        return defaultValue;
    }

    QJsonValue value = args[key];

    if (value.isString()) {
        return value.toString();
    } else if (value.isDouble()) {
        return QString::number(value.toDouble(), 'g', 15);
    } else {
        return compact(value);
    }
}

QJsonObject argObject(const QJsonObject& args, const QString& key, bool required = true)
{
    if (!hasArg(args, key))
    {
        if (required) {
            throw MCPToolError(QString("Missing required argument: %1").arg(key));
        }

        return QJsonObject();
    }

    QJsonValue value = args[key];

    if (value.isObject()) {
        return value.toObject();
    }

    // Tolerate an object passed as a JSON string
    if (value.isString())
    {
        QJsonDocument doc = QJsonDocument::fromJson(value.toString().toUtf8());

        if (doc.isObject()) {
            return doc.object();
        }
    }

    throw MCPToolError(QString("Argument %1 must be a JSON object").arg(key));
}

// direction: "rx" / "tx" / "mimo" or 0 / 1 / 2
int argDirection(const QJsonObject& args, const QString& key = "direction", bool required = false, int defaultValue = 0)
{
    if (!hasArg(args, key))
    {
        if (required) {
            throw MCPToolError(QString("Missing required argument: %1").arg(key));
        }

        return defaultValue;
    }

    QJsonValue value = args[key];

    if (value.isDouble()) {
        return (int) value.toDouble();
    }

    QString s = value.toString().trimmed().toLower();

    if ((s == "rx") || (s == "0") || (s == "receive") || (s == "source")) {
        return 0;
    } else if ((s == "tx") || (s == "1") || (s == "transmit") || (s == "sink")) {
        return 1;
    } else if ((s == "mimo") || (s == "2")) {
        return 2;
    }

    throw MCPToolError(QString("Argument %1 must be \"rx\", \"tx\" or \"mimo\"").arg(key));
}

QString directionName(int direction)
{
    switch (direction)
    {
    case 0: return "rx";
    case 1: return "tx";
    case 2: return "mimo";
    default: return QString::number(direction);
    }
}

// JSON schema builders
QJsonObject prop(const QString& type, const QString& description)
{
    QJsonObject p;
    p["type"] = type;
    p["description"] = description;
    return p;
}

QJsonObject intProp(const QString& description) { return prop("integer", description); }
QJsonObject numProp(const QString& description) { return prop("number", description); }
QJsonObject strProp(const QString& description) { return prop("string", description); }
QJsonObject objProp(const QString& description) { return prop("object", description); }

QJsonObject enumProp(const QString& description, const QStringList& values)
{
    QJsonObject p = strProp(description);
    p["enum"] = QJsonArray::fromStringList(values);
    return p;
}

QJsonObject directionProp(const QString& description)
{
    return enumProp(description, {"rx", "tx", "mimo"});
}

// Adds the bounds a handler enforces at runtime to the schema, so a client can see them
QJsonObject bounded(QJsonObject property, double minimum, double maximum)
{
    property["minimum"] = minimum;
    property["maximum"] = maximum;
    return property;
}

QJsonObject schema(const QJsonObject& properties, const QStringList& required = QStringList())
{
    QJsonObject s;
    s["type"] = "object";
    s["properties"] = properties;
    s["required"] = QJsonArray::fromStringList(required);
    s["additionalProperties"] = false;
    return s;
}

const QJsonObject deviceSetIndexProp = intProp("Index of the device set (0 for R0, the first device set)");
const QJsonObject channelIndexProp = intProp("Index of the channel within the device set");
const QJsonObject featureIndexProp = intProp("Index of the feature within the feature set");

// Settings keys in the form used by the Web API PATCH: nested keys are dotted, array elements indexed
void extractKeys(const QJsonObject& json, QStringList& keys, const QString& prefix = QString())
{
    for (const QString& key : json.keys())
    {
        QString full = prefix.isEmpty() ? key : prefix + "." + key;
        keys.append(full);

        if (json[key].isObject())
        {
            extractKeys(json[key].toObject(), keys, full);
        }
        else if (json[key].isArray())
        {
            QJsonArray array = json[key].toArray();

            for (int i = 0; i < array.count(); i++)
            {
                QString element = QString("%1[%2]").arg(full).arg(i);
                keys.append(element);

                if (array.at(i).isObject()) {
                    extractKeys(array.at(i).toObject(), keys, element);
                }
            }
        }
    }
}

// Applies patch on top of target. Objects are merged recursively, everything else replaced.
// Keys absent from target are reported (they may still be valid: empty strings are not serialized).
void mergeInto(QJsonObject& target, const QJsonObject& patch, QStringList& unknownKeys, const QString& prefix = QString())
{
    for (const QString& key : patch.keys())
    {
        QString full = prefix.isEmpty() ? key : prefix + "." + key;

        if (!target.contains(key)) {
            unknownKeys.append(full);
        }

        if (patch[key].isObject() && target[key].isObject())
        {
            QJsonObject sub = target[key].toObject();
            mergeInto(sub, patch[key].toObject(), unknownKeys, full);
            target[key] = sub;
        }
        else
        {
            target[key] = patch[key];
        }
    }
}

int leadingSpaces(const QString& line)
{
    int count = 0;

    while ((count < line.length()) && (line.at(count) == ' ')) {
        count++;
    }

    return count;
}

// Reads the property names and types out of one swagger definition. Only the properties of the
// definition itself are taken: the attributes of a property sit two spaces further in, so an
// items: block of an array cannot be mistaken for the array's own type.
SettingsSchema parseSettingsSchema(const QString& yaml)
{
    SettingsSchema schema;
    int propertiesIndent = -1;
    int nameIndent = -1;
    QString property;

    for (const QString& line : yaml.split('\n'))
    {
        QString text = line.trimmed();

        if (text.isEmpty() || text.startsWith('#')) {
            continue;
        }

        int indent = leadingSpaces(line);

        if (propertiesIndent < 0)
        {
            if (text == "properties:") {
                propertiesIndent = indent;
            }

            continue;
        }

        if (indent <= propertiesIndent) {
            break; // out of the properties block and into the next section of the definition
        }

        if (nameIndent < 0) {
            nameIndent = indent;
        }

        if (indent == nameIndent)
        {
            if (text.endsWith(':'))
            {
                property = text.left(text.length() - 1);
                schema.m_types.insert(property, QString());
            }
            else
            {
                property.clear();
            }
        }
        else if ((indent == nameIndent + 2) && !property.isEmpty())
        {
            if (text.startsWith("type:")) {
                schema.m_types[property] = text.mid(5).trimmed();
            } else if (text.startsWith("$ref:") && schema.m_types.value(property).isEmpty()) {
                schema.m_types[property] = "object";
            }
        }
    }

    schema.m_known = !schema.m_types.isEmpty();
    return schema;
}

// Checks a patch against the schema and returns it with the one conversion worth making. Values
// are checked here rather than left to the SWG object, whose setValue() turns a string into zero
// without complaint: "rfBandwidth": "wide" would otherwise stop the channel demodulating and be
// reported as a successful change.
QJsonObject checkAgainstSchema(const QJsonObject& patch, const SettingsSchema& schema,
    QStringList& unknownKeys, QStringList& typeErrors)
{
    QJsonObject checked;

    for (const QString& key : patch.keys())
    {
        QJsonValue value = patch[key];

        if (!schema.m_types.contains(key))
        {
            unknownKeys.append(key);
            checked[key] = value;
            continue;
        }

        QString type = schema.m_types[key];
        bool numeric = (type == "integer") || (type == "number");

        if (numeric && value.isBool())
        {
            // Unambiguous and lossless, and the natural way to write a 0/1 flag
            checked[key] = value.toBool() ? 1 : 0;
            continue;
        }

        bool ok = true;

        if (numeric) {
            ok = value.isDouble();
        } else if (type == "string") {
            ok = value.isString();
        } else if (type == "array") {
            ok = value.isArray();
        } else if (type == "object") {
            ok = value.isObject();
        }

        if (!ok)
        {
            QString got = value.isString() ? "string" :
                          value.isDouble() ? "number" :
                          value.isBool() ? "boolean" :
                          value.isArray() ? "array" :
                          value.isObject() ? "object" : "null";
            typeErrors.append(QString("%1 expects %2 but was given a %3 (%4)")
                .arg(key).arg(type).arg(got).arg(compact(QJsonObject{{key, value}})));
        }

        checked[key] = value;
    }

    return checked;
}

// The key holding the type specific settings, e.g. "rtlSdrSettings" or "ADSBDemodSettings"
QString findSettingsKey(const QJsonObject& json)
{
    for (const QString& key : json.keys())
    {
        if (key.endsWith("Settings") && json[key].isObject()) {
            return key;
        }
    }

    return QString();
}

// Generic settings patch: read current settings, merge the partial object given by the model
// into the type specific sub-object, then PATCH with the list of changed keys.
template<typename SWG>
QJsonObject applySettingsPatch(
    const std::function<int(SWG&, SWGSDRangel::SWGErrorResponse&)>& get,
    const std::function<int(const QStringList&, SWG&, SWGSDRangel::SWGErrorResponse&)>& put,
    const QJsonObject& partial,
    const QString& what,
    const std::function<SettingsSchema(const QString&)>& schemaFor)
{
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    SWG current;
    check(get(current, error), error, "Get " + what);
    QJsonObject json = toJson(current);
    QString settingsKey = findSettingsKey(json);

    if (settingsKey.isEmpty()) {
        throw MCPToolError(QString("Cannot find settings object in current %1: %2").arg(what).arg(compact(json)));
    }

    QJsonObject subPartial;

    if (partial.contains(settingsKey))
    {
        subPartial = partial[settingsKey].toObject();
    }
    else
    {
        for (const QString& key : partial.keys())
        {
            if (key.endsWith("Settings") && partial[key].isObject()) {
                throw MCPToolError(QString("Settings object key %1 does not match the current type. Expected %2 or the bare settings object").arg(key).arg(settingsKey));
            }
        }

        subPartial = partial;
    }

    if (subPartial.isEmpty()) {
        throw MCPToolError("No settings given");
    }

    QStringList unknownKeys;
    SettingsSchema schema = schemaFor(settingsKey);

    if (schema.m_known)
    {
        QStringList typeErrors;
        subPartial = checkAgainstSchema(subPartial, schema, unknownKeys, typeErrors);

        if (!typeErrors.isEmpty())
        {
            throw MCPToolError(QString("Wrong value type for %1: %2. Numbers must be given as JSON "
                "numbers, not strings. Nothing was changed.").arg(settingsKey).arg(typeErrors.join("; ")));
        }
    }

    QJsonObject sub = json[settingsKey].toObject();
    QStringList merged;
    mergeInto(sub, subPartial, merged);
    json[settingsKey] = sub;

    if (!schema.m_known)
    {
        // Without a schema the only test left is whether the key is in the current settings, which
        // misses any string setting that is currently empty. Better than nothing, but not by much
        unknownKeys = merged;
    }

    QStringList keys;
    extractKeys(subPartial, keys);

    SWG request;
    request.fromJsonObject(json);
    error.init();
    check(put(keys, request, error), error, "Set " + what);

    // Reply with only what was asked for, read back from the object that was sent. Echoing
    // the whole settings block cost a client about 500 tokens per call, carried for the rest
    // of the conversation, to confirm a one key change.
    QJsonObject after = toJson(request)[settingsKey].toObject();
    QJsonObject changed;

    for (const QString& key : subPartial.keys())
    {
        if (after.contains(key)) {
            changed[key] = after[key];
        }
    }

    QJsonObject result;
    result["changed"] = changed;

    if (!unknownKeys.isEmpty()) {
        result["warning"] = QString("Ignored unknown keys: %1. describe_settings lists the valid ones.").arg(unknownKeys.join(", "));
    }

    return result;
}

QString capitalize(const QString& s)
{
    return s.isEmpty() ? s : s.at(0).toUpper() + s.mid(1);
}

// The plugin lists carry a URI, a version and an index per entry that no caller needs. Each
// becomes one string: the id the other tools take, with the display name in brackets when it
// says something the id does not.
// Features with no run state: they work from the moment they are configured. Some answer the run
// request with 501 and the rest accept it and stay idle, so neither the request nor the state that
// follows it tells them apart from a feature that is slow to start
const QStringList passiveFeatureTypes = {
    "AIS", "AMBE", "AntennaTools", "APRS", "FreqDisplay", "LimeRFE", "Map", "Radiosonde",
    "RemoteControl", "SkyMap"
};

bool isPassiveFeature(const QString& featureType)
{
    return passiveFeatureTypes.contains(featureType);
}

QJsonArray idsAndNames(const QJsonArray& plugins)
{
    QJsonArray out;

    for (const QJsonValue& v : plugins)
    {
        QJsonObject p = v.toObject();
        QString id = p["id"].toString();
        QString name = p["name"].toString();

        if (!name.isEmpty() && (name.compare(id, Qt::CaseInsensitive) != 0)) {
            out.append(QString("%1 (%2)").arg(id).arg(name));
        } else {
            out.append(id);
        }
    }

    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// Event counter
// ---------------------------------------------------------------------------

MCPEventCounter::MCPEventCounter() :
    m_deviceSetAdded(0),
    m_deviceSetRemoved(0),
    m_deviceChanged(0),
    m_channelAdded(0),
    m_channelRemoved(0),
    m_featureAdded(0),
    m_featureRemoved(0)
{
    MainCore *mainCore = MainCore::instance();
    connect(mainCore, &MainCore::deviceSetAdded, this, &MCPEventCounter::onDeviceSetAdded);
    connect(mainCore, &MainCore::deviceSetRemoved, this, &MCPEventCounter::onDeviceSetRemoved);
    connect(mainCore, &MainCore::deviceChanged, this, &MCPEventCounter::onDeviceChanged);
    connect(mainCore, &MainCore::channelAdded, this, &MCPEventCounter::onChannelAdded);
    connect(mainCore, &MainCore::channelRemoved, this, &MCPEventCounter::onChannelRemoved);
    connect(mainCore, &MainCore::featureAdded, this, &MCPEventCounter::onFeatureAdded);
    connect(mainCore, &MainCore::featureRemoved, this, &MCPEventCounter::onFeatureRemoved);
}

void MCPEventCounter::onDeviceSetAdded(int index, DeviceAPI *device)
{
    (void) index;
    (void) device;
    m_deviceSetAdded.fetchAndAddRelease(1);
}

void MCPEventCounter::onDeviceSetRemoved(int index)
{
    (void) index;
    m_deviceSetRemoved.fetchAndAddRelease(1);
}

void MCPEventCounter::onDeviceChanged(int index)
{
    (void) index;
    m_deviceChanged.fetchAndAddRelease(1);
}

void MCPEventCounter::onChannelAdded(int deviceSetIndex, ChannelAPI *channel)
{
    (void) deviceSetIndex;
    (void) channel;
    m_channelAdded.fetchAndAddRelease(1);
}

void MCPEventCounter::onChannelRemoved(int deviceSetIndex, ChannelAPI *channel)
{
    (void) deviceSetIndex;
    (void) channel;
    m_channelRemoved.fetchAndAddRelease(1);
}

void MCPEventCounter::onFeatureAdded(int featureSetIndex, Feature *feature)
{
    (void) featureSetIndex;
    (void) feature;
    m_featureAdded.fetchAndAddRelease(1);
}

void MCPEventCounter::onFeatureRemoved(int featureSetIndex, Feature *feature)
{
    (void) featureSetIndex;
    (void) feature;
    m_featureRemoved.fetchAndAddRelease(1);
}

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------

MCPTools::MCPTools(WebAPIAdapterInterface *webAPIAdapterInterface) :
    m_adapter(webAPIAdapterInterface),
    m_capture(webAPIAdapterInterface),
    m_streams(nullptr),
    m_ownerFeature(nullptr),
    m_yamlLoaded(false)
{
    registerInstanceTools();
    registerDeviceSetTools();
    registerChannelTools();
    registerFeatureTools();
    registerPresetTools();
    registerWorkspaceTools();
    registerCaptureTools();
    registerIntentTools();
}

// Per tool behaviour. The MCP hints are narrower than they look: destructiveHint false means
// the tool is additive only, so anything that overwrites existing state is destructive, and
// openWorldHint true means the tool can reach beyond this application, which covers commanding
// the radio, features that talk to network services, and data arriving from unknown transmitters.
struct ToolBehaviour
{
    bool m_readOnly;
    bool m_destructive;
    bool m_idempotent;
    bool m_openWorld;
};

static ToolBehaviour behaviourFor(const QString& name)
{
    // Reads of this instance's own configuration and of documentation compiled into the plugin
    static const QStringList localReads = {
        "get_instance_summary", "list_available_devices", "list_channel_types", "list_feature_types",
        "describe_settings", "get_receiving_guide", "list_docs", "get_docs", "list_audio_devices",
        "get_location", "get_deviceset", "get_device_settings",
        "get_spectrum_settings", "get_spectrum_report", "get_spectrum_data", "get_channel_settings", "get_feature_settings",
        "list_presets", "list_configurations", "get_server_status", "get_status"
    };

    // Reads that return data received off the air, from transmitters this instance does not
    // control, or from the network services some features talk to. Reports carry it as well as
    // the dedicated data feeds do: an ADS-B report, for instance, lists the aircraft heard.
    static const QStringList offAirReads = {
        "get_packets", "get_map_items", "get_channel_report",
        "get_device_report", "get_feature_report"
    };

    // Purely additive: they create something new and overwrite nothing
    static const QStringList additive = {
        "add_deviceset", "add_channel", "add_feature", "add_workspace", "add_annotation"
    };

    if (localReads.contains(name)) {
        return {true, false, true, false};
    }

    if (offAirReads.contains(name)) {
        return {true, false, true, true};
    }

    if (additive.contains(name)) {
        return {false, false, false, true};
    }

    // Everything else changes or removes state. Only the workspace moves and the settings writes
    // land in the same place when repeated; captures, actions and creation do not.
    bool idempotent = name.startsWith("set_") || name.startsWith("stop_") || (name == "clear_packets");
    return {false, true, idempotent, true};
}

void MCPTools::add(const QString& name, const QString& description, const QJsonObject& inputSchema,
    std::function<QJsonValue(const QJsonObject& args)> handler)
{
    Tool tool;
    tool.name = name;
    tool.description = description;
    tool.inputSchema = inputSchema;
    tool.handler = handler;
    m_tools.append(tool);
}

QJsonArray MCPTools::listTools() const
{
    QJsonArray tools;

    for (const Tool& tool : m_tools)
    {
        QJsonObject t;
        t["name"] = tool.name;
        t["description"] = tool.description;
        t["inputSchema"] = tool.inputSchema;
        ToolBehaviour behaviour = behaviourFor(tool.name);
        // Clients carry the whole tool list on every turn, so only say what differs from the
        // defaults the MCP schema gives an absent hint (read-write, destructive, not
        // idempotent, open world), and no title, which would repeat the description
        QJsonObject annotations;
        if (behaviour.m_readOnly) { annotations["readOnlyHint"] = true; }
        if (!behaviour.m_destructive) { annotations["destructiveHint"] = false; }
        if (behaviour.m_idempotent) { annotations["idempotentHint"] = true; }
        if (!behaviour.m_openWorld) { annotations["openWorldHint"] = false; }
        if (!annotations.isEmpty()) { t["annotations"] = annotations; }
        tools.append(t);
    }

    return tools;
}

QJsonObject MCPTools::callTool(const QString& name, const QJsonObject& arguments)
{
    const Tool *tool = nullptr;

    for (const Tool& t : m_tools)
    {
        if (t.name == name)
        {
            tool = &t;
            break;
        }
    }

    if (!tool) {
        throw MCPError(-32602, QString("Unknown tool: %1").arg(name));
    }

    QJsonObject result;
    QJsonArray content;

    try
    {
        // Every input schema says additionalProperties: false, so an argument a tool does not
        // define is a mistake worth reporting. Dropping it silently turns a wrong call into a
        // plausible wrong answer: get_packets, which filters on source and type, looks as though
        // it is ignoring a deviceSetIndex and channelIndex filter it never had
        QJsonObject properties = tool->inputSchema.value("properties").toObject();
        QStringList unknown;

        for (const QString& key : arguments.keys())
        {
            if (!properties.contains(key)) {
                unknown.append(key);
            }
        }

        if (!unknown.isEmpty())
        {
            QStringList accepted = properties.keys();
            throw MCPToolError(QString("%1 does not take %2. It accepts: %3")
                .arg(name)
                .arg(unknown.join(", "))
                .arg(accepted.isEmpty() ? QString("no arguments") : accepted.join(", ")));
        }

        QJsonValue value = tool->handler(arguments);
        QJsonObject text;
        text["type"] = "text";
        text["text"] = compact(value);
        content.append(text);

        if (value.isObject()) {
            result["structuredContent"] = value;
        }

        result["isError"] = false;
    }
    catch (const MCPToolError& e)
    {
        qDebug() << "MCPTools::callTool:" << name << "error:" << e.message;
        QJsonObject text;
        text["type"] = "text";
        text["text"] = "Error: " + e.message;
        content.append(text);
        result["isError"] = true;
    }

    result["content"] = content;
    return result;
}

// ---------------------------------------------------------------------------
// Instance level data
// ---------------------------------------------------------------------------

QJsonObject MCPTools::getInstanceSummary()
{
    SWGSDRangel::SWGInstanceSummaryResponse response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    check(m_adapter->instanceSummary(response, error), error, "Get instance summary");
    return toJson(response);
}

QJsonObject MCPTools::getAvailableDevices(int direction)
{
    QJsonObject result;

    for (int dir = 0; dir < 3; dir++)
    {
        if ((direction >= 0) && (direction != dir)) {
            continue;
        }

        SWGSDRangel::SWGInstanceDevicesResponse response;
        SWGSDRangel::SWGErrorResponse error;
        error.init();
        check(m_adapter->instanceDevices(dir, response, error), error, "List devices");
        result[directionName(dir)] = toJson(response)["devices"].toArray();
    }

    return result;
}

QJsonObject MCPTools::getAvailableChannels(int direction)
{
    QJsonObject result;

    for (int dir = 0; dir < 3; dir++)
    {
        if ((direction >= 0) && (direction != dir)) {
            continue;
        }

        SWGSDRangel::SWGInstanceChannelsResponse response;
        SWGSDRangel::SWGErrorResponse error;
        error.init();
        check(m_adapter->instanceChannels(dir, response, error), error, "List channel types");
        result[directionName(dir)] = idsAndNames(toJson(response)["channels"].toArray());
    }

    return result;
}

QJsonObject MCPTools::getAvailableFeatures()
{
    SWGSDRangel::SWGInstanceFeaturesResponse response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    check(m_adapter->instanceFeatures(response, error), error, "List feature types");
    QJsonObject result;
    QJsonArray features;

    for (const QJsonValue& v : toJson(response)["features"].toArray())
    {
        QString entry = v.toObject()["id"].toString();
        features.append(isPassiveFeature(entry)
            ? QString("%1 [passive: no run state, do not start it]").arg(idsAndNames(QJsonArray{v}).first().toString())
            : idsAndNames(QJsonArray{v}).first().toString());
    }

    result["features"] = features;
    return result;
}

QJsonObject MCPTools::getPresets()
{
    SWGSDRangel::SWGPresets response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    check(m_adapter->instancePresetsGet(response, error), error, "List presets");
    return toJson(response);
}

QJsonObject MCPTools::getConfigurations()
{
    SWGSDRangel::SWGConfigurations response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    check(m_adapter->instanceConfigurationsGet(response, error), error, "List configurations");
    return toJson(response);
}

QJsonObject MCPTools::getDeviceSet(int deviceSetIndex)
{
    SWGSDRangel::SWGDeviceSet response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    check(m_adapter->devicesetGet(deviceSetIndex, response, error), error, QString("Get device set %1").arg(deviceSetIndex));
    return toJson(response);
}

void MCPTools::loadYamlDefinitions()
{
    if (m_yamlLoaded) {
        return;
    }

    m_yamlLoaded = true;
    QDir dir(":/webapi/doc/swagger/include");
    QRegularExpression definitionStart("^([A-Za-z0-9_]+):\\s*$");

    for (const QString& fileName : dir.entryList(QStringList("*.yaml"), QDir::Files))
    {
        QFile file(dir.filePath(fileName));

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        QString name;
        QStringList block;

        for (const QString& line : QString::fromUtf8(file.readAll()).split('\n'))
        {
            QRegularExpressionMatch match = definitionStart.match(line);

            if (match.hasMatch())
            {
                if (!name.isEmpty()) {
                    m_yamlDefinitions[name] = block.join('\n').trimmed();
                }

                name = match.captured(1);
                block.clear();
            }

            block.append(line);
        }

        if (!name.isEmpty()) {
            m_yamlDefinitions[name] = block.join('\n').trimmed();
        }
    }

    for (auto name = m_yamlDefinitions.keyBegin(); name != m_yamlDefinitions.keyEnd(); ++name) {
        m_yamlDefinitionsByLower.insert(name->toLower(), *name);
    }

    qDebug("MCPTools::loadYamlDefinitions: %d definitions", (int) m_yamlDefinitions.size());
}

// The JSON settings key does not always capitalise the way its swagger definition does:
// testSourceSettings comes from TestSourceSettings, and sdrPlayV3Settings from SDRPlayV3Settings.
// Only the case ever differs, so a name that does not match exactly is looked up without it.
// Returns an empty string when there is no such definition
QString MCPTools::resolveDefinition(const QString& name)
{
    loadYamlDefinitions();

    if (m_yamlDefinitions.contains(name)) {
        return name;
    }

    return m_yamlDefinitionsByLower.value(name.toLower());
}

QString MCPTools::describeType(const QString& typeIn, const QString& kindIn)
{
    loadYamlDefinitions();
    QString type = typeIn.trimmed();
    QString kind = kindIn.trimmed().toLower();
    // (settings JSON key, description of what matched)
    QList<QPair<QString, QString>> matches;

    // The spectrum display is not a type: its definitions are named after it directly
    const QString lower = type.toLower();

    if ((kind == "spectrum") || (lower == "spectrum") || (lower == "glspectrum"))
    {
        QStringList out;
        out.append("# main spectrum display of a device set\n# get_spectrum_settings / set_spectrum_settings, get_spectrum_report, spectrum_action\n"
                   "# get_docs spectrum explains the display and its controls\n");

        for (const QString& definition : {"GLSpectrum", "GLSpectrumReport", "SpectrumActions"})
        {
            if (m_yamlDefinitions.contains(definition))
            {
                out.append(m_yamlDefinitions[definition]);
                out.append("");
            }
        }

        return out.join('\n');
    }

    if (kind.isEmpty() || (kind == "channel"))
    {
        if (WebAPIUtils::m_channelTypeToSettingsKey.contains(type)) {
            matches.append({WebAPIUtils::m_channelTypeToSettingsKey[type], "channel type " + type});
        }
    }

    if (kind.isEmpty() || (kind == "device"))
    {
        if (WebAPIUtils::m_sourceDeviceHwIdToSettingsKey.contains(type)) {
            matches.append({WebAPIUtils::m_sourceDeviceHwIdToSettingsKey[type], "rx device " + type});
        }
        if (WebAPIUtils::m_sinkDeviceHwIdToSettingsKey.contains(type)) {
            matches.append({WebAPIUtils::m_sinkDeviceHwIdToSettingsKey[type], "tx device " + type});
        }
        if (WebAPIUtils::m_mimoDeviceHwIdToSettingsKey.contains(type)) {
            matches.append({WebAPIUtils::m_mimoDeviceHwIdToSettingsKey[type], "mimo device " + type});
        }
    }

    if (kind.isEmpty() || (kind == "feature"))
    {
        if (WebAPIUtils::m_featureTypeToSettingsKey.contains(type)) {
            matches.append({WebAPIUtils::m_featureTypeToSettingsKey[type], "feature type " + type});
        }
    }

    // Also accept a definition name given directly, e.g. "RtlSdrSettings" or "RtlSdr"
    if (matches.isEmpty())
    {
        if (!resolveDefinition(type + "Settings").isEmpty()) {
            matches.append({type + "Settings", type});
        } else if (!resolveDefinition(type).isEmpty()) {
            matches.append({type, type});
        }
    }

    if (matches.isEmpty())
    {
        throw MCPToolError(QString("Unknown type %1. Use the id values returned by list_channel_types, list_feature_types "
            "or the hwType values returned by list_available_devices").arg(type));
    }

    QStringList out;

    for (const auto& match : matches)
    {
        QString settingsKey = match.first;
        QString base = capitalize(settingsKey);

        if (base.endsWith("Settings")) {
            base.chop(QString("Settings").size());
        }

        out.append(QString("# %1\n# Settings JSON key: %2\n").arg(match.second).arg(settingsKey));
        bool found = false;

        for (const QString suffix : {"Settings", "Report", "Actions"})
        {
            QString definition = resolveDefinition(base + suffix);

            if (!definition.isEmpty())
            {
                out.append(m_yamlDefinitions[definition]);
                out.append("");
                found = true;
            }
        }

        if (!found) {
            out.append(QString("No documentation found for %1").arg(base));
        }
    }

    return out.join('\n');
}

// ---------------------------------------------------------------------------
// Device set helpers
// ---------------------------------------------------------------------------

int MCPTools::deviceSetCount() const
{
    return (int) MainCore::instance()->getDeviceSets().size();
}

int MCPTools::channelCount(int deviceSetIndex) const
{
    const std::vector<DeviceSet*>& deviceSets = MainCore::instance()->getDeviceSets();

    if ((deviceSetIndex < 0) || (deviceSetIndex >= (int) deviceSets.size())) {
        return -1;
    }

    return deviceSets[deviceSetIndex]->getNumberOfChannels();
}

int MCPTools::featureCount() const
{
    const std::vector<FeatureSet*>& featureSets = MainCore::instance()->getFeatureeSets();

    if (featureSets.empty()) {
        return -1;
    }

    return featureSets[0]->getNumberOfFeatures();
}

int MCPTools::deviceSetDirection(int deviceSetIndex)
{
    QJsonObject deviceSet = getDeviceSet(deviceSetIndex);
    return deviceSet["samplingDevice"].toObject()["direction"].toInt(0);
}

// Selects the sampling device of a device set from hwType / serial / sequence / displayedName / deviceIndex
// and waits for SDRangel to switch to it.
QJsonObject MCPTools::selectDevice(int deviceSetIndex, const QJsonObject& args)
{
    int direction = deviceSetDirection(deviceSetIndex);
    QString hwType = argString(args, "hwType", false);
    QString serial = argString(args, "serial", false);
    QString displayedName = argString(args, "displayedName", false);
    int sequence = argInt(args, "sequence", false, -1);
    int streamIndex = argInt(args, "deviceStreamIndex", false, -1);

    if (hasArg(args, "deviceIndex"))
    {
        // Resolve the index in the available devices list to concrete identifiers
        int deviceIndex = argInt(args, "deviceIndex");
        QJsonArray devices = getAvailableDevices(direction)[directionName(direction)].toArray();
        bool found = false;

        for (const auto& value : devices)
        {
            QJsonObject device = value.toObject();

            if (device["index"].toInt(-1) == deviceIndex)
            {
                hwType = device["hwType"].toString();
                serial = device["serial"].toString();
                sequence = device["sequence"].toInt(-1);
                streamIndex = device["deviceStreamIndex"].toInt(-1);
                displayedName = device["displayedName"].toString();
                found = true;
                break;
            }
        }

        if (!found) {
            throw MCPToolError(QString("No %1 device with index %2 in the available devices list").arg(directionName(direction)).arg(deviceIndex));
        }
    }

    if (hwType.isEmpty() && serial.isEmpty() && displayedName.isEmpty()) {
        throw MCPToolError("Specify at least one of hwType, serial, displayedName or deviceIndex to select a device");
    }

    SWGSDRangel::SWGDeviceListItem query;
    query.setDirection(direction);
    query.setIndex(-1);
    query.setSequence(sequence);
    query.setDeviceStreamIndex(streamIndex);

    if (!hwType.isEmpty()) {
        query.setHwType(new QString(hwType));
    }
    if (!serial.isEmpty()) {
        query.setSerial(new QString(serial));
    }
    if (!displayedName.isEmpty()) {
        query.setDisplayedName(new QString(displayedName));
    }

    SWGSDRangel::SWGDeviceListItem response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    int changedBefore = m_events.m_deviceChanged.loadAcquire();
    check(m_adapter->devicesetDevicePut(deviceSetIndex, query, response, error), error, "Select device");

    QJsonObject selected = toJson(response);
    QString wantHwType = selected["hwType"].toString();
    QString wantSerial = selected["serial"].toString();
    int wantSequence = selected["sequence"].toInt(-1);

    // The change completes on the main thread and is signalled by MainCore::deviceChanged
    QJsonObject deviceSet;
    bool done = waitFor([&]()
    {
        if (m_events.m_deviceChanged.loadAcquire() == changedBefore) {
            return false;
        }

        deviceSet = getDeviceSet(deviceSetIndex);
        QJsonObject sampling = deviceSet["samplingDevice"].toObject();
        return (sampling["hwType"].toString() == wantHwType)
            && (wantSerial.isEmpty() || (sampling["serial"].toString() == wantSerial))
            && ((wantSequence < 0) || (sampling["sequence"].toInt(-1) == wantSequence));
    });

    QJsonObject result;
    result["deviceSetIndex"] = deviceSetIndex;
    result["selectedDevice"] = selected;
    result["deviceSet"] = deviceSet;

    if (!done) {
        result["warning"] = "Timed out waiting for the device change to complete. Call get_deviceset to check.";
    }

    return result;
}

QJsonObject MCPTools::getDeviceSettings(int deviceSetIndex)
{
    SWGSDRangel::SWGDeviceSettings response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    check(m_adapter->devicesetDeviceSettingsGet(deviceSetIndex, response, error), error, "Get device settings");
    return toJson(response);
}

QJsonObject MCPTools::patchDeviceSettings(int deviceSetIndex, const QJsonObject& partial)
{
    return applySettingsPatch<SWGSDRangel::SWGDeviceSettings>(
        [&](SWGSDRangel::SWGDeviceSettings& r, SWGSDRangel::SWGErrorResponse& e) {
            return m_adapter->devicesetDeviceSettingsGet(deviceSetIndex, r, e);
        },
        [&](const QStringList& keys, SWGSDRangel::SWGDeviceSettings& r, SWGSDRangel::SWGErrorResponse& e) {
            return m_adapter->devicesetDeviceSettingsPutPatch(deviceSetIndex, false, keys, r, e);
        },
        partial, QString("device settings of device set %1").arg(deviceSetIndex),
        [this](const QString& definition) { return settingsSchema(definition); });
}

QSet<const void *> MCPTools::channelPointers(int deviceSetIndex) const
{
    QSet<const void *> result;
    const std::vector<DeviceSet*>& deviceSets = MainCore::instance()->getDeviceSets();

    if ((deviceSetIndex < 0) || (deviceSetIndex >= (int) deviceSets.size())) {
        return result;
    }

    DeviceSet *deviceSet = deviceSets[deviceSetIndex];

    for (int i = 0; i < deviceSet->getNumberOfChannels(); i++) {
        result.insert(deviceSet->getChannelAt(i));
    }

    return result;
}

QSet<const void *> MCPTools::featurePointers() const
{
    QSet<const void *> result;
    const std::vector<FeatureSet*>& featureSets = MainCore::instance()->getFeatureeSets();

    if (featureSets.empty()) {
        return result;
    }

    for (int i = 0; i < featureSets[0]->getNumberOfFeatures(); i++) {
        result.insert(featureSets[0]->getFeatureAt(i));
    }

    return result;
}

QSet<const void *> MCPTools::deviceSetPointers() const
{
    QSet<const void *> result;

    for (DeviceSet *deviceSet : MainCore::instance()->getDeviceSets()) {
        result.insert(deviceSet);
    }

    return result;
}

// Adds a channel and waits for SDRangel to create it (which happens on the main thread),
// returning its index. The channel is ready to accept settings when this returns.
//
// The new channel is found by identity rather than by taking the last index: the GUI or
// another request can create a channel at the same time, and picking the highest index would
// then configure or delete somebody else's channel.
int MCPTools::addChannelAndWait(int deviceSetIndex, const QString& channelType)
{
    QMutexLocker creationLock(&m_creationMutex);
    int direction = deviceSetDirection(deviceSetIndex);
    int before = channelCount(deviceSetIndex);

    if (before < 0) {
        throw MCPToolError(QString("No device set with index %1").arg(deviceSetIndex));
    }

    QSet<const void *> existing = channelPointers(deviceSetIndex);
    SWGSDRangel::SWGChannelSettings query;
    query.setDirection(direction);
    query.setChannelType(new QString(channelType));
    SWGSDRangel::SWGSuccessResponse response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    int addedBefore = m_events.m_channelAdded.loadAcquire();
    check(m_adapter->devicesetChannelPost(deviceSetIndex, query, response, error), error, QString("Add channel %1").arg(channelType));

    int channelIndex = -1;
    bool created = waitFor([&]()
    {
        if (m_events.m_channelAdded.loadAcquire() <= addedBefore) {
            return false;
        }

        const std::vector<DeviceSet*>& deviceSets = MainCore::instance()->getDeviceSets();

        if ((deviceSetIndex < 0) || (deviceSetIndex >= (int) deviceSets.size())) {
            return false;
        }

        DeviceSet *deviceSet = deviceSets[deviceSetIndex];

        for (int i = 0; i < deviceSet->getNumberOfChannels(); i++)
        {
            ChannelAPI *channel = deviceSet->getChannelAt(i);

            if (!channel || existing.contains(channel)) {
                continue;
            }

            QString id;
            channel->getIdentifier(id);

            if (id == channelType)
            {
                channelIndex = i;
                return true;
            }
        }

        return false;
    });

    if (!created || (channelIndex < 0)) {
        throw MCPToolError(QString("Timed out waiting for the %1 channel to be created").arg(channelType));
    }

    // The channel needs a moment before it can accept settings
    waitFor([&]()
    {
        SWGSDRangel::SWGChannelSettings s;
        SWGSDRangel::SWGErrorResponse e;
        e.init();
        return m_adapter->devicesetChannelSettingsGet(deviceSetIndex, channelIndex, s, e) / 100 == 2;
    }, 2000);

    return channelIndex;
}

// As for channels, the new feature is found by identity so that a feature created at the same
// time by the GUI or another request is never mistaken for this one.
int MCPTools::addFeatureAndWait(const QString& featureType)
{
    QMutexLocker creationLock(&m_creationMutex);
    if (featureCount() < 0) {
        throw MCPToolError("No feature set available");
    }

    QSet<const void *> existing = featurePointers();
    SWGSDRangel::SWGFeatureSettings query;
    query.setFeatureType(new QString(featureType));
    SWGSDRangel::SWGSuccessResponse response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    int addedBefore = m_events.m_featureAdded.loadAcquire();
    check(m_adapter->featuresetFeaturePost(0, query, response, error), error, QString("Add feature %1").arg(featureType));

    int featureIndex = -1;
    auto findNew = [&]()
    {
        const std::vector<FeatureSet*>& featureSets = MainCore::instance()->getFeatureeSets();

        if (featureSets.empty()) {
            return -1;
        }

        for (int i = 0; i < featureSets[0]->getNumberOfFeatures(); i++)
        {
            Feature *feature = featureSets[0]->getFeatureAt(i);

            if (!feature || existing.contains(feature)) {
                continue;
            }

            if (feature->getIdentifier() == featureType) {
                return i;
            }
        }

        return -1;
    };

    // The wait ends as soon as the feature appears, so a longer limit only costs time when one
    // genuinely fails to be created. The Map takes a while: it brings up a web engine
    waitFor([&]()
    {
        if (m_events.m_featureAdded.loadAcquire() <= addedBefore) {
            return false;
        }

        featureIndex = findNew();
        return featureIndex >= 0;
    }, 15000);

    if (featureIndex < 0)
    {
        // Look once more before reporting a failure. A caller told that creation failed when it
        // did not will retry, and every retry builds another one
        featureIndex = findNew();
    }

    if (featureIndex < 0) {
        throw MCPToolError(QString("Timed out waiting for the %1 feature to be created").arg(featureType));
    }

    // The feature needs a moment before it can accept settings
    waitFor([&]()
    {
        SWGSDRangel::SWGFeatureSettings s;
        SWGSDRangel::SWGErrorResponse e;
        e.init();
        return m_adapter->featuresetFeatureSettingsGet(0, featureIndex, s, e) / 100 == 2;
    }, 2000);

    return featureIndex;
}

// Device set creation runs as a state machine on the main thread; MainCore::deviceSetAdded
// marks the end. The new set is identified by pointer for the same reason as above.
int MCPTools::addDeviceSetAndWait(int direction)
{
    QMutexLocker creationLock(&m_creationMutex);
    QSet<const void *> existing = deviceSetPointers();
    int before = m_events.m_deviceSetAdded.loadAcquire();
    SWGSDRangel::SWGSuccessResponse response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    check(m_adapter->instanceDeviceSetPost(direction, response, error), error, "Add device set");

    int deviceSetIndex = -1;
    bool created = waitFor([&]()
    {
        if (m_events.m_deviceSetAdded.loadAcquire() <= before) {
            return false;
        }

        const std::vector<DeviceSet*>& deviceSets = MainCore::instance()->getDeviceSets();

        for (int i = 0; i < (int) deviceSets.size(); i++)
        {
            if (!existing.contains(deviceSets[i]))
            {
                deviceSetIndex = i;
                return true;
            }
        }

        return false;
    });

    if (!created || (deviceSetIndex < 0)) {
        throw MCPToolError("Timed out waiting for the device set to be created");
    }

    return deviceSetIndex;
}

// Deletes a channel and waits for SDRangel to remove it
// Watches the channel itself disappear rather than the removed signal, which is not emitted
// for the last channel of a device set
void MCPTools::deleteChannelAndWait(int deviceSetIndex, int channelIndex)
{
    const void *target = channelAt(deviceSetIndex, channelIndex);
    SWGSDRangel::SWGSuccessResponse response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    check(m_adapter->devicesetChannelDelete(deviceSetIndex, channelIndex, response, error), error, "Delete channel");

    if (!waitFor([&]() { return !channelPointers(deviceSetIndex).contains(target); })) {
        throw MCPToolError(QString("Timed out waiting for channel %1:%2 to be removed").arg(deviceSetIndex).arg(channelIndex));
    }

    m_capture.forgetRecording(target);
}

// Deletes a channel identified by the object itself. A caller that waited, for instance for a
// recording to finish, cannot rely on the index it started with: deleting a lower numbered
// channel meanwhile renumbers everything above it.
void MCPTools::deleteChannelObjectAndWait(const void *channel)
{
    int deviceSetIndex = -1;
    int channelIndex = -1;

    if (!MCPCapture::locateChannel(channel, deviceSetIndex, channelIndex))
    {
        m_capture.forgetRecording(channel);
        return; // already gone, which is what was asked for
    }

    deleteChannelAndWait(deviceSetIndex, channelIndex);
}

const void *MCPTools::featureAt(int featureIndex) const
{
    const std::vector<FeatureSet*>& featureSets = MainCore::instance()->getFeatureeSets();

    if (featureSets.empty() || (featureIndex < 0) || (featureIndex >= featureSets[0]->getNumberOfFeatures())) {
        return nullptr;
    }

    return featureSets[0]->getFeatureAt(featureIndex);
}

const void *MCPTools::channelAt(int deviceSetIndex, int channelIndex) const
{
    const std::vector<DeviceSet*>& deviceSets = MainCore::instance()->getDeviceSets();

    if ((deviceSetIndex < 0) || (deviceSetIndex >= (int) deviceSets.size())) {
        return nullptr;
    }

    return deviceSets[deviceSetIndex]->getChannelAt(channelIndex);
}

// Deleting or stopping the feature that is serving this request would tear down the HTTP
// listener from inside its own handler, so it is refused rather than attempted.
void MCPTools::requireNotSelf(int featureIndex, const QString& action) const
{
    if (m_ownerFeature && (featureAt(featureIndex) == m_ownerFeature))
    {
        throw MCPToolError(QString("Feature %1 is the MCP Server serving this request and cannot be %2 through it. "
            "Use the SDRangel GUI or the REST API instead.").arg(featureIndex).arg(action));
    }
}

QJsonObject MCPTools::getChannelSettings(int deviceSetIndex, int channelIndex)
{
    SWGSDRangel::SWGChannelSettings response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    check(m_adapter->devicesetChannelSettingsGet(deviceSetIndex, channelIndex, response, error), error, "Get channel settings");
    return toJson(response);
}

SettingsSchema MCPTools::settingsSchema(const QString& definition)
{
    if (m_schemaCache.contains(definition)) {
        return m_schemaCache[definition];
    }

    SettingsSchema schema;
    // Without this no device setting found its schema and none of them were type checked
    QString name = resolveDefinition(definition);

    if (!name.isEmpty()) {
        schema = parseSettingsSchema(m_yamlDefinitions[name]);
    }

    m_schemaCache.insert(definition, schema);
    return schema;
}

QJsonObject MCPTools::getSpectrumSettings(int deviceSetIndex)
{
    SWGSDRangel::SWGGLSpectrum response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    check(m_adapter->devicesetSpectrumSettingsGet(deviceSetIndex, response, error), error, "Get spectrum settings");
    return toJson(response);
}

QJsonObject MCPTools::patchSpectrumSettings(int deviceSetIndex, const QJsonObject& partial, QStringList *unknownKeys)
{
    SWGSDRangel::SWGGLSpectrum current;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    check(m_adapter->devicesetSpectrumSettingsGet(deviceSetIndex, current, error), error, "Get spectrum settings");
    QJsonObject json = toJson(current);
    QStringList unknown;
    QJsonObject checked = partial;
    SettingsSchema schema = settingsSchema("GLSpectrum");

    if (schema.m_known)
    {
        QStringList typeErrors;
        checked = checkAgainstSchema(partial, schema, unknown, typeErrors);

        if (!typeErrors.isEmpty())
        {
            throw MCPToolError(QString("Wrong value type for spectrum settings: %1. Numbers must be "
                "given as JSON numbers, not strings. Nothing was changed.").arg(typeErrors.join("; ")));
        }
    }

    QStringList merged;
    mergeInto(json, checked, merged);

    if (!schema.m_known) {
        unknown = merged;
    }

    if (unknownKeys) {
        *unknownKeys = unknown;
    }

    QStringList keys;
    extractKeys(checked, keys);
    SWGSDRangel::SWGGLSpectrum request;
    request.init();
    request.fromJsonObject(json);
    error.init();
    check(m_adapter->devicesetSpectrumSettingsPutPatch(deviceSetIndex, false, keys, request, error), error, "Set spectrum settings");
    return toJson(request);
}

void MCPTools::addRdsHint(QJsonObject& report, const QString& channelType, int deviceSetIndex, int channelIndex)
{
    if (channelType != "BFMDemod") {
        return;
    }

    if (report.value("BFMDemodReport").toObject().contains("rdsReport")) {
        return;
    }

    // The report is empty for a channel that is not running as well, where saying anything about
    // RDS would only mislead, so the hint is tied to the setting that actually suppresses it
    QJsonObject settings = getChannelSettings(deviceSetIndex, channelIndex).value("BFMDemodSettings").toObject();

    if (settings.value("rdsActive").toInt(0) == 0)
    {
        report["rdsHint"] = "RDS is not being decoded, which is why this report has no rdsReport. Set rdsActive to 1 with "
                            "set_channel_settings to get the programme service name (the station name), radio text, programme "
                            "type and PI code.";
    }
}

QJsonObject MCPTools::patchChannelSettings(int deviceSetIndex, int channelIndex, const QJsonObject& partial)
{
    return applySettingsPatch<SWGSDRangel::SWGChannelSettings>(
        [&](SWGSDRangel::SWGChannelSettings& r, SWGSDRangel::SWGErrorResponse& e) {
            return m_adapter->devicesetChannelSettingsGet(deviceSetIndex, channelIndex, r, e);
        },
        [&](const QStringList& keys, SWGSDRangel::SWGChannelSettings& r, SWGSDRangel::SWGErrorResponse& e) {
            return m_adapter->devicesetChannelSettingsPutPatch(deviceSetIndex, channelIndex, false, keys, r, e);
        },
        partial, QString("channel settings of channel %1:%2").arg(deviceSetIndex).arg(channelIndex),
        [this](const QString& definition) { return settingsSchema(definition); });
}

QJsonObject MCPTools::getFeatureSettings(int featureIndex)
{
    SWGSDRangel::SWGFeatureSettings response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    check(m_adapter->featuresetFeatureSettingsGet(0, featureIndex, response, error), error, "Get feature settings");
    return toJson(response);
}

QJsonObject MCPTools::patchFeatureSettings(int featureIndex, const QJsonObject& partial)
{
    return applySettingsPatch<SWGSDRangel::SWGFeatureSettings>(
        [&](SWGSDRangel::SWGFeatureSettings& r, SWGSDRangel::SWGErrorResponse& e) {
            return m_adapter->featuresetFeatureSettingsGet(0, featureIndex, r, e);
        },
        [&](const QStringList& keys, SWGSDRangel::SWGFeatureSettings& r, SWGSDRangel::SWGErrorResponse& e) {
            return m_adapter->featuresetFeatureSettingsPutPatch(0, featureIndex, false, keys, r, e);
        },
        partial, QString("feature settings of feature %1").arg(featureIndex),
        [this](const QString& definition) { return settingsSchema(definition); });
}

QJsonObject MCPTools::deviceState(int deviceSetIndex, int subsystemIndex, int run)
{
    SWGSDRangel::SWGDeviceState response;
    response.init();
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    int rc;

    // The subsystem variants only apply to MIMO devices; single stream devices use the plain calls
    bool mimo = deviceSetDirection(deviceSetIndex) == 2;

    auto runGet = [&](SWGSDRangel::SWGDeviceState& s, SWGSDRangel::SWGErrorResponse& e) {
        return mimo ? m_adapter->devicesetDeviceSubsystemRunGet(deviceSetIndex, subsystemIndex, s, e)
                    : m_adapter->devicesetDeviceRunGet(deviceSetIndex, s, e);
    };

    if (run == 1) {
        rc = mimo ? m_adapter->devicesetDeviceSubsystemRunPost(deviceSetIndex, subsystemIndex, response, error)
                  : m_adapter->devicesetDeviceRunPost(deviceSetIndex, response, error);
    } else if (run == 0) {
        rc = mimo ? m_adapter->devicesetDeviceSubsystemRunDelete(deviceSetIndex, subsystemIndex, response, error)
                  : m_adapter->devicesetDeviceRunDelete(deviceSetIndex, response, error);
    } else {
        rc = runGet(response, error);
    }

    check(rc, error, run == 1 ? "Start device" : (run == 0 ? "Stop device" : "Get device state"));

    // The state returned by POST/DELETE is the state before the change. Give the device a moment
    // and report the state actually reached.
    QJsonObject result;
    result["deviceSetIndex"] = deviceSetIndex;

    if (run >= 0)
    {
        QString want = run == 1 ? "running" : "idle";
        QString readError;

        bool reached = waitFor([&]()
        {
            SWGSDRangel::SWGDeviceState state;
            state.init();
            SWGSDRangel::SWGErrorResponse e;
            e.init();
            int getRC = runGet(state, e);

            if (getRC / 100 != 2)
            {
                readError = QString("HTTP %1 %2").arg(getRC).arg(e.getMessage() ? *e.getMessage() : QString());
                return true; // stop polling, reported below
            }

            response.init();
            *response.getState() = *state.getState();
            return *state.getState() == want || *state.getState() == "error";
        }, 3000);

        if (!readError.isEmpty()) {
            throw MCPToolError(QString("%1 was requested but the device state could not be read back: %2").arg(run == 1 ? "Start" : "Stop").arg(readError));
        }

        if (!reached) {
            result["warning"] = QString("The device did not reach the %1 state within 3 seconds. The state reported here is the last one observed").arg(want);
        }
    }

    result["state"] = *response.getState();
    return result;
}

QJsonObject MCPTools::featureState(int featureIndex, int run)
{
    // Asking a passive feature to run waits three seconds for a state it never reports, or fails
    // outright on the ones that answer 501, so say what it is instead of doing either
    if (run >= 0)
    {
        QString featureType = getFeatureSettings(featureIndex)["featureType"].toString();

        if (isPassiveFeature(featureType))
        {
            QJsonObject passive;
            passive["featureIndex"] = featureIndex;
            passive["featureType"] = featureType;
            passive["state"] = "idle";
            passive["passive"] = true;
            passive["note"] = QString("%1 has no run state and does not need starting: it works as soon as it "
                "is configured, and reports idle whether or not it is doing anything.").arg(featureType);
            return passive;
        }
    }

    SWGSDRangel::SWGDeviceState response;
    response.init();
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    int rc;

    if (run == 1) {
        rc = m_adapter->featuresetFeatureRunPost(0, featureIndex, response, error);
    } else if (run == 0) {
        rc = m_adapter->featuresetFeatureRunDelete(0, featureIndex, response, error);
    } else {
        rc = m_adapter->featuresetFeatureRunGet(0, featureIndex, response, error);
    }

    check(rc, error, run == 1 ? "Start feature" : (run == 0 ? "Stop feature" : "Get feature state"));

    QJsonObject result;
    result["featureIndex"] = featureIndex;

    if (run >= 0)
    {
        QString want = run == 1 ? "running" : "idle";
        QString readError;

        bool reached = waitFor([&]()
        {
            SWGSDRangel::SWGDeviceState state;
            state.init();
            SWGSDRangel::SWGErrorResponse e;
            e.init();
            int getRC = m_adapter->featuresetFeatureRunGet(0, featureIndex, state, e);

            if (getRC / 100 != 2)
            {
                readError = QString("HTTP %1 %2").arg(getRC).arg(e.getMessage() ? *e.getMessage() : QString());
                return true; // stop polling, reported below
            }

            response.init();
            *response.getState() = *state.getState();
            return *state.getState() == want || *state.getState() == "error";
        }, 3000);

        if (!readError.isEmpty()) {
            throw MCPToolError(QString("%1 was requested but the feature state could not be read back: %2").arg(run == 1 ? "Start" : "Stop").arg(readError));
        }

        if (!reached) {
            result["warning"] = QString("The feature did not reach the %1 state within 3 seconds. The state reported here is the last one observed").arg(want);
        }
    }

    result["state"] = *response.getState();
    return result;
}

// Resolves group name + name (and optionally centre frequency and type) to the full preset identifier
// SDRangel needs. For a save, the identifier of the current device is used when no matching preset exists.
QJsonObject MCPTools::presetIdentifier(const QJsonObject& args, int deviceSetIndex, bool forSave)
{
    QString groupName = argString(args, "groupName");
    QString name = argString(args, "name");
    QString type = argString(args, "type", false);
    bool hasFrequency = hasArg(args, "centerFrequency");
    qint64 centerFrequency = argInt64(args, "centerFrequency", false, 0);

    QJsonObject deviceSet;
    QString deviceType;
    qint64 deviceFrequency = 0;

    if (forSave)
    {
        deviceSet = getDeviceSet(deviceSetIndex);
        QJsonObject sampling = deviceSet["samplingDevice"].toObject();
        deviceFrequency = (qint64) sampling["centerFrequency"].toDouble();
        int direction = sampling["direction"].toInt(0);
        deviceType = direction == 0 ? "R" : (direction == 1 ? "T" : "M");
    }

    QJsonArray candidates;

    for (const auto& groupValue : getPresets()["groups"].toArray())
    {
        QJsonObject group = groupValue.toObject();

        if (group["groupName"].toString() != groupName) {
            continue;
        }

        for (const auto& presetValue : group["presets"].toArray())
        {
            QJsonObject preset = presetValue.toObject();

            if (preset["name"].toString() != name) {
                continue;
            }
            if (!type.isEmpty() && (preset["type"].toString() != type)) {
                continue;
            }
            if (hasFrequency && ((qint64) preset["centerFrequency"].toDouble() != centerFrequency)) {
                continue;
            }
            if (forSave && (preset["type"].toString() != deviceType)) {
                continue;
            }

            candidates.append(preset);
        }
    }

    QJsonObject identifier;
    identifier["groupName"] = groupName;
    identifier["name"] = name;

    if (candidates.count() == 1)
    {
        QJsonObject preset = candidates.first().toObject();
        identifier["type"] = preset["type"].toString();
        identifier["centerFrequency"] = preset["centerFrequency"].toDouble();
        identifier["exists"] = true;
    }
    else if (candidates.count() > 1)
    {
        throw MCPToolError(QString("Several presets match group %1 name %2. Specify centerFrequency and type to choose one of: %3")
            .arg(groupName).arg(name).arg(compact(candidates)));
    }
    else if (forSave)
    {
        identifier["type"] = deviceType;
        identifier["centerFrequency"] = (double) deviceFrequency;
        identifier["exists"] = false;
    }
    else
    {
        throw MCPToolError(QString("No preset with group %1 and name %2. Call list_presets to see what exists").arg(groupName).arg(name));
    }

    return identifier;
}

// ---------------------------------------------------------------------------
// Tool registration
// ---------------------------------------------------------------------------

void MCPTools::registerInstanceTools()
{
    add("get_instance_summary",
        "Get an overview of the SDRangel instance: version, all device sets with their sampling device and channels, and all features. "
        "Call this first to learn the current state and the indexes to use with other tools.",
        schema(QJsonObject()),
        [this](const QJsonObject&) { return getInstanceSummary(); });

    add("list_available_devices",
        "List the SDR hardware and software sources/sinks that can be selected in a device set. Each entry gives hwType (used by "
        "set_device and describe_settings), serial, sequence and index. Physical devices must be plugged in before SDRangel starts.",
        schema({{"direction", directionProp("Only list devices for this direction (rx, tx or mimo). Default: all directions")}}),
        [this](const QJsonObject& args) { return getAvailableDevices(argDirection(args, "direction", false, -1)); });

    add("list_channel_types",
        "List the channel plugins (demodulators for rx, modulators for tx) that can be added to a device set. The id is the channelType "
        "to pass to add_channel and describe_settings.",
        schema({{"direction", directionProp("Only list channels for this direction (rx, tx or mimo). Default: all directions")}}),
        [this](const QJsonObject& args) { return getAvailableChannels(argDirection(args, "direction", false, -1)); });

    add("list_feature_types",
        "List the feature plugins that can be added with add_feature. The id is the featureType.",
        schema(QJsonObject()),
        [this](const QJsonObject&) { return getAvailableFeatures(); });

    add("describe_settings",
        "Get the documentation (OpenAPI YAML) of the settings, report and actions keys of a device, channel or feature type, "
        "or of the spectrum display: what each key means, its units, range and enumeration values. Not needed for common keys; "
        "call it when a set_* tool reports an unknown key, or before setting a key whose values are codes, such as a style or "
        "mode. The output can run to a few thousand tokens.",
        schema({
            {"type", strProp("Type identifier: a device hwType such as RTLSDR or TestSource, a channelType such as ADSBDemod or NFMDemod, "
                             "a featureType such as Map, or spectrum for the spectrum display")},
            {"kind", strProp("Optional: device, channel, feature or spectrum, to disambiguate when the same id exists for several kinds")}
        }, {"type"}),
        [this](const QJsonObject& args) { return QJsonValue(describeType(argString(args, "type"), argString(args, "kind", false))); });

    add("get_receiving_guide",
        "Read the receiving guide: which demodulator and frequency to use for a given signal or protocol (ADS-B, AIS, APRS, "
        "radiosondes, weather satellites, broadcast FM, LoRa, pagers and more), the minimum device sample rate some modes need, "
        "and the usual reasons nothing is received. Read this before setting up a receiver for a named signal, rather than "
        "guessing frequencies.",
        schema(QJsonObject()),
        [this](const QJsonObject&) { return QJsonValue(m_docs.guide()); });

    add("list_docs",
        "List the user documentation (readme) available for the device, channel and feature plugins registered in this instance, "
        "and for the GUI's own pages such as the spectrum display, with the section headings of each. Use get_docs to read one.",
        schema({{"kind", strProp("Optional: only list device, channel, feature or gui docs")}}),
        [this](const QJsonObject& args)
        {
            QString kind = argString(args, "kind", false).trimmed().toLower();
            QJsonObject index = m_docs.index();

            if (kind.isEmpty()) {
                return index;
            }

            QJsonArray filtered;

            for (const auto& value : index["docs"].toArray())
            {
                if (value.toObject()["kind"].toString() == kind) {
                    filtered.append(value);
                }
            }

            QJsonObject result;
            result["count"] = filtered.count();
            result["docs"] = filtered;
            return result;
        });

    add("get_docs",
        "Read the user documentation (readme, as markdown) of a device, channel or feature plugin, or of a GUI page such as the "
        "spectrum display: what it does, what each setting means, frequencies and sample rates it needs, and how to use it. Pass "
        "section to get just one section (matched against the headings listed by list_docs) as the full documents can be long.",
        schema({
            {"type", strProp("Plugin id (e.g. ADSBDemod, RTLSDR, Map), its displayed name (e.g. \"ADS-B Demodulator\"), or a GUI page: "
                             "spectrum, spectrummarkers, spectrummeasurements, spectrumcalibration, mainspectrum, audio, configurations, deviceuserargs, transverter")},
            {"kind", strProp("Optional: device, channel, feature or gui, to disambiguate")},
            {"section", strProp("Optional: text of a heading; only that section is returned")}
        }, {"type"}),
        [this](const QJsonObject& args)
        {
            QString type = argString(args, "type");
            QString kind = argString(args, "kind", false);
            const MCPDocs::Doc *doc = m_docs.find(type, kind);

            if (!doc) {
                throw MCPToolError(QString("No documentation found for %1. Call list_docs to see what is available").arg(type));
            }

            QString section = argString(args, "section", false);
            QString text = m_docs.text(*doc, section);

            if (text.isEmpty()) {
                throw MCPToolError(QString("No section matching \"%1\" in the %2 documentation. Sections: %3").arg(section).arg(doc->id).arg(doc->headings.join(" | ")));
            }

            return QJsonValue(text);
        });

    add("get_packets",
        "Get recently decoded packets from packet-oriented demodulators (AIS, APRS/AX.25 packet, LoRa/ChirpChat, M17, Meshtastic, "
        "MeshCore, Inmarsat, radiosonde). Each packet has the raw bytes (hex and printable text) and, where SDRangel can decode it, "
        "a decoded object: AIS message type, MMSI, position, course and speed; AX.25 from/to/via and data with APRS position and "
        "comment; RS41 radiosonde serial, position and height. Packets are kept in a buffer of the last 1000; use sinceSequence "
        "with the lastSequence of a previous call to read only new packets.",
        schema({
            {"source", strProp("Only packets from this channel, e.g. \"R0:1\" (device set 0, channel 1)")},
            {"type", strProp("Only packets from channels of this type, e.g. AISDemod, PacketDemod, RadiosondeDemod")},
            {"limit", bounded(intProp("Maximum number of packets to return, most recent first. Default 50"), 1, MCPDataFeed::m_maxPackets)},
            {"sinceSequence", intProp("Only packets with a sequence number greater than this")}
        }),
        [this](const QJsonObject& args)
        {
            return m_dataFeed.getPackets(argString(args, "source", false), argString(args, "type", false),
                argInt(args, "limit", false, 50), argInt64(args, "sinceSequence", false, 0));
        });

    add("clear_packets",
        "Discard all buffered packets.",
        schema(QJsonObject()),
        [this](const QJsonObject&)
        {
            m_dataFeed.clearPackets();
            QJsonObject result;
            result["cleared"] = true;
            return result;
        });

    add("get_map_items",
        "Get the objects currently plotted on the map by the running plugins: aircraft from ADS-B, ships from AIS, APRS stations, "
        "radiosondes, satellites, the Sun and Moon from the star tracker, VOR/ILS beacons and so on. Each item has a name, position, "
        "altitude, heading, the label and text shown on the map, and for aircraft the full ADS-B state. Only sources that are "
        "running produce items; items disappear when their source removes them.",
        schema({
            {"source", strProp("Only items from this channel or feature id (e.g. \"R0:0\", \"F:1\") or plugin type (e.g. ADSBDemod, AIS)")},
            {"name", strProp("Only items whose name contains this text, e.g. an ICAO address, MMSI or callsign")},
            {"limit", bounded(intProp("Maximum number of items to return. Default 200"), 1, 5000)},
            {"includeTrack", prop("boolean", "Include the position history of each item. Default false, as tracks can be long")}
        }),
        [this](const QJsonObject& args)
        {
            bool includeTrack = args["includeTrack"].toBool(false) || (args["includeTrack"].toString().toLower() == "true");
            return m_dataFeed.getMapItems(argString(args, "source", false), argString(args, "name", false),
                argInt(args, "limit", false, 200), includeTrack);
        });

    add("list_audio_devices",
        "List the audio input and output devices available to SDRangel.",
        schema(QJsonObject()),
        [this](const QJsonObject&)
        {
            SWGSDRangel::SWGAudioDevices response;
            SWGSDRangel::SWGErrorResponse error;
            error.init();
            check(m_adapter->instanceAudioGet(response, error), error, "List audio devices");
            return toJson(response);
        });

    add("get_location",
        "Get the station location (latitude and longitude in degrees) used by features such as the Map and satellite tracker.",
        schema(QJsonObject()),
        [this](const QJsonObject&)
        {
            SWGSDRangel::SWGLocationInformation response;
            SWGSDRangel::SWGErrorResponse error;
            error.init();
            check(m_adapter->instanceLocationGet(response, error), error, "Get location");
            return toJson(response);
        });

    add("set_location",
        "Set the station location.",
        schema({
            {"latitude", bounded(numProp("Latitude in degrees, positive north"), -90.0, 90.0)},
            {"longitude", bounded(numProp("Longitude in degrees, positive east"), -180.0, 180.0)}
        }, {"latitude", "longitude"}),
        [this](const QJsonObject& args)
        {
            SWGSDRangel::SWGLocationInformation response;
            response.setLatitude((float) argDouble(args, "latitude"));
            response.setLongitude((float) argDouble(args, "longitude"));
            SWGSDRangel::SWGErrorResponse error;
            error.init();
            check(m_adapter->instanceLocationPut(response, error), error, "Set location");
            return toJson(response);
        });
}

void MCPTools::registerDeviceSetTools()
{
    add("add_deviceset",
        "Create a new device set and optionally select its sampling device in one go. Returns the index of the new device set. "
        "Identify the device with hwType and/or serial (from list_available_devices), or deviceIndex (its index in that list). "
        "Without a device the new device set uses the first available device of that direction.",
        schema({
            {"direction", directionProp("rx for a receiver (default), tx for a transmitter, mimo for a MIMO device")},
            {"hwType", strProp("Hardware type of the device to select, e.g. RTLSDR, HackRF, TestSource")},
            {"serial", strProp("Serial number of the device to select")},
            {"sequence", intProp("Sequence number of the device, to distinguish identical devices")},
            {"displayedName", strProp("Displayed name of the device, as listed by list_available_devices")},
            {"deviceIndex", intProp("Index of the device in the list_available_devices list for this direction")}
        }),
        [this](const QJsonObject& args)
        {
            int direction = argDirection(args, "direction", false, 0);
            int deviceSetIndex = addDeviceSetAndWait(direction);

            if (hasArg(args, "hwType") || hasArg(args, "serial") || hasArg(args, "displayedName") || hasArg(args, "deviceIndex"))
            {
                try
                {
                    return selectDevice(deviceSetIndex, args);
                }
                catch (const MCPToolError& e)
                {
                    throw MCPToolError(QString("Device set %1 was created but the requested device could not be selected: %2. "
                        "Use set_device on that device set, or remove_last_deviceset. Do not call add_deviceset again.")
                        .arg(deviceSetIndex).arg(e.message));
                }
            }

            QJsonObject result;
            result["deviceSetIndex"] = deviceSetIndex;
            result["deviceSet"] = getDeviceSet(deviceSetIndex);
            return result;
        });

    add("remove_last_deviceset",
        "Remove the device set with the highest index. In the GUI the first device set (R0) cannot be removed.",
        schema(QJsonObject()),
        [this](const QJsonObject&)
        {
            int before = m_events.m_deviceSetRemoved.loadAcquire();
            SWGSDRangel::SWGSuccessResponse response;
            SWGSDRangel::SWGErrorResponse error;
            error.init();
            check(m_adapter->instanceDeviceSetDelete(response, error), error, "Remove device set");
            waitFor([&]() { return m_events.m_deviceSetRemoved.loadAcquire() > before; });
            QJsonObject result;
            result["deviceSetCount"] = deviceSetCount();
            return result;
        });

    add("get_deviceset",
        "Get a device set: its sampling device (hwType, serial, centre frequency, state) and the list of its channels with their reports.",
        schema({{"deviceSetIndex", deviceSetIndexProp}}, {"deviceSetIndex"}),
        [this](const QJsonObject& args) { return getDeviceSet(argInt(args, "deviceSetIndex")); });

    add("set_device",
        "Change the sampling device of an existing device set. Identify the device with hwType and/or serial (from list_available_devices), "
        "or deviceIndex. Waits for the change to complete.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"hwType", strProp("Hardware type of the device to select, e.g. RTLSDR")},
            {"serial", strProp("Serial number of the device to select")},
            {"sequence", intProp("Sequence number of the device, to distinguish identical devices")},
            {"displayedName", strProp("Displayed name of the device")},
            {"deviceIndex", intProp("Index of the device in the list_available_devices list for the device set direction")}
        }, {"deviceSetIndex"}),
        [this](const QJsonObject& args) { return selectDevice(argInt(args, "deviceSetIndex"), args); });

    add("get_device_settings",
        "Get the settings of the sampling device of a device set (centre frequency, sample rate, gain...). The keys depend on the device type.",
        schema({{"deviceSetIndex", deviceSetIndexProp}}, {"deviceSetIndex"}),
        [this](const QJsonObject& args) { return getDeviceSettings(argInt(args, "deviceSetIndex")); });

    add("set_device_settings",
        "Change some settings of the sampling device of a device set. Give only the keys to change, e.g. {\"centerFrequency\": 1090000000, \"gain\": 400}. "
        "Use describe_settings with the device hwType to learn the valid keys, units and value ranges. Returns the keys changed with their applied values.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"settings", objProp("Object with the settings keys to change and their new values")}
        }, {"deviceSetIndex", "settings"}),
        [this](const QJsonObject& args) { return patchDeviceSettings(argInt(args, "deviceSetIndex"), argObject(args, "settings")); });

    add("set_center_frequency",
        "Tune the sampling device of a device set to a centre frequency in Hz. Channels keep their inputFrequencyOffset relative to it.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"frequency", numProp("Centre frequency in Hz")}
        }, {"deviceSetIndex", "frequency"}),
        [this](const QJsonObject& args)
        {
            QJsonObject settings;
            settings["centerFrequency"] = (double) argInt64(args, "frequency");
            return patchDeviceSettings(argInt(args, "deviceSetIndex"), settings);
        });

    add("start_device",
        "Start the sampling device of a device set so that it streams samples and its channels run. Returns the resulting state.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"subsystemIndex", bounded(intProp("MIMO devices only: 0 for the rx subsystem, 1 for tx. Default 0"), 0, 1)}
        }, {"deviceSetIndex"}),
        [this](const QJsonObject& args)
        {
            int deviceSetIndex = argInt(args, "deviceSetIndex");
            QJsonObject state = deviceState(deviceSetIndex, argInt(args, "subsystemIndex", false, 0), 1);
            QStringList warnings = basebandWarnings(deviceSetIndex);

            if (!warnings.isEmpty()) {
                state["warning"] = warnings.join(" ");
            }

            return QJsonValue(state);
        });

    add("stop_device",
        "Stop the sampling device of a device set.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"subsystemIndex", bounded(intProp("MIMO devices only: 0 for the rx subsystem, 1 for tx. Default 0"), 0, 1)}
        }, {"deviceSetIndex"}),
        [this](const QJsonObject& args) { return deviceState(argInt(args, "deviceSetIndex"), argInt(args, "subsystemIndex", false, 0), 0); });

    add("get_device_report",
        "Get the live report of the sampling device of a device set (for example the actual sample rate, gain values, "
        "or for file inputs the playback position). Keys depend on the device type.",
        schema({{"deviceSetIndex", deviceSetIndexProp}}, {"deviceSetIndex"}),
        [this](const QJsonObject& args)
        {
            SWGSDRangel::SWGDeviceReport response;
            SWGSDRangel::SWGErrorResponse error;
            error.init();
            check(m_adapter->devicesetDeviceReportGet(argInt(args, "deviceSetIndex"), response, error), error, "Get device report");
            return toJson(response);
        });

    add("device_action",
        "Trigger an action on the sampling device of a device set. Only some device types have actions. Use describe_settings to find "
        "the actions keys of the device type; pass them as the actions object, e.g. {\"record\": 1}.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"actions", objProp("Object with the action keys and values for this device type")}
        }, {"deviceSetIndex", "actions"}),
        [this](const QJsonObject& args)
        {
            int deviceSetIndex = argInt(args, "deviceSetIndex");
            QJsonObject actions = argObject(args, "actions");
            QJsonObject settings = getDeviceSettings(deviceSetIndex);
            QString hwType = settings["deviceHwType"].toString();
            int direction = settings["direction"].toInt(0);
            const QMap<QString, QString>& map = direction == 0 ? WebAPIUtils::m_sourceDeviceHwIdToActionsKey
                : (direction == 1 ? WebAPIUtils::m_sinkDeviceHwIdToActionsKey : WebAPIUtils::m_mimoDeviceHwIdToActionsKey);

            if (!map.contains(hwType)) {
                throw MCPToolError(QString("Device type %1 has no actions").arg(hwType));
            }

            QJsonObject json;
            json["deviceHwType"] = hwType;
            json["direction"] = direction;
            json[map[hwType]] = actions.contains(map[hwType]) ? actions[map[hwType]].toObject() : actions;
            QStringList keys;
            extractKeys(json[map[hwType]].toObject(), keys);

            SWGSDRangel::SWGDeviceActions query;
            query.init();
            query.fromJsonObject(json);
            SWGSDRangel::SWGSuccessResponse response;
            SWGSDRangel::SWGErrorResponse error;
            error.init();
            check(m_adapter->devicesetDeviceActionsPost(deviceSetIndex, keys, query, response, error), error, "Device action");
            return toJson(response);
        });

    add("get_spectrum_settings",
        "Get the settings of the main spectrum display of a device set, including FFT and scale, 2D/3D spectrum display, waterfall history, measurements and statistics, math, and saved-memory display settings.",
        schema({{"deviceSetIndex", deviceSetIndexProp}}, {"deviceSetIndex"}),
        [this](const QJsonObject& args)
        {
            SWGSDRangel::SWGGLSpectrum response;
            SWGSDRangel::SWGErrorResponse error;
            error.init();
            check(m_adapter->devicesetSpectrumSettingsGet(argInt(args, "deviceSetIndex"), response, error), error, "Get spectrum settings");
            return toJson(response);
        });

    add("set_spectrum_settings",
        "Change selected settings of the main spectrum display of a device set. Supports FFT and scale, 2D/3D spectrum display, waterfall history, measurements and statistics, math, and saved-memory display settings. Give only the keys to change (see get_spectrum_settings for the keys). "
        "Keys whose values are codes are documented by describe_settings with type spectrum: for instance spectrumStyle is 0 line, 1 filled solid colour, 2 filled colour gradient, and colorMap names the gradient. "
        "To smooth the trace set both averagingMode (1 moving) and averagingValue (the number of FFTs, e.g. 10): a mode with the value left at 1 averages nothing.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"settings", objProp("Object with the spectrum settings keys to change")}
        }, {"deviceSetIndex", "settings"}),
        [this](const QJsonObject& args)
        {
            int deviceSetIndex = argInt(args, "deviceSetIndex");
            QJsonObject partial = argObject(args, "settings");
            QStringList unknownKeys;

            auto read = [&]()
            {
                SWGSDRangel::SWGGLSpectrum current;
                SWGSDRangel::SWGErrorResponse error;
                error.init();
                check(m_adapter->devicesetSpectrumSettingsGet(deviceSetIndex, current, error), error, "Get spectrum settings");
                return toJson(current);
            };

            const QJsonObject before = read();
            patchSpectrumSettings(deviceSetIndex, partial, &unknownKeys);

            // Read back rather than echo the request: some values are rounded to what the
            // display offers, and the caller should see what it got. The patch is applied by
            // message, so give it a moment to land: until every key given has either moved or
            // was already where it was asked to be
            QJsonObject after = read();

            for (int attempt = 0; attempt < 10; attempt++)
            {
                bool landed = true;

                for (const QString& key : partial.keys())
                {
                    if (after.contains(key) && (after[key] == before[key]) && (after[key] != partial[key])) {
                        landed = false;
                    }
                }

                if (landed) {
                    break;
                }

                QThread::msleep(50);
                after = read();
            }

            QJsonObject changed;
            QStringList notes;

            for (const QString& key : partial.keys())
            {
                if (after.contains(key))
                {
                    changed[key] = after[key];

                    if (after[key].isDouble() && partial[key].isDouble() && (after[key].toDouble() != partial[key].toDouble())) {
                        notes.append(QString("%1 was rounded to %2, one of the values the display offers.").arg(key).arg(after[key].toDouble()));
                    }
                }
            }

            // A mode with a count of one averages nothing, which is easy to leave that way
            if ((after["averagingMode"].toInt() != 0) && (after["averagingValue"].toInt() <= 1)) {
                notes.append("averagingMode is set but averagingValue is 1, so no averaging is happening: set averagingValue as well, "
                             "10 to smooth a trace or 50 to measure a floor. The values available are 1, 2, 5, 10, 20, 50, 100 and so on.");
            }

            QJsonObject result;
            result["changed"] = changed;

            if (!unknownKeys.isEmpty()) {
                result["warning"] = QString("Ignored unknown keys: %1. describe_settings lists the valid ones.").arg(unknownKeys.join(", "));
            }

            if (!notes.isEmpty()) {
                result["note"] = notes.join(" ");
            }

            return result;
        });


    add("get_spectrum_report",
        "Get the latest spectrum measurement results of a device set: peaks, channel power, adjacent channel power, occupied "
        "bandwidth, 3 dB bandwidth, or SNR with THD and SINAD, depending on which measurement is selected with "
        "set_spectrum_settings. Only the fields the selected measurement produces are returned, so an absent field was not "
        "measured rather than measured as zero. With no measurement selected only measurement 0 comes back. The figures are "
        "those of the spectrum as displayed, so they follow the zoom.",
        schema({{"deviceSetIndex", deviceSetIndexProp}}, {"deviceSetIndex"}),
        [this](const QJsonObject& args)
        {
            SWGSDRangel::SWGGLSpectrumReport response;
            SWGSDRangel::SWGErrorResponse error;
            response.init();
            error.init();
            check(m_adapter->devicesetSpectrumReportGet(argInt(args, "deviceSetIndex"), response, error), error, "Get spectrum report");
            return toJson(response);
        });


    add("get_spectrum_data",
        "Get the current power spectrum of a device set, reduced to the number of bins asked for. Use it to see what is on a band "
        "without looking at the screen: which parts are occupied, how strong, and where the edges are. Each value covers "
        "binBandwidth Hz starting at startFrequency. Reduction is by max, which keeps a narrow carrier that averaging would bury "
        "in its neighbours; mean is better for measuring a noise floor. Every value costs tokens, so ask for the coarsest that "
        "answers the question and narrow the frequency range instead of asking for more bins.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"bins", bounded(intProp("Number of values to return, most recent spectrum. Default 64"), 1, 512)},
            {"startFrequency", numProp("Only from this frequency in Hz. Default the start of the spectrum")},
            {"stopFrequency", numProp("Only up to this frequency in Hz. Default the end of the spectrum")},
            {"reduce", strProp("max to keep peaks, the default, or mean to average")}
        }, {"deviceSetIndex"}),
        [this](const QJsonObject& args)
        {
            SWGSDRangel::SWGGLSpectrumData response;
            SWGSDRangel::SWGErrorResponse error;
            response.init();
            error.init();
            check(m_adapter->devicesetSpectrumDataGet(
                    argInt(args, "deviceSetIndex"),
                    argInt(args, "bins", false, 64),
                    argInt64(args, "startFrequency", false, 0),
                    argInt64(args, "stopFrequency", false, 0),
                    argString(args, "reduce", false, "max"),
                    response, error),
                error, "Get spectrum data");
            QJsonObject json = toJson(response);

            // The generated serialiser drops a list of plain floats, so the values go in by hand
            QJsonArray power;

            if (response.getPower())
            {
                for (float v : *response.getPower()) {
                    power.append(std::round((double) v * 10.0) / 10.0);
                }
            }

            json["power"] = power;
            return json;
        });

    add("spectrum_action",
        "Act on the main spectrum of a device set. autoscale sets the reference level and range from what is on screen, "
        "clearSpectrum discards the histogram and max hold traces, resetMeasurements starts the measurement statistics again, "
        "freeze stops spectrum processing and unfreezing resumes it, and gotoMarker retunes to one of the annotations. "
        "Everything except freeze acts on the display and so needs the GUI.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"autoscale", prop("boolean", "Set the reference level and range from the spectrum currently displayed")},
            {"clearSpectrum", prop("boolean", "Discard the accumulated histogram and max hold traces")},
            {"resetMeasurements", prop("boolean", "Discard the accumulated measurement statistics")},
            {"freeze", prop("boolean", "True to stop spectrum processing, false to resume it")},
            {"gotoMarker", intProp("Retune to the annotation at this index, counting from 0")}
        }, {"deviceSetIndex"}),
        [this](const QJsonObject& args)
        {
            int deviceSetIndex = argInt(args, "deviceSetIndex");
            QJsonObject actions;

            for (const QString& key : {QString("autoscale"), QString("clearSpectrum"), QString("resetMeasurements"), QString("freeze")})
            {
                if (hasArg(args, key)) {
                    actions[key] = args.value(key).toBool() ? 1 : 0;
                }
            }

            if (hasArg(args, "gotoMarker")) {
                actions["gotoMarker"] = argInt(args, "gotoMarker");
            }

            if (actions.isEmpty()) {
                throw MCPToolError("Give at least one of autoscale, clearSpectrum, resetMeasurements, freeze or gotoMarker");
            }

            QStringList keys;
            extractKeys(actions, keys);

            // fromJsonObject takes its argument by non-const reference and looks every field up
            // with operator[], which inserts a null for each one that is absent, so what is echoed
            // back is taken before the call rather than after it
            QJsonObject applied = actions;
            SWGSDRangel::SWGSpectrumActions query;
            query.init();
            query.fromJsonObject(actions);
            SWGSDRangel::SWGErrorResponse error;
            error.init();
            check(m_adapter->devicesetSpectrumActionsPost(deviceSetIndex, keys, query, error), error, "Spectrum action");

            QJsonObject result;
            result["deviceSetIndex"] = deviceSetIndex;
            result["applied"] = applied;
            return result;
        });

    add("add_annotation",
        "Add a labelled annotation to the main spectrum of a device set, for instance the name of the station a channel is tuned to. "
        "Appends to the annotations already there instead of replacing them, and turns annotation display on if it is off. "
        "Give centerFrequency, which is usually what you want as the bandwidth is then centred on it, or startFrequency.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"text", strProp("Label shown against the annotation, e.g. the RDS programme service name of a broadcast station")},
            {"centerFrequency", numProp("Centre frequency of the annotation in Hz. Give this or startFrequency")},
            {"startFrequency", numProp("Start frequency of the annotation in Hz. Give this or centerFrequency")},
            {"bandwidth", intProp("Width of the annotation in Hz. Default 0, which marks a single frequency")},
            {"color", intProp("Colour packed as blue*65536 + green*256 + red, each component 0 to 255. Default yellow")},
            {"show", bounded(intProp("0 hidden, 1 top marker only, 2 full with text and limits, 3 text only. Default 2"), 0, 3)}
        }, {"deviceSetIndex", "text"}),
        [this](const QJsonObject& args)
        {
            int deviceSetIndex = argInt(args, "deviceSetIndex");
            int bandwidth = argInt(args, "bandwidth", false, 0);

            if (bandwidth < 0) {
                throw MCPToolError("bandwidth cannot be negative");
            }

            qint64 startFrequency;

            if (hasArg(args, "startFrequency")) {
                startFrequency = argInt64(args, "startFrequency");
            } else if (hasArg(args, "centerFrequency")) {
                startFrequency = argInt64(args, "centerFrequency") - bandwidth / 2;
            } else {
                throw MCPToolError("Give either centerFrequency or startFrequency");
            }

            QJsonObject marker;
            marker["startFrequency"] = startFrequency;
            marker["bandwidth"] = bandwidth;
            marker["markerColor"] = argInt(args, "color", false, 65535); // Yellow
            marker["show"] = argInt(args, "show", false, 2);
            marker["text"] = argString(args, "text");

            QJsonObject current = getSpectrumSettings(deviceSetIndex);

            // Appended by the settings themselves rather than by reading the list, adding to it and
            // writing it back, which would lose an annotation added by anything else in between
            QJsonArray markers;
            markers.append(marker);

            QJsonObject patch;
            patch["annotationMarkers"] = markers;
            patch["annotationMarkersMode"] = 1;
            // Annotations are drawn only when their bit of markersDisplay is set, so an annotation
            // added while it is clear would otherwise be invisible with nothing to say why
            patch["markersDisplay"] = current.value("markersDisplay").toInt(0) | 0x2;

            QJsonObject after = patchSpectrumSettings(deviceSetIndex, patch);
            QJsonObject result;
            result["added"] = marker;
            result["annotationMarkers"] = after.value("annotationMarkers").toArray();
            return result;
        });

    add("remove_annotation",
        "Remove annotations from the main spectrum of a device set. Give text to remove the annotations carrying that label, "
        "frequency to remove those whose span covers it, or all to remove every annotation. Returns what was removed and what is left.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"text", strProp("Remove annotations whose label is exactly this")},
            {"frequency", numProp("Remove annotations whose span covers this frequency in Hz")},
            {"all", prop("boolean", "Remove every annotation. Default false")}
        }, {"deviceSetIndex"}),
        [this](const QJsonObject& args)
        {
            int deviceSetIndex = argInt(args, "deviceSetIndex");
            bool all = args.value("all").toBool(false);
            bool byText = hasArg(args, "text");
            bool byFrequency = hasArg(args, "frequency");

            // Without a criterion the intent is ambiguous, and either reading of it, removing
            // nothing or removing everything, would be wrong often enough to be worth refusing
            if (!all && !byText && !byFrequency) {
                throw MCPToolError("Give text, frequency or all to say which annotations to remove");
            }

            QString text = argString(args, "text", false);
            qint64 frequency = argInt64(args, "frequency", false);
            QJsonArray markers = getSpectrumSettings(deviceSetIndex).value("annotationMarkers").toArray();
            QJsonArray kept;
            QJsonArray removed;

            for (const QJsonValue& value : markers)
            {
                QJsonObject marker = value.toObject();
                qint64 start = (qint64) marker.value("startFrequency").toDouble();
                qint64 stop = start + (qint64) marker.value("bandwidth").toDouble();
                bool matches = all
                    || (byText && (marker.value("text").toString() == text))
                    || (byFrequency && (frequency >= start) && (frequency <= stop));

                if (matches) {
                    removed.append(marker);
                } else {
                    kept.append(marker);
                }
            }

            QJsonObject result;
            result["removed"] = removed;

            if (removed.isEmpty())
            {
                result["annotationMarkers"] = markers;
                result["warning"] = "No annotation matched, so nothing was removed";
                return result;
            }

            QJsonObject patch;
            patch["annotationMarkers"] = kept;
            QJsonObject after = patchSpectrumSettings(deviceSetIndex, patch);
            result["annotationMarkers"] = after.value("annotationMarkers").toArray();
            return result;
        });
}

void MCPTools::registerChannelTools()
{
    add("add_channel",
        "Add a channel (demodulator or modulator) to a device set and optionally apply initial settings. Returns the index of the new channel. "
        "channelType is an id from list_channel_types, e.g. ADSBDemod, NFMDemod, AMDemod, BFMDemod, SSBDemod, AISDemod, PacketDemod, FileSink.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"channelType", strProp("Channel type id from list_channel_types")},
            {"settings", objProp("Optional initial settings for the channel, e.g. {\"inputFrequencyOffset\": 0}. Use describe_settings for the keys")}
        }, {"deviceSetIndex", "channelType"}),
        [this](const QJsonObject& args)
        {
            int deviceSetIndex = argInt(args, "deviceSetIndex");
            QString channelType = argString(args, "channelType");
            int channelIndex = addChannelAndWait(deviceSetIndex, channelType);
            QJsonObject result;
            result["deviceSetIndex"] = deviceSetIndex;
            result["channelIndex"] = channelIndex;
            QString warning = basebandWarning(deviceSetIndex, channelType);

            if (!warning.isEmpty()) {
                result["warning"] = warning;
            }
            QJsonObject settings = argObject(args, "settings", false);

            if (!settings.isEmpty())
            {
                QJsonObject patched;

                try
                {
                    patched = patchChannelSettings(deviceSetIndex, channelIndex, settings);
                }
                catch (const MCPToolError& e)
                {
                    // The channel exists even though its settings were refused. Say so and give
                    // its index, otherwise a retry would leave a second one behind.
                    throw MCPToolError(QString("The %1 channel was created as channel %2 of device set %3, but its initial "
                        "settings were refused, so it is left with defaults: %4. Use set_channel_settings to correct it, "
                        "or delete_channel to remove it. Do not call add_channel again.")
                        .arg(channelType).arg(channelIndex).arg(deviceSetIndex).arg(e.message));
                }

                result["changed"] = patched["changed"];

                if (patched.contains("warning")) {
                    result["warning"] = patched["warning"];
                }
            }

            return result;
        });

    add("delete_channel",
        "Remove a channel from a device set. Channels with a higher index are renumbered down by one.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"channelIndex", channelIndexProp}
        }, {"deviceSetIndex", "channelIndex"}),
        [this](const QJsonObject& args)
        {
            int deviceSetIndex = argInt(args, "deviceSetIndex");

            // Waits for the channel itself to disappear. Waiting for the removed signal would
            // sit out the whole timeout for the last channel of a device set, which the device
            // set drops from its list before MainCore can look it up to signal it.
            deleteChannelAndWait(deviceSetIndex, argInt(args, "channelIndex"));
            QJsonObject result;
            result["deviceSetIndex"] = deviceSetIndex;
            result["channelCount"] = channelCount(deviceSetIndex);
            return result;
        });

    add("get_channel_settings",
        "Get the settings of a channel. The keys depend on the channel type. inputFrequencyOffset is the channel frequency relative to the device centre frequency in Hz.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"channelIndex", channelIndexProp}
        }, {"deviceSetIndex", "channelIndex"}),
        [this](const QJsonObject& args) { return getChannelSettings(argInt(args, "deviceSetIndex"), argInt(args, "channelIndex")); });

    add("set_channel_settings",
        "Change some settings of a channel. Give only the keys to change, e.g. {\"inputFrequencyOffset\": 25000, \"squelch\": -40}. "
        "Use describe_settings with the channel type to learn the valid keys. Returns the keys changed with their applied values.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"channelIndex", channelIndexProp},
            {"settings", objProp("Object with the settings keys to change and their new values")}
        }, {"deviceSetIndex", "channelIndex", "settings"}),
        [this](const QJsonObject& args)
        {
            return patchChannelSettings(argInt(args, "deviceSetIndex"), argInt(args, "channelIndex"), argObject(args, "settings"));
        });

    add("get_channel_report",
        "Get the live report of a channel: signal level, squelch state, decoder statistics and so on, depending on the channel type. "
        "Omit channelIndex for the reports of every channel of the device set. "
        "For DAB, wait briefly after selecting a programme or changing receiver gain and then read the report again. Do not accept "
        "sync or audioActive alone as proof of clean reception: if synchronized audio has SNR at or below 0 dB, RTL-SDR overload "
        "is likely, so reduce gain through its supported values and keep the setting that gives the best positive SNR.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"channelIndex", intProp("Index of the channel within the device set; omit for all channels")}
        }, {"deviceSetIndex"}),
        [this](const QJsonObject& args)
        {
            int deviceSetIndex = argInt(args, "deviceSetIndex");
            SWGSDRangel::SWGErrorResponse error;
            error.init();

            if (hasArg(args, "channelIndex"))
            {
                int channelIndex = argInt(args, "channelIndex");
                SWGSDRangel::SWGChannelReport response;
                check(m_adapter->devicesetChannelReportGet(deviceSetIndex, channelIndex, response, error), error, "Get channel report");
                QJsonObject report = toJson(response);
                addRdsHint(report, report.value("channelType").toString(), deviceSetIndex, channelIndex);
                return report;
            }

            SWGSDRangel::SWGChannelsDetail response;
            check(m_adapter->devicesetChannelsReportGet(deviceSetIndex, response, error), error, "Get channels report");
            QJsonObject json = toJson(response);
            QJsonArray channels = json.value("channels").toArray();

            for (int i = 0; i < channels.size(); i++)
            {
                QJsonObject channel = channels.at(i).toObject();
                QJsonObject report = channel.value("report").toObject();
                // The channel type is on the entry here rather than on the report it carries
                addRdsHint(report, channel.value("id").toString(), deviceSetIndex, channel.value("index").toInt());
                channel["report"] = report;
                channels[i] = channel;
            }

            json["channels"] = channels;
            return json;
        });

    add("channel_action",
        "Trigger an action on a channel. Only some channel types have actions. Use describe_settings to find the actions keys of the "
        "channel type; pass them as the actions object.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"channelIndex", channelIndexProp},
            {"actions", objProp("Object with the action keys and values for this channel type")}
        }, {"deviceSetIndex", "channelIndex", "actions"}),
        [this](const QJsonObject& args)
        {
            int deviceSetIndex = argInt(args, "deviceSetIndex");
            int channelIndex = argInt(args, "channelIndex");
            QJsonObject actions = argObject(args, "actions");
            QJsonObject settings = getChannelSettings(deviceSetIndex, channelIndex);
            QString channelType = settings["channelType"].toString();

            if (!WebAPIUtils::m_channelTypeToActionsKey.contains(channelType)) {
                throw MCPToolError(QString("Channel type %1 has no actions").arg(channelType));
            }

            QString actionsKey = WebAPIUtils::m_channelTypeToActionsKey[channelType];
            QJsonObject json;
            json["channelType"] = channelType;
            json["direction"] = settings["direction"].toInt(0);
            json[actionsKey] = actions.contains(actionsKey) ? actions[actionsKey].toObject() : actions;
            QStringList keys;
            extractKeys(json[actionsKey].toObject(), keys);

            SWGSDRangel::SWGChannelActions query;
            query.init();
            query.fromJsonObject(json);
            SWGSDRangel::SWGSuccessResponse response;
            SWGSDRangel::SWGErrorResponse error;
            error.init();
            check(m_adapter->devicesetChannelActionsPost(deviceSetIndex, channelIndex, keys, query, response, error), error, "Channel action");
            return toJson(response);
        });
}

void MCPTools::registerFeatureTools()
{
    add("add_feature",
        "Add a feature (device independent plugin such as Map, AIS, APRS, SatelliteTracker) and optionally apply initial settings. "
        "Returns the index of the new feature. featureType is an id from list_feature_types.",
        schema({
            {"featureType", strProp("Feature type id from list_feature_types")},
            {"settings", objProp("Optional initial settings for the feature. Use describe_settings for the keys")}
        }, {"featureType"}),
        [this](const QJsonObject& args)
        {
            QString featureType = argString(args, "featureType");
            int featureIndex = addFeatureAndWait(featureType);
            QJsonObject result;
            result["featureIndex"] = featureIndex;
            QJsonObject settings = argObject(args, "settings", false);

            if (!settings.isEmpty())
            {
                QJsonObject patched;

                try
                {
                    patched = patchFeatureSettings(featureIndex, settings);
                }
                catch (const MCPToolError& e)
                {
                    throw MCPToolError(QString("The %1 feature was created as feature %2, but its initial settings were "
                        "refused, so it is left with defaults: %3. Use set_feature_settings to correct it, or "
                        "delete_feature to remove it. Do not call add_feature again.")
                        .arg(featureType).arg(featureIndex).arg(e.message));
                }

                result["changed"] = patched["changed"];

                if (patched.contains("warning")) {
                    result["warning"] = patched["warning"];
                }
            }

            return result;
        });

    add("delete_feature",
        "Remove a feature. Features with a higher index are renumbered down by one.",
        schema({{"featureIndex", featureIndexProp}}, {"featureIndex"}),
        [this](const QJsonObject& args)
        {
            int featureIndex = argInt(args, "featureIndex");
            requireNotSelf(featureIndex, "deleted");
            const void *target = featureAt(featureIndex);
            SWGSDRangel::SWGSuccessResponse response;
            SWGSDRangel::SWGErrorResponse error;
            error.init();
            check(m_adapter->featuresetFeatureDelete(0, featureIndex, response, error), error, "Delete feature");

            // FeatureSet drops the feature from its own list before MainCore looks it up, so
            // removing the last feature emits no featureRemoved. Watch the feature itself go.
            if (!waitFor([&]() { return !featurePointers().contains(target); })) {
                throw MCPToolError(QString("Timed out waiting for feature %1 to be removed").arg(featureIndex));
            }

            QJsonObject result;
            result["featureCount"] = featureCount();
            return result;
        });

    add("get_feature_settings",
        "Get the settings of a feature. The keys depend on the feature type.",
        schema({{"featureIndex", featureIndexProp}}, {"featureIndex"}),
        [this](const QJsonObject& args) { return getFeatureSettings(argInt(args, "featureIndex")); });

    add("set_feature_settings",
        "Change some settings of a feature. Give only the keys to change. Use describe_settings with the feature type to learn the valid keys.",
        schema({
            {"featureIndex", featureIndexProp},
            {"settings", objProp("Object with the settings keys to change and their new values")}
        }, {"featureIndex", "settings"}),
        [this](const QJsonObject& args) { return patchFeatureSettings(argInt(args, "featureIndex"), argObject(args, "settings")); });

    add("get_feature_report",
        "Get the live report of a feature, depending on the feature type.",
        schema({{"featureIndex", featureIndexProp}}, {"featureIndex"}),
        [this](const QJsonObject& args)
        {
            SWGSDRangel::SWGFeatureReport response;
            SWGSDRangel::SWGErrorResponse error;
            error.init();
            check(m_adapter->featuresetFeatureReportGet(0, argInt(args, "featureIndex"), response, error), error, "Get feature report");
            return toJson(response);
        });

    add("start_feature",
        "Start a feature. Most features must be started to do anything, but the passive ones - AIS, AMBE, AntennaTools, "
        "APRS, FreqDisplay, LimeRFE, Map, Radiosonde, RemoteControl and SkyMap - have no run state, work as soon as they "
        "are configured and always report idle. Starting one of those is answered without doing anything.",
        schema({{"featureIndex", featureIndexProp}}, {"featureIndex"}),
        [this](const QJsonObject& args) { return featureState(argInt(args, "featureIndex"), 1); });

    add("stop_feature",
        "Stop a feature.",
        schema({{"featureIndex", featureIndexProp}}, {"featureIndex"}),
        [this](const QJsonObject& args)
        {
            int featureIndex = argInt(args, "featureIndex");
            requireNotSelf(featureIndex, "stopped");
            return featureState(featureIndex, 0);
        });

    add("feature_action",
        "Trigger an action on a feature. Use describe_settings to find the actions keys of the feature type; pass them as the actions object.",
        schema({
            {"featureIndex", featureIndexProp},
            {"actions", objProp("Object with the action keys and values for this feature type")}
        }, {"featureIndex", "actions"}),
        [this](const QJsonObject& args)
        {
            int featureIndex = argInt(args, "featureIndex");
            QJsonObject actions = argObject(args, "actions");
            QJsonObject settings = getFeatureSettings(featureIndex);
            QString featureType = settings["featureType"].toString();

            if (!WebAPIUtils::m_featureTypeToActionsKey.contains(featureType)) {
                throw MCPToolError(QString("Feature type %1 has no actions").arg(featureType));
            }

            QString actionsKey = WebAPIUtils::m_featureTypeToActionsKey[featureType];
            QJsonObject json;
            json["featureType"] = featureType;
            json[actionsKey] = actions.contains(actionsKey) ? actions[actionsKey].toObject() : actions;
            QStringList keys;
            extractKeys(json[actionsKey].toObject(), keys);

            SWGSDRangel::SWGFeatureActions query;
            query.init();
            query.fromJsonObject(json);
            SWGSDRangel::SWGSuccessResponse response;
            SWGSDRangel::SWGErrorResponse error;
            error.init();
            check(m_adapter->featuresetFeatureActionsPost(0, featureIndex, keys, query, response, error), error, "Feature action");
            return toJson(response);
        });
}

void MCPTools::registerPresetTools()
{
    add("list_presets",
        "List the saved device set presets. A preset stores a device set's device and channel settings and is identified by group name, "
        "name, centre frequency and type (R for rx, T for tx, M for MIMO).",
        schema(QJsonObject()),
        [this](const QJsonObject&) { return getPresets(); });

    add("load_preset",
        "Load a preset into a device set, replacing its device settings and channels. The preset type must match the device set direction.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"groupName", strProp("Preset group name")},
            {"name", strProp("Preset name")},
            {"centerFrequency", numProp("Preset centre frequency in Hz. Only needed when several presets share the group and name")},
            {"type", strProp("Preset type R, T or M. Only needed when several presets share the group and name")}
        }, {"deviceSetIndex", "groupName", "name"}),
        [this](const QJsonObject& args)
        {
            int deviceSetIndex = argInt(args, "deviceSetIndex");
            QJsonObject identifier = presetIdentifier(args, deviceSetIndex, false);
            SWGSDRangel::SWGPresetTransfer query;
            query.setDeviceSetIndex(deviceSetIndex);
            SWGSDRangel::SWGPresetIdentifier *preset = new SWGSDRangel::SWGPresetIdentifier();
            preset->setGroupName(new QString(identifier["groupName"].toString()));
            preset->setName(new QString(identifier["name"].toString()));
            preset->setType(new QString(identifier["type"].toString()));
            preset->setCenterFrequency((qint64) identifier["centerFrequency"].toDouble());
            query.setPreset(preset);
            SWGSDRangel::SWGPresetIdentifier response;
            response.init();
            SWGSDRangel::SWGErrorResponse error;
            error.init();
            check(m_adapter->instancePresetPatch(query, response, error), error, "Load preset");
            QThread::msleep(500); // Loading replaces the channels; give it a moment before the caller looks
            QJsonObject result;
            result["loaded"] = toJson(response);
            result["deviceSet"] = getDeviceSet(deviceSetIndex);
            return result;
        });

    add("save_preset",
        "Save the current device settings and channels of a device set as a preset. Updates the preset if one with the same group, name, "
        "centre frequency and type exists, otherwise creates it.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"groupName", strProp("Preset group name")},
            {"name", strProp("Preset name")}
        }, {"deviceSetIndex", "groupName", "name"}),
        [this](const QJsonObject& args)
        {
            int deviceSetIndex = argInt(args, "deviceSetIndex");
            QJsonObject identifier = presetIdentifier(args, deviceSetIndex, true);
            SWGSDRangel::SWGPresetTransfer query;
            query.setDeviceSetIndex(deviceSetIndex);
            SWGSDRangel::SWGPresetIdentifier *preset = new SWGSDRangel::SWGPresetIdentifier();
            preset->setGroupName(new QString(identifier["groupName"].toString()));
            preset->setName(new QString(identifier["name"].toString()));
            preset->setType(new QString(identifier["type"].toString()));
            preset->setCenterFrequency((qint64) identifier["centerFrequency"].toDouble());
            query.setPreset(preset);
            SWGSDRangel::SWGPresetIdentifier response;
            response.init();
            SWGSDRangel::SWGErrorResponse error;
            error.init();
            bool exists = identifier["exists"].toBool();

            if (exists) {
                check(m_adapter->instancePresetPut(query, response, error), error, "Update preset");
            } else {
                check(m_adapter->instancePresetPost(query, response, error), error, "Create preset");
            }

            QJsonObject result;
            result["preset"] = toJson(response);
            result["created"] = !exists;
            return result;
        });

    add("list_configurations",
        "List the saved configurations. A configuration stores the whole instance: all device sets, features and workspaces.",
        schema(QJsonObject()),
        [this](const QJsonObject&) { return getConfigurations(); });

    add("load_configuration",
        "Load a saved configuration, replacing all current device sets and features.",
        schema({
            {"groupName", strProp("Configuration group name")},
            {"name", strProp("Configuration name")}
        }, {"groupName", "name"}),
        [this](const QJsonObject& args)
        {
            SWGSDRangel::SWGConfigurationIdentifier response;
            response.setGroupName(new QString(argString(args, "groupName")));
            response.setName(new QString(argString(args, "name")));
            SWGSDRangel::SWGErrorResponse error;
            error.init();
            check(m_adapter->instanceConfigurationPatch(response, error), error, "Load configuration");
            QThread::msleep(1000); // Loading rebuilds everything
            QJsonObject result;
            result["configuration"] = toJson(response);
            result["instance"] = getInstanceSummary();
            return result;
        });

    add("save_configuration",
        "Save the whole current instance as a configuration. Updates the configuration if one with the same group and name exists, otherwise creates it.",
        schema({
            {"groupName", strProp("Configuration group name")},
            {"name", strProp("Configuration name")}
        }, {"groupName", "name"}),
        [this](const QJsonObject& args)
        {
            QString groupName = argString(args, "groupName");
            QString name = argString(args, "name");
            bool exists = false;

            for (const auto& groupValue : getConfigurations()["groups"].toArray())
            {
                QJsonObject group = groupValue.toObject();

                if (group["groupName"].toString() != groupName) {
                    continue;
                }

                for (const auto& item : group["configurations"].toArray())
                {
                    if (item.toObject()["name"].toString() == name) {
                        exists = true;
                    }
                }
            }

            SWGSDRangel::SWGConfigurationIdentifier response;
            response.setGroupName(new QString(groupName));
            response.setName(new QString(name));
            SWGSDRangel::SWGErrorResponse error;
            error.init();

            if (exists) {
                check(m_adapter->instanceConfigurationPut(response, error), error, "Update configuration");
            } else {
                check(m_adapter->instanceConfigurationPost(response, error), error, "Create configuration");
            }

            QJsonObject result;
            result["configuration"] = toJson(response);
            result["created"] = !exists;
            return result;
        });
}

void MCPTools::registerWorkspaceTools()
{
    add("add_workspace",
        "GUI only: add a new workspace (a tabbed area holding device, channel and feature windows). Workspaces are numbered from 0.",
        schema(QJsonObject()),
        [this](const QJsonObject&)
        {
            SWGSDRangel::SWGSuccessResponse response;
            SWGSDRangel::SWGErrorResponse error;
            error.init();
            check(m_adapter->instanceWorkspacePost(response, error), error, "Add workspace");
            return toJson(response);
        });

    add("delete_empty_workspaces",
        "GUI only: remove all workspaces that contain no windows.",
        schema(QJsonObject()),
        [this](const QJsonObject&)
        {
            SWGSDRangel::SWGSuccessResponse response;
            SWGSDRangel::SWGErrorResponse error;
            error.init();
            check(m_adapter->instanceWorkspaceDelete(response, error), error, "Delete empty workspaces");
            return toJson(response);
        });

    add("arrange_workspace",
        "GUI only: arrange the windows of a workspace, as its title bar buttons do. cascade, tile, stackVertical and stack "
        "rearrange them once and turn auto stacking and tabs off; autostack stacks them now and again whenever a window is "
        "added or resized, until another arrangement is asked for; tab shows one window at a time with a tab for each. "
        "stack is the usual choice: devices, spectra and channels in columns.",
        schema({
            {"workspaceIndex", intProp("Index of the workspace, from 0")},
            {"arrange", enumProp("cascade, tile, stackVertical, stack, autostack or tab", {"cascade", "tile", "stackVertical", "stack", "autostack", "tab"})}
        }, {"workspaceIndex", "arrange"}),
        [this](const QJsonObject& args)
        {
            SWGSDRangel::SWGWorkspaceActions query;
            query.init();
            query.setArrange(new QString(argString(args, "arrange")));
            SWGSDRangel::SWGSuccessResponse response;
            SWGSDRangel::SWGErrorResponse error;
            error.init();
            check(m_adapter->workspaceActionsPost(argInt(args, "workspaceIndex"), query, response, error), error, "Arrange workspace");
            return toJson(response);
        });

    add("set_workspace",
        "GUI only: move a window to a workspace. kind is device, spectrum or channel (with deviceSetIndex, plus channelIndex for a "
        "channel) or feature (with featureIndex).",
        schema({
            {"kind", strProp("device, spectrum, channel or feature")},
            {"workspaceIndex", intProp("Target workspace index")},
            {"deviceSetIndex", intProp("For device, spectrum and channel")},
            {"channelIndex", intProp("For channel")},
            {"featureIndex", intProp("For feature")}
        }, {"kind", "workspaceIndex"}),
        [this](const QJsonObject& args)
        {
            QString kind = argString(args, "kind").trimmed().toLower();
            SWGSDRangel::SWGWorkspaceInfo query;
            query.setIndex(argInt(args, "workspaceIndex"));
            SWGSDRangel::SWGSuccessResponse response;
            SWGSDRangel::SWGErrorResponse error;
            error.init();

            if (kind == "device") {
                check(m_adapter->devicesetDeviceWorkspacePut(argInt(args, "deviceSetIndex"), query, response, error), error, "Set device workspace");
            } else if (kind == "spectrum") {
                check(m_adapter->devicesetSpectrumWorkspacePut(argInt(args, "deviceSetIndex"), query, response, error), error, "Set spectrum workspace");
            } else if (kind == "channel") {
                check(m_adapter->devicesetChannelWorkspacePut(argInt(args, "deviceSetIndex"), argInt(args, "channelIndex"), query, response, error), error, "Set channel workspace");
            } else if (kind == "feature") {
                check(m_adapter->featuresetFeatureWorkspacePut(argInt(args, "featureIndex"), query, response, error), error, "Set feature workspace");
            } else {
                throw MCPToolError("kind must be device, spectrum, channel or feature");
            }

            return toJson(response);
        });
}

void MCPTools::registerCaptureTools()
{

    add("get_server_status",
        "This server's own state, in one call: the capture directory and any recordings running; which channels and features "
        "feed packets and map items and how much has arrived; and the client event streams (open streams, subscriptions, "
        "notifications sent). Open a stream with an HTTP GET on the MCP endpoint with Accept: text/event-stream, then "
        "resources/subscribe.",
        schema(QJsonObject()),
        [this](const QJsonObject&)
        {
            QJsonObject result;
            result["capture"] = m_capture.status();
            result["dataFeed"] = m_dataFeed.getStatus();
            result["streams"] = m_streams ? m_streams->status() : QJsonObject();
            return result;
        });

    add("capture_audio",
        QString("Record the demodulated audio of a channel to a WAV file and report where it went, how loud it was and whether it "
                "was silent. Works with demodulators that feed the Demod Analyzer, such as NFMDemod, AMDemod, SSBDemod, WFMDemod, "
                "BFMDemod, DSDDemod and M17Demod. The device must be running. This call blocks for the duration, so it is limited "
                "to %1 seconds. Set inline to true (up to %2 seconds) to also get the WAV back as base64 for playing directly.")
            .arg(MCPCapture::m_maxBlockingSeconds).arg(MCPCapture::m_maxInlineSeconds),
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"channelIndex", channelIndexProp},
            {"seconds", bounded(numProp(QString("How long to record for, up to %1").arg(MCPCapture::m_maxBlockingSeconds)), 0.1, MCPCapture::m_maxBlockingSeconds)},
            {"fileName", strProp("File name relative to the capture directory. The .wav extension is added. Default: audio_<timestamp>")},
            {"inline", prop("boolean", QString("Also return the WAV as base64 in audioBase64. Only for clips of %1 seconds or less. Default false").arg(MCPCapture::m_maxInlineSeconds))}
        }, {"deviceSetIndex", "channelIndex", "seconds"}),
        [this](const QJsonObject& args)
        {
            int deviceSetIndex = argInt(args, "deviceSetIndex");
            int channelIndex = argInt(args, "channelIndex");
            double seconds = argDouble(args, "seconds");
            bool inlineAudio = args["inline"].toBool(false) || (args["inline"].toString().toLower() == "true");

            if ((seconds <= 0) || (seconds > MCPCapture::m_maxBlockingSeconds)) {
                throw MCPToolError(QString("seconds must be between 0 and %1").arg(MCPCapture::m_maxBlockingSeconds));
            }

            if (inlineAudio && (seconds > MCPCapture::m_maxInlineSeconds)) {
                throw MCPToolError(QString("inline audio is only available for %1 seconds or less").arg(MCPCapture::m_maxInlineSeconds));
            }

            QJsonObject state = deviceState(deviceSetIndex, 0, -1);

            if (state["state"].toString() != "running") {
                throw MCPToolError("The device is not running, so there is no audio to record. Call start_device first.");
            }

            return m_capture.captureAudio(deviceSetIndex, channelIndex, seconds,
                argString(args, "fileName", false), inlineAudio);
        });

    add("start_iq_recording",
        "Start recording baseband IQ samples to a file. Adds a FileSink channel to the device set unless channelIndex names an "
        "existing one, and starts it recording. The device must be running for samples to be written. Recording continues until "
        "stop_iq_recording, so use this for long captures and record_iq for short ones. Files are written under the capture "
        "directory shown by get_server_status; fileName is relative to it.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"fileName", strProp("File name relative to the capture directory. A timestamp and the .sdriq extension are added. Default: iq_<timestamp>")},
            {"frequencyOffset", intProp("Channel frequency offset from the device centre frequency in Hz. Default 0, which records the full baseband centred on the device frequency")},
            {"log2Decim", bounded(intProp("Decimation as a power of two: 0 records at the device sample rate, 1 halves it, and so on. Default 0"), 0, 6)},
            {"channelIndex", intProp("Use this existing FileSink channel instead of adding one")}
        }, {"deviceSetIndex"}),
        [this](const QJsonObject& args)
        {
            int deviceSetIndex = argInt(args, "deviceSetIndex");
            bool created = !hasArg(args, "channelIndex");
            int channelIndex = created ? addChannelAndWait(deviceSetIndex, "FileSink") : argInt(args, "channelIndex");

            try
            {
                QJsonObject result = m_capture.startIQRecording(deviceSetIndex, channelIndex,
                    argString(args, "fileName", false), argInt(args, "frequencyOffset", false, 0),
                    argInt(args, "log2Decim", false, 0));
                result["channelCreated"] = created;
                QJsonObject state = deviceState(deviceSetIndex, 0, -1);

                if (state["state"].toString() != "running") {
                    result["warning"] = "The device is not running, so no samples will be recorded. Call start_device.";
                }

                return result;
            }
            catch (const MCPToolError&)
            {
                if (created) { // do not leave a channel behind when the recording could not be set up
                    try { deleteChannelAndWait(deviceSetIndex, channelIndex); } catch (const MCPToolError&) {}
                }

                throw;
            }
        });

    add("stop_iq_recording",
        "Stop an IQ recording started with start_iq_recording and report the files written, with their size and duration.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"channelIndex", intProp("Index of the FileSink channel that is recording")},
            {"deleteChannel", prop("boolean", "Delete the FileSink channel afterwards. Default false")}
        }, {"deviceSetIndex", "channelIndex"}),
        [this](const QJsonObject& args)
        {
            int deviceSetIndex = argInt(args, "deviceSetIndex");
            int channelIndex = argInt(args, "channelIndex");
            const void *channel = channelAt(deviceSetIndex, channelIndex);
            QJsonObject result = m_capture.stopIQRecording(deviceSetIndex, channelIndex);
            bool deleteChannel = args["deleteChannel"].toBool(false) || (args["deleteChannel"].toString().toLower() == "true");

            if (deleteChannel)
            {
                deleteChannelObjectAndWait(channel);
                result["channelDeleted"] = true;
            }

            return result;
        });

    add("record_iq",
        QString("Record baseband IQ samples for a fixed number of seconds and return the files written. Adds a FileSink channel, "
                "records, stops and removes the channel again. This call blocks for the duration, so it is limited to %1 seconds; "
                "for longer captures use start_iq_recording and stop_iq_recording. The device must already be running. "
                "Note that IQ files are large: roughly 4 bytes per sample per second of the device sample rate.")
            .arg(MCPCapture::m_maxBlockingSeconds),
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"seconds", bounded(numProp(QString("How long to record for, up to %1").arg(MCPCapture::m_maxBlockingSeconds)), 0.1, MCPCapture::m_maxBlockingSeconds)},
            {"fileName", strProp("File name relative to the capture directory. A timestamp and the .sdriq extension are added")},
            {"frequencyOffset", intProp("Channel frequency offset from the device centre frequency in Hz. Default 0")},
            {"log2Decim", bounded(intProp("Decimation as a power of two. Default 0"), 0, 6)}
        }, {"deviceSetIndex", "seconds"}),
        [this](const QJsonObject& args)
        {
            int deviceSetIndex = argInt(args, "deviceSetIndex");
            double seconds = argDouble(args, "seconds");

            if ((seconds <= 0) || (seconds > MCPCapture::m_maxBlockingSeconds))
            {
                throw MCPToolError(QString("seconds must be between 0 and %1. For a longer capture use start_iq_recording and stop_iq_recording")
                    .arg(MCPCapture::m_maxBlockingSeconds));
            }

            QJsonObject state = deviceState(deviceSetIndex, 0, -1);

            if (state["state"].toString() != "running") {
                throw MCPToolError("The device is not running, so there is nothing to record. Call start_device first.");
            }

            int channelIndex = addChannelAndWait(deviceSetIndex, "FileSink");

            // This call sleeps for up to half a minute and is not serialised against the other
            // tools, so another request can delete a channel and renumber this one meanwhile.
            // Everything after the sleep therefore works from the channel itself, not its index.
            const void *channel = channelAt(deviceSetIndex, channelIndex);

            if (!channel) // cannot happen, but do not leave the channel behind if it does
            {
                try { deleteChannelAndWait(deviceSetIndex, channelIndex); } catch (const MCPToolError&) {}

                throw MCPToolError("The FileSink channel went away as soon as it was added");
            }

            QJsonObject result;

            try
            {
                QJsonObject started = m_capture.startIQRecording(deviceSetIndex, channelIndex,
                    argString(args, "fileName", false), argInt(args, "frequencyOffset", false, 0),
                    argInt(args, "log2Decim", false, 0));
                QThread::msleep((unsigned long) (seconds * 1000.0));
                result = m_capture.stopIQRecording(channel);
                result["fileBase"] = started["fileBase"];
                result["requestedSeconds"] = seconds;
            }
            catch (const MCPToolError&)
            {
                try { deleteChannelObjectAndWait(channel); } catch (const MCPToolError&) {}
                throw;
            }

            deleteChannelObjectAndWait(channel);

            if (result["files"].toArray().isEmpty()) {
                result["warning"] = "No files were written. Check that the device is running and producing samples.";
            }

            return result;
        });
}

// ---------------------------------------------------------------------------
// Intent level tools: a whole task in one call
//
// Setting up a receiver with the object level tools takes an agent around ten round trips,
// each one a model turn. These do the common tasks in one, using the same helpers, and reply
// with a short summary rather than the objects they created.
// ---------------------------------------------------------------------------

namespace {

// What "listen to X in mode Y" needs: the demodulator, its initial settings and the smallest
// baseband that carries it. Values are the ones the receiving guide and readmes give.
struct ListenMode
{
    const char *m_alias;
    const char *m_channelType;
    int m_rfBandwidth;      //!< Hz. Negative selects the lower sideband, as the SSB demodulator does
    int m_minBaseband;      //!< Hz
    const char *m_settings; //!< JSON of further initial settings
    int m_scanStep;         //!< Hz. The channel spacing scan steps by unless told otherwise; 0 for a mode on fixed frequencies, which is not scanned for
    const char *m_feature;  //!< A feature the mode's output goes to, added if absent; null for none
    int m_pairOffset;       //!< Hz. Non-zero for a mode on two channels this far either side of the frequency, one demodulator each
};

const char *const bfmSettings = "{\"afBandwidth\":15000,\"audioStereo\":1,\"rdsActive\":1,\"volume\":4,\"squelch\":-60}";
const char *const amSettings = "{\"afBandwidth\":3000,\"bandpassEnable\":1,\"volume\":4,\"squelch\":-60}";

const ListenMode listenModes[] = {
    // Broadcast FM asks for twice the baseband its channel needs: a 180 kHz channel in a 256 kS/s
    // baseband sits against the edge of the decimation filters, and RDS suffers first; it also
    // leaves room to put the channel clear of the device's DC spike
    {"bfm",        "BFMDemod",        180000,  512000, bfmSettings,                          100000, nullptr,          0},
    {"fm",         "BFMDemod",        180000,  512000, bfmSettings,                          100000, nullptr,          0},
    {"wfm",        "WFMDemod",        150000,  512000, "{\"volume\":2,\"squelch\":-60}",     100000, nullptr,          0},
    {"nfm",        "NFMDemod",         12500,   48000, "{\"afBandwidth\":3000,\"volume\":2}", 12500, nullptr,          0},
    {"am",         "AMDemod",           8000,   48000, amSettings,                            25000, nullptr,          0},
    {"airband",    "AMDemod",           8000,   48000, amSettings,                            25000, nullptr,          0},
    {"ssb",        "SSBDemod",          3000,   48000, "{\"volume\":2}",                       3000, nullptr,          0},
    {"usb",        "SSBDemod",          3000,   48000, "{\"volume\":2}",                       3000, nullptr,          0},
    {"lsb",        "SSBDemod",         -3000,   48000, "{\"volume\":2}",                       3000, nullptr,          0},
    {"dab",        "DABDemod",             0, 2048000, "{}",                                      0, nullptr,          0},
    {"adsb",       "ADSBDemod",            0, 2400000, "{}",                                      0, nullptr,          0},
    {"ais",        "AISDemod",             0,  128000, "{}",                                      0, "AIS",        25000},
    {"dsc",        "DSCDemod",           450,   48000, "{}",                                      0, nullptr,          0},
    {"dsd",        "DSDDemod",         12500,   48000, "{}",                                  12500, nullptr,          0},
    {"dmr",        "DSDDemod",         12500,   48000, "{}",                                  12500, nullptr,          0},
    {"pager",      "PagerDemod",       20000,   48000, "{}",                                  12500, nullptr,          0},
    {"pocsag",     "PagerDemod",       20000,   48000, "{}",                                  12500, nullptr,          0},
    {"sonde",      "RadiosondeDemod",   9600,   48000, "{}",                                  10000, "Radiosonde",     0},
    {"radiosonde", "RadiosondeDemod",   9600,   48000, "{}",                                  10000, "Radiosonde",     0},
    // AX.25 packet at 1200 baud AFSK. APRS is the same signal on a fixed regional frequency,
    // with the APRS feature to plot the stations
    {"packet",     "PacketDemod",      12500,   48000, "{}",                                      0, nullptr,          0},
    {"aprs",       "PacketDemod",      12500,   48000, "{}",                                      0, "APRS",           0},
};

const char *const listenModeList = "bfm, wfm, nfm, am, ssb, usb, lsb, dab, adsb, ais, dsc, dsd, pager, sonde, packet, aprs";

// The feature a demodulator of this type feeds, by the modes that add one; null for none
const char *featureForChannelType(const QString& channelType)
{
    for (const ListenMode& mode : listenModes)
    {
        if (mode.m_feature && (channelType == QLatin1String(mode.m_channelType))) {
            return mode.m_feature;
        }
    }

    return nullptr;
}

// What listen and scan say about the features a reclaim took away
QStringList featureNotes(const QStringList& reclaimedFeatures)
{
    if (reclaimedFeatures.isEmpty()) {
        return QStringList();
    }

    return QStringList(QString("Removed the %1 feature%2 an earlier listen added, as nothing feeds %3 now.")
        .arg(reclaimedFeatures.join(" and ")).arg(reclaimedFeatures.size() == 1 ? "" : "s")
        .arg(reclaimedFeatures.size() == 1 ? "it" : "them"));
}

const ListenMode *findListenMode(const QString& alias)
{
    for (const ListenMode& mode : listenModes)
    {
        if (alias.compare(QLatin1String(mode.m_alias), Qt::CaseInsensitive) == 0) {
            return &mode;
        }
    }

    return nullptr;
}

// The device rate and decimation that give at least the wanted baseband. These are the rates
// an RTL-SDR runs reliably; DAB needs exactly 2.048 MS/s.
void rateFor(int minBaseband, int& devSampleRate, int& log2Decim)
{
    if (minBaseband <= 256000)       { devSampleRate = 1024000; log2Decim = 2; }
    else if (minBaseband <= 512000)  { devSampleRate = 1024000; log2Decim = 1; }
    else if (minBaseband <= 1024000) { devSampleRate = 1024000; log2Decim = 0; }
    else if (minBaseband <= 2048000) { devSampleRate = 2048000; log2Decim = 0; }
    else                             { devSampleRate = 2400000; log2Decim = 0; }
}

// The baseband a channel type needs, from the table listen uses, so that there is one set of
// numbers rather than two that can disagree. 0 when the type is not in it.
int minimumBasebandFor(const QString& channelType)
{
    for (const ListenMode& mode : listenModes)
    {
        if (channelType.compare(QLatin1String(mode.m_channelType), Qt::CaseInsensitive) == 0) {
            return mode.m_minBaseband;
        }
    }

    return 0;
}

// Devices that are not radios attached to this machine, skipped when choosing one automatically
const QStringList virtualDeviceTypes = {
    "TestSource", "FileInput", "SigMFFileInput", "LocalInput", "RemoteInput", "RemoteTCPInput",
    "KiwiSDR", "AudioInput", "AaroniaRTSA", "AndroidSdrDriverInput"
};

// The type specific sub-object of a settings reply, e.g. the rtlSdrSettings of a device
QJsonObject settingsOf(const QJsonObject& reply)
{
    return reply[findSettingsKey(reply)].toObject();
}

// A signal level from a channel report, whatever the plugin calls it
bool signalLevel(const QJsonObject& report, double& db)
{
    for (const QString& key : report.keys())
    {
        if (!key.endsWith("Report") || !report[key].isObject()) {
            continue;
        }

        QJsonObject sub = report[key].toObject();

        for (const QString& k : sub.keys())
        {
            if ((k.contains("PowerDB", Qt::CaseInsensitive) || (k == "rssi")) && sub[k].isDouble())
            {
                db = sub[k].toDouble();
                return true;
            }
        }
    }

    return false;
}

} // namespace

// A channel whose baseband is too narrow produces nothing, or silence, with no error anywhere:
// the demodulator simply never sees the signal. The rule is in this server's instructions, but
// it is read once at the start of a conversation and needed much later, so it is repeated here,
// where the numbers are known and the fix can be named.
QString MCPTools::basebandWarning(int deviceSetIndex, const QString& channelType)
{
    int needed = minimumBasebandFor(channelType);

    if (needed <= 0) {
        return QString(); // nothing known about what this type needs
    }

    QJsonObject settings;

    try
    {
        settings = settingsOf(getDeviceSettings(deviceSetIndex));
    }
    catch (const MCPToolError&)
    {
        return QString(); // no device, or it cannot be read: not this call's problem to report
    }

    // Only devices that express their rate this way can be checked. Saying nothing is better
    // than guessing at a device whose keys are not these
    if (!settings.contains("devSampleRate") || !settings.contains("log2Decim")) {
        return QString();
    }

    int baseband = settings["devSampleRate"].toInt() >> settings["log2Decim"].toInt();

    if (baseband >= needed) {
        return QString();
    }

    int wantedRate = 0;
    int wantedDecim = 0;
    rateFor(needed, wantedRate, wantedDecim);

    return QString("%1 needs a baseband of at least %2 Hz, but device set %3 provides %4 Hz, so the channel "
                   "cannot work as it stands. Call set_device_settings with devSampleRate %5 and log2Decim %6.")
        .arg(channelType).arg(needed).arg(deviceSetIndex).arg(baseband).arg(wantedRate).arg(wantedDecim);
}

QStringList MCPTools::basebandWarnings(int deviceSetIndex)
{
    QStringList warnings;
    QJsonObject deviceSet;

    try
    {
        deviceSet = getDeviceSet(deviceSetIndex);
    }
    catch (const MCPToolError&)
    {
        return warnings;
    }

    QSet<QString> seen;

    for (const QJsonValue& value : deviceSet["channels"].toArray())
    {
        QString channelType = value.toObject()["id"].toString();

        if (channelType.isEmpty() || seen.contains(channelType)) {
            continue; // several of a type would repeat the same sentence
        }

        seen.insert(channelType);
        QString warning = basebandWarning(deviceSetIndex, channelType);

        if (!warning.isEmpty()) {
            warnings.append(warning);
        }
    }

    return warnings;
}

QStringList MCPTools::channelTypeIds(int direction)
{
    SWGSDRangel::SWGInstanceChannelsResponse response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    check(m_adapter->instanceChannels(direction, response, error), error, "List channel types");
    QStringList ids;

    for (const QJsonValue& v : toJson(response)["channels"].toArray()) {
        ids.append(v.toObject()["id"].toString());
    }

    return ids;
}

void MCPTools::relocateChannel(const void *channel, int& deviceSetIndex, int& channelIndex, const QString& what)
{
    if (!MCPCapture::locateChannel(channel, deviceSetIndex, channelIndex)) {
        throw MCPToolError(QString("The %1 channel no longer exists: another request removed it, or its device set, "
                                   "while this call was running").arg(what));
    }
}

uint64_t MCPTools::channelUid(const void *channel)
{
    return channel ? static_cast<const ChannelAPI *>(channel)->getUID() : 0;
}

void MCPTools::trackIntentChannel(const void *channel)
{
    QMutexLocker locker(&m_intentMutex);
    m_intentChannels.insert(channelUid(channel));
}

void MCPTools::untrackIntentChannel(const void *channel)
{
    QMutexLocker locker(&m_intentMutex);
    m_intentChannels.remove(channelUid(channel));
}

QSet<uint64_t> MCPTools::intentChannels()
{
    QMutexLocker locker(&m_intentMutex);
    return m_intentChannels;
}

// What a device set carries besides one channel, and which of it listen or scan put there.
// Read from the live device set rather than any earlier snapshot, as a reclaim in between
// changes both the indices and the answer
QList<MCPTools::ChannelNote> MCPTools::channelNotes(int deviceSetIndex, const QSet<const void *>& except)
{
    const QSet<uint64_t> intent = intentChannels();
    QList<ChannelNote> notes;

    for (const QJsonValue& v : getDeviceSet(deviceSetIndex)["channels"].toArray())
    {
        QJsonObject channel = v.toObject();
        int index = channel["index"].toInt();
        const void *pointer = channelAt(deviceSetIndex, index);

        if (pointer && !except.contains(pointer)) {
            notes.append({pointer, index, channel["id"].toString(), intent.contains(channelUid(pointer))});
        }
    }

    return notes;
}

QStringList MCPTools::reclaimIntentChannels(int deviceSetIndex, const void *keep)
{
    QStringList removed;

    // Each is deleted by identity, wherever it has been renumbered to by then, and highest
    // index first so that the ids come out in the order the device set listed them
    QList<ChannelNote> notes = channelNotes(deviceSetIndex);

    for (int i = notes.size() - 1; i >= 0; i--)
    {
        if (!notes[i].m_intent || (notes[i].m_channel == keep)) {
            continue;
        }

        try
        {
            deleteChannelObjectAndWait(notes[i].m_channel);
            removed.prepend(notes[i].m_id);
        }
        catch (const MCPToolError&) {}

        // Tracked no longer either way: if it could not be deleted now it will not be later
        untrackIntentChannel(notes[i].m_channel);
    }

    return removed;
}

// Retuning the demodulator a previous listen added is what a person would do, and spares the
// audio device and the GUI a close and reopen. Only when it is the one intent channel of that
// type, and no scan's FreqScanner is there to be driving it
const void *MCPTools::reusableIntentChannel(int deviceSetIndex, const QString& channelType, int& channelIndex)
{
    const void *candidate = nullptr;
    int candidates = 0;

    for (const ChannelNote& note : channelNotes(deviceSetIndex))
    {
        if (!note.m_intent) {
            continue;
        }

        if (note.m_id == "FreqScanner") {
            return nullptr;
        }

        if (note.m_id == channelType)
        {
            candidate = note.m_channel;
            channelIndex = note.m_index;
            candidates++;
        }
    }

    return (candidates == 1) ? candidate : nullptr;
}

// A retune moves every channel on the device set, since each is an offset from the centre. A
// channel the user added is kept on the frequency it had when the new baseband holds it; an
// audio demodulator it cannot hold is removed, as all it could do is play noise; anything else
// is left, with a note, since a file sink or an analyser may be wanted regardless
MCPTools::RetuneOutcome MCPTools::retuneOtherChannels(int deviceSetIndex, double previousCentre, double centre, int baseband)
{
    RetuneOutcome outcome;

    if ((qint64) previousCentre == (qint64) centre) {
        return outcome;
    }

    static const QStringList audioDemods = {
        "AMDemod", "BFMDemod", "DABDemod", "DATVDemod", "DSDDemod", "FreeDVDemod", "ILSDemod", "M17Demod",
        "NFMDemod", "SSBDemod", "VORDemod", "VORDemodMC", "WFMDemod", "WDSPRx"
    };

    const QSet<uint64_t> intent = intentChannels();
    QJsonObject set = getDeviceSet(deviceSetIndex);

    if (baseband <= 0) {
        baseband = set["samplingDevice"].toObject()["bandwidth"].toInt();
    }

    struct Item { int m_index; QString m_id; const void *m_channel; qint64 m_offset; };
    QList<Item> items;

    for (const QJsonValue& v : set["channels"].toArray())
    {
        QJsonObject channel = v.toObject();
        const int index = channel["index"].toInt();
        const void *pointer = channelAt(deviceSetIndex, index);

        if (pointer && !intent.contains(channelUid(pointer))) {
            items.append({index, channel["id"].toString(), pointer, (qint64) channel["deltaFrequency"].toDouble()});
        }
    }

    // Highest index first, so that a removal does not renumber one still to be visited
    for (int i = items.size() - 1; i >= 0; i--)
    {
        const Item& item = items[i];
        const qint64 wanted = (qint64) previousCentre + item.m_offset; // where it was listening
        const qint64 offset = wanted - (qint64) centre;
        const QString where = QString("%1 at %2 MHz").arg(item.m_id).arg(wanted / 1e6, 0, 'f', 3);

        // The offset key differs by type: most have inputFrequencyOffset, the Channel Analyzer
        // frequency, ATV intFrequencyOffset, DATV centerFrequency; a sink or a scanner has none
        // and is left alone. Read the live settings to find which, rather than guess
        QString offsetKey;

        if ((baseband > 0) && (qAbs(offset) <= baseband * 0.45)) // a tenth kept clear at each edge
        {
            try
            {
                QJsonObject current = settingsOf(getChannelSettings(deviceSetIndex, item.m_index));

                for (const char *key : {"inputFrequencyOffset", "frequency", "intFrequencyOffset", "centerFrequency"})
                {
                    if (current.contains(key))
                    {
                        offsetKey = key;
                        break;
                    }
                }
            }
            catch (const MCPToolError&) {}
        }

        if (!offsetKey.isEmpty())
        {
            try
            {
                QJsonObject partial;
                partial[offsetKey] = (double) offset;
                patchChannelSettings(deviceSetIndex, item.m_index, partial);
                outcome.m_kept.append(where);
            }
            catch (const MCPToolError&)
            {
                outcome.m_stranded.append(where);
            }
        }
        else if ((baseband > 0) && audioDemods.contains(item.m_id))
        {
            try
            {
                deleteChannelObjectAndWait(item.m_channel);
                outcome.m_removed.append(where);
            }
            catch (const MCPToolError&)
            {
                outcome.m_stranded.append(where);
            }
        }
        else
        {
            outcome.m_stranded.append(where);
        }
    }

    return outcome;
}

QStringList MCPTools::intentNotes(int deviceSetIndex, const QSet<const void *>& added, const QStringList& reclaimed,
    bool reused, double previousCentre, double centre, const RetuneOutcome& outcome)
{
    QStringList notes;

    if (!reclaimed.isEmpty())
    {
        notes.append(QString("Removed %1 channel%2 (%3) that earlier listen or scan calls added to device set %4; pass replace false to keep them.")
            .arg(reclaimed.size()).arg(reclaimed.size() == 1 ? "" : "s").arg(reclaimed.join(", ")).arg(deviceSetIndex));
    }

    // Read from the live device set rather than the snapshot pickReceiver made, which still
    // lists whatever the reclaim has since removed
    QStringList leftovers;
    QStringList others;

    for (const ChannelNote& note : channelNotes(deviceSetIndex, added)) {
        (note.m_intent ? leftovers : others).append(note.m_id);
    }

    // Only possible with replace false, and then it is the caller's own trail
    if (!leftovers.isEmpty())
    {
        notes.append(QString("Device set %1 still has %2 channel%3 (%4) from earlier listen or scan calls, any audio ones still playing; cleanup removes them.")
            .arg(deviceSetIndex).arg(leftovers.size()).arg(leftovers.size() == 1 ? "" : "s").arg(leftovers.join(", ")));
    }

    // What the retune did to the channels the user added, and how to put back what it took
    if (reused && ((qint64) previousCentre != (qint64) centre)
        && !(outcome.m_kept.isEmpty() && outcome.m_removed.isEmpty() && outcome.m_stranded.isEmpty()))
    {
        QString note = QString("Retuned device set %1 from %2 to %3 MHz.").arg(deviceSetIndex)
            .arg(previousCentre / 1e6, 0, 'f', 3).arg(centre / 1e6, 0, 'f', 3);

        if (!outcome.m_kept.isEmpty()) {
            note += QString(" Kept %1 other channel%2 on frequency by moving the offset: %3.")
                .arg(outcome.m_kept.size()).arg(outcome.m_kept.size() == 1 ? "" : "s").arg(outcome.m_kept.join(", "));
        }

        if (!outcome.m_removed.isEmpty()) {
            note += QString(" Removed %1 audio demodulator%2 the new baseband could not hold, which would only have played noise: %3. "
                "They were not added by listen or scan; add_channel puts one back.")
                .arg(outcome.m_removed.size()).arg(outcome.m_removed.size() == 1 ? "" : "s").arg(outcome.m_removed.join(", "));
        }

        if (!outcome.m_stranded.isEmpty()) {
            note += QString(" Left %1 other channel%2 that the new baseband does not hold, not being audio: %3. "
                "set_channel_settings inputFrequencyOffset retunes one, delete_channel removes it.")
                .arg(outcome.m_stranded.size()).arg(outcome.m_stranded.size() == 1 ? "" : "s").arg(outcome.m_stranded.join(", "));
        }

        notes.append(note);
    }

    (void) others;
    return notes;
}

QJsonObject MCPTools::channelReport(int deviceSetIndex, int channelIndex)
{
    SWGSDRangel::SWGChannelReport response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    check(m_adapter->devicesetChannelReportGet(deviceSetIndex, channelIndex, response, error), error, "Get channel report");
    return toJson(response);
}

void MCPTools::postChannelAction(int deviceSetIndex, int channelIndex, const QJsonObject& actions)
{
    QJsonObject settings = getChannelSettings(deviceSetIndex, channelIndex);
    QString channelType = settings["channelType"].toString();

    if (!WebAPIUtils::m_channelTypeToActionsKey.contains(channelType)) {
        throw MCPToolError(QString("Channel type %1 has no actions").arg(channelType));
    }

    QJsonObject json;
    json["channelType"] = channelType;
    json["direction"] = settings["direction"].toInt(0);
    json[WebAPIUtils::m_channelTypeToActionsKey[channelType]] = actions;
    QStringList keys;
    extractKeys(actions, keys);

    SWGSDRangel::SWGChannelActions query;
    query.init();
    query.fromJsonObject(json);
    SWGSDRangel::SWGSuccessResponse response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    check(m_adapter->devicesetChannelActionsPost(deviceSetIndex, channelIndex, keys, query, response, error), error, "Channel action");
}

// Finds or creates a receive device set holding a suitable device and sets its sample rate.
// An existing set holding the same device is reused rather than fought for the hardware; the
// caller tunes the centre frequency and is told what else that set carries.
QJsonObject MCPTools::pickReceiver(const QJsonObject& args, int minBaseband, const QSet<uint64_t>& doomed, bool profileGain)
{
    SWGSDRangel::SWGInstanceDevicesResponse devicesResponse;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    check(m_adapter->instanceDevices(0, devicesResponse, error), error, "List devices");
    QJsonArray devices = toJson(devicesResponse)["devices"].toArray();

    // Which device: the one the caller named; else the first real radio that was actually
    // enumerated, which is the one with a serial number - some plugins list a placeholder
    // entry for hardware that is not attached, and it has none; else anything
    QJsonObject chosen;
    QJsonObject fallback;
    QString wanted = argString(args, "device", false).trimmed();

    for (const QJsonValue& v : devices)
    {
        QJsonObject d = v.toObject();

        if (!wanted.isEmpty())
        {
            if ((d["serial"].toString().compare(wanted, Qt::CaseInsensitive) == 0)
                || (d["displayedName"].toString().compare(wanted, Qt::CaseInsensitive) == 0)
                || (d["hwType"].toString().compare(wanted, Qt::CaseInsensitive) == 0))
            {
                chosen = d;
                break;
            }
        }
        else if (!virtualDeviceTypes.contains(d["hwType"].toString()))
        {
            if (!d["serial"].toString().isEmpty()) { chosen = d; break; }
            if (fallback.isEmpty()) { fallback = d; }
        }
    }

    if (chosen.isEmpty() && wanted.isEmpty()) {
        chosen = fallback.isEmpty() && !devices.isEmpty() ? devices.first().toObject() : fallback;
    }

    if (chosen.isEmpty()) {
        throw MCPToolError(QString("No receive device matches %1. list_available_devices shows what there is.").arg(wanted.isEmpty() ? "anything" : wanted));
    }

    QString hwType = chosen["hwType"].toString();
    QString serial = chosen["serial"].toString();

    // Reuse a device set that already holds this device
    int deviceSetIndex = -1;
    QJsonArray otherChannels;
    QJsonArray deviceSets = getInstanceSummary()["devicesetlist"].toObject()["deviceSets"].toArray();

    for (const QJsonValue& v : deviceSets)
    {
        QJsonObject ds = v.toObject();
        QJsonObject sd = ds["samplingDevice"].toObject();

        if (sd["direction"].toInt() != 0) {
            continue;
        }

        bool same = serial.isEmpty() ? (sd["hwType"].toString() == hwType) : (sd["serial"].toString() == serial);

        if (same)
        {
            deviceSetIndex = sd["index"].toInt();

            // Channels the caller is about to remove are not ones the set has to go on
            // serving, so they neither hold the baseband width up nor count as carried
            for (const QJsonValue& c : ds["channels"].toArray())
            {
                QJsonObject channel = c.toObject();

                if (!doomed.contains(channelUid(channelAt(deviceSetIndex, channel["index"].toInt())))) {
                    otherChannels.append(channel["id"].toString());
                }
            }

            break;
        }
    }

    bool reused = deviceSetIndex >= 0;

    if (!reused)
    {
        deviceSetIndex = addDeviceSetAndWait(0);
        QJsonObject select;
        select["hwType"] = hwType;

        if (!serial.isEmpty()) {
            select["serial"] = serial;
        }

        selectDevice(deviceSetIndex, select);
    }

    QJsonObject before = settingsOf(getDeviceSettings(deviceSetIndex));

    // Only the devices whose rate keys and supported rates are known are set; others keep
    // their configured rate and the caller is told to check it
    QString rateKey;

    if (hwType == "RTLSDR") {
        rateKey = "devSampleRate";
    } else if (hwType == "TestSource") {
        rateKey = "sampleRate";
    }

    // A reused device set is already feeding its other channels the width it has. Widening it for
    // the new channel leaves them working, narrowing it to what only the new one needs takes their
    // signal away, so the width already configured is a floor rather than something to overwrite
    int existingBaseband = 0;

    if (reused && !otherChannels.isEmpty() && !rateKey.isEmpty())
    {
        int rate = before[rateKey].toInt();

        if (rate > 0) {
            existingBaseband = rate >> before["log2Decim"].toInt();
        }
    }

    int devSampleRate = 0;
    int log2Decim = 0;
    rateFor(qMax(minBaseband, existingBaseband), devSampleRate, log2Decim);
    QJsonObject partial;
    QString rateNote;
    int baseband = 0;

    if (!rateKey.isEmpty())
    {
        baseband = devSampleRate >> log2Decim;

        if (baseband < existingBaseband)
        {
            // Wider than rateFor is able to ask for, so leave the rate alone altogether
            baseband = existingBaseband;
            rateNote = QString("Device set %1 was left at %2 S/s of baseband, which its other channels need.")
                .arg(deviceSetIndex).arg(existingBaseband);
        }
        else
        {
            partial[rateKey] = devSampleRate;
            partial["log2Decim"] = log2Decim;

            if (existingBaseband > minBaseband) {
                rateNote = QString("Device set %1 was kept at %2 S/s of baseband rather than the %3 this channel needs, for its other channels.")
                    .arg(deviceSetIndex).arg(baseband).arg(minBaseband);
            }
        }
    }

    // Only a device set this call created gets the receiver profile: one that was found is the
    // user's, and its gain stays whatever they set
    if (!reused && (hwType == "RTLSDR"))
    {
        partial["dcBlock"] = 1;
        partial["iqImbalance"] = 1;

        // A fixed 40 dB when the caller is not about to measure one: the tuner's own AGC is
        // unreliable on weak signals, and the default gain left the airband 35 dB down on what
        // a hand-set receiver saw
        if (profileGain)
        {
            partial["agc"] = 0;
            partial["gain"] = 402;
            rateNote = (rateNote.isEmpty() ? QString() : rateNote + " ")
                + QString("Device set %1 is new, so its RTL-SDR was set to 40 dB gain with AGC off; set_device_settings changes it.")
                    .arg(deviceSetIndex);
        }
    }

    if (!partial.isEmpty()) {
        patchDeviceSettings(deviceSetIndex, partial);
    }

    QJsonObject result;
    result["deviceSetIndex"] = deviceSetIndex;
    result["hwType"] = hwType;
    result["device"] = chosen["displayedName"].toString();
    result["reused"] = reused;
    result["previousCentre"] = before["centerFrequency"].toDouble();
    result["otherChannels"] = otherChannels;
    result["baseband"] = baseband;
    result["rateNote"] = rateNote;
    return result;
}

// The tune_gain tool, also run by listen and scan when they set up a receiver on a new
// device set or move one to another band
QJsonObject MCPTools::tuneGain(const QJsonObject& args)
{
    int deviceSetIndex = argInt(args, "deviceSetIndex");
    QJsonObject reply = getDeviceSettings(deviceSetIndex);
    const QString hwType = reply["deviceHwType"].toString();
    QJsonObject settings = settingsOf(reply);

    if (deviceState(deviceSetIndex, 0, -1)["state"].toString() != "running") {
        throw MCPToolError("The device is not running, so there is nothing to measure. start_device first.");
    }

    // What this knows about each device's gain: the key, its manual AGC setting, and the
    // values to try. Where a device has several gain stages the one before the mixer is
    // tuned, as that is where overload happens; the rest keep their settings
    struct Knob { QString key; QString agcKey; int agcOff; QList<double> values; QString unit; QString note; };
    Knob knob;
    const bool custom = hasArg(args, "gainKey");

    if (custom)
    {
        knob.key = argString(args, "gainKey");
        knob.agcKey = argString(args, "agcKey", false);
        knob.agcOff = 0;
        knob.unit = "the key's own units";

        for (const QJsonValue& v : args["values"].toArray()) {
            knob.values.append(v.toDouble());
        }

        if (knob.values.size() < 2) {
            throw MCPToolError("Give at least two values to try with gainKey");
        }
    }
    else if (hwType == "RTLSDR")
    {
        // The tuner takes only the values it lists, in tenths of a decibel
        knob = {"gain", "agc", 0, {}, "tenths of a dB", "The tuner's AGC is off, as it must be for the gain to hold"};
        SWGSDRangel::SWGDeviceReport report;
        SWGSDRangel::SWGErrorResponse error;
        error.init();
        check(m_adapter->devicesetDeviceReportGet(deviceSetIndex, report, error), error, "Get device report");
        QJsonObject json = toJson(report);

        for (const QString& key : json.keys())
        {
            for (const QJsonValue& g : json[key].toObject()["gains"].toArray()) {
                knob.values.append(g.toObject()["gainCB"].toDouble());
            }
        }

        if (knob.values.isEmpty()) {
            throw MCPToolError("The RTL-SDR reported no supported gain values; is the device open?");
        }
    }
    else if (hwType == "HackRF") {
        knob = {"lnaGain", "", 0, {0, 8, 16, 24, 32, 40}, "dB", "Only the LNA is stepped; vgaGain is left as it is"};
    }
    else if (hwType == "Airspy") {
        knob = {"lnaGain", "lnaAGC", 0, {0, 2, 4, 6, 8, 10, 12, 14}, "steps", "Only the LNA is stepped; mixerGain and vgaGain are left as they are, with the LNA AGC off"};
    }
    else if (hwType == "LimeSDR") {
        knob = {"gain", "gainMode", 1, {0, 10, 20, 30, 40, 50, 60, 70}, "dB", "gainMode set to manual"};
    }
    else if (hwType == "PlutoSDR") {
        knob = {"gain", "gainMode", 0, {0, 10, 20, 30, 40, 50, 60, 70}, "dB", "gainMode set to manual"};
    }
    else if (hwType == "USRP") {
        knob = {"gain", "gainMode", 1, {0, 10, 20, 30, 40, 50, 60, 70}, "dB", "gainMode set to manual"};
    }
    else
    {
        throw MCPToolError(QString("tune_gain does not know the gain settings of a %1. Read them with describe_settings and "
            "give gainKey, values and, if it has one, agcKey.").arg(hwType));
    }

    std::sort(knob.values.begin(), knob.values.end());
    knob.values.erase(std::unique(knob.values.begin(), knob.values.end()), knob.values.end());

    // Spread the steps over the range rather than trying every value a tuner offers
    const int steps = qBound(3, argInt(args, "steps", false, 8), 16);
    QList<double> candidates;

    if (knob.values.size() <= steps) {
        candidates = knob.values;
    }
    else
    {
        for (int i = 0; i < steps; i++) {
            candidates.append(knob.values[(int) std::round(i * (knob.values.size() - 1) / (double) (steps - 1))]);
        }
    }

    const bool apply = args.value("apply").toBool(true);
    const double previous = settings[knob.key].toDouble();
    const double previousAgc = knob.agcKey.isEmpty() ? 0 : settings[knob.agcKey].toDouble();
    const qint64 frequency = (qint64) argDouble(args, "frequency", false, 0);
    const qint64 bandwidth = (qint64) argDouble(args, "bandwidth", false, 200000);

    // One reading: the floor is the median over the baseband, the signal the strongest bin,
    // in the window asked for or anywhere. Max reduction keeps a narrow carrier
    auto measure = [&](double& floorDb, double& peakDb)
    {
        SWGSDRangel::SWGGLSpectrumData response;
        SWGSDRangel::SWGErrorResponse error;
        response.init();
        error.init();
        check(m_adapter->devicesetSpectrumDataGet(deviceSetIndex, 256, 0, 0, "max", response, error), error, "Get spectrum data");
        // Read from the object: the generated JSON serialiser drops a list of plain floats
        const bool linear = response.getLinear() != 0;
        QList<double> powers;

        if (response.getPower())
        {
            for (float v : *response.getPower()) {
                powers.append(linear ? 10.0 * std::log10(std::max((double) v, 1e-20)) : (double) v);
            }
        }

        if (powers.isEmpty()) {
            throw MCPToolError("The spectrum returned no data");
        }

        QList<double> sorted = powers;
        std::sort(sorted.begin(), sorted.end());
        floorDb = sorted[sorted.size() / 2];

        if (frequency > 0)
        {
            // The data call clamps to the baseband, which would silently measure noise at
            // its edge for a frequency outside it
            const qint64 low = response.getCenterFrequency() - response.getBandwidth() / 2;
            const qint64 high = response.getCenterFrequency() + response.getBandwidth() / 2;

            if ((frequency < low) || (frequency > high))
            {
                throw MCPToolError(QString("%1 MHz is outside the baseband, which covers %2 to %3 MHz; tune the device so that it is inside, or leave frequency out")
                    .arg(frequency / 1e6, 0, 'f', 3).arg(low / 1e6, 0, 'f', 3).arg(high / 1e6, 0, 'f', 3));
            }

            SWGSDRangel::SWGGLSpectrumData window;
            SWGSDRangel::SWGErrorResponse windowError;
            window.init();
            windowError.init();
            check(m_adapter->devicesetSpectrumDataGet(deviceSetIndex, 32, frequency - bandwidth / 2, frequency + bandwidth / 2, "max", window, windowError),
                windowError, "Get spectrum data");
            peakDb = -1e9;

            if (window.getPower())
            {
                for (float v : *window.getPower()) {
                    peakDb = std::max(peakDb, linear ? 10.0 * std::log10(std::max((double) v, 1e-20)) : (double) v);
                }
            }

            if (peakDb < -1e8) {
                throw MCPToolError("The spectrum has no bins at that frequency; it must be inside the baseband");
            }
        }
        else
        {
            peakDb = sorted.last();
        }
    };

    struct Row { double gain; double floorDb; double peakDb; double snrDb; };
    QList<Row> rows;

    auto setGain = [&](double gain, bool agcOff)
    {
        QJsonObject partial;
        partial[knob.key] = gain;

        if (!knob.agcKey.isEmpty()) {
            partial[knob.agcKey] = agcOff ? knob.agcOff : previousAgc;
        }

        patchDeviceSettings(deviceSetIndex, partial);
    };

    try
    {
        for (double gain : candidates)
        {
            setGain(gain, true);
            QThread::msleep(700); // the tuner, the spectrum averaging and a stale FFT
            double floorSum = 0.0;
            double peakSum = 0.0;
            const int samples = 3;

            for (int i = 0; i < samples; i++)
            {
                if (i > 0) {
                    QThread::msleep(250);
                }

                double floorDb, peakDb;
                measure(floorDb, peakDb);
                floorSum += floorDb;
                peakSum += peakDb;
            }

            rows.append({gain, floorSum / samples, peakSum / samples, (peakSum - floorSum) / samples});
        }
    }
    catch (const MCPToolError&)
    {
        setGain(previous, false); // as it was, AGC included
        throw;
    }

    auto toDb = [&](double gain) { return (knob.unit == "tenths of a dB") ? gain / 10.0 : gain; };

    // Overload shows as the floor climbing faster than the gain did. The first step
    // where it does bounds the usable range from above
    QStringList notes;
    int compressed = 0;
    int overloadFrom = rows.size(); // index of the first row that is overloaded

    for (int i = 1; i < rows.size(); i++)
    {
        const double gainStep = toDb(rows[i].gain) - toDb(rows[i - 1].gain);

        if ((gainStep > 0) && (rows[i].floorDb - rows[i - 1].floorDb > gainStep + 3.0))
        {
            compressed++;
            overloadFrom = std::min(overloadFrom, i);
        }
    }

    if (compressed > 0) {
        notes.append(QString("The noise floor rose faster than the gain over %1 step%2, which is the front end overloading; the higher gains are not usable here.")
            .arg(compressed).arg(compressed == 1 ? "" : "s"));
    }

    // Best SNR, then the lowest gain within a decibel of it
    double best = -1e9;
    for (const Row& r : rows) { best = std::max(best, r.snrDb); }
    const Row *chosen = nullptr;
    QString judgedBy = frequency > 0 ? QString("the signal at %1 MHz").arg(frequency / 1e6, 0, 'f', 3) : QString("the strongest signal in the baseband");

    if (best >= 6.0)
    {
        for (const Row& r : rows) { if ((r.snrDb >= best - 1.0) && !chosen) { chosen = &r; } }
    }
    else
    {
        // Nothing stood clear of the floor, so the floor itself has to decide: at low gain
        // it is the converter's own noise and does not move; as gain rises the antenna's
        // noise comes through and lifts it, which is when weak signals become receivable;
        // and past overload it lifts faster than the gain. Take the lowest gain that puts
        // the floor 8 dB above the converter's, which is the antenna's noise dominating
        // by the usual 10 dB rule with a little allowance for the steps being coarse, or
        // the last gain before overload if none does
        double adcFloor = 1e9;
        for (const Row& r : rows) { adcFloor = std::min(adcFloor, r.floorDb); }
        const int lastUsable = std::max(0, overloadFrom - 1);
        static const double antennaDominates = 8.0;

        for (int i = 0; (i <= lastUsable) && !chosen; i++)
        {
            if (rows[i].floorDb >= adcFloor + antennaDominates) {
                chosen = &rows[i];
            }
        }

        if (!chosen) {
            chosen = &rows[lastUsable];
        }

        judgedBy = "the noise floor alone, as no signal stood clear of it";
        notes.append(QString("No signal stood more than 6 dB above the floor, so the gain is set from the floor instead: %1.")
            .arg((rows[lastUsable].floorDb < adcFloor + antennaDominates)
                ? "the floor never rose 8 dB above the converter's own noise before overload, so the last gain before overload is taken; check the antenna if that seems low"
                : "the lowest gain at which the antenna's noise, not the converter's, sets the floor"));
    }

    if (chosen == &rows.last()) {
        notes.append("The best was the highest gain tried: the signal is weak or the site quiet, and more gain would not overload yet.");
    } else if (chosen == &rows.first()) {
        notes.append("The best was the lowest gain tried: a strong-signal site. Consider an attenuator if the floor still rises with the lowest gain.");
    }

    if (apply)
    {
        setGain(chosen->gain, true);
    }
    else
    {
        setGain(previous, false);
        notes.append("Not applied: the gain is back as it was.");
    }

    if (!knob.note.isEmpty()) {
        notes.append(knob.note + ".");
    }

    QJsonObject result;
    result["deviceSetIndex"] = deviceSetIndex;
    result["hwType"] = hwType;
    result["gainKey"] = knob.key;
    result["unit"] = knob.unit;
    result["previous"] = previous;
    result["chosen"] = chosen->gain;
    result["applied"] = apply;
    result["bestSnrDb"] = chosen->snrDb;
    result["judgedBy"] = judgedBy;
    QJsonArray table;

    for (const Row& r : rows)
    {
        QJsonObject o;
        o["gain"] = r.gain;
        o["floorDb"] = std::round(r.floorDb * 10) / 10;
        o["signalDb"] = std::round(r.peakDb * 10) / 10;
        o["snrDb"] = std::round(r.snrDb * 10) / 10;
        table.append(o);
    }

    result["table"] = table;

    if (!notes.isEmpty()) {
        result["note"] = notes.join(" ");
    }

    return result;
}

// A mode whose output is only useful through a feature, such as AIS to the AIS feature's ship
// list and the map, gets that feature added with the demodulator. One that is there already is
// left alone, whatever it is set to
int MCPTools::ensureFeature(const QString& featureType, int& featureIndex)
{
    for (const QJsonValue& v : getInstanceSummary()["featureset"].toObject()["features"].toArray())
    {
        QJsonObject f = v.toObject();

        if (f["id"].toString() == featureType)
        {
            featureIndex = f["index"].toInt();
            return 0;
        }
    }

    featureIndex = addFeatureAndWait(featureType);
    trackIntentFeature(featureAt(featureIndex));
    return 1;
}

void MCPTools::trackIntentFeature(const void *feature)
{
    if (!feature) {
        return;
    }

    QMutexLocker locker(&m_intentMutex);
    m_intentFeatures.insert(static_cast<const Feature *>(feature)->getUID());
}

// A feature listen added goes with the demodulators that fed it: once the last of those is
// reclaimed, on whichever device set, it would only show stale data. One the next mode
// needs, or that a demodulator on another device set still feeds, stays
QStringList MCPTools::reclaimIntentFeatures(const QString& keepType)
{
    QSet<uint64_t> tracked;
    {
        QMutexLocker locker(&m_intentMutex);
        tracked = m_intentFeatures;
    }

    if (tracked.isEmpty()) {
        return QStringList();
    }

    // The feature types the remaining intent channels feed
    QSet<QString> needed;

    if (!keepType.isEmpty()) {
        needed.insert(keepType);
    }

    for (int deviceSetIndex = 0; deviceSetIndex < deviceSetCount(); deviceSetIndex++)
    {
        for (const ChannelNote& note : channelNotes(deviceSetIndex))
        {
            const char *feature = note.m_intent ? featureForChannelType(note.m_id) : nullptr;

            if (feature) {
                needed.insert(QString(feature));
            }
        }
    }

    QStringList removed;
    const std::vector<FeatureSet*>& featureSets = MainCore::instance()->getFeatureeSets();

    if (featureSets.empty()) {
        return removed;
    }

    // Collected first: deleting renumbers the rest
    QList<const Feature *> doomed;

    for (int i = 0; i < featureSets[0]->getNumberOfFeatures(); i++)
    {
        const Feature *feature = featureSets[0]->getFeatureAt(i);

        if (feature && tracked.contains(feature->getUID()) && !needed.contains(feature->getIdentifier())) {
            doomed.append(feature);
        }
    }

    for (const Feature *feature : doomed)
    {
        const QString id = feature->getIdentifier();

        try
        {
            deleteFeatureObjectAndWait(feature);
            removed.append(id);
        }
        catch (const MCPToolError&) {}
    }

    // Tracked no longer: those removed, and any the user had already deleted
    QMutexLocker locker(&m_intentMutex);

    for (uint64_t uid : tracked)
    {
        bool present = false;

        for (int i = 0; i < featureSets[0]->getNumberOfFeatures(); i++)
        {
            const Feature *feature = featureSets[0]->getFeatureAt(i);

            if (feature && (feature->getUID() == uid))
            {
                present = true;
                break;
            }
        }

        if (!present) {
            m_intentFeatures.remove(uid);
        }
    }

    return removed;
}

void MCPTools::deleteFeatureObjectAndWait(const void *feature)
{
    int featureIndex = -1;
    const std::vector<FeatureSet*>& featureSets = MainCore::instance()->getFeatureeSets();

    if (!featureSets.empty())
    {
        for (int i = 0; i < featureSets[0]->getNumberOfFeatures(); i++)
        {
            if (featureSets[0]->getFeatureAt(i) == feature)
            {
                featureIndex = i;
                break;
            }
        }
    }

    if (featureIndex < 0) {
        return; // Already gone
    }

    requireNotSelf(featureIndex, "deleted");
    SWGSDRangel::SWGSuccessResponse response;
    SWGSDRangel::SWGErrorResponse error;
    error.init();
    check(m_adapter->featuresetFeatureDelete(0, featureIndex, response, error), error, "Delete feature");

    if (!waitFor([&]() { return !featurePointers().contains(feature); })) {
        throw MCPToolError(QString("Timed out waiting for feature %1 to be removed").arg(featureIndex));
    }
}

// The gain suits a band and an antenna, not a mode: a device set that was just created has
// whatever gain it came with, and one moved to another band has the gain of the last one. A
// move within a band, or a change of mode on the same frequency, needs no new measurement
bool MCPTools::gainWorthTuning(bool reused, double previousCentre, double centre)
{
    if (!reused) {
        return true;
    }

    return std::abs(centre - previousCentre) > 0.1 * std::max(centre, previousCentre);
}

void MCPTools::registerIntentTools()
{
    static const QString deviceHint = "Optional: which device, by serial, displayed name or type (e.g. RTLSDR) as shown by "
        "list_available_devices. Default: the first radio attached to this machine";

    add("listen",
        "Receive a frequency in one call: picks a device, creates or reuses its device set, sets the sample rate, adds the "
        "demodulator with sensible settings, starts the device and reports the signal level. Audio plays on the default output. "
        "Use the individual tools afterwards to adjust, e.g. set_channel_settings for squelch or volume. Modes whose output "
        "goes to a feature add it too: ais adds the AIS feature, sonde the Radiosonde feature and aprs the APRS feature, whose "
        "reports and map items carry what was decoded; a later listen or scan removes the feature again once nothing it added "
        "feeds it. ais is received on two channels, so give 162000000 and it puts a demodulator on each of 161.975 and "
        "162.025 MHz.",
        schema({
            {"frequency", numProp("Frequency to receive in Hz, e.g. 97300000 for 97.3 MHz")},
            {"mode", strProp("bfm (broadcast FM with stereo and RDS), wfm, nfm, am or airband, ssb/usb, lsb, dab, adsb, ais, dsc, dsd/dmr, "
                             "pager (POCSAG), sonde (RS41 radiosondes), packet (AX.25), aprs (APRS with its feature), or any channel type id from list_channel_types. Default nfm")},
            {"device", strProp(deviceHint)},
            {"replace", prop("boolean", "Retune the demodulator a previous listen added to this device set when it is of the same type, and remove every other channel earlier listen and scan calls added there, so that exploring a band does not leave a trail of demodulators all playing audio and mis-tuned for the current centre frequency. Channels added any other way are kept on their frequency where the baseband still holds them; audio demodulators it cannot hold are removed, anything else is left and named. Default true")},
            {"tuneGain", prop("boolean", "Measure and set the receiver gain, as tune_gain does, when the device set is new or the retune is to another band (more than a tenth of the frequency away). Adds several seconds in those cases and none otherwise. false leaves the gain as it is. Default true")}
        }, {"frequency"}),
        [this](const QJsonObject& args)
        {
            qint64 frequency = (qint64) argDouble(args, "frequency");
            QString modeName = argString(args, "mode", false, "nfm").trimmed();
            const ListenMode *mode = findListenMode(modeName);
            QString channelType;
            int rfBandwidth = 0;
            int minBaseband = 256000;
            QJsonObject initial;
            QString companionFeature;
            int pairOffset = 0;

            if (mode)
            {
                channelType = mode->m_channelType;
                rfBandwidth = mode->m_rfBandwidth;
                minBaseband = mode->m_minBaseband;
                initial = QJsonDocument::fromJson(mode->m_settings).object();
                companionFeature = mode->m_feature ? QString(mode->m_feature) : QString();
                pairOffset = mode->m_pairOffset;
            }
            else
            {
                for (const QString& id : channelTypeIds(0))
                {
                    if (id.compare(modeName, Qt::CaseInsensitive) == 0) {
                        channelType = id;
                    }
                }

                if (channelType.isEmpty()) {
                    throw MCPToolError(QString("Unknown mode %1. Use one of %2, or a channel type from list_channel_types").arg(modeName).arg(listenModeList));
                }
            }

            const bool replace = args.value("replace").toBool(true);
            const bool wantTuneGain = args.value("tuneGain").toBool(true);
            QJsonObject receiver = pickReceiver(args, minBaseband, replace ? intentChannels() : QSet<uint64_t>(), !wantTuneGain);
            int deviceSetIndex = receiver["deviceSetIndex"].toInt();
            int baseband = receiver["baseband"].toInt();

            // Keep the channel off the device's DC spike when the baseband has room for it: far
            // enough that the spike falls outside the channel, or not at all. A spike inside a
            // wide FM channel is an interferer 25 kHz from the carrier, which is worse than one
            // on the carrier itself
            int offset = 0;

            if ((pairOffset == 0) && (rfBandwidth != 0) && (baseband > 0))
            {
                const int clear = qAbs(rfBandwidth) / 2 + 25000; // spike 25 kHz beyond the channel edge

                if (baseband / 2 >= clear + qAbs(rfBandwidth) / 2) {
                    offset = -clear;
                }
            }

            // A paired mode, such as AIS on 161.975 and 162.025 MHz, centres the device on the
            // frequency asked for and puts a demodulator either side of it
            const qint64 centre = (pairOffset != 0) ? frequency : frequency - offset;

            if (pairOffset != 0) {
                offset = -pairOffset;
            }

            QJsonObject tune;
            tune["centerFrequency"] = (double) centre;
            patchDeviceSettings(deviceSetIndex, tune);
            const RetuneOutcome outcome = receiver["reused"].toBool()
                ? retuneOtherChannels(deviceSetIndex, receiver["previousCentre"].toDouble(), (double) centre, baseband)
                : RetuneOutcome();

            // A demodulator of the right type that a previous listen added is retuned rather
            // than replaced; anything else the intent tools added goes as before
            int channelIndex = -1;
            const void *channel = replace ? reusableIntentChannel(deviceSetIndex, channelType, channelIndex) : nullptr;
            const bool retuned = channel != nullptr;
            const QStringList reclaimed = replace ? reclaimIntentChannels(deviceSetIndex, channel) : QStringList();
            const QStringList reclaimedFeatures = replace ? reclaimIntentFeatures(companionFeature) : QStringList();

            if (retuned)
            {
                // The reclaim may have renumbered it
                relocateChannel(channel, deviceSetIndex, channelIndex, "demodulator");
            }
            else
            {
                channelIndex = addChannelAndWait(deviceSetIndex, channelType);
                channel = channelAt(deviceSetIndex, channelIndex);
                trackIntentChannel(channel);
            }

            initial["inputFrequencyOffset"] = offset;

            if (rfBandwidth != 0) {
                initial["rfBandwidth"] = rfBandwidth;
            }

            QJsonObject patched;
            QString state;

            // Do not leave a half configured channel behind if setting it up fails. One that was
            // there already stays, as the previous listen left it
            try
            {
                patched = patchChannelSettings(deviceSetIndex, channelIndex, initial);
                state = deviceState(deviceSetIndex, 0, 1)["state"].toString();
            }
            catch (const MCPToolError&)
            {
                if (!retuned)
                {
                    try { deleteChannelObjectAndWait(channel); } catch (const MCPToolError&) {}
                    untrackIntentChannel(channel);
                }

                throw;
            }

            QJsonObject result;
            result["deviceSetIndex"] = deviceSetIndex;
            result["channelIndex"] = channelIndex;
            result["device"] = receiver["device"];
            result["frequency"] = (double) frequency;
            result["mode"] = modeName;
            result["channelType"] = channelType;
            result["state"] = state;
            QStringList notes;

            if (retuned)
            {
                result["retuned"] = true;
                notes.append(QString("Retuned the %1 a previous listen added to device set %2 rather than adding another; its other settings are as they were.")
                    .arg(channelType).arg(deviceSetIndex));
            }

            // The second demodulator of a paired mode, on the other channel. A retuned first
            // one may already have its partner from the previous listen; a fresh one needs it
            const void *partner = nullptr;

            if (pairOffset != 0)
            {
                int partnerIndex = -1;

                for (const ChannelNote& note : channelNotes(deviceSetIndex, {channel}))
                {
                    if (note.m_intent && (note.m_id == channelType))
                    {
                        partnerIndex = note.m_index;
                        partner = note.m_channel;
                        break;
                    }
                }

                if (!partner)
                {
                    partnerIndex = addChannelAndWait(deviceSetIndex, channelType);
                    partner = channelAt(deviceSetIndex, partnerIndex);
                    trackIntentChannel(partner);
                }

                QJsonObject partnerSettings = initial;
                partnerSettings["inputFrequencyOffset"] = pairOffset;

                try
                {
                    patchChannelSettings(deviceSetIndex, partnerIndex, partnerSettings);
                }
                catch (const MCPToolError& e)
                {
                    notes.append(QString("The second %1 could not be configured: %2").arg(channelType).arg(e.message));
                }

                relocateChannel(partner, deviceSetIndex, partnerIndex, "second demodulator");
                relocateChannel(channel, deviceSetIndex, channelIndex, "demodulator");
                result["channelIndex"] = channelIndex;
                result["secondChannelIndex"] = partnerIndex;
                QJsonArray frequencies;
                frequencies.append((double) (frequency - pairOffset));
                frequencies.append((double) (frequency + pairOffset));
                result["frequencies"] = frequencies;
                notes.append(QString("%1 is received on two channels, so there are two %2: channel %3 on %4 MHz and channel %5 on %6 MHz, with the device centred between them.")
                    .arg(modeName.toUpper()).arg(channelType).arg(channelIndex).arg((frequency - pairOffset) / 1e6, 0, 'f', 3)
                    .arg(partnerIndex).arg((frequency + pairOffset) / 1e6, 0, 'f', 3));
            }

            // The feature the mode's output goes to, so that the decoded data has somewhere to be
            // read from and shown
            if (!companionFeature.isEmpty())
            {
                try
                {
                    int featureIndex = -1;
                    const int added = ensureFeature(companionFeature, featureIndex);
                    result["featureIndex"] = featureIndex;
                    result["featureType"] = companionFeature;
                    notes.append(added
                        ? QString("Added the %1 feature (F%2), which collects what the demodulator decodes; get_feature_report reads it, and it feeds the map.").arg(companionFeature).arg(featureIndex)
                        : QString("The %1 feature (F%2) is already there and collects what the demodulator decodes.").arg(companionFeature).arg(featureIndex));
                }
                catch (const MCPToolError& e)
                {
                    notes.append(QString("The %1 feature could not be added: %2").arg(companionFeature).arg(e.message));
                }
            }

            if (state == "running")
            {
                // A new device set, or one moved to another band, has no gain to speak of yet
                if (wantTuneGain && gainWorthTuning(receiver["reused"].toBool(), receiver["previousCentre"].toDouble(), (double) centre))
                {
                    QJsonObject gainArgs;
                    gainArgs["deviceSetIndex"] = deviceSetIndex;
                    gainArgs["frequency"] = (double) frequency;
                    gainArgs["bandwidth"] = (double) (rfBandwidth != 0 ? qAbs(rfBandwidth) : 2000000);
                    gainArgs["steps"] = 6;

                    try
                    {
                        QJsonObject tuned = tuneGain(gainArgs);
                        QJsonObject gain;
                        gain["key"] = tuned["gainKey"];
                        gain["value"] = tuned["chosen"];
                        gain["unit"] = tuned["unit"];
                        gain["snrDb"] = tuned["bestSnrDb"];
                        result["gain"] = gain;
                        notes.append(QString("Gain measured and set to %1 (%2), judged by %3, as %4; tune_gain shows the table, and tuneGain false leaves the gain alone.")
                            .arg(tuned["chosen"].toDouble()).arg(tuned["unit"].toString()).arg(tuned["judgedBy"].toString())
                            .arg(receiver["reused"].toBool() ? "the retune was to another band" : "the device set is new"));
                    }
                    catch (const MCPToolError& e)
                    {
                        notes.append(QString("The gain was not measured: %1").arg(e.message));
                    }
                }

                QThread::msleep(1500);
                // The channel may have been renumbered while we waited
                relocateChannel(channel, deviceSetIndex, channelIndex, "demodulator");
                result["deviceSetIndex"] = deviceSetIndex;
                result["channelIndex"] = channelIndex;

                // The power measurement sits at the channel's floor until the demodulator has run
                // for a moment, and a floor reading looks exactly like an empty band. Wait for a
                // real one: reporting the floor has sent callers away from a signal that was
                // decoding perfectly well. CalcDb::dbPower returns 10*log10(floor) while there is
                // nothing to measure, which is -100 dB or below for every floor in use here
                double db = 0.0;
                bool measured = false;
                bool noReport = false;
                QElapsedTimer settling;
                settling.start();

                while (true)
                {
                    QJsonObject report;

                    // A demodulator with no report at all (APT, ATV) is not a failure of the
                    // receiver that was just built
                    try
                    {
                        report = channelReport(deviceSetIndex, channelIndex);
                    }
                    catch (const MCPToolError&)
                    {
                        noReport = true;
                        break;
                    }

                    if (signalLevel(report, db))
                    {
                        measured = true;

                        if (db > -100.0) {
                            break;
                        }
                    }

                    if (settling.elapsed() > 4000) {
                        break;
                    }

                    QThread::msleep(250);
                }

                if (noReport)
                {
                    notes.append(QString("%1 has no channel report, so there is no signal level to give; %2.")
                        .arg(channelType)
                        .arg(companionFeature.isEmpty() ? QString("get_spectrum_data shows whether there is a signal")
                            : QString("what it decodes appears in the %1 feature's report and in get_packets").arg(companionFeature)));
                }
                else if (measured)
                {
                    result["signalDb"] = db;

                    if (db <= -100.0) {
                        notes.append("signalDb is still at the measurement floor, which usually means the channel "
                            "has not settled rather than that the band is empty. Read get_channel_report again in "
                            "a few seconds before concluding nothing is there.");
                    }
                }
            }
            else
            {
                notes.append("The device did not start. If another device set holds the same hardware, stop it first, or pass a different device.");
            }

            if (baseband > 0) {
                result["basebandSampleRate"] = baseband;
            } else {
                notes.append(QString("The sample rate of a %1 was left as configured; check it exceeds %2 S/s with get_device_settings.").arg(receiver["hwType"].toString()).arg(minBaseband));
            }

            if (!receiver["rateNote"].toString().isEmpty()) {
                notes.append(receiver["rateNote"].toString());
            }

            notes.append(intentNotes(deviceSetIndex, partner ? QSet<const void *>({channel, partner}) : QSet<const void *>({channel}),
                reclaimed, receiver["reused"].toBool(),
                receiver["previousCentre"].toDouble(), (double) centre, outcome));
            notes.append(featureNotes(reclaimedFeatures));

            if (patched.contains("warning")) {
                notes.append(patched["warning"].toString());
            }

            if (!notes.isEmpty()) {
                result["note"] = notes.join(" ");
            }

            return result;
        });

    QJsonObject frequencyList = prop("array", "Frequencies to scan in Hz. Alternatively give startFrequency, stopFrequency and stepFrequency");
    QJsonObject items;
    items["type"] = "number";
    frequencyList["items"] = items;

    add("scan",
        "Scan a set of frequencies for activity and listen to whichever is active, in one call: sets up a receiver, a demodulator "
        "and a Frequency Scanner, starts scanning, watches for a while and reports what it found. active lists every frequency "
        "above the threshold with its level, strongest first; that is the list to use for stations and other continuous "
        "signals. heard lists where the scanner parked and for how long, which only tells intermittent signals apart, since "
        "on a continuous carrier it parks on the first and stays. "
        "The scanner keeps running afterwards; get_channel_report on the scanner channel shows its state (2 scanning, 3 receiving, "
        "4 holding), channel_action {\"run\": 0} stops it. The demodulator and scanner it added stay on the device set until the "
        "next listen or scan there replaces them, or cleanup removes them; they are not removed when this call returns. "
        "Blocks for the watch period, so it is capped at 30 seconds.",
        schema({
            {"frequencies", frequencyList},
            {"startFrequency", numProp("Start of a range in Hz, with stopFrequency and stepFrequency")},
            {"stopFrequency", numProp("End of the range in Hz, inclusive")},
            {"stepFrequency", numProp("Channel spacing in Hz. Optional: without it the spacing usual for the mode is used (bfm 100 kHz, am 25 kHz, "
                                      "nfm/dsd/pager 12.5 kHz, sonde 10 kHz, ssb 3 kHz), which is the right choice unless the band is known to differ, "
                                      "e.g. 8333 for airband 8.33 kHz channels in Europe. Broadcast FM stations sit on a 100 kHz raster everywhere; only "
                                      "the Americas confine them to the odd tenths, so bfm/wfm ranges are scanned at 100 kHz whatever step is given")},
            {"mode", strProp("Demodulator, as for listen: bfm, wfm, nfm, am, ssb/usb/lsb, dsd, pager, sonde. Modes on fixed frequencies (adsb, ais, "
                             "dab, dsc) cannot be scanned for; listen to those directly. Default am")},
            {"threshold", numProp("Power in dB above which a frequency counts as active. Leave it out unless you have a reason: the scale "
                                  "depends on the device and its gain, so by default the noise floor is measured first and the threshold set "
                                  "12 dB above it. Both are reported, so a second scan can adjust")},
            {"seconds", bounded(numProp("How long to watch before replying, 3 to 30. Default 10"), 3, 30)},
            {"device", strProp(deviceHint)},
            {"replace", prop("boolean", "Remove every channel earlier listen and scan calls added to this device set first, as listen does. Channels added any other way are kept on their frequency where the baseband still holds them; audio demodulators it cannot hold are removed, anything else is left and named. Default true")},
            {"tuneGain", prop("boolean", "Measure and set the receiver gain first, as tune_gain does, when the device set is new or the retune is to another band. Adds several seconds in those cases and none otherwise. false leaves the gain as it is. Default true")}
        }),
        [this](const QJsonObject& args)
        {
            QList<qint64> frequencies;
            const bool fromRange = !hasArg(args, "frequencies");
            qint64 rangeStart = 0;
            qint64 rangeStop = 0;
            qint64 rangeStep = 0; // as given; zero to step by the mode's usual channel spacing

            if (fromRange)
            {
                rangeStart = (qint64) argDouble(args, "startFrequency");
                rangeStop = (qint64) argDouble(args, "stopFrequency");
                rangeStep = hasArg(args, "stepFrequency") ? (qint64) argDouble(args, "stepFrequency") : 0;

                if ((rangeStep < 0) || (rangeStop < rangeStart)) {
                    throw MCPToolError("stepFrequency must be positive and stopFrequency at least startFrequency");
                }
            }
            else
            {
                for (const QJsonValue& v : args["frequencies"].toArray()) {
                    frequencies.append((qint64) v.toDouble());
                }

                if (frequencies.isEmpty()) {
                    throw MCPToolError("Give frequencies, or startFrequency and stopFrequency");
                }
            }

            QString modeName = argString(args, "mode", false, "am").trimmed();
            const ListenMode *mode = findListenMode(modeName);

            if (!mode) {
                throw MCPToolError(QString("Unknown mode %1. Use one of %2").arg(modeName).arg(listenModeList));
            }

            // A mode on fixed frequencies has nothing to be found by stepping
            if (mode->m_scanStep == 0)
            {
                throw MCPToolError(QString("%1 is received on fixed frequencies, so there is nothing to scan for: listen to them directly. "
                    "get_receiving_guide lists them (ADS-B 1090 MHz; AIS 161.975 and 162.025 MHz; DSC 2187.5 kHz, 8414.5 kHz, "
                    "16804.5 kHz and 156.525 MHz; DAB on the Band III blocks, one dab channel per block).").arg(mode->m_channelType));
            }

            // Without a threshold, measure the noise floor first and sit 12 dB above it: the
            // level scale depends on the device and its gain, so no fixed number suits all
            bool autoThreshold = !hasArg(args, "threshold");
            double threshold = autoThreshold ? -100.0 : argDouble(args, "threshold");
            int seconds = qBound(3, (int) argDouble(args, "seconds", false, 10.0), 30);
            int channelBandwidth = qAbs(mode->m_rfBandwidth) > 0 ? qAbs(mode->m_rfBandwidth) : 8000;

            // Broadcast FM stations sit on a 100 kHz raster everywhere but the Americas, where
            // they keep to the odd tenths. A 200 kHz step from 87.5 MHz, the usual assumption,
            // lands beside every even-tenth station: with a channel this wide the station still
            // shows up, 100 kHz off and weaker, and then nothing decodes. So a coarser step on
            // a broadcast range is scanned at 100 kHz, which loses nothing where the odd tenths
            // are the rule
            const bool broadcast = (mode->m_channelType == "BFMDemod") || (mode->m_channelType == "WFMDemod");
            QString rasterNote;

            if (fromRange)
            {
                qint64 step = (rangeStep > 0) ? rangeStep : mode->m_scanStep;

                if (broadcast && (step > 100000))
                {
                    step = 100000;
                    rasterNote = QString("Scanned at 100 kHz rather than the %1 kHz step given: broadcast FM stations are on a 100 kHz raster "
                        "outside the Americas, and a coarser step lands beside them rather than on them.").arg(rangeStep / 1000.0, 0, 'g', 4);
                }
                else if (rangeStep > mode->m_scanStep)
                {
                    rasterNote = QString("The %1 kHz step given is coarser than the %2 kHz channel spacing usual for %3, so channels between the steps were not seen.")
                        .arg(rangeStep / 1000.0, 0, 'g', 4).arg(mode->m_scanStep / 1000.0, 0, 'g', 4).arg(modeName);
                }
                else if (rangeStep == 0)
                {
                    rasterNote = QString("Stepped by %1 kHz, the channel spacing usual for %2; give stepFrequency for another.")
                        .arg(step / 1000.0, 0, 'g', 4).arg(modeName);
                }

                for (qint64 f = rangeStart; f <= rangeStop; f += step) {
                    frequencies.append(f);
                }
            }

            if (frequencies.size() > 2000) {
                throw MCPToolError(QString("%1 frequencies is too many; use a coarser step or a narrower range (2000 at most)").arg(frequencies.size()));
            }

            std::sort(frequencies.begin(), frequencies.end());

            // A wide baseband means fewer retunes per sweep
            const bool replace = args.value("replace").toBool(true);
            const bool wantTuneGain = args.value("tuneGain").toBool(true);
            QJsonObject receiver = pickReceiver(args, 2400000, replace ? intentChannels() : QSet<uint64_t>(), !wantTuneGain);
            int deviceSetIndex = receiver["deviceSetIndex"].toInt();
            QJsonObject tune;
            tune["centerFrequency"] = (double) ((frequencies.first() + frequencies.last()) / 2);
            patchDeviceSettings(deviceSetIndex, tune);
            const RetuneOutcome outcome = receiver["reused"].toBool()
                ? retuneOtherChannels(deviceSetIndex, receiver["previousCentre"].toDouble(), tune["centerFrequency"].toDouble(), receiver["baseband"].toInt())
                : RetuneOutcome();

            const QStringList reclaimed = replace ? reclaimIntentChannels(deviceSetIndex) : QStringList();
            const QStringList reclaimedFeatures = replace ? reclaimIntentFeatures() : QStringList();
            int demod = addChannelAndWait(deviceSetIndex, mode->m_channelType);
            const void *demodChannel = channelAt(deviceSetIndex, demod);
            trackIntentChannel(demodChannel);
            int scanner = -1;
            const void *scannerChannel = nullptr;

            // Anything that fails from here on removes both channels rather than leaving a
            // half built scanner running on the user's radio
            auto discard = [&]()
            {
                if (scannerChannel)
                {
                    try { deleteChannelObjectAndWait(scannerChannel); } catch (const MCPToolError&) {}
                    untrackIntentChannel(scannerChannel);
                }

                try { deleteChannelObjectAndWait(demodChannel); } catch (const MCPToolError&) {}
                untrackIntentChannel(demodChannel);
            };

            try
            {
                QJsonObject initial = QJsonDocument::fromJson(mode->m_settings).object();
                initial["inputFrequencyOffset"] = 0;

                if (mode->m_rfBandwidth != 0) {
                    initial["rfBandwidth"] = mode->m_rfBandwidth;
                }

                patchChannelSettings(deviceSetIndex, demod, initial);
                scanner = addChannelAndWait(deviceSetIndex, "FreqScanner");
                scannerChannel = channelAt(deviceSetIndex, scanner);
                trackIntentChannel(scannerChannel);
            }
            catch (const MCPToolError&)
            {
                discard();
                throw;
            }

            QString state = deviceState(deviceSetIndex, 0, 1)["state"].toString();

            if (state != "running")
            {
                discard();
                throw MCPToolError(QString("The device did not start (state %1), so the demodulator and scanner were removed again.").arg(state));
            }

            // Before the scanner measures anything: the gain sets the floor it measures against
            QString gainNote;

            if (wantTuneGain && gainWorthTuning(receiver["reused"].toBool(), receiver["previousCentre"].toDouble(), tune["centerFrequency"].toDouble()))
            {
                QJsonObject gainArgs;
                gainArgs["deviceSetIndex"] = deviceSetIndex;
                gainArgs["steps"] = 6;

                try
                {
                    QJsonObject tuned = tuneGain(gainArgs);
                    gainNote = QString("Gain measured and set to %1 (%2) first, as %3; tuneGain false leaves it alone.")
                        .arg(tuned["chosen"].toDouble()).arg(tuned["unit"].toString())
                        .arg(receiver["reused"].toBool() ? "the retune was to another band" : "the device set is new");
                }
                catch (const MCPToolError& e)
                {
                    gainNote = QString("The gain was not measured: %1").arg(e.message);
                }
            }

            // The scanner's baseband only takes settings while it is running, so configure it
            // after the device has started
            QThread::msleep(1500);
            relocateChannel(demodChannel, deviceSetIndex, demod, "demodulator");
            relocateChannel(scannerChannel, deviceSetIndex, scanner, "scanner");
            QJsonArray list;

            for (qint64 f : frequencies)
            {
                QJsonObject entry;
                entry["frequency"] = (double) f;
                entry["enabled"] = 1;
                list.append(entry);
            }

            QJsonObject scannerSettings;
            scannerSettings["channel"] = QString("R%1:%2").arg(deviceSetIndex).arg(demod);
            scannerSettings["channelBandwidth"] = channelBandwidth;
            scannerSettings["channelFrequencyOffset"] = 25000;
            scannerSettings["threshold"] = threshold;
            scannerSettings["scanTime"] = 0.1;
            scannerSettings["tuneTime"] = 250;
            scannerSettings["retransmitTime"] = 2.5;
            scannerSettings["priority"] = 0;
            scannerSettings["measurement"] = 0;
            scannerSettings["mode"] = autoThreshold ? 2 : 1; // scan only while measuring, else continuous
            scannerSettings["frequencies"] = list;
            patchChannelSettings(deviceSetIndex, scanner, scannerSettings);
            QJsonObject run;
            run["run"] = 1;
            postChannelAction(deviceSetIndex, scanner, run);
            double measuredFloor = 0.0;
            bool haveFloor = false;

            if (autoThreshold)
            {
                // The power scale depends on the device and its gain, so the only threshold that
                // is right everywhere is one measured here: the median over the frequencies is
                // the floor, as most of any band is empty. Wait for the first sweep to have
                // measured every frequency rather than guess after a fixed time; a long list
                // at a low sample rate takes a while
                QList<double> powers;

                for (int attempt = 0; (attempt < 30) && (powers.size() < frequencies.size()); attempt++)
                {
                    QThread::msleep(500);
                    relocateChannel(scannerChannel, deviceSetIndex, scanner, "scanner");
                    powers.clear();

                    for (const QJsonValue& v : channelReport(deviceSetIndex, scanner)["FreqScannerReport"].toObject()["channelState"].toArray())
                    {
                        QJsonObject s = v.toObject();

                        if (s.contains("power")) {
                            powers.append(s["power"].toDouble());
                        }
                    }
                }

                if (powers.isEmpty())
                {
                    discard();
                    throw MCPToolError("The scanner measured nothing in 15 seconds, so no threshold could be set. "
                        "Check the device is running and producing samples, or give threshold explicitly.");
                }

                std::sort(powers.begin(), powers.end());
                measuredFloor = powers[powers.size() / 2];
                haveFloor = true;
                threshold = measuredFloor + 12.0;

                // Changing the mode restarts the scan with the new threshold
                QJsonObject arm;
                arm["threshold"] = threshold;
                arm["mode"] = 1;
                patchChannelSettings(deviceSetIndex, scanner, arm);
            }

            // Watch: where it parks, and how strong each channel got
            QMap<qint64, int> heard;
            QMap<qint64, double> strongest;

            for (int i = 0; i < seconds; i++)
            {
                QThread::msleep(1000);
                relocateChannel(scannerChannel, deviceSetIndex, scanner, "scanner");
                relocateChannel(demodChannel, deviceSetIndex, demod, "demodulator");
                QJsonObject report = channelReport(deviceSetIndex, scanner)["FreqScannerReport"].toObject();
                int scanState = report["scanState"].toInt();

                for (const QJsonValue& v : report["channelState"].toArray())
                {
                    QJsonObject s = v.toObject();
                    qint64 f = (qint64) s["frequency"].toDouble();
                    strongest[f] = qMax(strongest.value(f, -999.0), s["power"].toDouble());
                }

                if ((scanState == 3) || (scanState == 4))
                {
                    // One snapshot for both halves of the sum: read separately, a retune
                    // between them would pair a new centre with an old offset
                    QJsonObject set = getDeviceSet(deviceSetIndex);
                    qint64 centre = (qint64) set["samplingDevice"].toObject()["centerFrequency"].toDouble();

                    for (const QJsonValue& v : set["channels"].toArray())
                    {
                        QJsonObject ch = v.toObject();

                        if (ch["index"].toInt() == demod) {
                            heard[centre + (qint64) ch["deltaFrequency"].toDouble()] += 1;
                        }
                    }
                }
            }

            QJsonObject result;
            result["deviceSetIndex"] = deviceSetIndex;
            result["demodChannelIndex"] = demod;
            result["scannerChannelIndex"] = scanner;
            result["device"] = receiver["device"];
            result["frequencies"] = frequencies.size();
            result["threshold"] = threshold;
            result["secondsWatched"] = seconds;

            // A channel this wide takes in a station on the neighbouring raster steps as well,
            // weaker and off frequency, so of the neighbours within a channel of each other
            // only the strongest is a station; the rest are its skirts
            auto isLocalPeak = [&](qint64 f)
            {
                const double power = strongest.value(f, -999.0);

                for (auto it = strongest.lowerBound(f - channelBandwidth); (it != strongest.end()) && (it.key() <= f + channelBandwidth); ++it)
                {
                    if ((it.key() != f) && ((it.value() > power) || ((it.value() == power) && (it.key() < f)))) {
                        return false;
                    }
                }

                return true;
            };

            QList<QPair<int, qint64>> byTime;
            for (auto it = heard.begin(); it != heard.end(); ++it) { if (isLocalPeak(it.key())) { byTime.append(qMakePair(it.value(), it.key())); } }
            std::sort(byTime.begin(), byTime.end(), [](const QPair<int, qint64>& a, const QPair<int, qint64>& b) { return a.first > b.first; });
            QJsonArray heardArray;

            for (const auto& p : byTime)
            {
                QJsonObject h;
                h["frequency"] = (double) p.second;
                h["seconds"] = p.first;
                heardArray.append(h);
            }

            result["heard"] = heardArray;

            QList<QPair<double, qint64>> byPower;
            QList<double> allPowers; // every frequency, for the floor estimate
            for (auto it = strongest.begin(); it != strongest.end(); ++it) { allPowers.append(it.value()); if (isLocalPeak(it.key())) { byPower.append(qMakePair(it.value(), it.key())); } }
            std::sort(byPower.begin(), byPower.end(), [](const QPair<double, qint64>& a, const QPair<double, qint64>& b) { return a.first > b.first; });
            std::sort(allPowers.begin(), allPowers.end());
            // Everything above the threshold, strongest first: for a band of continuous carriers
            // this is the answer, as the scanner parks on the first one it finds and never gets
            // to the rest. Capped well above any real band's station count, so that a threshold
            // set too low on a long list cannot flood the reply
            QJsonArray activeArray;
            QJsonArray strongestArray;
            static const int activeCap = 100;

            for (int i = 0; i < byPower.size(); i++)
            {
                QJsonObject s;
                s["frequency"] = (double) byPower[i].second;
                s["dB"] = byPower[i].first;

                if (i < 5) {
                    strongestArray.append(s);
                }

                if ((byPower[i].first >= threshold) && (activeArray.size() < activeCap)) {
                    activeArray.append(s);
                }
            }

            result["active"] = activeArray;
            result["activeCount"] = activeArray.size();
            result["strongest"] = strongestArray;

            if (haveFloor) {
                result["noiseFloorDb"] = measuredFloor;
            } else if (!allPowers.isEmpty()) {
                result["noiseFloorDb"] = allPowers[allPowers.size() / 2];
            }

            if (autoThreshold) {
                result["thresholdAuto"] = true;
            }

            QStringList notes;

            if (!rasterNote.isEmpty()) {
                notes.append(rasterNote);
            }

            if (!gainNote.isEmpty()) {
                notes.append(gainNote);
            }

            if (!receiver["rateNote"].toString().isEmpty()) {
                notes.append(receiver["rateNote"].toString());
            }

            if (activeArray.isEmpty()) {
                notes.append("Nothing exceeded the threshold. Compare threshold with noiseFloorDb and the strongest channels; a second scan with threshold given lowers it.");
            } else if (heardArray.isEmpty()) {
                notes.append("The scanner did not park on any of the active frequencies while watching; active is still the list of what was found.");
            }

            notes.append(intentNotes(deviceSetIndex, {demodChannel, scannerChannel}, reclaimed, receiver["reused"].toBool(),
                receiver["previousCentre"].toDouble(), tune["centerFrequency"].toDouble(), outcome));
            notes.append(featureNotes(reclaimedFeatures));

            if (!notes.isEmpty()) {
                result["note"] = notes.join(" ");
            }

            return result;
        });

    add("cleanup",
        "Remove the channels that listen and scan added, on one device set or all of them, and the features listen added "
        "for them (AIS, Radiosonde, APRS) once nothing feeds them. Each is otherwise left running, every demodulator playing "
        "audio, until the next listen or scan on the same device set replaces it. Channels and features added any other way "
        "are left alone; delete_channel and delete_feature remove those.",
        schema({
            {"deviceSetIndex", intProp("Only this device set. Default: every device set")}
        }),
        [this](const QJsonObject& args)
        {
            QList<int> deviceSetIndices;

            if (hasArg(args, "deviceSetIndex"))
            {
                deviceSetIndices.append(argInt(args, "deviceSetIndex"));
            }
            else
            {
                for (const QJsonValue& v : getInstanceSummary()["devicesetlist"].toObject()["deviceSets"].toArray()) {
                    deviceSetIndices.append(v.toObject()["samplingDevice"].toObject()["index"].toInt());
                }
            }

            QJsonArray removed;
            int count = 0;

            for (int deviceSetIndex : deviceSetIndices)
            {
                for (const QString& id : reclaimIntentChannels(deviceSetIndex))
                {
                    removed.append(QString("R%1:%2").arg(deviceSetIndex).arg(id));
                    count++;
                }
            }

            QJsonArray features;

            for (const QString& id : reclaimIntentFeatures())
            {
                features.append(id);
                count++;
            }

            QJsonObject result;
            result["removed"] = count;
            result["channels"] = removed;
            result["features"] = features;

            if (count == 0) {
                result["note"] = "Nothing to remove: no channel or feature added by listen or scan is still there.";
            }

            return result;
        });

    add("tune_gain",
        "Find the receiver gain that gives the best signal to noise ratio and apply it: steps the device through its gain range "
        "and at each step measures the noise floor and the strongest signal on the main spectrum. Too little gain leaves weak "
        "signals under the converter's own noise; too much overloads the front end, which raises the floor faster than the "
        "signal and adds spurious products, so the best gain is where the difference between them peaks, and of gains within a "
        "decibel of that the lowest is taken, for headroom. On a quiet band, where no signal stands clear of the floor, the "
        "floor alone decides: the lowest gain at which the antenna's noise rather than the converter's sets it, short of "
        "overload. Turns the device's AGC off, as it has to for a manual gain to mean "
        "anything. The device must be running. Blocks for a few seconds per step; the reply carries the whole table so the "
        "choice can be judged. Give frequency to judge by a particular signal rather than the strongest one.",
        schema({
            {"deviceSetIndex", deviceSetIndexProp},
            {"frequency", numProp("Judge by the signal at this frequency in Hz rather than the strongest in the baseband. It must be inside the baseband")},
            {"bandwidth", numProp("Width in Hz around frequency to look for the signal in. Default 200000")},
            {"steps", bounded(intProp("How many gain values to try, spread over the range. Default 8"), 3, 16)},
            {"apply", prop("boolean", "Apply the chosen gain. false measures and reports but puts the gain back as it was. Default true")},
            {"gainKey", strProp("For a device type this does not know: the settings key of its gain, from describe_settings. Then give values too")},
            {"values", prop("array", "For a device type this does not know: the gain values to try, in the key's own units, lowest first")},
            {"agcKey", strProp("For a device type this does not know: the settings key that turns its AGC off when set to 0")}
        }, {"deviceSetIndex"}),
        [this](const QJsonObject& args) { return tuneGain(args); });

    add("get_status",
        "One line per device set and feature: device, frequency, run state and channels. The cheapest way to see what is going on.",
        schema(QJsonObject()),
        [this](const QJsonObject&)
        {
            QJsonObject summary = getInstanceSummary();
            QStringList lines;

            for (const QJsonValue& v : summary["devicesetlist"].toObject()["deviceSets"].toArray())
            {
                QJsonObject ds = v.toObject();
                QJsonObject sd = ds["samplingDevice"].toObject();
                QStringList channels;

                for (const QJsonValue& c : ds["channels"].toArray())
                {
                    QJsonObject ch = c.toObject();
                    channels.append(QString("%1 %2%3k").arg(ch["id"].toString())
                        .arg(ch["deltaFrequency"].toDouble() >= 0 ? "+" : "").arg(ch["deltaFrequency"].toDouble() / 1000.0, 0, 'f', 1));
                }

                static const char *const prefixes[] = {"R", "T", "M"};
                lines.append(QString("%1%2 %3 %4 %5 MHz %6%7")
                    .arg(prefixes[qBound(0, sd["direction"].toInt(), 2)]).arg(sd["index"].toInt())
                    .arg(sd["hwType"].toString()).arg(sd["serial"].toString())
                    .arg(sd["centerFrequency"].toDouble() / 1e6, 0, 'f', 3).arg(sd["state"].toString())
                    .arg(channels.isEmpty() ? QString() : ": " + channels.join(", ")));
            }

            for (const QJsonValue& v : summary["featureset"].toObject()["features"].toArray())
            {
                QJsonObject f = v.toObject();
                int index = f["index"].toInt();
                lines.append(QString("F%1 %2 %3").arg(index).arg(f["id"].toString()).arg(featureState(index, -1)["state"].toString()));
            }

            return QJsonValue(lines.isEmpty() ? QString("Nothing configured") : lines.join("\n"));
        });
}
