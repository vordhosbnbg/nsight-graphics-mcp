# Visual workflow acceptance

The R-010/R-002/R-006/R-007/R-015/R-014 visual first-release group completes at
**0.3.0**. The implementation evidence below is versioned through 0.2.12; 0.3.0
records acceptance and delivery without changing capture/inspection behavior.

The first visual release covers source-available Vulkan diagnosis, an actual
source/shader edit, rebuild, fresh capture and comparison through Codex's normal
development tools and this server's local stdio MCP interface. It also supports
capture and available inspection for unmodified applications. Source repair is
qualified against the repository fixture, not promised for arbitrary applications.

## Qualified scope

| Capability | Nsight 2026.3.1.0 / build 38722833 | Nsight 2026.2.0.0 / build 37991608 | Evidence and limits |
| --- | --- | --- | --- |
| Uninstrumented basic/advanced capture and exports | Pass | Pass | 54 fresh captures across nine workload pairs; [NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md) |
| Typed metadata/event/object queries | Pass | Pass | All 54 retained captures, including pagination/restart; [INSPECTION.md](INSPECTION.md) |
| Optional SDK-selected basic graphics boundaries | Pass, SDK 0.9.2 | Pass, SDK 0.9.0 | 12 SDK captures and six default regressions; advanced SDK cases remain unqualified; [SDK_CONTROL.md](SDK_CONTROL.md) |
| Generated C++ capture/source relationships | Pass | Pass | Separate capture mode, bounded literal source grammar; [CPP_CAPTURE.md](CPP_CAPTURE.md), [CPP_INSPECTION.md](CPP_INSPECTION.md) |
| Serialized resource-byte queries | Pass | Pass | Qualified optional worker per producer; 43 product captures, 666 reads, 159 independent byte oracles and six range-only resources; [RESOURCE_QUERIES.md](RESOURCE_QUERIES.md) |
| Nine visual source-repair scenarios | Pass | Pass | Actual edits/builds, retained before/after captures and exact repaired/reference RGB equality; matrix below |
| Arbitrary executed event state/descriptor selection | Unavailable | Unavailable | Source relationships and serialized inputs do not establish it |
| Standalone GPU replay execution | Failed qualification: timeout | Failed qualification: timeout | I-007/I-011; export readability is separate |

The recorded environment is Linux x86-64, RTX 3080 Ti, NVIDIA driver 615.71.09,
KDE Wayland/Xwayland with XCB presentation. Builds and shader settings are retained
per result; the current GCC Debug aggregate at 0.2.12 passes 25/25 CPU checks.
The two releases use matching capture/replay/CLI binaries. Cross-release replay,
additional GPUs/drivers/desktops, live shader stepping, reusable target sessions,
and fully headless operation are not established by this qualification.

## Actual source-repair coverage

Every row passes on both releases with equivalent inputs, unchanged faulty
scenario selection, new build identities, retained shader identities and fresh targets. Shader
bytes change where the repair edits a shader; C++ repairs can retain identical shaders. The repository
keeps the deliberate faults; repairs are performed in isolated source checkouts.

| Fault scenarios | Source change | First qualified product | Evidence |
| --- | --- | --- | --- |
| `shader-error` | Remove the fragment shader's red/blue swizzle | 0.2.4 | [SHADER_REPAIR.md](SHADER_REPAIR.md) |
| `pass-output-error`, `combined-pass-error` | Correct the postpass channel-order push constant | 0.2.8 | [SOURCE_REPAIR.md](SOURCE_REPAIR.md) |
| `binding-error`, `pipeline-error` | Correct descriptor selection / color-write mask | 0.2.9 | [STATE_REPAIR.md](STATE_REPAIR.md) |
| `resource-selection-error`, `combined-resource-error` | Correct the palette-index XOR | 0.2.10 | [ADVANCED_REPAIR.md](ADVANCED_REPAIR.md) |
| `indirect-parameter-error`, `combined-indirect-error` | Correct indirect instance count | 0.2.10 | [ADVANCED_REPAIR.md](ADVANCED_REPAIR.md) |

Those documents separate the agent's evidence-backed diagnosis and development
actions from harness-private verification expectations. Selecting a prebuilt
correct variant is not counted as repair. Application readback and Nsight evidence
have distinct origins. Historical fixed-input descriptor hydration experiments
remain qualified only for their stated exact inputs; the later generic byte
reader does not promote them into a general executed-descriptor API.

An independent fresh-context acceptance audit of HEAD `15df044` / 0.2.12 found
the R-007 and R-015 criteria satisfied by the versioned qualification slices:

- 28 retained successful repair reports, including preliminary runs/regressions,
  cover all 18 scenario/release pairs. It independently decoded 84 captures and
  140 RGB comparisons; all repaired/reference and capture/readback matches are exact.
- Resource review checked 43 cases, 666 reads, 159 byte oracles, 1,332 input
  snapshot identities, 624 source references and five expected provenance refusals.
- Advanced/resource snapshot checks verified 2,401 payload hashes and 879
  referenced pin/manifest identities. SDK checks independently compared all
  18 selected images, and the capture batch's 18 workload/release runs passed.

This audit does not claim a new autonomous Codex diagnosis session or rerun every
historical repair at the latest product version. The original exact product,
tool, compiler and application identities remain authoritative for each result.
The accepted workflow uses Codex's ordinary development actions; C++ MCP clients
provide repeatable integration checks. Client interoperability is separately
recorded in [MCP.md](MCP.md).

The acceptance audit and source-installation qualification are retained in
complete/pinned `bundle-49217a19ae75f41fbe607152bbbd3699` in
`artifacts/source-install-validation`. Its publication review verifies 121 files
and 902 unique referenced pins/manifest identities. [BUILD_VALIDATION.md](BUILD_VALIDATION.md)
records the snapshot scope and fresh installation results.

## Source installation and operational boundary

[INSTALL.md](INSTALL.md) provides the build-to-repair walkthrough. Its clean
checkout validation is separate from the historical two-release matrix and
is recorded in [BUILD_VALIDATION.md](BUILD_VALIDATION.md). The source-built
dependency graph, runtime exceptions and optional generated-helper policy are in
[DEPENDENCIES.md](DEPENDENCIES.md). No hosted CI or prebuilt release distribution
is required for this release.

Large evidence remains on disk in managed bundles. Explicit pins protect
investigation and verification baselines; source captures and resource-read
bundles need independent pins. Source hashes identify observed files, and
provenance is checked for consistency rather than treated as authentication.
General unknown applications can expose unsupported source forms or absent
evidence; the server reports these limits rather than inferring missing state.

All failed integration attempts and revisit conditions remain in
[INVESTIGATIONS.md](INVESTIGATIONS.md). The subsequent compute milestone is
qualified in [COMPUTE.md](COMPUTE.md); performance analysis and Streamable HTTP
remain unfinished work in [ROADMAP.md](ROADMAP.md).
