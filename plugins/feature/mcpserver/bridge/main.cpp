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

// Bridges the MCP stdio transport to SDRangel's Streamable HTTP MCP server, for clients
// such as Claude Desktop that can only launch a local server on stdin and stdout.
// JSON-RPC messages are passed through untouched: this program understands the transport,
// not the protocol.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

#include "httpclient.h"
#include "jsonscan.h"
#include "ssereader.h"

namespace
{

const char *versionText = "sdrangel-mcp-bridge " SDRANGEL_MCP_BRIDGE_VERSION;

struct Options
{
    std::string m_host = "127.0.0.1";
    int m_port = 8092;
    std::string m_path = "/mcp";
    std::string m_token;
    bool m_verbose = false;
};

void note(const Options& options, const std::string& message)
{
    if (options.m_verbose) {
        std::fprintf(stderr, "sdrangel-mcp-bridge: %s\n", message.c_str());
    }
}

void warn(const std::string& message)
{
    std::fprintf(stderr, "sdrangel-mcp-bridge: %s\n", message.c_str());
}

//!< An extension bundle cannot leave an argument out, so a user_config value the user did not
//!< set can reach us as the placeholder text itself. Treated as "not given" rather than taken
//!< literally, which would mean a nonsense port or a token the server never issued.
bool isPlaceholder(const std::string& value)
{
    return (value.size() > 3) && (value.compare(0, 2, "${") == 0) && (value.back() == '}');
}

//!< The tool list this build of SDRangel provides, written beside the bridge when the
//!< extension was packaged. A client reads the tool list once when a conversation begins and
//!< does not ask again, so a conversation that starts before SDRangel is up would have no
//!< tools for its whole life if the bridge could only answer with what it had seen. Shipping
//!< the list means it is right the first time the extension is ever used, and it comes from
//!< the server's own registry, so it cannot drift from what the tools really are.
std::string toolsFile(const char *program)
{
    std::string path;

#ifdef _WIN32
    char module[MAX_PATH];

    if (GetModuleFileNameA(nullptr, module, MAX_PATH) > 0) {
        path = module;
    }
#endif

    if (path.empty() && program) {
        path = program;
    }

    size_t slash = path.find_last_of("/\\");
    return (slash == std::string::npos) ? std::string("tools.json") : path.substr(0, slash + 1) + "tools.json";
}

std::string readFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file) {
        return std::string();
    }

    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

//!< JSON string escaping for the messages this program generates itself
std::string escaped(const std::string& text)
{
    std::string result;

    for (char c : text)
    {
        switch (c)
        {
        case '"':  result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                result += ' ';
            } else {
                result += c;
            }
        }
    }

    return result;
}

} // namespace

class Bridge
{
public:
    Bridge(const Options& options, const std::string& shippedTools) :
        m_options(options),
        m_shippedTools(shippedTools)
    {
    }

    int run();

private:
    void handle(const std::string& message);
    void post(const std::string& message);
    bool exchange(const std::string& message, HttpClient::Head& head, std::string& body, std::string& error);
    void noteSession(const HttpClient::Head& head);
    bool reinitialize(const std::string& staleSession);
    bool answerLocally(const std::string& id, const std::string& method, const std::string& message);
    bool openSession();
    void settleHandshake();
    void waitForHandshake();
    void streamLoop();
    void startStream();
    void stopStream();
    void endSession();

    std::vector<HttpClient::Header> headers(bool eventStream) const;
    void writeMessage(const std::string& message);
    void writeError(const std::string& id, int code, const std::string& reason);

    std::string sessionId() const;

    const Options m_options;
    mutable std::mutex m_stateMutex;
    std::string m_sessionId;
    std::string m_protocolVersion;
    std::string m_initializeMessage;    //!< Kept so the session can be rebuilt without the client
    std::mutex m_reinitMutex;           //!< One rebuild at a time, however many calls failed at once
    std::atomic<bool> m_degraded { false };  //!< Handshake answered without SDRangel: it was not up
    std::string m_shippedTools;         //!< tools/list result packaged with the bridge, may be empty

    // Every request but the handshake itself waits for the handshake to finish. Whether
    // SDRangel was reachable decides how the listings are answered, and a request that
    // overtook the handshake would be answered on a state that had not been worked out yet
    std::mutex m_handshakeMutex;
    std::condition_variable m_handshakeReady;
    bool m_handshakeSettled { false };

    std::mutex m_outputMutex;

    static constexpr int m_retryMinMs = 1000;   //!< First wait before reopening the event stream
    static constexpr int m_retryMaxMs = 60000;  //!< And the longest, once it keeps failing

    std::atomic<bool> m_running { true };
    std::atomic<bool> m_streamWanted { false };
    std::thread m_streamThread;
    std::mutex m_streamMutex;
    HttpClient *m_streamClient = nullptr;   //!< Owned by the stream thread, closed from ours to unblock it
};

std::string Bridge::sessionId() const
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_sessionId;
}

std::vector<HttpClient::Header> Bridge::headers(bool eventStream) const
{
    std::vector<HttpClient::Header> result;
    result.emplace_back("Host", m_options.m_host + ":" + std::to_string(m_options.m_port));
    result.emplace_back("User-Agent", versionText);
    result.emplace_back("Accept", eventStream ? "text/event-stream" : "application/json, text/event-stream");

    if (!eventStream) {
        result.emplace_back("Content-Type", "application/json");
    }

    if (!m_options.m_token.empty()) {
        result.emplace_back("Authorization", "Bearer " + m_options.m_token);
    }

    std::string session;
    std::string version;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        session = m_sessionId;
        version = m_protocolVersion;
    }

    if (!session.empty()) {
        result.emplace_back("Mcp-Session-Id", session);
    }

    // Sent on every request after initialize, as the Streamable HTTP transport requires
    if (!version.empty()) {
        result.emplace_back("MCP-Protocol-Version", version);
    }

    return result;
}

void Bridge::writeMessage(const std::string& message)
{
    // One JSON-RPC message per line. Compact JSON has no raw newlines in it, so nothing
    // here can break the framing, but a stray carriage return would confuse a strict reader
    std::string line(message);
    line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());

    std::lock_guard<std::mutex> lock(m_outputMutex);
    std::fwrite(line.data(), 1, line.size(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

void Bridge::writeError(const std::string& id, int code, const std::string& reason)
{
    // An absent or unreadable id becomes null, which is what JSON-RPC asks for when the
    // request could not be identified. The client cannot match it to a call, so the text
    // has to stand on its own in the log
    std::string identifier = id.empty() ? "null" : id;
    writeMessage("{\"jsonrpc\":\"2.0\",\"id\":" + identifier
        + ",\"error\":{\"code\":" + std::to_string(code)
        + ",\"message\":\"" + escaped(reason) + "\"}}");
}

void Bridge::handle(const std::string& message)
{
    post(message);
}

//!< One request and its whole reply. The error is ready to hand to the client as it is.
bool Bridge::exchange(const std::string& message, HttpClient::Head& head, std::string& body, std::string& error)
{
    HttpClient client;

    if (!client.open(m_options.m_host, m_options.m_port, 0, error))
    {
        error = "Cannot reach SDRangel's MCP server at " + m_options.m_host + ":"
            + std::to_string(m_options.m_port) + ": " + error + ". Start SDRangel, add the MCP Server "
            "feature and start it, then try again.";
        return false;
    }

    if (!client.send("POST", m_options.m_path, headers(false), message, error))
    {
        error = "Sending to SDRangel failed: " + error;
        return false;
    }

    if (!client.readHead(head, error))
    {
        error = "No reply from SDRangel: " + error;
        return false;
    }

    if (!client.readBody(head, body, error))
    {
        error = "Truncated reply from SDRangel: " + error;
        return false;
    }

    return true;
}

void Bridge::noteSession(const HttpClient::Head& head)
{
    std::string session = head.header("Mcp-Session-Id");

    if (session.empty() || (session == sessionId())) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_sessionId = session;
    }

    note(m_options, "session " + session);
    startStream();  // no effect once it is running: it picks the new session up on its own
}

//!< Replays the client's initialize to get a working session again. SDRangel forgets every
//!< session it has issued when it restarts, and answers 404 to the ones it used to know. The
//!< client will not initialize a second time on its own, and the bridge outlives many runs of
//!< SDRangel, so without this the connector is dead until the client restarts it.
bool Bridge::reinitialize(const std::string& staleSession)
{
    std::lock_guard<std::mutex> lock(m_reinitMutex);

    if (sessionId() != staleSession) {
        return true; // another call rebuilt it while this one waited
    }

    std::string initialize;
    {
        std::lock_guard<std::mutex> stateLock(m_stateMutex);
        initialize = m_initializeMessage;
        m_sessionId.clear();  // or the replay carries the dead id and is refused in its turn
    }

    if (initialize.empty()) {
        return false;
    }

    HttpClient::Head head;
    std::string body;
    std::string error;

    if (!exchange(initialize, head, body, error))
    {
        note(m_options, "could not start a new session: " + error);
        return false;
    }

    if ((head.m_status < 200) || (head.m_status > 299))
    {
        note(m_options, "could not start a new session: HTTP " + std::to_string(head.m_status));
        return false;
    }

    noteSession(head);

    std::string version = jsonStringMember(jsonMember(body, "result"), "protocolVersion");

    if (!version.empty())
    {
        std::lock_guard<std::mutex> stateLock(m_stateMutex);
        m_protocolVersion = version;
    }

    // This server does nothing with it, but a client that has initialized sends it, and the
    // reply is of no interest either way
    HttpClient::Head ignoredHead;
    std::string ignoredBody;
    std::string ignoredError;
    exchange("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}", ignoredHead, ignoredBody, ignoredError);

    return !sessionId().empty();
}

//!< Answers what the bridge can answer on its own. Returns true when it has replied.
//!<
//!< A stdio server's health is judged by the client from this conversation, and the bridge is
//!< healthy whether or not SDRangel happens to be running: it is a long lived process that
//!< outlives many runs of the application behind it. Letting SDRangel's absence turn into
//!< failed pings or a failed handshake makes the client give up on the bridge for good, since
//!< it will not restart it until the application itself is restarted.
bool Bridge::answerLocally(const std::string& id, const std::string& method, const std::string& message)
{
    if (!m_degraded.load()) {
        return false;
    }

    // SDRangel was not up for the handshake, so there is nothing to list yet. An empty list
    // keeps the client working; it is told to ask again as soon as SDRangel appears
    static const char *emptyListings[][2] = {
        {"tools/list", "tools"},
        {"resources/list", "resources"},
        {"resources/templates/list", "resourceTemplates"},
        {"prompts/list", "prompts"},
    };

    for (const auto& listing : emptyListings)
    {
        if (method == listing[0])
        {
            // The tools are shipped with the bridge; resources and prompts are whatever the
            // running instance happens to hold, so those stay empty until it is reachable
            std::string result = (method == "tools/list") && !m_shippedTools.empty()
                ? m_shippedTools
                : std::string("{\"") + listing[1] + "\":[]}";

            writeMessage("{\"jsonrpc\":\"2.0\",\"id\":" + (id.empty() ? "null" : id)
                + ",\"result\":" + result + "}");
            return true;
        }
    }

    (void) message;
    return false;
}

//!< Establishes a session upstream by replaying the client's initialize. Tells the client to
//!< ask for the tools again if the handshake had to be answered without SDRangel.
bool Bridge::openSession()
{
    if (!sessionId().empty()) {
        return true;
    }

    std::string initialize;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        initialize = m_initializeMessage;
    }

    if (initialize.empty()) {
        return false;
    }

    HttpClient::Head head;
    std::string body;
    std::string error;

    if (!exchange(initialize, head, body, error) || (head.m_status < 200) || (head.m_status > 299)) {
        return false;
    }

    noteSession(head);

    if (sessionId().empty()) {
        return false;
    }

    std::string version = jsonStringMember(jsonMember(body, "result"), "protocolVersion");

    if (!version.empty())
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_protocolVersion = version;
    }

    if (m_degraded.exchange(false))
    {
        note(m_options, "SDRangel is up, telling the client to list again");
        writeMessage("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/tools/list_changed\"}");
        writeMessage("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/resources/list_changed\"}");
        writeMessage("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/prompts/list_changed\"}");
    }

    return true;
}

void Bridge::settleHandshake()
{
    {
        std::lock_guard<std::mutex> lock(m_handshakeMutex);
        m_handshakeSettled = true;
    }

    m_handshakeReady.notify_all();
}

void Bridge::waitForHandshake()
{
    std::unique_lock<std::mutex> lock(m_handshakeMutex);

    // Bounded, so that a client which never gets as far as initialize cannot wedge the bridge
    m_handshakeReady.wait_for(lock, std::chrono::seconds(20), [this]() { return m_handshakeSettled; });
}

void Bridge::post(const std::string& message)
{
    std::string id = jsonMember(message, "id");
    std::string method = jsonStringMember(message, "method");

    // Whether this process is alive has nothing to do with the handshake or with SDRangel
    if (method == "ping")
    {
        writeMessage("{\"jsonrpc\":\"2.0\",\"id\":" + (id.empty() ? "null" : id) + ",\"result\":{}}");
        return;
    }

    if (method == "initialize")
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_initializeMessage = message;
    }
    else
    {
        waitForHandshake();
    }

    if (answerLocally(id, method, message)) {
        return;
    }

    // A call arriving while there is no session, because SDRangel was away: try to build one
    // rather than failing on a stale or absent session
    if ((method != "initialize") && sessionId().empty()) {
        openSession();
    }

    // Releases anything waiting on the handshake however this call ends
    struct Settle
    {
        Bridge *bridge;
        bool wanted;
        ~Settle() { if (wanted) { bridge->settleHandshake(); } }
    } settle { this, method == "initialize" };

    HttpClient::Head head;
    std::string body;
    std::string error;

    if (!exchange(message, head, body, error))
    {
        // The handshake must not fail, whatever SDRangel is doing: a client that is told the
        // server is broken during initialize never comes back to it
        if (method == "initialize")
        {
            note(m_options, "SDRangel is not up: answering the handshake without it");
            m_degraded.store(true);
            startStream();  // watches for SDRangel and lists again once it is there

            std::string requested = jsonStringMember(jsonMember(message, "params"), "protocolVersion");

            if (requested.empty()) {
                requested = "2025-06-18";
            }

            writeMessage("{\"jsonrpc\":\"2.0\",\"id\":" + (id.empty() ? "null" : id)
                + ",\"result\":{\"protocolVersion\":\"" + escaped(requested) + "\""
                  ",\"capabilities\":{\"tools\":{\"listChanged\":true}"
                  ",\"resources\":{\"listChanged\":true,\"subscribe\":true}"
                  ",\"prompts\":{\"listChanged\":true}}"
                  ",\"serverInfo\":{\"name\":\"SDRangel\",\"version\":\"" + escaped(SDRANGEL_MCP_BRIDGE_VERSION) + "\"}"
                  // Written in the past tense: the client reads this once, when the
                  // conversation starts, and never again, so anything phrased as the state
                  // right now is wrong within seconds of SDRangel being started
                  ",\"instructions\":\"SDRangel is a software defined radio (SDR) application. This "
                  "server controls it.\\n\\nSDRangel was not running when this conversation started, so "
                  "these tools are the ones shipped with it rather than a listing from the running "
                  "instance. They work as soon as SDRangel is running with the MCP Server feature added "
                  "and started, with nothing to restart. Until then a call reports that it cannot reach "
                  "it.\"}}");
            return;
        }

        writeError(id, -32001, error);
        return;
    }

    noteSession(head);

    // A session the server has never heard of: it has been restarted, or the session was
    // expired. Rebuild it and send the call again, so that neither the client nor the user
    // has to know anything happened
    std::string session = sessionId();

    if ((head.m_status == 404) && !session.empty() && (method != "initialize"))
    {
        note(m_options, "session refused, starting a new one");

        if (reinitialize(session))
        {
            HttpClient::Head retryHead;
            std::string retryBody;
            std::string retryError;

            if (exchange(message, retryHead, retryBody, retryError))
            {
                head = retryHead;
                body = retryBody;
                noteSession(head);
            }
        }
    }

    if (head.m_status == 202) {
        return; // a notification or a response of our own: nothing comes back
    }

    if ((head.m_status < 200) || (head.m_status > 299))
    {
        // The server's own errors are {"error": "..."} rather than JSON-RPC
        std::string detail = jsonStringMember(body, "error");

        if (detail.empty()) {
            detail = head.m_reason.empty() ? body : head.m_reason;
        }

        writeError(id, -32001, "SDRangel returned HTTP " + std::to_string(head.m_status) + ": " + detail);
        return;
    }

    if (body.empty())
    {
        writeError(id, -32001, "SDRangel returned an empty reply");
        return;
    }

    if (method == "initialize")
    {
        std::string version = jsonStringMember(jsonMember(body, "result"), "protocolVersion");

        if (!version.empty())
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_protocolVersion = version;
        }
    }

    writeMessage(body);
}

void Bridge::startStream()
{
    bool expected = false;

    if (!m_streamWanted.compare_exchange_strong(expected, true)) {
        return; // already running
    }

    m_streamThread = std::thread([this]() { streamLoop(); });
}

void Bridge::stopStream()
{
    if (!m_streamWanted.load()) {
        return;
    }

    {
        // Closing the socket under the thread is what unblocks its read
        std::lock_guard<std::mutex> lock(m_streamMutex);

        if (m_streamClient) {
            m_streamClient->close();
        }
    }

    if (m_streamThread.joinable()) {
        m_streamThread.join();
    }
}

void Bridge::streamLoop()
{
    // The server closes the stream when its lifetime backstop expires, so this reopens it
    // for as long as the bridge is running. It is also what notices SDRangel coming back
    // after the handshake had to be answered without it.
    //
    // Retries back off. A client that is sitting idle sends nothing, so nothing else here
    // ever notices that something is wrong, and a fixed one second retry turns an
    // unreachable or unhappy server into hundreds of log lines a minute, for hours.
    int retryMs = m_retryMinMs;

    while (m_running.load())
    {
        HttpClient client;
        std::string error;
        bool opened = false;

        if (sessionId().empty() && !openSession())
        {
            // Nothing to stream from yet: wait and look again
            for (int waited = 0; m_running.load() && (waited < retryMs); waited += 50) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            retryMs = std::min(retryMs * 2, m_retryMaxMs);
            continue;
        }

        if (!client.open(m_options.m_host, m_options.m_port, 0, error))
        {
            note(m_options, "event stream cannot connect: " + error);
        }
        else
        {
            {
                std::lock_guard<std::mutex> lock(m_streamMutex);
                m_streamClient = &client;
            }

            if (client.send("GET", m_options.m_path, headers(true), std::string(), error))
            {
                HttpClient::Head head;

                if (!client.readHead(head, error))
                {
                    note(m_options, "event stream failed: " + error);
                }
                else if (head.m_status == 404)
                {
                    // The session has been forgotten, which SDRangel restarting does to all of
                    // them. Only calls coming from the client recovered this before, so an idle
                    // client left the stream asking after a session that would never come back
                    note(m_options, "event stream session refused, starting a new one");

                    std::string stale = sessionId();

                    if (!stale.empty()) {
                        reinitialize(stale);
                    }
                }
                else if (head.m_status != 200)
                {
                    note(m_options, "event stream refused with HTTP " + std::to_string(head.m_status));
                }
                else
                {
                    note(m_options, "event stream open");
                    opened = true;
                    retryMs = m_retryMinMs;
                    SseReader reader;
                    std::string chunk;

                    // The server delimits the stream by closing the connection rather than
                    // chunking it, but a proxy or a later server might chunk, so follow
                    // whichever framing the response actually used
                    while (head.m_chunked ? client.readChunk(chunk, error) : client.readSome(chunk, error))
                    {
                        for (const auto& event : reader.feed(chunk))
                        {
                            if (!event.empty()) {
                                writeMessage(event);
                            }
                        }
                    }

                    if (!error.empty()) {
                        note(m_options, "event stream ended: " + error);
                    }
                }
            }

            {
                std::lock_guard<std::mutex> lock(m_streamMutex);
                m_streamClient = nullptr;
            }
        }

        // Reopening immediately would spin if the server has gone away
        for (int waited = 0; m_running.load() && (waited < retryMs); waited += 50) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // A stream that opened and then ended is the normal lifetime backstop, so that starts
        // again promptly; anything else waits longer each time
        retryMs = opened ? m_retryMinMs : std::min(retryMs * 2, m_retryMaxMs);
    }
}

void Bridge::endSession()
{
    std::string session = sessionId();

    if (session.empty()) {
        return;
    }

    HttpClient client;
    std::string error;

    if (!client.open(m_options.m_host, m_options.m_port, 0, error)) {
        return; // going away anyway, and the server expires idle sessions
    }

    if (client.send("DELETE", m_options.m_path, headers(false), std::string(), error))
    {
        HttpClient::Head head;
        std::string body;

        if (client.readHead(head, error)) {
            client.readBody(head, body, error);
        }
    }

    note(m_options, "session ended");
}

int Bridge::run()
{
    std::string error;

    if (!HttpClient::startup(error))
    {
        warn(error);
        return 1;
    }

    note(m_options, std::string("relaying to http://") + m_options.m_host + ":"
        + std::to_string(m_options.m_port) + m_options.m_path);

    // A worker, and the flag it raises once it has run to completion. A thread that is still
    // working is joinable too, so joinable() cannot tell the loop below which of them it is able
    // to reap without blocking
    struct Worker
    {
        std::thread m_thread;
        std::shared_ptr<std::atomic<bool>> m_finished;
    };

    std::vector<Worker> workers;
    std::string line;

    while (std::getline(std::cin, line))
    {
        if (!line.empty() && (line.back() == '\r')) {
            line.pop_back();
        }

        if (line.empty()) {
            continue;
        }

        // A thread each, so that a long call such as record_iq does not hold up the
        // messages that follow it, cancellation among them
        auto finished = std::make_shared<std::atomic<bool>>(false);
        workers.push_back(Worker{
            std::thread([this, line, finished]() {
                handle(line);
                finished->store(true);
            }),
            finished});

        // Reap only the ones that have finished. Joining a worker that is still inside a
        // record_iq, listen, scan or audio call would stop this loop reading input for as long as
        // that call runs, which is exactly when a cancellation needs to get through
        for (auto worker = workers.begin(); worker != workers.end(); )
        {
            if (worker->m_finished->load())
            {
                worker->m_thread.join();
                worker = workers.erase(worker);
            }
            else
            {
                ++worker;
            }
        }
    }

    note(m_options, "input closed");
    m_running.store(false);

    for (auto& worker : workers)
    {
        if (worker.m_thread.joinable()) {
            worker.m_thread.join();
        }
    }

    stopStream();
    endSession();
    HttpClient::cleanup();
    return 0;
}

int main(int argc, char *argv[])
{
    Options options;

    for (int i = 1; i < argc; i++)
    {
        std::string argument(argv[i]);
        auto value = [&](const char *name) -> std::string
        {
            if (i + 1 >= argc)
            {
                warn(std::string(name) + " needs a value");
                std::exit(2);
            }

            return std::string(argv[++i]);
        };

        if (argument == "--host")
        {
            std::string host = value("--host");

            if (!isPlaceholder(host) && !host.empty()) {
                options.m_host = host;
            }
        }
        else if (argument == "--port")
        {
            std::string port = value("--port");

            if (!isPlaceholder(port) && !port.empty()) {
                options.m_port = std::atoi(port.c_str());
            }
        }
        else if (argument == "--path")
        {
            std::string path = value("--path");

            if (!isPlaceholder(path) && !path.empty()) {
                options.m_path = path;
            }
        }
        else if (argument == "--token")
        {
            std::string token = value("--token");

            // "" is how a shell or a manifest spells an empty value it could not omit
            if (!isPlaceholder(token) && (token != "\"\"")) {
                options.m_token = token;
            }
        }
        else if (argument == "--verbose") {
            options.m_verbose = true;
        } else if ((argument == "--version") || (argument == "-v")) {
            std::printf("%s\n", versionText);
            return 0;
        } else if ((argument == "--help") || (argument == "-h")) {
            std::printf("%s\n\n", versionText);
            std::printf("Relays the MCP stdio transport to SDRangel's HTTP MCP server.\n\n");
            std::printf("  --host <address>  SDRangel's MCP server address (default 127.0.0.1)\n");
            std::printf("  --port <port>     SDRangel's MCP server port (default 8092)\n");
            std::printf("  --path <path>     Endpoint path (default /mcp)\n");
            std::printf("  --token <token>   Bearer token, if the server has one set\n");
            std::printf("  --verbose         Report what it is doing on stderr\n");
            return 0;
        } else {
            warn("unknown argument " + argument);
            return 2;
        }
    }

    if ((options.m_port <= 0) || (options.m_port > 65535))
    {
        warn("port must be between 1 and 65535");
        return 2;
    }

#ifdef _WIN32
    // Without this the C runtime turns \n into \r\n on the way out and eats \r on the way
    // in, either of which corrupts the message framing
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    std::ios::sync_with_stdio(false);

    std::string shippedTools = readFile(toolsFile(argv[0]));

    // Trim, so that a trailing newline cannot land in the middle of a JSON-RPC message
    while (!shippedTools.empty() && ((shippedTools.back() == '\n') || (shippedTools.back() == '\r'))) {
        shippedTools.pop_back();
    }

    if (options.m_verbose && shippedTools.empty()) {
        warn("no tools.json beside the executable: no tools until SDRangel is reachable");
    }

    Bridge bridge(options, shippedTools);
    return bridge.run();
}
