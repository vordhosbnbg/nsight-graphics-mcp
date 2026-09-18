# Nsight CLI backend

`include/ngm/Nsight.hpp` and `src/nsight/Nsight.cpp` implement synchronous,
cancellable backend operations. The job coordinator owns scheduling and state;
the caller owns isolated attempt directories, artifact leases, publication, and
retention. No MCP handler or artifact index is embedded in this adapter.

`inspect_nsight` reuses prerequisite discovery: an explicit installation root
selects its Linux host directory or tools directly in that root; otherwise the
existing PATH/adjacent-tool search applies. Capture and replay must resolve into
the same directory and report identical version and build identifiers. A
discovered `ngfx` must match too. Missing `ngfx` alone does not prevent the
capture/replay interface from being inspected. Cross-version replay is outside
this adapter's current contract.

Inspection retains each tool's version/help stdout and stderr separately. It
recognizes options advertised by that tool's help and reports observations,
not GPU support. `interface_ready` means matching recognizable interfaces exist;
it does not establish that capture will work. `cleanup_confirmed` aggregates
every probe, including failure paths. A caller must retain its GPU reservation
when cleanup is unconfirmed.

`NsightRunContext` requires an existing caller-owned log directory, a complete
explicit environment, and positive timeout. Each operation rejects existing
output/log filenames, including dangling symlinks. Callers must use exclusive
attempt directories and retain their write leases until cleanup finishes. The
process primitive supplies a fresh owned process tree, timeout/stop forwarding,
separate logs, and bounded cleanup. Prelaunch validation failures own no process
and report confirmed cleanup.

Capture uses an explicit executable, working directory, frame number, frame
count, and delimiter. It requests application termination after capture and
omits the bundled replayer because replay is selected from the same installation.
Frame numbers must be at least two: current help both calls the numbering
one-based and says the value must exceed one. Frame count is restricted to the
advertised interval 1–600. The default delimiter is presentation; callers selecting
the documented experimental frame-boundary/graphics-capture-API alternatives
must separately establish that their application emits those boundaries. A
workload without its selected delimiter cannot be captured by this path.

The process executable and working directory are independent argv values and
can contain spaces. The current `--args` adapter joins only nonempty ASCII
tokens containing letters, digits, `_`, `-`, `.`, `/`, `:`, `=`, `,`, `+`, `@`, or
`%`; it supplies the result in one `--args=...` argv value. It rejects empty,
whitespace-containing, quoted, escaped, Unicode, and other target arguments.
Neither the inspected help nor the documentation defines sufficient quoting
semantics to promise a lossless representation for those cases. This restriction
is a current adapter limit and remains subject to actual launch validation; it
does not establish a limitation of Nsight itself. No shell is introduced.

A successful capture operation requires exit zero, confirmed cleanup, and a
readable nonempty regular file at the exact requested `.ngfx-capture` path. It
does not parse the proprietary capture format. A matching successful metadata
export is additionally required before a higher layer calls a capture readable.
The public documentation uses `.ngfx-capture` in its saved-file and replay
examples, although its introductory paragraph uses `ngfx-bincap`. Help calls
`--output-file` a filename without specifying extension rewriting; exact-name
preservation was established by the 2026.3.1.0 runs below.

`export_nsight_capture` runs a separate matching `ngfx-replay` process for each
metadata mode. It compares the retained capture tool version/build with the
selected replayer before launch. Its contracts are deliberately narrow:

| Export | Validation and interpretation |
| --- | --- |
| Metadata | Bounded nonempty raw stdout from `--metadata`; no field schema assumed. |
| Functions | Bounded nonempty raw stdout; no event identity or state schema assumed. |
| Objects | Bounded syntactically valid JSON; no object, binding, or resource schema assumed. |
| Logs | Bounded raw stdout; empty is valid when no captured messages exist. |
| Screenshot | Readable nonempty bounded `.png` output; the adapter treats bytes as opaque. The 2026.3.1.0 samples were decoded and compared separately. The documented origin is the embedded final present, not a new replay rendering. |

Raw process stdout remains in the log directory; accepted text/JSON is copied
to the requested export destination without overwriting an existing file.
Malformed and oversized stdout remains available for investigation. The byte
limit bounds accepted exports and in-memory reads; it does not impose a live
disk-write quota on a subprocess. Artifact/storage policy belongs to the caller.
Regular-file inspection rejects symlinks and nonregular files without blocking
on FIFOs. Capture or export exit one always fails; it never inherits the special
discovery behavior of `ngfx --version` and `ngfx --help-all`.

On 2026-09-18 the installed 2026.3 tools reported `2026.3.1.0`, build `38722833`.
Capture/replay version and help queries exited zero. `ngfx --version` and
`ngfx --help-all` emitted recognizable output and exited one. Exact inspection
outputs are retained in ignored `build/nsight-interface/`. These initial queries
are interface observations only. Real integration results and failed attempts
are recorded separately below and in `docs/INVESTIGATIONS.md`.

`cmake/Nsight.cmake` provides the `ngm_nsight` library and CPU check registration.
On 2026-09-18, isolated GCC C++20 builds with `-Wall -Wextra -Wpedantic -Werror`
and the executable Nsight stand-in check passed. Tests exercise actual process
launches for discovery quirks, missing/mismatched tools, fresh target instances,
safe argument delivery, rejected ambiguous arguments, independent output checks,
malformed/bounded exports, timeouts, cancellation, and escaped-child cleanup.
Their synthetic bytes establish backend behavior, not Nsight file compatibility.

Flags and limitations were checked against the installed help and NVIDIA's
[Graphics Capture Command-Line Interface](https://docs.nvidia.com/nsight-graphics/UserGuide/graphics-capture-cli.html).
Detailed pipeline state, shader bytecode, descriptor bindings, and resource
contents remain unimplemented until real documented exports establish them.

## Real capture and export qualification, 2026-09-18

The initial injected launch could not find the installed Vulkan driver. Preserving
the caller's `XDG_DATA_DIRS`, or explicitly supplying the standard XDG defaults
for unset/empty values before Nsight adds its layer path, corrected discovery.
The shared capture service applies this policy while retaining isolated HOME,
writable per-user XDG directories, and TMPDIR. See I-001/I-004 for failure and
successful control evidence; no driver/display or global environment change was
made. The pure backend still receives its entire explicit environment from its
caller.

The corrected production MCP server completed two `reference` and one
`shader-error` fixture captures on RTX 3080 Ti / driver 615.71.09 / KDE Wayland
through XCB/Xwayland, using matching Nsight **2026.3.1.0/build 38722833** tools.
Each request launched a distinct application, saved the exact requested capture
filename, and completed metadata/functions/objects/logs/screenshot export with
exit zero and confirmed process cleanup. Pinned evidence and the C++ stdio
validation transcript are listed in I-005. Both reference capture files were
100,968 bytes; the shader-error capture was 99,912 bytes.

The sample function and object exports are JSON arrays, with event inventory and
named object properties. Their presence does not associate a draw with a specific
pipeline, shader, descriptor, or resource contents. The final-present PNGs decode
as 192×128 images. Two reference runs have identical pixels; the fixture's
standalone application frame 2 matches Nsight capture frame 2 exactly for these
inputs. Frame 1 does not; do not infer an application-frame mapping solely from
the CLI's numbering description. Application shader files remain a separately
labelled evidence source.

Metadata carries a version warning comparing capture `2026.3.1` with replayer
`2026.3` despite matching tool versions and build 38722833. Metadata commands
exit without GPU replay, so successful export alone does not settle that warning
or qualify GPU replay. A separate complete basic matrix also passes on matching
**2026.2.0.0/build 37991608** tools, without that metadata warning. Actual GPU
replay times out during initialization on both releases (I-007/I-011).
[NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md) records both matrices and pinned
evidence. Typed metadata/event/object queries pass on the retained 2026.3.1.0
pair; detailed event state, SDK capture, advanced captures, and a second typed
producer profile remain separate validation work.
