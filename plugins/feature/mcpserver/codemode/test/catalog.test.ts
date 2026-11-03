import assert from "node:assert/strict";
import { test } from "node:test";
import { searchTools, unwrapResult, checkEndpoint } from "../src/catalog.ts";

const tools = [
  { name: "get_map_items", description: "Decoded aircraft and ships", inputSchema: { type: "object" } },
  { name: "get_status", description: "Current receiver status", inputSchema: { type: "object" } },
  { name: "stop_device", description: "Stop the receiver", inputSchema: { type: "object" } }
];

test("discovery prioritises exact names, returns schemas and respects the limit", () => {
  assert.deepEqual(searchTools(tools, "get_status", 1), [{ ...tools[1], method: "sdrangel.get_status" }]);
  assert.equal(searchTools(tools, "receiver").length, 2);
  assert.equal(searchTools(tools, "", 1).length, 1);
  assert.deepEqual(searchTools(tools, "missing"), []);
});

test("tool failures abort a code sequence rather than becoming plausible data", () => {
  assert.throws(() => unwrapResult({ isError: true, content: [{ type: "text", text: "No such channel" }] }), /No such channel/);
});

test("structured data and JSON text are usable; binary content is retained", () => {
  assert.deepEqual(unwrapResult({ structuredContent: { count: 2 } }), { count: 2 });
  assert.deepEqual(unwrapResult({ content: [{ type: "text", text: '{"count":2}' }] }), { count: 2 });
  const image = { content: [{ type: "image", data: "abc", mimeType: "image/png" }] };
  assert.deepEqual(unwrapResult(image), image);
});

test("only fixed HTTP loopback upstreams are accepted", () => {
  for (const address of ["http://127.0.0.1:8092/mcp", "http://localhost:8092/mcp", "http://[::1]:8092/mcp"]) {
    assert.ok(checkEndpoint(address));
  }
  for (const address of ["https://example.org/mcp", "http://192.168.1.2/mcp", "http://user:secret@localhost/mcp", "http://localhost/mcp#fragment"]) {
    assert.throws(() => checkEndpoint(address), /loopback/);
  }
});
