# Header-only GPU Trace regression inputs

These small TSV files are byte-identical copies of the retained
`graphics-training-box-3/BASE_UNLOCKED` auto-exports from 2026-10-14, produced by
Nsight **2026.3.1.0/build 38722833**. `provenance.json` records the original paths
and raw/fixture SHA-256 identities. They contain metric names/numeric values and
empty headers, without application or host details in their table contents.

- `D3DPERF_EVENTS`: two header fields, no data rows.
- `GPUTRACE_REGIMES`: 147 header fields, no data rows.
- `GPUTRACE_FRAME`: 146 frame metric rows.
- `FRAME`: the exported frame value 2.33344, without inferred unit conversion.

Parser checks preserve every header name and the original numeric text. The
test-only process stand-in can emit these four shapes with its separately labelled
synthetic reproduction metadata, allowing actual managed-job and typed-query
checks. That is CPU compatibility validation. It does not turn these external
files into a service-produced hardware profile, identify a draw, or qualify an
import API. Public MCP checks separately use synthetic empty/oversized headers.
