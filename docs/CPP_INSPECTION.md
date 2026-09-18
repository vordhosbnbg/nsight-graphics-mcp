# Generated C++ source inspection

Product **0.2.6** adds `capture_cpp_source` and `capture_cpp_draws` over complete
retained `capture_cpp` bundles. These are generated replay **source relationships**:
a draw's direct graphics-pipeline bind, that pipeline's stage initializers, and
shader-module resource handles and declared byte counts. They do not establish
executed GPU state, submission order, descriptor contents, extracted shader bytes,
or arbitrary resource contents at an event.

The normal graphics metadata/event/object tools retain their separate inventory
contract. Original GLSL/SPIR-V extraction and remaining visual defect repairs are
still R-007 work. A generated resource handle is a reference for further inspection,
not the contents of that resource. See [INSPECTION.md](INSPECTION.md) for the
previous fixed-capture extraction investigations.

## Requests and evidence

`capture_cpp_source` takes `capture_id`, `source_path`, optional one-based
`start_line` (default 1), and `max_lines` (default 100, maximum 200). Select the
bundle-relative path from `derived/cpp-project.json`'s `source_files`. The result
contains numbered lines, total line count, `next_line`, and the inspected file's
path, byte count and SHA-256. An offset past the end returns an empty page.
Line text excludes the separating LF; an existing CR is preserved. The source
file must be UTF-8 without NUL and at most 4 MiB. One over-limit line fails
explicitly; ordinary pages stop at 256 KiB of structured JSON.

`capture_cpp_draws` takes `capture_id`, optional `offset` (default 0), `limit`
(default 50, maximum 100), and `section`:

- `draws`: literal draw arguments, source lines, last direct pipeline binding,
  and available pipeline/stage/module resource references.
- `unsupported_recordings`: recording name, source span, and refusal reason.
- `unsupported_objects`: ambiguous or unsupported creation definitions and reasons.

Each result includes all three totals. Only the selected section contains rows;
page that section using `next_offset` until null. A partially supported capture
can contain resolved draws, draws with unavailable associations, and unsupported
recordings. The returned totals make that incomplete coverage explicit. An
unsupported recording emits no partial draw results. Global malformed or
excessive input fails the query instead of silently discarding evidence.

Draw IDs and object symbols belong to this generated project and capture. They
are not cross-capture identities or a promised join to the separate graphics
metadata inventory. Every contributing command/resource file has a content hash;
returned spans identify the exact source lines. Hashes identify inspected bytes
and do not authenticate a producer. Pages account for the escaped MCP text
fallback and framing as well as structured data, within the 1 MiB response budget.

## Qualification and limits

Both methods hold an artifact usage lease from validation through result
construction. They require a complete, unquarantined service bundle whose
manifest, report, job identity, generated-project index, successful CLI operation,
and same-bundle metadata agree. The index must name every published direct project
`.cpp` file, including `CommandList00.cpp` and `Resources00.cpp`. Paths cannot
escape the project or substitute unindexed files. Source reads remain available
when metadata reports unsupported operations; association queries reject that
capture explicitly.

The exact retained profiles are:

| Matching CLI/capture/replay version | Build | C++ metadata version string |
| --- | --- | --- |
| 2026.3.1.0 | 38722833 | 2026.3.1 |
| 2026.2.0.0 | 37991608 | 2026.2.0 |

Queries use retained observations and need no current Nsight installation or
desktop. They do not execute generated code or open its resource database.

The pure parser accepts qualified single-part recording wrappers with explicit
Begin/End boundaries, matching command-buffer operands, unique event annotations,
and a checked straight-line command vocabulary. It resolves literal creation
outputs and direct typed positional initializers with matching array extents,
stage counts, null extension/specialization pointers, and matching shader byte
counts. A referenced local declaration must precede its use. Duplicate or
unassignable outputs prevent resolution. Comments and strings cannot manufacture
commands or initializer fields.

The accepted resource setup vocabulary includes the observed Vulkan value types,
allocation/window helpers, progress logging, nested unrelated platform scopes,
and exact XCB native-handle expressions. Unsupported direct statements, side
effects, unknown constructors/helpers, shadowing, conditional relevant definitions,
multiple recording parts, and C++ line splicing fail qualification. This is a
bounded grammar for the observed generated source, not a general C++ interpreter
or verification of helper implementations. New generator forms require explicit
qualification; a refusal does not prove that Nsight cannot represent that case.

Parser input is limited to 64 command/resource sources, 4 MiB each and 16 MiB
combined. Each source has at most 500,000 tokens, 64 levels of delimiter nesting,
and 64 KiB tokens. Expressions are bounded to 16 KiB. Derived records, including
unsupported coverage, share a 10,000-record/16 MiB construction budget. Recording
functions are capped at 4,096 with unique names. Service source indexes are capped
at 256 files; these bounds are independent of raw artifact retention limits.

## Validation

`ngm_cpp_evidence_check` exercises source associations and refusal cases with
explicitly synthetic generated-source inputs. `ngm_cpp_inspection_check` checks
retained provenance, both producer profiles, missing/duplicate/traversal source
entries, unsupported operations, source/response limits, and leases. The stdio
`ngm_mcp_check` exercises both tools against executable Nsight stand-ins; these CPU
checks do not establish real producer compatibility.

The opt-in `ngm_retained_cpp_integration` uses explicit JSON cases and retained
real captures. It pages draws one at a time, checks available shader references,
retrieves source excerpts, independently checks file hashes, retrieves coverage
sections, and repeats first-page queries after server restart without desktop or
Nsight environment. It is excluded from ordinary CTest and default builds.

```sh
cmake --build --preset linux-gcc-debug --target ngm_retained_cpp_integration
build/linux-gcc-debug/tests/ngm_retained_cpp_integration \
  SERVER ARTIFACT_ROOT CASES_JSON RUN_ROOT
```

Each case specifies `capture_id`, `expected_draws`, and `tool_version`. The existing
capture matrices supply these inputs; this harness creates no fresh capture and
performs no GPU replay or shader/resource extraction. Exact executed results and
retained evidence are recorded in [BUILD_VALIDATION.md](BUILD_VALIDATION.md).
The initial resource-grammar refusals and correction are recorded as I-018–I-020 in
[INVESTIGATIONS.md](INVESTIGATIONS.md).

Final retained qualification passes **36 captures and 48 draws** on both producer
profiles, including single-row pagination and server restart. All **276 MCP tool
calls** pass an independent schema/source audit. Complete success/failure evidence,
source subsets, checks and exact binary identities are pinned as
**`bundle-ce6ae6385721977dfd7091469cb57888`** in `artifacts/nsight-repair-evidence`.
The final GCC Debug aggregate passes **22 checks**; these results do not complete
the remaining R-007 repair and resource-access cases.
