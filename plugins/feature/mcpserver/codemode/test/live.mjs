// Optional integration test. SDRangel and npm run dev must already be running.
// Uses only read operations, and never retunes or changes the current radio setup.
import assert from "node:assert/strict";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StreamableHTTPClientTransport } from "@modelcontextprotocol/sdk/client/streamableHttp.js";

const adapter = new Client({ name: "sdrangel-codemode-test", version: "1" });
const upstream = new Client({ name: "sdrangel-catalog-test", version: "1" });
try {
  await adapter.connect(new StreamableHTTPClientTransport(new URL("http://127.0.0.1:8093/mcp")));
  await upstream.connect(new StreamableHTTPClientTransport(new URL("http://127.0.0.1:8092/mcp")));
  const list = await adapter.listTools();
  assert.deepEqual(list.tools.map(t => t.name).sort(), ["execute", "search"]);
  const original = await upstream.listTools();
  console.log(JSON.stringify({ upstreamTools: original.tools.length, adapterTools: list.tools.length,
    upstreamToolListBytes: Buffer.byteLength(JSON.stringify(original)), adapterToolListBytes: Buffer.byteLength(JSON.stringify(list)) }));
  const search = await adapter.callTool({ name: "search", arguments: { query: "get_map_items", limit: 1 } });
  assert.ok(!search.isError, JSON.stringify(search));
  assert.equal(JSON.parse(search.content[0].text).tools[0].name, "get_map_items");
  const execute = async code => adapter.callTool({ name: "execute", arguments: { code } }, undefined, { timeout: 90000 });
  const normal = await execute("async () => { const status = await sdrangel.get_status({}); const map = await sdrangel.get_map_items({}); return {status, count: map.count}; }");
  assert.ok(!normal.isError, JSON.stringify(normal));
  const result = JSON.parse(normal.content[0].text);
  assert.equal(result.calls, 2);
  assert.equal(typeof result.result.count, "number");
  console.log("Live composition: two reads in one execute call passed");
  for (const code of [
    'async () => fetch("http://127.0.0.1:8091/sdrangel")',
    'async () => sdrangel.no_such_operation({})',
    'async () => sdrangel.get_status({unexpected: true})',
    'async () => { for(let i=0;i<33;i++) await sdrangel.get_status({}); }'
  ]) {
    const failed = await execute(code);
    assert.equal(failed.isError, true, JSON.stringify(failed));
  }
  console.log("Network isolation, unknown tool, upstream errors and call limit passed");
} finally {
  await adapter.close();
  await upstream.close();
}
