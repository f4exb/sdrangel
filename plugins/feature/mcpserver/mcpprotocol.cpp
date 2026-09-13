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

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QCoreApplication>

#include "mcpstreams.h"
#include "mcpprotocol.h"

const char* const MCPProtocol::m_serverName = "SDRangel";
const char* const MCPProtocol::m_latestProtocolVersion = "2025-06-18";

MCPProtocol::MCPProtocol(WebAPIAdapterInterface *webAPIAdapterInterface) :
    m_webAPIAdapterInterface(webAPIAdapterInterface),
    m_tools(webAPIAdapterInterface),
    m_streams(nullptr)
{
}

QJsonObject MCPProtocol::makeResult(const QJsonValue& id, const QJsonValue& result)
{
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = result;
    return response;
}

QJsonObject MCPProtocol::makeError(const QJsonValue& id, int code, const QString& message)
{
    QJsonObject error;
    error["code"] = code;
    error["message"] = message;
    QJsonObject response;
    response["jsonrpc"] = "2.0";
    response["id"] = id.isUndefined() ? QJsonValue() : id;
    response["error"] = error;
    return response;
}

// JSON-RPC 2.0 rules: a message with an "id" member is a request and always gets a reply
// carrying that id, even if the id is null (MCP forbids null ids but a client that sends one
// must still learn what happened). A message with a method and no id is a notification and
// gets no reply. Anything that is neither a valid request, notification nor response is
// answered with an Invalid Request error whose id is null when it cannot be identified.
bool MCPProtocol::handleMessage(const QJsonObject& message, QJsonObject& response, Context& context)
{
    bool hasId = message.contains("id");
    QJsonValue id = hasId ? message["id"] : QJsonValue();
    bool hasMethod = message["method"].isString() && !message["method"].toString().isEmpty();

    if (!hasMethod)
    {
        // A response to a server initiated request. We never send any, so there is nothing to do.
        if (hasId && (message.contains("result") || message.contains("error"))) {
            return false;
        }

        response = makeError(id, InvalidRequest, "Invalid request: missing method");
        return true;
    }

    if (message["jsonrpc"].toString() != "2.0")
    {
        response = makeError(id, InvalidRequest, "Invalid request: jsonrpc must be \"2.0\"");
        return true;
    }

    QString method = message["method"].toString();
    QJsonObject params = message["params"].toObject();

    try
    {
        // Calls are serialized so that tool handlers never run concurrently, except for the
        // capture tools: those deliberately block for up to 30 seconds, and holding the lock
        // across one would stall every other request, including ping and the status calls a
        // client needs while a capture runs. They protect their own state instead.
        QJsonValue result;

        if (isLongRunning(method, params))
        {
            result = dispatch(method, params, context);
        }
        else
        {
            QMutexLocker locker(&m_mutex);
            result = dispatch(method, params, context);
        }

        if (!hasId) {
            return false; // notification: no reply
        }

        response = makeResult(id, result);
        return true;
    }
    catch (const MCPError& e)
    {
        qDebug() << "MCPProtocol::handleMessage:" << method << "error:" << e.code << e.message;

        if (!hasId) {
            return false;
        }

        response = makeError(id, e.code, e.message);
        return true;
    }
    catch (const std::exception& e)
    {
        qWarning() << "MCPProtocol::handleMessage:" << method << "exception:" << e.what();

        if (!hasId) {
            return false;
        }

        response = makeError(id, InternalError, QString("Internal error: %1").arg(e.what()));
        return true;
    }
}

// The capture tools sleep for the requested duration, so they run without the dispatch lock.
// MCPCapture and MCPAudioCapture each serialize their own work, and everything they call
// through the Web API adapter is safe to use from several threads.
bool MCPProtocol::isLongRunning(const QString& method, const QJsonObject& params)
{
    if (method != "tools/call") {
        return false;
    }

    static const QStringList blocking = {"record_iq", "capture_audio", "listen", "scan"};
    return blocking.contains(params["name"].toString());
}

QJsonValue MCPProtocol::dispatch(const QString& method, const QJsonObject& params, Context& context)
{
    if (method == "initialize") {
        return initialize(params, context);
    } else if (method == "ping") {
        return QJsonObject();
    } else if (method.startsWith("notifications/")) {
        return QJsonValue(); // initialized, cancelled, progress, roots/list_changed: nothing to do
    } else if (method == "tools/list") {
        return toolsList(params);
    } else if (method == "tools/call") {
        return toolsCall(params);
    } else if (method == "resources/list") {
        return resourcesList(params);
    } else if (method == "resources/templates/list") {
        return resourcesTemplatesList(params);
    } else if (method == "resources/read") {
        return resourcesRead(params);
    } else if (method == "resources/subscribe") {
        return resourcesSubscribe(params, context, true);
    } else if (method == "resources/unsubscribe") {
        return resourcesSubscribe(params, context, false);
    } else if (method == "prompts/list") {
        return promptsList(params);
    } else if (method == "prompts/get") {
        return promptsGet(params);
    } else if (method == "logging/setLevel") {
        return QJsonObject();
    } else if (method == "completion/complete") {
        QJsonObject completion;
        completion["values"] = QJsonArray();
        completion["hasMore"] = false;
        QJsonObject result;
        result["completion"] = completion;
        return result;
    } else {
        throw MCPError(MethodNotFound, QString("Method not found: %1").arg(method));
    }
}

// Protocol revisions this server implements, newest first. 2024-11-05 is deliberately absent:
// it specifies the two endpoint HTTP+SSE transport, with a separate message endpoint announced
// by an initial endpoint event, and this server only implements Streamable HTTP.
QStringList MCPProtocol::supportedVersions()
{
    return QStringList{"2025-06-18", "2025-03-26"};
}

bool MCPProtocol::isSupportedVersion(const QString& version)
{
    return supportedVersions().contains(version);
}

QJsonValue MCPProtocol::initialize(const QJsonObject& params, Context& context)
{
    // Echo the client's version if we implement it, otherwise offer the newest we have
    QString requested = params["protocolVersion"].toString();
    QString version = isSupportedVersion(requested) ? requested : QString(m_latestProtocolVersion);

    QJsonObject clientInfo = params["clientInfo"].toObject();
    qInfo("MCPProtocol::initialize: client %s %s protocol %s",
        qPrintable(clientInfo["name"].toString()),
        qPrintable(clientInfo["version"].toString()),
        qPrintable(requested));

    QJsonObject tools;
    tools["listChanged"] = false;
    QJsonObject resources;
    resources["subscribe"] = true;
    resources["listChanged"] = false;
    QJsonObject prompts;
    prompts["listChanged"] = false;
    QJsonObject capabilities;
    capabilities["tools"] = tools;
    capabilities["resources"] = resources;
    capabilities["prompts"] = prompts;

    QJsonObject serverInfo;
    serverInfo["name"] = m_serverName;
    serverInfo["title"] = "SDRangel SDR";
    serverInfo["version"] = qApp->applicationVersion();

    // A session lets subscriptions and the event stream of this client be told apart from
    // those of any other. Clients that ignore the session id share an anonymous one.
    if (m_streams) {
        context.m_newSessionId = m_streams->createSession();
    }

    QJsonObject result;
    result["protocolVersion"] = version;
    result["capabilities"] = capabilities;
    result["serverInfo"] = serverInfo;
    result["instructions"] = instructions();
    return result;
}

// resources/subscribe and resources/unsubscribe. Updates are delivered as
// notifications/resources/updated on the SSE stream opened with GET.
QJsonValue MCPProtocol::resourcesSubscribe(const QJsonObject& params, Context& context, bool subscribe)
{
    QString uri = params["uri"].toString();

    if (uri.isEmpty()) {
        throw MCPError(InvalidParams, "Invalid params: missing uri");
    }

    if (!m_streams) {
        throw MCPError(InternalError, "Subscriptions are not available");
    }

    // Only the resources that change are worth subscribing to
    static const QStringList subscribable = {
        "sdrangel://instance", "sdrangel://packets", "sdrangel://map/items"
    };

    if (subscribe && !subscribable.contains(uri) && !uri.startsWith("sdrangel://deviceset/"))
    {
        throw MCPError(InvalidParams, QString("Resource %1 does not change, so it cannot be subscribed to. "
            "Subscribable resources are %2 and sdrangel://deviceset/{index}").arg(uri).arg(subscribable.join(", ")));
    }

    if (subscribe) {
        m_streams->subscribe(context.m_sessionId, uri);
    } else {
        m_streams->unsubscribe(context.m_sessionId, uri);
    }

    return QJsonObject();
}

QString MCPProtocol::instructions()
{
    return
        "SDRangel is a software defined radio (SDR) application. This server controls a running SDRangel instance.\n"
        "\n"
        "Start with the intent tools: listen (receive a frequency in a mode, one call) and scan (find and follow activity across "
        "frequencies). get_status is the cheapest view of what is running. Use the object level tools to adjust what they set up.\n"
        "\n"
        "Object model: a device set (R0, R1... for receivers, T0... for transmitters) holds one sampling device (an SDR such as an "
        "RTL-SDR) and any number of channels (demodulators/modulators) processing its baseband. Device settings include centerFrequency "
        "(Hz), sample rate and gain; channel settings include inputFrequencyOffset (Hz from the device centre). Features are device "
        "independent plugins (Map, AIS, APRS...) in the single feature set (index 0).\n"
        "\n"
        "Settings keys are specific to each type, but the common ones (centerFrequency, inputFrequencyOffset, rfBandwidth, squelch, "
        "volume, gain) can be set straight away: a set_* tool applies what it recognises, replies with just the keys changed, and names "
        "any it did not recognise. Call describe_settings only for an unfamiliar key or when you need its range. get_receiving_guide "
        "says which demodulator, frequency and sample rate a named signal needs; get_docs has a plugin's full readme, by section.\n"
        "\n"
        "A receiver's baseband rate (for RTL-SDR, devSampleRate / 2^log2Decim) must exceed every channel's RF bandwidth; listen "
        "chooses it for you, and get_channel_report shows the resulting channelSampleRate.\n"
        "\n"
        "Recording: record_iq writes baseband IQ for a fixed time, start_iq_recording/stop_iq_recording for longer; capture_audio "
        "writes a demodulator's audio to WAV and says whether it was silent. Files go under the capture directory shown by "
        "get_server_status. Live data: get_packets (AIS, APRS, LoRa, M17, Meshtastic, Inmarsat, radiosondes) and get_map_items "
        "(aircraft, ships, sondes, satellites) return what running plugins have decoded.\n"
        "\n"
        "Frequencies are in Hz. Direction is \"rx\" or \"tx\". Creation tools wait for SDRangel to finish and return the new index; "
        "setting changes are applied asynchronously.";
}

QJsonValue MCPProtocol::toolsList(const QJsonObject& params)
{
    (void) params;
    QJsonObject result;
    result["tools"] = m_tools.listTools();
    return result;
}

QJsonValue MCPProtocol::toolsCall(const QJsonObject& params)
{
    QString name = params["name"].toString();

    if (name.isEmpty()) {
        throw MCPError(InvalidParams, "Invalid params: missing tool name");
    }

    return m_tools.callTool(name, params["arguments"].toObject());
}

QJsonObject MCPProtocol::textContent(const QString& text)
{
    QJsonObject content;
    content["type"] = "text";
    content["text"] = text;
    return content;
}

struct MCPResourceDef {
    const char *uri;
    const char *name;
    const char *description;
};

static const MCPResourceDef staticResources[] = {
    {"sdrangel://instance", "Instance summary", "Version, device sets with their devices and channels, and features currently configured"},
    {"sdrangel://plugins/devices", "Available sampling devices", "SDR hardware (and software sources/sinks) that can be selected in a device set"},
    {"sdrangel://plugins/channels", "Available channel types", "Demodulator and modulator plugins that can be added to a device set"},
    {"sdrangel://plugins/features", "Available feature types", "Feature plugins that can be added to the feature set"},
    {"sdrangel://presets", "Presets", "Saved device set presets (device and channel settings) grouped by name"},
    {"sdrangel://configurations", "Configurations", "Saved whole-instance configurations grouped by name"},
    {"sdrangel://guide", "Receiving guide", "Which demodulator and frequency to use for a given signal, the minimum sample rate some modes need, and why nothing is received"},
    {"sdrangel://docs", "Documentation index", "Documentation available for the devices, channels and features registered in this instance, and for GUI pages such as the spectrum display, with section headings"},
    {"sdrangel://packets", "Recent packets", "The most recent packets decoded by AIS, packet (AX.25/APRS), LoRa, M17, Meshtastic, MeshCore, Inmarsat and radiosonde demodulators"},
    {"sdrangel://map/items", "Map items", "Objects currently plotted on the map: aircraft, ships, APRS stations, radiosondes, satellites, beacons"},
};

QJsonValue MCPProtocol::resourcesList(const QJsonObject& params)
{
    (void) params;
    QJsonArray resources;

    for (const auto& def : staticResources)
    {
        QJsonObject resource;
        resource["uri"] = def.uri;
        resource["name"] = def.name;
        resource["description"] = def.description;
        resource["mimeType"] = "application/json";
        resources.append(resource);
    }

    QJsonObject result;
    result["resources"] = resources;
    return result;
}

QJsonValue MCPProtocol::resourcesTemplatesList(const QJsonObject& params)
{
    (void) params;
    QJsonArray templates;

    QJsonObject schema;
    schema["uriTemplate"] = "sdrangel://schema/{type}";
    schema["name"] = "Settings schema";
    schema["description"] = "Documentation of the settings, report and actions keys for a device (hardware id such as RTLSDR), channel (type such as ADSBDemod) or feature (type such as Map)";
    schema["mimeType"] = "text/plain";
    templates.append(schema);

    QJsonObject deviceset;
    deviceset["uriTemplate"] = "sdrangel://deviceset/{deviceSetIndex}";
    deviceset["name"] = "Device set";
    deviceset["description"] = "Sampling device and channels of a device set";
    deviceset["mimeType"] = "application/json";
    templates.append(deviceset);

    QJsonObject docs;
    docs["uriTemplate"] = "sdrangel://docs/{kind}/{id}";
    docs["name"] = "Plugin documentation";
    docs["description"] = "User documentation (readme) of a plugin: kind is device, channel or feature and id is the type id (e.g. channel/ADSBDemod, device/RTLSDR, feature/Map)";
    docs["mimeType"] = "text/markdown";
    templates.append(docs);

    QJsonObject result;
    result["resourceTemplates"] = templates;
    return result;
}

QJsonValue MCPProtocol::resourcesRead(const QJsonObject& params)
{
    QString uri = params["uri"].toString();
    QJsonObject content;
    content["uri"] = uri;

    if (uri == "sdrangel://instance")
    {
        content["mimeType"] = "application/json";
        content["text"] = QString(QJsonDocument(m_tools.getInstanceSummary()).toJson(QJsonDocument::Compact));
    }
    else if (uri == "sdrangel://plugins/devices")
    {
        content["mimeType"] = "application/json";
        content["text"] = QString(QJsonDocument(m_tools.getAvailableDevices()).toJson(QJsonDocument::Compact));
    }
    else if (uri == "sdrangel://plugins/channels")
    {
        content["mimeType"] = "application/json";
        content["text"] = QString(QJsonDocument(m_tools.getAvailableChannels()).toJson(QJsonDocument::Compact));
    }
    else if (uri == "sdrangel://plugins/features")
    {
        content["mimeType"] = "application/json";
        content["text"] = QString(QJsonDocument(m_tools.getAvailableFeatures()).toJson(QJsonDocument::Compact));
    }
    else if (uri == "sdrangel://presets")
    {
        content["mimeType"] = "application/json";
        content["text"] = QString(QJsonDocument(m_tools.getPresets()).toJson(QJsonDocument::Compact));
    }
    else if (uri == "sdrangel://configurations")
    {
        content["mimeType"] = "application/json";
        content["text"] = QString(QJsonDocument(m_tools.getConfigurations()).toJson(QJsonDocument::Compact));
    }
    else if (uri.startsWith("sdrangel://schema/"))
    {
        content["mimeType"] = "text/plain";
        content["text"] = m_tools.describeType(uri.mid(QString("sdrangel://schema/").size()), "");
    }
    else if (uri == "sdrangel://guide")
    {
        content["mimeType"] = "text/markdown";
        content["text"] = m_tools.docs().guide();
    }
    else if (uri == "sdrangel://packets")
    {
        content["mimeType"] = "application/json";
        content["text"] = QString(QJsonDocument(m_tools.dataFeed().getPackets(QString(), QString(), 100, 0)).toJson(QJsonDocument::Compact));
    }
    else if (uri == "sdrangel://map/items")
    {
        content["mimeType"] = "application/json";
        content["text"] = QString(QJsonDocument(m_tools.dataFeed().getMapItems(QString(), QString(), 500, false)).toJson(QJsonDocument::Compact));
    }
    else if (uri == "sdrangel://docs")
    {
        content["mimeType"] = "application/json";
        content["text"] = QString(QJsonDocument(m_tools.docs().index()).toJson(QJsonDocument::Compact));
    }
    else if (uri.startsWith("sdrangel://docs/"))
    {
        QString rest = uri.mid(QString("sdrangel://docs/").size());
        QString kind = rest.section('/', 0, 0);
        QString id = rest.section('/', 1);
        const MCPDocs::Doc *doc = m_tools.docs().find(id, kind);

        if (!doc) {
            throw MCPError(InvalidParams, QString("Resource not found: %1. Read sdrangel://docs for the available documentation").arg(uri));
        }

        content["mimeType"] = "text/markdown";
        content["text"] = m_tools.docs().text(*doc);
    }
    else if (uri.startsWith("sdrangel://deviceset/"))
    {
        bool ok;
        int index = uri.mid(QString("sdrangel://deviceset/").size()).toInt(&ok);

        if (!ok) {
            throw MCPError(InvalidParams, QString("Invalid device set index in %1").arg(uri));
        }

        content["mimeType"] = "application/json";
        content["text"] = QString(QJsonDocument(m_tools.getDeviceSet(index)).toJson(QJsonDocument::Compact));
    }
    else
    {
        throw MCPError(InvalidParams, QString("Resource not found: %1").arg(uri));
    }

    QJsonArray contents;
    contents.append(content);
    QJsonObject result;
    result["contents"] = contents;
    return result;
}

QJsonObject MCPProtocol::promptMessage(const QString& text)
{
    QJsonObject message;
    message["role"] = "user";
    message["content"] = textContent(text);
    return message;
}

QJsonValue MCPProtocol::promptsList(const QJsonObject& params)
{
    (void) params;
    QJsonArray prompts;

    {
        QJsonObject arg1;
        arg1["name"] = "signal";
        arg1["description"] = "What to receive, e.g. ADS-B, AIS, APRS, broadcast FM, an airband frequency in MHz";
        arg1["required"] = true;
        QJsonObject arg2;
        arg2["name"] = "device";
        arg2["description"] = "Which SDR to use (hardware type or serial). Optional: the first available receiver is used otherwise";
        arg2["required"] = false;
        QJsonObject prompt;
        prompt["name"] = "setup_receiver";
        prompt["title"] = "Set up a receiver";
        prompt["description"] = "Configure an SDR device and demodulator to receive a given signal or protocol";
        prompt["arguments"] = QJsonArray({arg1, arg2});
        prompts.append(prompt);
    }
    {
        QJsonObject arg1;
        arg1["name"] = "frequency";
        arg1["description"] = "Centre frequency to record, e.g. 126 MHz";
        arg1["required"] = true;
        QJsonObject arg2;
        arg2["name"] = "duration";
        arg2["description"] = "How long to record for, e.g. 30 seconds";
        arg2["required"] = false;
        QJsonObject arg3;
        arg3["name"] = "file";
        arg3["description"] = "Output file path";
        arg3["required"] = false;
        QJsonObject prompt;
        prompt["name"] = "record_iq";
        prompt["title"] = "Record IQ data";
        prompt["description"] = "Record baseband IQ samples from an SDR to a file";
        prompt["arguments"] = QJsonArray({arg1, arg2, arg3});
        prompts.append(prompt);
    }
    {
        QJsonObject prompt;
        prompt["name"] = "explain_configuration";
        prompt["title"] = "Explain the current configuration";
        prompt["description"] = "Describe what SDRangel is currently set up to do";
        prompt["arguments"] = QJsonArray();
        prompts.append(prompt);
    }

    QJsonObject result;
    result["prompts"] = prompts;
    return result;
}

QJsonValue MCPProtocol::promptsGet(const QJsonObject& params)
{
    QString name = params["name"].toString();
    QJsonObject args = params["arguments"].toObject();
    QJsonObject result;
    QJsonArray messages;

    if (name == "setup_receiver")
    {
        QString signal = args["signal"].toString();
        QString device = args["device"].toString();
        result["description"] = QString("Set up SDRangel to receive %1").arg(signal);
        messages.append(promptMessage(QString(
            "Set up SDRangel to receive: %1.%2\n\n"
            "1. If you do not already know the frequency and which demodulator this signal needs, call get_receiving_guide.\n"
            "2. Call listen with that frequency and mode%3. It picks a device, sets a sample rate that suits the mode, adds the "
            "demodulator, starts everything and reports the run state and signal level, so one call replaces the whole "
            "device set, settings and channel sequence. Modes are bfm, wfm, nfm, am, ssb, usb, lsb, dab, adsb, ais and dsd, "
            "or any channel type id from list_channel_types.\n"
            "3. Read what listen returned. If it reports a note, act on it. Adjust with set_channel_settings if the signal needs "
            "it (squelch, volume, rfBandwidth) and use get_channel_report to confirm.\n"
            "4. Summarise what is now running, including anything that did not work.")
            .arg(signal)
            .arg(device.isEmpty() ? "" : QString(" Use this device: %1.").arg(device))
            .arg(device.isEmpty() ? "" : QString(" and device \"%1\"").arg(device))));
    }
    else if (name == "record_iq")
    {
        QString frequency = args["frequency"].toString();
        QString duration = args["duration"].toString();
        QString file = args["file"].toString();
        result["description"] = QString("Record IQ data at %1").arg(frequency);
        messages.append(promptMessage(QString(
            "Record baseband IQ data from an SDR centred on %1%2%3.\n\n"
            "1. Call get_status. If a receiver is already running on that frequency, record from it. Otherwise get one running: "
            "listen is the quickest way and confirms a signal is there, or use add_deviceset, set_device_settings and "
            "start_device if you want no demodulator.\n"
            "2. Call record_iq with the device set, duration and file name for a short capture, or start_iq_recording and "
            "stop_iq_recording for a longer one. It adds and removes its own File Sink channel; get_server_status shows the "
            "directory the files go to.\n"
            "3. Report the files written with their size and duration. Warn the user if the capture will be large: the file grows "
            "by roughly four bytes per sample, so a 2 MS/s capture uses about 8 MB per second.")
            .arg(frequency)
            .arg(duration.isEmpty() ? "" : QString(" for %1").arg(duration))
            .arg(file.isEmpty() ? "" : QString(" to the file %1").arg(file))));
    }
    else if (name == "explain_configuration")
    {
        result["description"] = "Explain the current SDRangel configuration";
        messages.append(promptMessage(
            "Call get_status: it returns one line per device set and feature, with the device, its frequency, its run state and "
            "the channels on it. That is usually enough. Only call get_device_settings, get_channel_settings or get_channel_report "
            "for a device set the user asks about in more detail, since those return every key and are far larger.\n\n"
            "Then explain in plain language what SDRangel is currently configured to do: which SDRs are in use, what frequencies they "
            "are tuned to, which demodulators are running and what they are decoding, and whether the devices are running or stopped."));
    }
    else
    {
        throw MCPError(InvalidParams, QString("Prompt not found: %1").arg(name));
    }

    result["messages"] = messages;
    return result;
}
