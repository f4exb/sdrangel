# SPDX-License-Identifier: GPL-3.0-or-later
"""Scripted protocol/data comparison, not an LLM speed or billed-token benchmark.

Only loopback is supported. Writes are opt-in and require a TestSource receiver.
Optional tiktoken counts normalized protocol JSON with o200k_base as a proxy, not
an assertion about the tokenizer, prompt layout or caching of any particular model.
"""
import argparse
import http.client
import json
import statistics
import time
from pathlib import Path

try:
    import tiktoken
except ImportError:
    tiktoken = None


def serialized(value):
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"), sort_keys=True)


def material_size(value, encoding):
    text = serialized(value)
    return {"utf8_bytes": len(text.encode()),
            "tokenizer_proxy_tokens": len(encoding.encode(text, disallowed_special=())) if encoding else None}


class Client:
    def __init__(self, port, compact, encoding):
        self.port, self.compact, self.encoding = port, compact, encoding
        self.session = None
        self.calls = []
        init = self.rpc("initialize", {"protocolVersion": "2025-06-18",
                        "clientInfo": {"name": "sdrangel-mode-comparison", "version": "1"}})
        self.catalog = self.rpc("tools/list")
        self.initial = {tool["name"]: tool for tool in self.catalog["tools"]}
        self.schemas = dict(self.initial)
        self.startup = {"instructions": init["instructions"], "tools": self.catalog["tools"]}

    def rpc(self, method, params=None, allow_error=False):
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=10)
        headers = {"Content-Type": "application/json"}
        if self.session:
            headers["Mcp-Session-Id"] = self.session
        body = {"jsonrpc": "2.0", "id": 1, "method": method, "params": params or {}}
        try:
            connection.request("POST", "/mcp" + ("?tools=compact" if self.compact else ""),
                               serialized(body), headers)
            response = connection.getresponse()
            value = json.loads(response.read())
            new_session = response.getheader("Mcp-Session-Id")
            if response.status != 200:
                raise RuntimeError(f"HTTP {response.status}: {value}")
            if new_session:
                self.session = new_session
            if "error" in value:
                if allow_error:
                    return value
                raise RuntimeError(value["error"])
            return value["result"]
        finally:
            connection.close()

    def raw_call(self, name, arguments):
        start = time.perf_counter()
        params = {"name": name, "arguments": arguments}
        result = self.rpc("tools/call", params)
        elapsed = (time.perf_counter() - start) * 1000
        self.calls.append({"call": params, "result": result, "http_roundtrip_ms": elapsed})
        return result

    @staticmethod
    def unpack(result):
        if result.get("isError"):
            raise RuntimeError(result)
        if "structuredContent" in result:
            return result["structuredContent"]
        text = result["content"][0]["text"]
        try:
            return json.loads(text)
        except json.JSONDecodeError:
            return text

    def invoke(self, name, arguments=None):
        arguments = arguments or {}
        if name in self.initial:
            return self.unpack(self.raw_call(name, arguments))
        if not self.compact:
            raise RuntimeError(f"Operation absent from full catalog: {name}")
        if name not in self.schemas:
            found = self.unpack(self.raw_call("search_tools", {"query": name, "limit": 1}))
            if not found["tools"] or found["tools"][0]["name"] != name:
                raise RuntimeError(f"Exact discovery failed for {name}")
            self.schemas[name] = found["tools"][0]
        router = "read_tool" if self.schemas[name].get("annotations", {}).get("readOnlyHint") else "write_tool"
        return self.unpack(self.raw_call(router, {"name": name, "arguments": arguments}))

    def metrics(self, calls):
        material = [{"call": call["call"], "result": call["result"]} for call in calls]
        return {"tool_calls": len(calls),
                "discovery_calls": sum(call["call"]["name"] == "search_tools" for call in calls),
                "call_and_result_material": material_size(material, self.encoding),
                "median_http_roundtrip_ms": round(statistics.median(
                    call["http_roundtrip_ms"] for call in calls), 3) if calls else 0}

    def close(self):
        if self.session:
            connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=10)
            try:
                connection.request("DELETE", "/mcp", headers={"Mcp-Session-Id": self.session})
                connection.getresponse().read()
            finally:
                connection.close()
                self.session = None


def wait_for(client, key, expected):
    for _ in range(20):
        settings = client.invoke("get_device_settings", {"deviceSetIndex": 0})
        if settings["testSourceSettings"][key] == expected:
            return settings
        time.sleep(0.05)
    raise RuntimeError(f"Applied setting {key} did not reach {expected}")


def exercise(client, name, original, iteration):
    if name == "status":
        status = client.invoke("get_status")
        if "TestSource" not in status:
            raise RuntimeError("Comparison is restricted to the isolated TestSource instance")
    elif name == "tune_and_verify":
        frequency = original["centerFrequency"] + (iteration + 1) * 12500
        client.invoke("set_center_frequency", {"deviceSetIndex": 0, "frequency": frequency})
        wait_for(client, "centerFrequency", frequency)
    elif name == "sample_rate_change_verify_restore":
        rate = 384000 if original["sampleRate"] != 384000 else 768000
        before = client.invoke("get_device_settings", {"deviceSetIndex": 0})
        if before["deviceHwType"] != "TestSource":
            raise RuntimeError("Only simulated writes are permitted")
        client.invoke("set_device_settings", {"deviceSetIndex": 0, "settings": {"sampleRate": rate}})
        wait_for(client, "sampleRate", rate)
        client.invoke("set_device_settings", {"deviceSetIndex": 0,
                      "settings": {"sampleRate": original["sampleRate"]}})
    elif name == "plugin_documentation":
        docs = client.invoke("get_docs", {"type": "TestSource", "kind": "device"})
        if "Test source" not in docs:
            raise RuntimeError("Expected TestSource documentation")
    elif name == "recording_status":
        status = client.invoke("get_server_status")
        if "activeRecordings" not in status["capture"]:
            raise RuntimeError("Invalid capture status response")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8095)
    parser.add_argument("--rounds", type=int, default=5)
    parser.add_argument("--simulated-writes", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535 or not 1 <= args.rounds <= 100:
        parser.error("port must be 1..65535 and rounds must be 1..100")
    encoding = tiktoken.get_encoding("o200k_base") if tiktoken else None
    report = {"kind": "scripted protocol comparison; no model inference",
              "tokenizer_proxy": "o200k_base" if encoding else None,
              "rounds": args.rounds, "simulated_writes": args.simulated_writes, "modes": {}}
    probe = Client(args.port, False, encoding)
    try:
        settings = probe.invoke("get_device_settings", {"deviceSetIndex": 0})
        if settings.get("deviceHwType") != "TestSource":
            raise RuntimeError("Only a TestSource receiver is allowed")
        original = settings["testSourceSettings"]
    finally:
        probe.close()
    workflows = ["status", "plugin_documentation", "recording_status"]
    if args.simulated_writes:
        workflows[1:1] = ["tune_and_verify", "sample_rate_change_verify_restore"]
    for compact in [False, True]:
        client = Client(args.port, compact, encoding)
        rows = []
        workload_calls = []
        try:
            for iteration in range(args.rounds):
                for workflow in workflows:
                    first = len(client.calls)
                    exercise(client, workflow, original, iteration)
                    calls = client.calls[first:]
                    workload_calls.extend(calls)
                    rows.append({"round": iteration + 1, "workflow": workflow, **client.metrics(calls)})
            final_catalog = client.rpc("tools/list")
            if final_catalog != client.catalog:
                raise RuntimeError("Catalog changed during the workload")
        finally:
            try:
                if args.simulated_writes:
                    # Restoration is separate from the measured workload, including on failure.
                    client.invoke("set_device_settings", {"deviceSetIndex": 0, "settings": {
                        "centerFrequency": original["centerFrequency"], "sampleRate": original["sampleRate"],
                        "log2Decim": original["log2Decim"]}})
                    wait_for(client, "centerFrequency", original["centerFrequency"])
                    wait_for(client, "sampleRate", original["sampleRate"])
            finally:
                client.close()
        material = {"startup": client.startup,
                    "calls": [{"call": c["call"], "result": c["result"]} for c in workload_calls]}
        report["modes"]["compact" if compact else "full"] = {
            "catalog_tools": len(client.catalog["tools"]),
            "catalog_material": material_size(client.catalog, encoding),
            "startup_material": material_size(client.startup, encoding),
            "workload": client.metrics(workload_calls),
            "startup_plus_workload_material": material_size(material, encoding),
            "workflows": rows}
    text = json.dumps(report, indent=2) + "\n"
    if args.output:
        args.output.write_text(text)
    print(text)


if __name__ == "__main__":
    main()
