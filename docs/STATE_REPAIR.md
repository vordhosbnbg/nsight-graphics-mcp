# Binding and pipeline C++ source repair

Product **0.2.9** extends the opt-in `ngm_source_repair_integration` harness
with `reference binding-error` and `reference pipeline-error` pairs. The existing
combined and standalone postpass pairs remain supported. There are still
**20 MCP tools**; this change extends source-repair qualification, not the
product's resource-query surface.

The agent diagnoses and edits with normal development tools. The harness consumes
that source/build record and verifies fresh reference, faulty, and repaired
applications. It does not diagnose or edit code autonomously. The repository's
fixture keeps its intentional defects; each repair lives in a separate isolated
source tree and continues using the same faulty scenario name.

## Diagnosis before editing

Three fresh 2026.3.1.0/build38722833 generated captures were made from the frozen
0.2.7 original fixture: reference, binding fault, and pipeline fault. MCP queries
retrieve the `scene.raster` draw, its pipeline/shader association, and numbered,
hash-bound source excerpts. Independent review verified those excerpts and the
screenshot correspondence. The exact source and scene shaders are unchanged in
0.2.8, which supplies the isolated build sources below.

| Diagnosis role | Pinned bundle |
| --- | --- |
| Reference | `bundle-e83c37426bb3baa7178499a2d6dca8e4` |
| Binding fault | `bundle-54d3ae012be0fec9c0cfe1fdc743567f` |
| Pipeline fault | `bundle-cea853eb32e7ad1523349da031f6f51e` |

All evidence in this document uses `artifacts/nsight-state-repair-evidence`.
The failed initial invocation omitted the fixture's required output-directory
option. I-022 in [INVESTIGATIONS.md](INVESTIGATIONS.md) retains that pinned failure;
the corrected captures above passed. No machine settings changed.

The pipeline-fault screenshot lacks the triangle's red contribution. Its bound
pipeline's generated color-blend initializer has a G|B|A write mask, while the
reference has R|G|B|A. Following the associated create-info and attachment links
identifies the responsible state. The isolated repair restores red writes:

```diff
-            attachment.colorWriteMask &= ~VK_COLOR_COMPONENT_R_BIT;
+            attachment.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
```

The binding-fault draw selects the descriptor named `fixture.palette_set.1`;
the reference selects `.0`. Debug names are application annotations retained by
Nsight. Two fixed-input experiments compile the unchanged generated descriptor
helper with ASan/UBSan and verify the complete helper/database/source closure.
Typed setup writes establish binding0/element0, a uniform-buffer descriptor,
offset0 and range16 in each case: the reference points to its primary palette
buffer, and the fault points to its secondary palette buffer. IDs are resolved
inside each capture, not joined across captures by numeric equality.

Separately inspected application source assigns those buffers to the descriptor
sets and derives a dimmer red/blue secondary tint. `scene.frag` multiplies vertex
color by that uniform tint. These application-source semantics explain the
observed tint change. The isolated source fix selects the primary descriptor:

```diff
-        const auto descriptor = descriptors_[options_.scenario == "binding-error" ? 1 : 0];
+        const auto descriptor = descriptors_[0];
```

No frame-time palette bytes were extracted in this diagnosis. The exports contain
later frame memory updates, so initial setup contents must not be substituted for
frame-time contents. The helper experiment establishes setup descriptor fields
for its exact trusted captures only; it is not a safe general resource parser,
a product MCP extractor, or an observation of executed GPU state.

## Source builds and harness contract

Two independent source archives of commit `4636b18` use the initialized pinned
dependency sources. Each tree is built normally with CMake/Ninja, then receives
only its one prescribed source-line edit and is rebuilt in the same directory.
Original and repaired applications report 0.2.8; server and harness report 0.2.9.
The shader bundle is frozen before editing and shared by all three roles.

Inputs and normal compiler/build records live under:

- `build/source-repair-inputs/scene-descriptor-selection`
- `build/source-repair-inputs/scene-color-write-mask`

The harness requires an exact scenario-pair/repair-kind match, a unique permitted
source replacement with no other source difference, distinct executable hashes,
and complete embedded build identities that differ only in the `Fixture.cpp`
hash. It checks the live compiler input, successful verbose compile/link records,
and unchanged shader files. All original build inputs are independently checked
against the source archive during qualification.

Each role launches independently for application readback and again for capture:
seed42, 192x128, frame2; capture targets run to frame120 unless stopped by cleanup.
The basic cases must report one render pass with no offscreen, postprocessing,
bindless, or indirect features. Fresh source queries must resolve one direct
scene draw and its scene fragment shader. Screenshots must exactly match their
independent application readbacks; the original fault must differ from the
reference and the repaired fault must match exactly. The run retains source,
executable/build identities, clean owned-process shutdown, persistent pins, and
comparison after server restart. See [SOURCE_REPAIR.md](SOURCE_REPAIR.md) for
the common provenance and capture-side identity limits.

Run using absolute paths, substituting one isolated input directory and matching
Nsight installation:

```sh
cmake --build --preset linux-gcc-debug --target ngm_source_repair_integration
build/linux-gcc-debug/tests/ngm_source_repair_integration \
  /absolute/server /absolute/original-fixture /absolute/repaired-fixture \
  /absolute/shaders /absolute/record /absolute/nsight-installation \
  /absolute/artifact-store /absolute/run-root reference binding-error
```

Use `reference pipeline-error` with the independently prepared pipeline repair.
The existing CMake `_run` defaults remain the combined postpass case; direct
positional invocation selects these cases. These commands consume local build
preparation, not a complete clean-checkout installation recipe.

## Qualification

The final binding/pipeline matrix passes on matching Nsight **2026.3.1.0/build
38722833** and **2026.2.0.0/build 37991608**, on RTX 3080 Ti/driver 615.71.09,
KDE Wayland with XCB through Xwayland. Four runs supply **12 fresh captures and
12 independent application readbacks**. All captured images exactly match their
application baselines; repaired/reference RGB equality is exact.

| Run | Pinned report bundle | Fault differing pixels | Fault maximum channel error | Repaired error |
| --- | --- | ---: | ---: | ---: |
| 2026.2-binding-error | `bundle-2460053d5e5bcaeac798cad515a7ea38` | 7372 | 116 | 0 |
| 2026.2-pipeline-error | `bundle-5d10352e5cc6f3f63ebc086754c25d56` | 7372 | 142 | 0 |
| 2026.3-binding-error | `bundle-d21dc61b49e1673638fcf3a8272053a1` | 7372 | 116 | 0 |
| 2026.3-pipeline-error | `bundle-fefa4a681b07e0497de54cb3ccb4121c` | 7372 | 142 | 0 |

Pixel comparisons use 192x128 RGB8, zero tolerance, and channel-step units.
Report bundles contain all capture/baseline IDs, source/build records, protocol
transcripts and pin/restart checks. Both existing postpass profiles also pass
with the final harness on 2026.3: combined report
`bundle-83f67556554a9a3e54580cc0136772f6` and standalone report
`bundle-0c83d0b2e0238238e4aa9ac39f80a398`. These add six captures and six readbacks,
for **18 captures and 18 readbacks** in the final matrix.

The separate read-only Pillow/source audit passes all 18 image correspondences,
source/build identities, matching producer tuples, cleanup, and pin/restart checks.
For each new basic capture it independently resolves the selected descriptor name
and the associated pipeline mask: repaired selection is the primary descriptor,
and repaired mask includes all RGBA channels. These are generated-source
observations; no descriptor hydration is claimed for every final capture.
The two diagnosis captures are the exact inputs qualified by the helper experiment.

The GCC Debug CPU aggregate passes 22/22. Six negative preflight checks reject
wrong repair kinds, extra source changes and reused executables before launching
an application. I-023 records the corrected offscreen-label assertion failure;
the final postpass regressions pass. Preliminary successful attempts and the
failed regression remain retained separately from the final matrix.

Resource-index and indirect-parameter repairs, generic bounded resource access, and standalone GPU
replay remain outside these checks. R-007 and R-015 remain in progress.

## Retained qualification and review

Compact qualification bundle **`bundle-c55c149703d53c5a11ec75913cb5d747`** is
explicitly pinned in `artifacts/nsight-state-repair-evidence`. It retains
1,779 payload files (25,094,069 bytes including the outer bundle metadata):
final and preliminary reports/transcripts, diagnosis screenshots and source
queries, exact helper inputs/results, both source-build records, negative checks,
CPU log, implementation identities, and independent review scripts/results.
The snapshot records 79 pinned referenced bundles, including the failed attempts.
Repeated executables are represented by hashes in the compact snapshot; the
complete successful run bundles retain their frozen binaries and images.

Fresh-context code, diagnosis, and acceptance reviews found no remaining blocking
issue after the I-023 correction. The acceptance reviewer independently compared
all 178 tracked files in each new isolated source tree against its source archive:
only the intended `Fixture.cpp` line differs. A post-publication read verified
all 1,778 payload hashes listed in the qualification manifest, all implementation
hashes against the current tree, and all 79 referenced persistent pins.

Five of nine visual defect scenarios now have verified source repairs. The
remaining standalone/combined resource-index and indirect-parameter cases,
generic bounded resource access, and full release qualification remain open.
