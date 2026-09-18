# Nsight Graphics 2026.3.1 metadata fixtures

These are sanitized **real exports**, read on 2026-09-18 from explicitly pinned
bundles in `artifacts/nsight-evidence/bundles/`. They are not stand-in outputs.
Matching `ngfx-capture` and `ngfx-replay` reported **2026.3.1.0, build 38722833**;
the exported metadata reports `nsight_version: "2026.3.1"` and
`metadata_version: 1`. The source captures used the windowed Vulkan fixture on
RTX 3080 Ti / driver 615.71.09 / KDE Wayland through Xwayland. Exported driver text
is `"615.71"`; it is preserved without normalization.

| Fixture prefix | Scenario | Source bundle | Retention at extraction |
| --- | --- | --- | --- |
| `reference-` | `reference` | `bundle-019e2411c941dafdbfba953018451416` | `pin.json` has `pinned: true` |
| `shader-error-` | `shader-error` | `bundle-3f5db7a4c55c1a3e1b6a53636b091066` | `pin.json` has `pinned: true` |

Both captures selected seed 42, resolution 192×128, and application frame 20;
the exported `captured_frame` value is the string `"2"`. The parser retains this
value without treating it as an application frame index. Each functions array
contains 22 entries, 10 with `sequence_id`; each objects array contains 32 entries.
The two function exports are identical. The fragment shader object label differs
between the pair, but these exports contain no shader bytes or draw association.

The earlier diagnostic bundle `bundle-0863b91c8d1a178482d1cd3abebff32c` and repeated
reference bundle `bundle-55689147d2b220f4b7b72aff355e0800` were also inspected while
choosing this schema. Both were explicitly pinned; their observed field shapes
match these fixtures. Their exports are not duplicated here.

## Sanitization

Each `<prefix>-<kind>.json` comes from the source bundle's
`raw/exports/<kind>.raw`, parsed as JSON and written with two-space indentation
and a trailing newline. Functions and objects retain all actual entries, values,
types, order, and application labels. No fields were added.

Only metadata process context was changed:

- The executable path in `process_command_line` became
  `/fixture/ngm-vulkan-fixture`, and its `--output` path became
  `/fixture/artifacts`. Actual scenario, seed, dimensions, frame arguments, and
  their order were retained.
- Every `process_environment` entry was removed, leaving the existing field as
  an empty array. No environment keys or values remain.

Capture UUIDs, tool version/build, request times, device/OS strings, warning
text, and the empty `d3d12_core_version` field are unchanged. The retained warning
says the capture reports 2026.3.1 while the replayer reports 2026.3 despite the
matching build. A parser regression test must not silently drop that warning.
Malformed/oversized inputs and hypothetical extra fields are constructed only in
`NsightEvidenceCheck.cpp`, separate from these observed fixtures.

The original raw bytes have these SHA-256 identities:

| Fixture | Source SHA-256 |
| --- | --- |
| `reference-metadata.json` | `1fa813d09d8f90cdbddf7f3f72d986ac1bc7c9d22d24df545d92a172cacf1925` |
| `reference-functions.json` | `5331b6dface10342f0d46cc8f5190ac89003436d931eafc2e2bd361c23b34a02` |
| `reference-objects.json` | `94280398c318bc56436c13b32691db93fc9cf8be0a16ce4c8e6f36c3284a2303` |
| `shader-error-metadata.json` | `524b0e9c35ab1cd4b8b7bed079b8d033c7c1df40b44a41fdf552a757bbb28ae7` |
| `shader-error-functions.json` | `5331b6dface10342f0d46cc8f5190ac89003436d931eafc2e2bd361c23b34a02` |
| `shader-error-objects.json` | `7e570d844a5fa962bec2efac8623735f1e97941306c65b0684bcb629b643c7d0` |

The checked-in fixtures are small parser inputs. They do not include proprietary
capture data, screenshots, environment values, host/user paths, or application
shader sources. They establish observed parsing behavior for this export shape;
they do not establish GPU replay, a second Nsight release, or detailed inspection.

## Advanced indirect inventory samples

The following real exports are from the 0.2.1 workload matrix on 2026-09-18.
Each source is complete and explicitly pinned. Workload inputs remain seed 42,
192x128, and capture frame 2; the fixture now retains seven shader sources/SPIR-V.
Metadata uses the same sanitization described above, additionally replacing any
`--shader-dir` path with `/fixture/shaders`. Functions and objects are unchanged
apart from JSON whitespace.

| Prefix | Pinned source bundle |
| --- | --- |
| `indirect-reference-` | `bundle-865227a4400b16f08abf0287d6ab2150` |
| `indirect-parameter-error-` | `bundle-39fec3a9565a6b250f140f4f5c482934` |

| Fixture | Original raw SHA-256 |
| --- | --- |
| `indirect-reference-metadata.json` | `d93333451cb22cdd0095f658cfcfbe8a9036e931ea9a4cdd654463f0ea65cfd5` |
| `indirect-reference-functions.json` | `aaf46fd9303a166305631235716345c1d452f0d3597904e398d8c89b299f9579` |
| `indirect-reference-objects.json` | `47e27f5b239ca38570efb933c819e734a7b64b8b625d0f5a5b488d52b1117574` |
| `indirect-parameter-error-metadata.json` | `15586dc61af386ff5badce58fd1b12ceb80131c84dabc6c634c097b577e36ba2` |
| `indirect-parameter-error-functions.json` | `aaf46fd9303a166305631235716345c1d452f0d3597904e398d8c89b299f9579` |
| `indirect-parameter-error-objects.json` | `47e27f5b239ca38570efb933c819e734a7b64b8b625d0f5a5b488d52b1117574` |

Each functions array contains 23 records, including `vkCmdDrawIndirect` at
event 14 and `vkCmdDraw` at event 15. The 2026.3.1 producer additionally emits
`indirect_index: 0` on event 15; 2026.2.0 omits that member. Retain its presence
and unsigned value without inferring parent/child links, arguments, buffer
contents, or command expansion semantics. Both object arrays contain 34 entries.
