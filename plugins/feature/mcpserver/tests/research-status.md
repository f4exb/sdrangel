# Research status

This draft is parked as research. Compact discovery reduces catalog material, but
does not establish better end-to-end model performance or usability. It requires
an opt-in URL, adds initial discovery calls, and groups target permissions under
read/write routes. It does not implement code execution, dependent call batching,
or filtering intermediate results before returning them to the model.

The final audit fixes keep the compact catalog at eight tools after any search,
reject direct hidden calls, reject sessions lost during dispatch rather than
falling back to full mode, and protect newly created sessions from immediate
capacity eviction when timestamps tie. `list_windows` is classified as read-only.
Regression tests cover these cases alongside existing routing and HTTP behavior.

Optional developer probes:

- `compare_modes.py`: compares serialized protocol material for predefined tasks.
  Writes require `--simulated-writes` and a TestSource receiver. Optional tiktoken
  counts are proxies, not actual model input or billed tokens. This is not an LLM
  performance benchmark.
- `check_contracts.py`: compares full and routed validation errors using invalid
  arguments, checks route classifications, and verifies the fixed compact catalog.

Both scripts use an isolated loopback test listener (default port 8095), not a live
receiver. Run each with `--help` for arguments. They add no runtime dependency to
SDRangel.

Future research could consider native batching with result selection or an embedded
JavaScript engine. The latter would require isolation, resource limits and careful
permission handling; bundling an engine alone does not provide a sandbox.
