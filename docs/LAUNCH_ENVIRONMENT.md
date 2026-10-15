# Per-job launch environments

Version **0.4.3** adds optional `launch_environment` to `capture`, `capture_cpp`
and `profile`, over both stdio and HTTP. The native `ngm-capture` accepts repeated
`--launch-env KEY=VALUE` options. The core uses the same validator for all paths.

The overlay applies to Nsight discovery, collection, the fresh target and exports.
It is copied at submission and merged over the service defaults before HOME,
per-user XDG directories and TMPDIR are isolated inside the attempt. Other jobs,
the parent process and resource-reader workers keep their own environments.
No shell wrapper, global environment mutation or machine configuration is used.

## Accepted inputs

| Key | Accepted values |
| --- | --- |
| `LD_LIBRARY_PATH` | Colon-separated existing absolute directories; empty or relative entries are refused. |
| `VK_DRIVER_FILES` or `VK_ICD_FILENAMES` | Colon-separated existing absolute ICD JSON files; choose one selector. Manifests must identify a concrete existing library candidate. Bare library names can use the explicit `LD_LIBRARY_PATH`. |
| `SDL_VIDEODRIVER` | `wayland` or `x11`. |
| `QT_QPA_PLATFORM` | `xcb` or `wayland`. |
| `DISABLE_VK_LAYER_LUNARG_override` | Literal `0` or `1`. |
| `VK_LOADER_LAYERS_DISABLE` | One or both distinct names `VK_LAYER_LUNARG_override`, `VK_LAYER_KHRONOS_validation`, separated by a comma. |
| `VK_LOADER_DEBUG` | `layer`. |

Bounds are eight keys, 4 KiB per value, sixteen entries per path list and 16 KiB
combined key/value bytes. Unknown keys, NULs, invalid paths, conflicting ICD
selectors, arbitrary preload/PATH controls and broader layer-disable patterns
are refused before a job or artifact is allocated. Capabilities advertises the
allowlist and limits without executing a process.

For example, add this field to a capture or profile request, replacing the paths
with the operator's existing matching driver files:

```json
{
  "launch_environment": {
    "LD_LIBRARY_PATH": "/absolute/matching-driver-libraries",
    "VK_DRIVER_FILES": "/absolute/matching-driver-libraries/nvidia_icd.json",
    "QT_QPA_PLATFORM": "xcb",
    "VK_LOADER_LAYERS_DISABLE": "VK_LAYER_LUNARG_override"
  }
}
```

The equivalent capture CLI options are:

```sh
--launch-env LD_LIBRARY_PATH=/absolute/matching-driver-libraries \
--launch-env VK_DRIVER_FILES=/absolute/matching-driver-libraries/nvidia_icd.json \
--launch-env QT_QPA_PLATFORM=xcb \
--launch-env VK_LOADER_LAYERS_DISABLE=VK_LAYER_LUNARG_override
```

The server's inherited `LD_LIBRARY_PATH` is still filtered by the public capture
path. Supply these settings in each job request. Nsight's target-only environment
option cannot configure the launcher's own library selection.

## Evidence and isolation

`raw/report.json` separates `launch_environment.requested`, validated `selection`
identities and the final applied `environment`. The selection records canonical
directory paths, ICD manifest hashes and concrete library candidate hashes. They
are rechecked before launch against the frozen submission. Mutation while queued
fails the job before discovery; hashing respects cancellation and the job deadline.
Failures retain the same request and available selection evidence. An environment
that was never applied remains null.

A library candidate hash is selection evidence. It does not establish which
object the dynamic loader used, a working desktop backend, or compatibility with
the running kernel driver. Exported GPU/driver observations remain separate.
The default injection policy is preserved: the service strips inherited layer
disabling, and only an explicit validated job overlay enables the two names above.
Nsight's required injection-layer controls cannot be disabled through this field.

## Validation

`ngm_launch_environment_check` exercises path/scalar/list/byte bounds, manifest
structure, layer policy, conflicts, hashes, mutation and deadline/cancellation.
`ngm_capture_service_check` uses real executable stand-ins for discovery,
graphics/C++ capture, profile and exports, queued overlays and absent-key defaults,
parent isolation, mutation refusal and the actual capture CLI including spaces
and duplicate-key refusal. Existing failure, timeout, cancellation and shutdown
cases use an overlay too. `ngm_mcp_check`, `ngm_profile_workflow_check` and
`ngm_http_check` exercise actual public requests; resource workers keep their
separately checked empty-environment confinement path.

The opt-in `ngm_launch_environment_integration` C++ executable takes explicit
server, fixture, shader, Nsight, external managed-store, report-root and overlay
JSON paths. It first runs an independent fresh fixture baseline with the same
selected environment, then attempts a seeded 192×128 frame-2 capture through MCP.
It retains
executable/shader identities, requested/applied selections, metadata, cleanup
and a persistent source pin. Its scope is the fixture's XCB/Xwayland backend;
SDL Wayland requires a distinct target and qualification. See BUILD_VALIDATION.md
for executed checks and hardware outcomes. Profiling permissions and machine
clock/worker controls are separate prerequisites and are never changed here.

### Recorded 0.4.3 outcomes

All **32 CPU checks pass** in the serial preset. Actual fresh fixture baselines
also pass with the matching driver selections. Three current Nsight capture
attempts time out after connection; cleanup and persistent failed-evidence pins
pass, including a test without the layer-disable filter. These probes qualify
requested/applied selection propagation and the standalone fixture path, while
fresh Nsight capture compatibility remains unqualified. SDL Wayland and fresh
profiling remain separate checks. I-038 records the exact scope and revisit gate.
The complete implementation/validation snapshot is pinned as
`bundle-2a69ab0ef0bd54fd2df1fdeadc6fc423` in the configured external artifact root.
