# Resource-selection and indirect-parameter source repairs

Product **0.2.10** extends `ngm_source_repair_integration` with standalone and
combined resource-selection/indirect-parameter cases. The harness now tracks
multipass, bindless and indirect features separately, selects the implicated
scene or postpass draw, and checks both vertex and fragment module names against
the fresh capture. The prior four C++ profiles remain supported. There are still
**20 MCP tools**; generic bounded resource extraction remains unfinished.

## Diagnosis and edits

Seven fresh generated captures on matching Nsight 2026.3.1.0/build 38722833
cover three references and four faults. They use the frozen original 0.2.7 fixture,
seed 42, 192x128, frame 2. MCP queries retain numbered source excerpts and draw,
pipeline and shader relationships. Source-referenced push, indirect-buffer and
shader bytes are then read with the unchanged generated ReadOnlyDatabase/DataScope
helpers, separately from product queries.

Each experiment checks a compiled-in case identity and exact helper/database/source
fingerprints before helper initialization. Seven ASan/UBSan workers extract
33 selected resources, including 20 shader modules; an independent reviewer
reproduced all seven reads with no sanitizer diagnostics. Seven altered-database
checks reject before helper initialization. I-024 records the corrected initial
worker compile failure. These exact trusted-input experiments do not establish
a safe generic resource reader or arbitrary executed state.

The standalone and combined resource faults push `(resource_xor, phase) = (1, 0)`
into the scene pipeline, while both references push `(0, 0)`. The fragment modules
byte-match the diagnostic application's `bindless.frag` SPIR-V. Its retained GLSL
computes `((pixel_x / 8) ^ phase ^ resource_xor) & 1`; the changed XOR inverts the
valid palette index selected in each stripe, explaining the different tint.
The isolated application-source fix is:

```diff
-    result.resource_xor = scenario == "resource-selection-error" || scenario == "combined-resource-error" ? 1u : 0u;
+    result.resource_xor = 0u;
```

Each indirect fault draws from a capture-local parameter buffer at offset 0,
with draw count 1 and stride 16. Its source-referenced setup bytes decode to
`(vertexCount, instanceCount, firstVertex, firstInstance) = (3, 1, 0, 0)`,
versus `(3, 2, 0, 0)` in the reference. The generated source links creation,
memory binding, population and command use; subsequent frame updates target
palette memory, not the indirect allocation. The associated vertex modules
byte-match `indirect.vert`. Its retained GLSL positions instance 0 on the left
and instance 1 on the right, matching the missing right-hand triangle.
Only the predicate beneath the instance-count assignment changes:

```diff
-        result.indirect && scenario != "indirect-parameter-error" && scenario != "combined-indirect-error" ? 2u : 1u;
+        result.indirect ? 2u : 1u;
```

This preserves one instance for non-indirect workloads. Both fixes preserve the
faulty scenario names; neither selects a prebuilt correct scenario. The repository
fixture retains its intentional defects.

Combined captures record `scene.offscreen` followed by `post.present`. The
postpass push stays zero, with byte-matched corresponding shaders. The resource
fault retains indirect parameters `(3, 2, 0, 0)`; the indirect fault retains scene
selection `(0, 0)`. These selected equal fields do not establish equality of all
state. No fresh descriptor-array hydration, frame-time palette contents, or
intermediate attachment pixels are claimed.

## Source and shader build identity

Two independent archives of commit `8d0be10` supply normal CMake/Ninja original
and repaired builds under `build/source-repair-inputs/scene-resource-selection`
and `build/source-repair-inputs/scene-indirect-instance-count`. Fixtures report
0.2.9; the server/harness report 0.2.10. Within each pair, complete build identities
differ only in the prescribed `Fixture.cpp` line, and every role uses one frozen
shader bundle. The common validation and executable-identity limits are in
[SOURCE_REPAIR.md](SOURCE_REPAIR.md).

The earlier diagnostic and new qualification builds have identical fixture and
GLSL source, but their compiled SPIR-V hashes differ because debug paths differ.
Independent review records 60 raw and normalized comparisons: all instructions
are identical after excluding only `OpString`. Capture/build records preserve
actual hashes; normalization is used only for this explicit correspondence check.
Captured diagnostic modules are exact-byte matches to their own application
artifacts. The initial hashed diagnosis note is preserved with a separate
build-correspondence clarification in the evidence snapshot.

## Running the matrix

Use the existing opt-in executable with absolute paths:

```sh
cmake --build --preset linux-gcc-debug --target ngm_source_repair_integration
build/linux-gcc-debug/tests/ngm_source_repair_integration \
  /absolute/server /absolute/original-fixture /absolute/repaired-fixture \
  /absolute/shaders /absolute/record /absolute/nsight-installation \
  /absolute/artifact-store /absolute/run-root \
  bindless-reference resource-selection-error
```

The other accepted pairs are `combined-reference combined-resource-error`,
`indirect-reference indirect-parameter-error`, and
`combined-reference combined-indirect-error`, using the corresponding repair's
isolated source/build record. These inputs are local retained preparation, not
a complete clean-checkout installation recipe. Hardware checks remain separate
from default CTest and CPU checks.

Each run verifies reference/original-fault/repaired-fault through independent
application baselines and fresh MCP captures at the same inputs/frame, exact RGB
comparison, observed workload features, source/shader associations, executable
and build identities, cleanup, pins, and comparison after restart. Expected edits
are private harness checks, not MCP diagnosis responses. The agent's diagnosis
and normal edit/build steps are retained separately.

## Qualification

All four new scenarios pass on matching Nsight **2026.3.1.0/build 38722833** and
**2026.2.0.0/build 37991608**: **24 fresh captures and 24 independent application
readbacks**. Each capture exactly matches its application baseline, each original
fault differs from its reference, and each repaired fault exactly matches the
reference. The observed host remains RTX 3080 Ti/driver 615.71.09, KDE Wayland
with XCB through Xwayland, GCC 16.2.1 Debug.

| Run | Pinned report bundle | Fault differing pixels | Fault maximum channel error | Repaired error |
| --- | --- | ---: | ---: | ---: |
| 2026.2-combined-indirect-error | `bundle-fb5b6e89aff01de95f81284c7c52ce75` | 3686 | 108 | 0 |
| 2026.2-combined-resource-error | `bundle-2400a7ba86bc9a40be849387afbe25cc` | 7372 | 87 | 0 |
| 2026.2-indirect-parameter-error | `bundle-3c4b627699b16230e07ee4665b9a5b49` | 3686 | 142 | 0 |
| 2026.2-resource-selection-error | `bundle-c6e9dfc17775e7bfb1f95ee044f25425` | 7372 | 116 | 0 |
| 2026.3-combined-indirect-error | `bundle-fd10814e5fa5e288994a626017a4e767` | 3686 | 108 | 0 |
| 2026.3-combined-resource-error | `bundle-8b1fcfa2fa0853f8bfa287e654c7daee` | 7372 | 87 | 0 |
| 2026.3-indirect-parameter-error | `bundle-91221dad1fc46196a773202b71c84f44` | 3686 | 142 | 0 |
| 2026.3-resource-selection-error | `bundle-bc1c20e515f4597184b1ddeb60dd5d5e` | 7372 | 116 | 0 |

Comparisons use 192x128 RGB8, zero tolerance, and channel-step units. All bundles
in this document use `artifacts/nsight-advanced-repair-evidence`. Reports retain
capture/baseline IDs, source/build records, protocol transcripts and restart checks.
The four prior C++ profile regressions also pass on 2026.3: binding, pipeline,
combined postpass and standalone postpass. The complete matrix therefore covers
12 runs, 36 fresh captures and 36 independent application baselines. Independent
acceptance and a separate root audit pass all 12 runs, including exact source
edits, build identities, image comparisons, cleanup and retention checks. The CPU
aggregate passes 22/22 in 189.54 seconds; eight preflight checks reject mismatched
repair kinds/pairs, additional source edits and reused executables before GPU
work. R-007/R-015 remain open for bounded product resource access and full workflow
qualification; resource helper experiments do not implement that product surface.

The complete qualification snapshot is pinned as
`bundle-1e6ebc236bc4232d22303d0be4ca3583` in the same store. Its 2,165 payload files
total 32,217,853 bytes (32,599,269 bytes including bundle metadata), with 91
referenced pinned bundles. It retains matrix reports, diagnosis and helper inputs,
the I-024 failed compile, source/build records, implementation snapshots, CPU and
negative-check results, and independent review. Post-publication verification
checks every inventoried payload hash, current implementation identities, and all
referenced manifest hashes and pins. Frozen executables remain in the referenced
run bundles; the compact snapshot records their identities rather than copying
them again.
