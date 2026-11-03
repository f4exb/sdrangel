// SPDX-License-Identifier: GPL-3.0-or-later
export interface RadioTool {
  name: string;
  description?: string;
  inputSchema: Record<string, unknown>;
  annotations?: Record<string, unknown>;
}

/** Return only matching schemas, without sending the entire catalog to the model. */
export function searchTools(tools: RadioTool[], query: string, limit = 5) {
  const terms = query.toLowerCase().match(/[a-z0-9_]+/g) ?? [];
  return tools.map((tool, index) => {
    const name = tool.name.toLowerCase();
    const description = (tool.description ?? "").toLowerCase();
    const score = terms.reduce((sum, term) => sum
      + (name === term ? 20 : name.includes(term) ? 8 : 0)
      + (description.includes(term) ? 1 : 0), 0);
    return { tool, score, index };
  }).filter(({ score }) => score > 0 || terms.length === 0)
    .sort((a, b) => b.score - a.score || a.index - b.index)
    .slice(0, limit)
    .map(({ tool }) => ({ ...tool, method: `sdrangel.${tool.name}` }));
}

/** Prefer structured data; retain images/audio and mixed content as MCP content blocks. */
export function unwrapResult(result: Record<string, unknown>): unknown {
  const content = result.content as { type: string; text?: string }[] | undefined;
  if (result.isError) {
    throw new Error(content?.filter(c => c.type === "text").map(c => c.text).join("\n") || "SDRangel tool failed");
  }
  if (result.structuredContent !== undefined) return result.structuredContent;
  if (content?.length === 1 && content[0].type === "text") {
    try { return JSON.parse(content[0].text ?? ""); }
    catch { return content[0].text; }
  }
  return result;
}

export function checkEndpoint(endpoint: string): URL {
  const url = new URL(endpoint);
  if (!["127.0.0.1", "localhost", "[::1]"].includes(url.hostname)
      || url.protocol !== "http:" || url.username || url.password || url.hash) {
    throw new Error("SDRANGEL_MCP_URL must be an HTTP loopback endpoint without credentials or a fragment");
  }
  return url;
}
