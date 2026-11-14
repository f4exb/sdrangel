# Manual validation — 2026-11-03

These checks address the review asking for several tasks, an assessment of the initial
tool selection, and another code review. They ran through Codex's registered MCP tools
against an isolated native SDRangel server on Linux (GCC 13, Qt 5.15.13), using only a
simulated TestSource receiver. They are functional checks, not a controlled model or
token-performance benchmark. The installed hardware receiver was not retuned.

## Tasks exercised

| Task | Entry points exercised | Observed result |
| --- | --- | --- |
| Receiver status | `get_status` | R0 identified as TestSource; frequency and idle/running state reported. |
| Tune and verify | `search_tools`, `write_tool`, `read_tool`, `get_status` | Simulated R0 tuned to 145.5 MHz and verified through both settings and status. No hidden named-tool or shell/HTTP fallback. |
| Change sample rate | `search_tools`, `read_tool`, `write_tool` | Read 768,000 S/s, changed to 384,000 S/s, verified, then restored 768,000 S/s. |
| Read plugin documentation | `search_tools`, `read_tool` | Found `get_docs`; TestSource documentation returned 4,653 characters. After the filtering fix, the documentation query returned three documentation tools and no `search_tools` self-match. |
| Recording/server status | `search_tools`, `read_tool` | `get_server_status` reported capture limits/directory and no active recordings. |
| Receiving guidance | `get_receiving_guide` | Receiving guide returned successfully; its existing full-text payload was 29,092 characters. |
| Start receiving | `listen` | NFM on TestSource at 145.5 MHz; one demodulator created and device running. |
| Retune an existing intent channel | `listen` | Second call at 145.5125 MHz reported `retuned: true` and reused channel index 0. |
| Blocking operation through a call route | `search_tools`, `write_tool`, `get_status` | Routed `listen` completed successfully. A separate two-connection native HTTP check returned status in 2.7 ms while the approximately 1.55-second listen was still pending; chat-level scheduling alone did not demonstrate this concurrency. |
| Scan | `scan` | Two frequencies watched for three seconds; completed with `activeCount: 0` and an explicit no-signals-above-threshold note. This does not establish real RF reception or sensitivity. |
| Cleanup and restore | `cleanup`, `search_tools`, `write_tool`, `read_tool`, `get_status` | Removed the NFM and scanner channels, stopped TestSource, restored 145.5 MHz / 768,000 S/s / decimation 16, and verified idle state with no channels. |

The status, listen, scan and cleanup operations work without discovery. Keeping these
intent tools in the initial catalog makes the common receive/retune/scan/cleanup tasks
direct calls. The receiving guide helps select a protocol, but its large result can
matter to total context cost; no claim is made that it should always be called or that
this is the optimal initial selection.

The initial catalog has eight tools: the original six core/discovery tools plus
`read_tool` and `write_tool`. The two call tools are required for this Codex environment,
where the first direct-discovery attempt did not make a newly found tool callable during
the same turn. The subsequent eight-tool chat test passed through the stable routes.

## Review findings and regression coverage

- Full mode was advertising redundant search. `search_tools`, `read_tool` and
  `write_tool` are now compact-only, omitted from the default full list and rejected
  when invoked in full/anonymous sessions. Full mode exposes 69 operation definitions;
  a fresh compact session exposes eight initial definitions.
- Search was including itself in some keyword results. Searching the operation catalog
  now excludes compact-only helper tools, with real-registry regression coverage.
- The new call routes reused legacy coercion helpers that accepted numeric names or
  string-encoded JSON objects despite their advertised schema. The routes now require
  a non-empty string name and an actual JSON argument object; legacy target handlers
  retain their existing validation behavior.
- Read/write routing was checked against the server's behavior classification, not a
  `get_` naming rule. In particular, `get_spectrum_history` can enable collection and
  must remain a write-route target. A regression test rejects it through `read_tool`.
- Reviewed nested-call rejection, original result/error forwarding, compact/full profile
  gates, session cleanup/isolation and dispatch-lock selection for blocking routed calls.
  Regression tests cover profile gates, validation, forwarding and session cleanup;
  the separate routed-listen check also verified concurrent native status requests.
  The call routes retain grouped permissions rather than claiming per-target client
  approval isolation.

All three native CTest suites pass: catalog, protocol/session and HTTP/SSE. The latter
also contains a client that freezes its initial catalog and performs discovery/calls
without SSE or a tool-list refresh. The native plugin builds successfully. The bundle's
static catalog generator also built and produced exactly the same 69 definitions as
the native full endpoint, with no compact helper tools.

## Limits

Initial serialized JSON catalog size is not a measurement of model tokens, total task
context, latency or task success across clients. Discovery adds calls. Windows/macOS
runtime behavior, GUI layout operations, real RF reception and recording output have
not been validated by these checks. Full mode remains the default.

For this build, normalized UTF-8 JSON `tools/list` results measured 55,023 bytes for
full mode and 10,067 bytes for a fresh eight-tool compact session. These figures describe
only serialized tool definitions, not total model tokens or task costs.
