# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the production native transport and protocol on an isolated loopback listener."""
import http.client
import json
import sys

port = int(sys.argv[1])
assert 0 < port <= 65535

def rpc(method, params=None, session=None, profile=""):
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
    headers = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
    if session:
        headers["Mcp-Session-Id"] = session
    conn.request("POST", "/mcp" + profile,
                 json.dumps({"jsonrpc": "2.0", "id": 1, "method": method, "params": params or {}}), headers)
    response = conn.getresponse()
    status, new_session = response.status, response.getheader("Mcp-Session-Id")
    value = json.loads(response.read())
    conn.close()
    return status, new_session, value

status, full, init = rpc("initialize", {"protocolVersion": "2025-06-18"})
assert status == 200 and full
all_tools = rpc("tools/list", session=full)[2]["result"]["tools"]
assert not {"search_tools", "read_tool", "write_tool"} & {tool["name"] for tool in all_tools}
assert "error" in rpc("tools/call", {"name": "search_tools", "arguments": {"query": "list_docs"}}, full)[2]
status, compact, init = rpc("initialize", profile="?tools=compact")
assert status == 200 and compact
assert init["result"]["capabilities"]["tools"]["listChanged"]
initial = rpc("tools/list", session=compact)[2]["result"]["tools"]
assert len(initial) == 8 and len(all_tools) > 60
# Session-less calls never opt in accidentally, even on the compact URL.
assert rpc("tools/list", profile="?tools=compact")[2]["result"]["tools"] == all_tools
assert rpc("initialize", profile="?tools=invalid")[0] == 400

# Regression for clients with a fixed tool set during a turn: never refresh this list
# and never open SSE. Only call methods that were advertised at initialization.
_, frozen_session, _ = rpc("initialize", profile="?tools=compact")
frozen_names = frozenset(tool["name"] for tool in rpc("tools/list", session=frozen_session)[2]["result"]["tools"])
assert "read_tool" in frozen_names and "write_tool" in frozen_names and "list_docs" not in frozen_names
def fixed_call(name, arguments):
    assert name in frozen_names, f"Client cannot call unregistered tool {name}"
    return rpc("tools/call", {"name": name, "arguments": arguments}, frozen_session)[2]["result"]
assert not fixed_call("search_tools", {"query": "list_docs", "limit": 1})["isError"]
docs = fixed_call("read_tool", {"name": "list_docs", "arguments": {}})
assert not docs["isError"] and "structuredContent" in docs
assert fixed_call("read_tool", {"name": "set_center_frequency", "arguments": {}})["isError"]
assert fixed_call("write_tool", {"name": "list_docs", "arguments": {}})["isError"]
assert fixed_call("read_tool", {"name": "write_tool", "arguments": {}})["isError"]
assert fixed_call("read_tool", {"name": "list_docs", "arguments": {"invalid": 1}})["isError"]
assert fixed_call("read_tool", {"name": "list_docs", "arguments": "{}"})["isError"]
assert "error" in rpc("tools/call", {"name": "write_tool", "arguments": {"name": "set_center_frequency", "arguments": {}}}, full)[2]
print("Fixed-catalog client: search -> read_tool passed without SSE or catalog refresh; route and validation guards passed")

conn = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
conn.request("GET", "/mcp", headers={"Accept": "text/event-stream", "Mcp-Session-Id": compact})
events = conn.getresponse()
assert events.status == 200
assert events.readline().startswith(b": stream opened")
events.readline()
result = rpc("tools/call", {"name": "search_tools", "arguments": {"query": "list_docs", "limit": 1}}, compact)[2]["result"]
assert not result["isError"]
definition = result["structuredContent"]["tools"][0]
assert definition in all_tools
notification = json.loads(events.readline().removeprefix(b"data: "))
assert notification["method"] == "notifications/tools/list_changed"
expanded = rpc("tools/list", session=compact)[2]["result"]["tools"]
assert len(expanded) == 9 and definition in expanded
assert not rpc("tools/call", {"name": "list_docs", "arguments": {}}, compact)[2]["result"]["isError"]
assert rpc("tools/list", session=full)[2]["result"]["tools"] == all_tools
invalid = rpc("tools/call", {"name": "search_tools", "arguments": {"query": "list_docs", "limit": 99}}, compact)[2]["result"]
assert invalid["isError"]
assert rpc("tools/list", session=compact)[2]["result"]["tools"] == expanded
deletion = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
deletion.request("DELETE", "/mcp", headers={"Mcp-Session-Id": compact})
assert deletion.getresponse().status == 200
deletion.close()
assert rpc("tools/list", session=compact)[0] == 404
conn.close()
print(f"Native HTTP: full={len(all_tools)}, compact={len(initial)}, discovered={len(expanded)}; search, SSE refresh, direct call, isolation, errors and deletion passed")
