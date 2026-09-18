# Generated C++ capture

Product 0.2.3 adds `capture_cpp`, bringing the stdio surface to **16 tools**.
It uses Nsight's documented [Generate C++ Capture
activity](https://docs.nvidia.com/nsight-graphics/UserGuide/generate-cpp-activity.html)
to retain generated Vulkan API source, metadata, a screenshot, and resource data.
This is a separate capture format from the graphics inventories exposed by
`capture_metadata`, `capture_events`, and `capture_objects`.

## Request and evidence

Call `capture_cpp` with absolute `executable` and `working_directory` paths,
optional `arguments`, `wait_frames` (default 2, range 2–1000000), `timeout_ms`
(default 120000, maximum 600000), `pin`, and `application_output_option`.
Argument and path bounds match `capture`; no shell is used. `wait_frames` is the
Generate C++ activity option, not the graphics capture delimiter ordinal.
`capture_frame` and `delimiter` are rejected. Optional SDK control is not selected
by this mode. Every request owns a fresh target process and uses the existing
coordinator, deadlines, cancellation, cleanup, and artifact publication.

Poll `job_status` until terminal with `worker_running` and `finalization_pending`
both false. A successful job publishes a bundle whose report has
`evidence_origin: nsight_cpp_capture` and `generated_cpp_project: true`.
`readable_capture: false` retains its graphics-metadata meaning.
Read **`derived/cpp-project.json`** with `artifact_read`: it identifies the job,
generated project, source paths, selected producer/API/GPU metadata, screenshot,
resource database, and validation limits. Generated files stay under `raw/cpp`.
Read bounded source files through `artifact_read`; use `artifact_files` and local
file references for files exceeding the text limit. Binary files are retained
on disk and are not a UTF-8 text interface. These artifact operations also work
after server restart without a desktop or Nsight installation.

The native `ngm-capture` interface selects `--format cpp --wait-frames 2` with its
usual application/storage arguments. Default format remains `graphics`.
The native command rejects `--wait-frames` with graphics format and rejects
`--capture-frame` / `--delimiter` with C++ format, even when supplied defaults.

## Backend contract

The adapter requires matching installation tools and one exact producer tuple:
**2026.3.1.0/build 38722833** or **2026.2.0.0/build 37991608**. It checks retained
CLI help before using the activity, platform, target, output, wait-frame, argument,
environment, and timeout options. It exclusively creates a fresh output directory
before launching `ngfx`; existing paths are rejected (I-016).

`--no-timeout` disables the launcher's internal timeout; the server's owned-process
deadline still applies. `--env=NSIGHT_SUGGEST_GRAPHICS_CAPTURE=0` follows the exact
tool-generated runtime instruction retained in I-015 and suppresses the migration
notice for this invocation. This changes no persistent desktop or driver setting.
Production capture does not enable verbose logging or `VK_LOADER_DEBUG`.

Success requires exactly one metadata-bearing project, matching generated producer
metadata, a project descriptor, resource data/helper files, a BMP header, and
nonempty generated source. Metadata uses the bounded duplicate-key-rejecting JSON
parser; its build ID is numeric. The adapter reads the observed literal
`GeneratedReplayHeaders` / `GeneratedReplay` CMake file lists and validates every
declared header/unit, including numbered partitions. It never executes the
project's CMake or decodes the private project/database formats. The index exposes `has_unsupported_operation` from Nsight metadata. A true flag
is retained rather than rejected: successful publication does not guarantee
absence of unsupported operations. Inspect that flag before relying on workload
completeness. Unknown layouts,
unsafe source names, missing units, symlinks, nonregular files, and multiply linked
files fail explicitly.

Limits are 3500 generated tree entries, depth 8, relative output paths 384 bytes,
source files 16 MiB, metadata 1 MiB, screenshot 64 MiB, literal source basenames
255 bytes, and 1024 entries per validated source list. These leave room under the
artifact store's independent limits. Stop/deadline checks also cover validation.

After a rejected output and confirmed process cleanup, the service removes unsafe
entries or over-limit subtrees only inside that attempt's `raw/cpp`, without
following symlinks. It retains bounded regular evidence and process logs, and
records removal counts and examples in `report.rejected_cpp_output`. This permits
a readable failed bundle rather than an unpublishable invalid tree. Failed
cleanup retains the existing quarantine behavior.

## Qualification and limits

CPU stand-ins exercise generated metadata and source validation, missing numbered
units, invalid paths, producer mismatch, duplicate JSON keys, ambiguous projects,
symlinks/FIFOs/entry overflow, deadlines, child cleanup, cancellation, failed-bundle
publication, MCP source retrieval, and pins/indexes after restart.

The opt-in native MCP harness is built with:

```sh
cmake --build --preset linux-gcc-debug --target ngm_cpp_capture_integration
```

Its positional arguments are `SERVER FIXTURE SHADERS NSIGHT_ROOT ARTIFACT_ROOT
RUN_ROOT REFERENCE_SCENARIO FAULT_SCENARIO`. Use absolute paths, disjoint artifact
and run roots, and an existing desktop. It snapshots the server, fixture, and
shader inputs, captures reference/reference/fault, retrieves real source through
MCP, checks exact screenshot file repeat/difference and retained pins/indexes after
restart, and imports its report/transcripts/inputs as a pinned evidence bundle.
It is excluded from default builds, CTest, and CPU aggregate checks.
The basic reference/reference/shader-error matrix passes on both exact releases:
six fresh captures, source retrieval, screenshot file comparison, and pins/indexes
after restart. Reports and frozen inputs are pinned as
`bundle-d2579d201899eba43073dfdcecf2387f` (2026.3) and
`bundle-8287081bf6c6f039ced6341fd9aa59e4` (2026.2).
[NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md) records exact captures and identities.

A generated project is useful inspection evidence. Success does not establish
compilation of that project, standalone GPU replay, decoded image correspondence,
a generic resource extractor, arbitrary event-state reconstruction, or source
repair. Fixed-capture shader/resource experiments and independent image comparison
are recorded separately in [INSPECTION.md](INSPECTION.md). Separate advanced qualification adds 24 captures across standalone multipass,
bindless, indirect, and a combined pass-defect pair on both releases. The pinned
matrix is `bundle-cb069965dca0cc90e2b672194195beb5`; exact scope and unrun variants
are in [NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md). R-006's investigation is complete;
R-007 still owns typed deeper queries, preview/comparison, and actual source repair.
NVIDIA's [release notes](https://docs.nvidia.com/nsight-graphics/ReleaseNotes/index.html#deprecations)
deprecate Vulkan C++ Capture and announce future removal; this backend is scoped
to the exact tested producers, not a promise of future-release support.
