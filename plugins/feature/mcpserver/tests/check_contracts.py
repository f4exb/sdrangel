# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only contract audit against a running native server.

Every target receives an unknown top-level key. The audit first verifies that each
schema rejects additional properties, so no target handler should execute. It then
compares direct full-mode and routed compact-mode errors for every operation.
"""
import argparse
import json
from pathlib import Path

from compare_modes import Client


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8095)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    full = Client(args.port, False, None)
    compact = Client(args.port, True, None)
    counts = {"matched_target_errors": 0, "wrong_route_rejections": 0,
              "hidden_direct_calls_rejected": 0, "wrapper_type_rejections": 0}
    marker = "__unexpected_contract_audit_key__"
    try:
        for tool in full.catalog["tools"]:
            schema = tool["inputSchema"]
            if schema.get("additionalProperties") is not False or marker in schema.get("properties", {}):
                raise RuntimeError(f"Unsafe test precondition for {tool['name']}")
        for tool in full.catalog["tools"]:
            name = tool["name"]
            invalid = {marker: True}
            direct = full.raw_call(name, invalid)
            router = "read_tool" if tool.get("annotations", {}).get("readOnlyHint") else "write_tool"
            routed = compact.raw_call(router, {"name": name, "arguments": invalid})
            if not direct.get("isError") or routed != direct:
                raise RuntimeError(f"Target validation/error mismatch for {name}")
            counts["matched_target_errors"] += 1
            wrong = "write_tool" if router == "read_tool" else "read_tool"
            result = compact.raw_call(wrong, {"name": name, "arguments": invalid})
            if not result.get("isError") or f"Use {router}" not in result["content"][0]["text"]:
                raise RuntimeError(f"Wrong-route guard failed for {name}")
            counts["wrong_route_rejections"] += 1
            if name not in compact.initial:
                response = compact.rpc("tools/call", {"name": name, "arguments": invalid}, allow_error=True)
                if "error" not in response:
                    raise RuntimeError(f"Unadvertised direct call accepted for {name}")
                counts["hidden_direct_calls_rejected"] += 1
        for value in [None, [], True, 1, "{}"]:
            result = compact.raw_call("read_tool", {"name": "list_docs", "arguments": value})
            if not result.get("isError"):
                raise RuntimeError("Wrapper accepted non-object arguments")
            counts["wrapper_type_rejections"] += 1
        for value in [None, [], {}, True, 1, "", " "]:
            result = compact.raw_call("read_tool", {"name": value, "arguments": {}})
            if not result.get("isError"):
                raise RuntimeError("Wrapper accepted an invalid name")
            counts["wrapper_type_rejections"] += 1
        if compact.rpc("tools/list") != compact.catalog or full.rpc("tools/list") != full.catalog:
            raise RuntimeError("Contract audit changed a catalog")
    finally:
        compact.close()
        full.close()
    text = json.dumps(counts, indent=2) + "\n"
    if args.output:
        args.output.write_text(text)
    print(text)


if __name__ == "__main__":
    main()
