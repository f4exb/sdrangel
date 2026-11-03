# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the production native transport and protocol on an isolated loopback listener."""
import http.client
import json
import subprocess
import sys

process = subprocess.Popen([sys.argv[1], "--serve"], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
try:
    line = process.stdout.readline().strip()
    assert line.startswith("PORT="), line
    port = int(line.split("=")[1])

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
    status, compact, init = rpc("initialize", profile="?tools=compact")
    assert status == 200 and compact
    assert init["result"]["capabilities"]["tools"]["listChanged"]
    initial = rpc("tools/list", session=compact)[2]["result"]["tools"]
    assert len(initial) == 6 and len(all_tools) > 60
    # Session-less calls never opt in accidentally, even on the compact URL.
    assert rpc("tools/list", profile="?tools=compact")[2]["result"]["tools"] == all_tools
    assert rpc("initialize", profile="?tools=invalid")[0] == 400

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
    assert len(expanded) == 7 and definition in expanded
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
finally:
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
