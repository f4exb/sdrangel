<h1>SDRangel MCP stdio bridge</h1>

<h2>Introduction</h2>

Relays the MCP stdio transport to the HTTP transport of the [MCP Server feature](../readme.md), for
clients that can only launch a local server on stdin and stdout. Claude Desktop is the one that
matters: it has no way to connect to an MCP server on `http://127.0.0.1`, since its custom connectors
are fetched from Anthropic's cloud, which cannot reach your machine.

Without it, those clients need `npx mcp-remote`, which means installing Node.js and hand editing a
JSON configuration file. The bridge is a single executable with no dependencies beyond the C++
runtime, so it can be shipped inside a Claude Desktop extension bundle (`.mcpb`) and installed with
a double click.

JSON-RPC messages pass through untouched. This program understands the transport, not the protocol,
so it needs no changes when tools are added or the schema changes.

<h2>Building</h2>

It is not built by default, as only a release that ships an extension bundle needs it:

    cmake -S . -B build -DBUILD_MCP_BRIDGE=ON
    cmake --build build --target sdrangel-mcp-bridge

There is no Qt and no SDRangel library in it, and on Windows it links the static runtime, so the
executable stands alone.

<h2>Packaging</h2>

    cmake -S . -B build -DBUILD_MCP_BUNDLE=ON
    cmake --build build --target mcpbundle

produces `bin/sdrangel-<version>-<platform>.mcpb`, a Claude Desktop extension holding this
executable, the manifest and an icon. A `.mcpb` is a zip with `manifest.json` at its root, so
CMake builds one on its own; the `mcpb` CLI only adds validation and signing, and requiring
npm to build SDRangel would be a poor trade for that. The bundle is not signed, so Claude
warns that the publisher is unverified when it is installed.

Both options default to off, so a normal build is untouched. With `BUILD_MCP_BUNDLE=ON` the
bundle is part of `all`, so building the Windows installer picks it up with nothing else to
remember:

    cmake --build build --target package

<h2>Running</h2>

    sdrangel-mcp-bridge [--host <address>] [--port <port>] [--path <path>] [--token <token>] [--verbose]

The defaults match the MCP Server feature's own: `127.0.0.1`, port 8092, path `/mcp` and no token.
`--verbose` reports what it is doing on stderr, which Claude Desktop collects in its logs.

To use it without an extension bundle, in `%APPDATA%\Claude\claude_desktop_config.json`:

    "mcpServers": {
      "sdrangel": {
        "command": "C:\\Program Files\\SDRangel\\sdrangel-mcp-bridge.exe",
        "args": ["--port", "8092"]
      }
    }

<h2>How it works</h2>

Each line arriving on stdin is one JSON-RPC message, and is POSTed to the server on its own
connection and its own thread, so that a long call such as `record_iq` does not hold up the messages
that follow it. Replies are written back to stdout, one line each.

The server's session id, returned as a header on the reply to `initialize`, is sent on every request
after it, and the negotiated protocol version with it. Once there is a session, a GET is held open
for the server to client event stream, which is where `notifications/resources/updated` arrives for
anything subscribed to with `resources/subscribe`. The server closes that stream when its lifetime
backstop expires, so it is reopened for as long as the bridge runs. When stdin closes, the session is
ended with a DELETE. SDRangel forgets every session when it restarts: the bridge then replays the
client's `initialize` to get a new one, and subscribes again to every resource the client had
subscribed to, so the client sees neither the restart nor a gap in its updates.

Anything that goes wrong with the transport becomes a JSON-RPC error against the id of the request
that provoked it, rather than a dropped message. The one users will meet is SDRangel not running,
which says so in as many words.

<h2>Testing</h2>

The bridge can be driven by hand, which is the quickest way to check it against a running SDRangel:

    echo {"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"test","version":"0"}}} | sdrangel-mcp-bridge --verbose

Worth covering when changing it: a tool call that takes tens of seconds arriving before a quick one,
a `resources/subscribe` followed by a change in SDRangel, and starting the bridge with nothing
listening on the port.
