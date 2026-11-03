# Optional local Code Mode adapter

This adapter exposes SDRangel's MCP tools through `search` and `execute`, using
Cloudflare's `DynamicWorkerExecutor`. It runs locally under Wrangler/workerd. No
Cloudflare account or cloud deployment is required, and no radio data is sent to
a Cloudflare-hosted Worker. Wrangler may collect its own CLI usage telemetry;
set `WRANGLER_SEND_METRICS=false` to disable that.

The installed SDRangel application and its native MCP endpoint remain unchanged.
There is no new dependency in the CMake build or desktop application. This is an
optional Node.js development tool, not a native plugin replacement.

## Why

The native server advertises individual tools with their complete input schemas.
That is convenient for direct tool calling. For clients that benefit from a
smaller initial catalog and composing operations in code, this adapter advertises
only two tools:

- `search({query, limit})`: discover matching upstream tool names, descriptions,
  schemas and annotations. No SDR operation is executed.
- `execute({code})`: run an async JavaScript function against the dynamically
  discovered `sdrangel` namespace, returning only its final result and logs.

For example, search for `get_feature_report`, then execute:

```js
async () => {
  const report = await sdrangel.get_feature_report({ featureIndex: 2 });
  return report;
}
```

Feature and device indices must be discovered in the running instance; `2` above
is illustrative. Tools are discovered through the native MCP connection, so
there is no separately maintained list of SDRangel methods or settings schemas.

## Run locally

Requirements: Node.js 22.16 or newer, npm, and SDRangel with its MCP Server
feature running at `http://127.0.0.1:8092/mcp`.

```sh
cd plugins/feature/mcpserver/codemode
npm ci
WRANGLER_SEND_METRICS=false npm run dev
```

The adapter listens at `http://127.0.0.1:8093/mcp`. Keep this process running while
the client is connected. The port differs from SDRangel's native port, 8092.

Change `SDRANGEL_MCP_URL` in `wrangler.jsonc` for another **HTTP loopback** endpoint.
For an upstream bearer token, put the following in a local `.dev.vars` file
(git-ignored), never in `wrangler.jsonc`:

```text
SDRANGEL_MCP_TOKEN="your-native-server-token"
```

Point an MCP client at port 8093 **instead of** port 8092 to avoid advertising both
catalogs and losing the context-size benefit. For Codex:

```sh
codex mcp add sdrangel --url http://127.0.0.1:8093/mcp
```

Restart the client if it only reads MCP configuration at startup. Restore the
original interface at any time with the same command using port 8092.

## Execution boundaries and trade-offs

- Generated code runs in an isolated Worker with `globalOutbound: null`; it
  cannot make direct network requests or access the host filesystem. Only
  discovered `sdrangel` methods are passed across the RPC boundary. The upstream
  token stays in the host Worker.
- The adapter itself uses the MCP SDK for upstream initialization, sessions,
  pagination, tool calls and transport cleanup. Only loopback upstreams and
  loopback request hosts are accepted. Browser Origin requests are rejected.
  Do not expose this developer server through a tunnel or bind it to all interfaces.
- The upstream server retains input validation and operation semantics. Tool
  failures are thrown into the generated code instead of returned as successful
  data. Structured results and single JSON text results are unwrapped; mixed and
  binary content retains the MCP content-block representation.
- Executions are limited to 32 upstream calls and 60 seconds. Each upstream call
  has a 45-second timeout; returned output is limited to 250,000 characters.
  These are backstops, not transactions: a timeout or later error does not undo
  earlier effects or guarantee cancellation of an already submitted radio action.
- `execute` is correctly marked as potentially destructive and non-idempotent.
  Clients cannot apply per-tool approvals inside a single execution. Users should
  inspect generated code before approving it when their client requires approval.
- Independent reads can use `Promise.all`; dependent changes should be awaited
  sequentially. SDRangel's native dispatch remains responsible for synchronization.
- This adapter exposes tools only. Native resources, prompts, subscriptions and
  streaming notifications remain on port 8092; it does not proxy them.
- The SDK and Cloudflare Code Mode APIs are evolving. Dependencies are locked,
  and this optional adapter should be tested after upgrades.

## Verification

```sh
npm run check
npm test
# With SDRangel and npm run dev already running:
node test/live.mjs
```

The live test uses reads only: it checks discovery, composes two reads, verifies
that direct sandbox networking is blocked, and checks unknown methods, upstream
argument errors and call limits. It prints the serialized sizes of both tool
catalogs. A test against SDRangel 8.0.0 with 69 tools measured 54,997 bytes for the
native `tools/list` result and 1,584 bytes for the two-tool adapter, about 97%
smaller. These are JSON byte counts, not token counts or an end-to-end savings
claim; discovery results and generated code also consume context.

References:

- [Cloudflare Code Mode MCP server patterns](https://developers.cloudflare.com/agents/model-context-protocol/guides/build-codemode-mcp-server/)
- [Cloudflare executor API](https://developers.cloudflare.com/agents/tools/codemode/api-reference/)
- [Native SDRangel MCP Server](../readme.md)
