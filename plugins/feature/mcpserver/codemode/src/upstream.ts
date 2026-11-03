// SPDX-License-Identifier: GPL-3.0-or-later
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StreamableHTTPClientTransport } from "@modelcontextprotocol/sdk/client/streamableHttp.js";
import { checkEndpoint, type RadioTool, unwrapResult } from "./catalog";

export async function connectRadio(endpoint: string, token?: string) {
  const client = new Client({ name: "sdrangel-codemode", version: "0.1.0" });
  const transport = new StreamableHTTPClientTransport(checkEndpoint(endpoint), {
    requestInit: token ? { headers: { Authorization: `Bearer ${token}` } } : undefined
  });
  try {
    await client.connect(transport);
    const tools: RadioTool[] = [];
    let cursor: string | undefined;
    do {
      const page = await client.listTools(cursor ? { cursor } : undefined);
      tools.push(...page.tools);
      cursor = page.nextCursor;
    } while (cursor);
    return {
      tools,
      async call(name: string, args: unknown) {
        if (!tools.some(tool => tool.name === name)) throw new Error(`Unknown SDRangel tool: ${name}`);
        if (args === undefined) args = {};
        if (!args || typeof args !== "object" || Array.isArray(args)) throw new Error("Tool arguments must be an object");
        const result = await client.callTool({ name, arguments: args as Record<string, unknown> }, undefined,
          { timeout: 45000 });
        return unwrapResult(result);
      },
      close: () => client.close()
    };
  } catch (error) {
    await client.close().catch(() => {});
    throw error;
  }
}
