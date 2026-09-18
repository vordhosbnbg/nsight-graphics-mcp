# Postpass C++ source repair

Product **0.2.8** adds an explicit C++ source-repair qualification harness for
`combined-pass-error` and `pass-output-error`. It consumes a separately prepared
source edit/build record and runs reference, faulty, and repaired applications
through the same MCP capture and comparison service. Normal development tools
perform the edit and build; the harness verifies their inputs and outcomes.
It does not autonomously diagnose or edit code. The MCP surface remains 20 tools.

## Diagnosis and source change

The earlier combined capture investigation linked the postpass draw to the
fragment module whose extracted SPIR-V byte-matched `post.frag`. Its captured
`channel_order` push value was 0 in the reference and 1 in the faulty case; the
matched GLSL swaps red/blue input when nonzero. The subsequent fixed-helper
experiment established equal inspected setup descriptor fields despite different
serialized descriptor bytes. These are separately pinned observations described
in [INSPECTION.md](INSPECTION.md) and
[DESCRIPTOR_HYDRATION.md](DESCRIPTOR_HYDRATION.md). They do not establish that all
state was equal or provide intermediate attachment pixels.

The application source selects that bad value in `workload()`. The isolated
repair changes only its assignment in `src/fixture/Fixture.cpp`:

```diff
-    result.channel_order = scenario == "pass-output-error" || scenario == "combined-pass-error" ? 1u : 0u;
+    result.channel_order = 0u;
```

Both faulty scenario names remain accepted and are used unchanged for the
repaired launches. This same assignment controls the standalone multipass and
combined paths. Their final pass receives the corrected value; other feature
selection and rendering code remains unchanged. The repository fixture retains
its intentional defects. This is an actual C++ edit and rebuilt executable,
not selection of the prebuilt correct scenario or a shader replacement.

## Build evidence

The original source is an isolated `git archive` of commit `744cb68`, using
symlinks to the already initialized exact-commit dependency source trees.
Normal CMake/Ninja builds compile the original fixture, then recompile/link it
after the one-line edit in the same source/build directory. No dependency download
or machine-setting change is involved. Both fixture builds report product 0.2.7;
the server/harness report 0.2.8. Application and server versions are independent.

The input directory is
`build/source-repair-inputs/post-channel-order`. It retains the frozen original
and repaired executables, one common shader bundle, source-origin/submodule
record, complete before/after `FixtureBuildIdentity` documents, both source files,
patch, compiler/CMake/Ninja identities, and verbose build argv/log/exit records.
Both identities are byte-equivalent JSON except for the `Fixture.cpp` input hash.
The harness checks each independently launched application's embedded build
identity, linking the source/build records to the actual binary that runs.

| Input | SHA-256 |
| --- | --- |
| Original fixture | `e6ced467def8f7df41d0c9a1b2ff2a60a52d717a6b8462d402bd05041597d302` |
| Rebuilt fixture | `4e6104556a425eb9ab397460398fef7e6f750f62accbff7348a3a7a1d59ca7b4` |
| Original Fixture.cpp | `96fadb90a274ce45fa6e2f27c82748c9c4c2035c15291a840f3622a2f5985b21` |
| Edited Fixture.cpp | `69b53427d8615a235f46d751f1601f15660488a4d65ca26412bf1d1cefb90d71` |

Every role uses the exact same retained GLSL/SPIR-V/provenance files through an
explicit `--shader-dir`. Application readback reports retain their observed shader
identities. Capture reports independently hash the selected executable before
and after launch. A captured application can be stopped before writing its final
result; its build identity is linked through the separately verified executable
and application baseline, not a presumed final capture-side application report.

## Running the qualification

```sh
cmake --build --preset linux-gcc-debug --target ngm_source_repair_integration
build/linux-gcc-debug/tests/ngm_source_repair_integration \
  build/linux-gcc-debug/nsight-graphics-mcp \
  build/source-repair-inputs/post-channel-order/original-fixture \
  build/source-repair-inputs/post-channel-order/repaired-fixture \
  build/source-repair-inputs/post-channel-order/shaders \
  build/source-repair-inputs/post-channel-order/record \
  /opt/nsight-graphics/NVIDIA-Nsight-Graphics-2026.3 \
  artifacts/nsight-repair-evidence build/source-repair-validation/example \
  combined-reference combined-pass-error
```

The second pair is `multipass-reference pass-output-error`. Select the second
matching installation explicitly for its runs. These commands consume the retained
local build preparation; they are not a clean-checkout installation recipe.
R-014 still owns source-delivery qualification. The target is excluded from
ordinary builds, CTest, and `ngm_check`. Its optional `_run` target accepts configured
`NGM_SOURCE_REPAIR_ORIGINAL_FIXTURE`, `NGM_REPAIR_FIXTURE`,
`NGM_SOURCE_REPAIR_SHADERS`, `NGM_REPAIR_RECORD_ROOT`, `NGM_NSIGHT_ROOT`,
`NGM_CAPTURE_ARTIFACT_ROOT`, and `NGM_CAPTURE_VALIDATION_ROOT`, and selects the
combined scenario pair by default.

Each run uses seed42, 192×128, frame2 application readbacks with synchronization
validation, C++ capture `wait_frames=2`, final application frame120, and no SDK.
The C++ experiment runner supplies three independent application baselines. Three
fresh MCP captures then use original/reference, original/fault, and repaired/fault.
Every captured screenshot must exactly match its separately labelled application
baseline. Fault/reference must differ; repaired/reference must have zero RGB error.

Fresh `capture_cpp_draws` queries must resolve both passes without unsupported
coverage, with the expected direct/indirect scene call and direct postpass draw.
Each capture's fragment-module symbol is checked against its own `post.frag` debug
name, and the numbered post-draw excerpt is checked against the retained source
hash and location. Historical event/object numbers are never reused as joins.
This validates generated-source relationships, not executed-state reconstruction
or fresh generic resource extraction.

Capture and baseline pins, generated indexes, and repaired image comparisons
must survive server restart without desktop variables or an Nsight override.
Structured preflight failures retain their reason and input directory before any
GPU operation. Five negative checks cover extra source edits, reused executables,
changed build identities, missing link records, and changed shader bytes.

Executed outcomes and explicit retained pins are recorded in
[BUILD_VALIDATION.md](BUILD_VALIDATION.md). Binding, pipeline, resource-index,
and indirect-parameter repairs remain separate R-007 work; these two postpass
cases do not complete R-007 or R-015.

## Observed results and retained runs

All four runs pass on RTX3080Ti / driver615.71.09 / KDE Wayland through Xwayland
and XCB, with GCC16.2.1 20260810 Debug builds. Both matching releases produce the
same RGB comparison metrics for each workload:

| Case | Faulty/reference differing pixels | Maximum channel difference | Mean absolute channel difference | Repaired/reference |
| --- | ---: | ---: | ---: | --- |
| Combined postpass | 24,472 | 97 | 8.603407118055555 | 0 differing pixels |
| Standalone multipass | 24,529 | 105 | 10.2586669921875 | 0 differing pixels |

Differences are RGB8 channel steps at tolerance0, without resizing or color
conversion beyond decoding the retained image layout. All twelve captures match
their independent application readbacks exactly. All capture cleanup checks pass.

These complete run-report bundles are explicitly pinned in
`artifacts/nsight-repair-evidence`; each `raw/imported/report.json` names its three
separately pinned capture bundles and three pinned application baselines:

| Release / case | Pinned run report |
| --- | --- |
| 2026.3.1.0/build38722833 combined | `bundle-6502aaa90e78a89e5bbbb56635b8f01d` |
| 2026.3.1.0/build38722833 standalone | `bundle-c274a92c04336d5e65e3f80b59c89a56` |
| 2026.2.0.0/build37991608 combined | `bundle-74dbef8e7502dc966effa15694e6b993` |
| 2026.2.0.0/build37991608 standalone | `bundle-f7979c5e98e987501167cbdba378f8c9` |

The four invocations and terminal exit0 records are under
`build/source-repair-validation/matrix`. Captured server identity:
`be5d3293d808b4af15758ee2c7f00cc3d373ec915cbbc563e2b0e5eba43e5940`;
harness identity:
`2fa3886376c06d70fdf522ead40dae3e97e0734dd2fd24d81e828ba22e0f9da1`.
No new integration failure occurred; the five preflight refusals were intentional
negative checks and did not launch GPU work.

Independent review decoded all twelve image pairs and verified source/build,
producer, query, pin, cleanup, and restart evidence. The 0.2.8 CPU aggregate passes
22/22. The compact qualification snapshot, including the five preflight rejection
cases and audit script/results, is explicitly pinned as
**`bundle-59b3155112a30535883c2d4fc68a3c1d`** in
`artifacts/nsight-repair-evidence`. Its bundle-reference manifest identifies all
28 separately pinned full run/capture/baseline bundles. Repeated binaries and
images are retained there rather than duplicated into the compact snapshot.

## Binding and pipeline extension at 0.2.9

The same harness now also accepts `reference binding-error` and
`reference pipeline-error`, with separate exact source edits and build records.
Basic cases require one scene draw; the original postpass cases keep their
feature/draw requirements. [STATE_REPAIR.md](STATE_REPAIR.md) records the new
diagnoses, two-release repair evidence, and postpass regression results.
