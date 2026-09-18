# Basic shader source repair

Product **0.2.4** demonstrates the basic shader-calculation repair on matching
Nsight **2026.3.1.0/build 38722833** and **2026.2.0.0/build 37991608**. Each run
uses three fresh targets: correct reference, faulty original shader, and the
same faulty scenario with an edited and rebuilt shader. Six real C++ captures
pass, with exact repaired/reference RGB equality. This is one R-007 case;
binding, pipeline, and advanced defect repairs remain unfinished.

## Diagnosis and actual edit

The prior fixed-capture investigation established generated event 18 → pipeline
38 → fragment module 36/resource 14, whose SPIR-V matches the retained faulty
GLSL. That shader swaps red and blue before applying the RGB palette tint.
The source and resource comparison is pinned in the original
`artifacts/nsight-evidence` store as `bundle-4d0ca74d88003db698ae2dca0b952e11`;
fault capture `bundle-3bbbc048cc769e4e4d2a46d84ac1fa1f` and extracted resources
`bundle-8eb503c687623794de4198f365ad6552` remain pinned there. See
[INSPECTION.md](INSPECTION.md) for the extraction contract and limits.

Normal development tools copied the original shader bundle into an isolated
directory, changed only `shader-error.frag`, removed its previous SPIR-V output,
and invoked the same source-built glslang compiler:

```diff
-    output_color = vec4(vertex_color.bgr * palette.tint.rgb, 1.0);
+    output_color = vec4(vertex_color.rgb * palette.tint.rgb, 1.0);
```

```sh
build/linux-gcc-debug/external/glslang/StandAlone/glslang \
  -V --target-env vulkan1.3 -g -Od \
  -o build/shader-repair-validation/inputs/repaired-shaders/shader-error.frag.spv \
  build/shader-repair-validation/inputs/repaired-shaders/shader-error.frag
```

Compilation exited 0. The retained record contains the exact absolute argv,
stdout/stderr, exit result, patch, and compiler/source/output hashes. The fixture
source's intentional defect remains available for subsequent tests. The new
SPIR-V differs from the prebuilt reference shader; copying that reference is
explicitly rejected by the harness.

| Identity | SHA-256 |
| --- | --- |
| Original GLSL | `47d1aa834987b66ccd8100b3e594e5a064693aff1c0f4e495983917e73a91a03` |
| Edited GLSL | `c5e63dfc93b0c2dd827a1fe622053c982200c62b09291835ff6f56e9da758263` |
| Original SPIR-V | `d8f019e218a9fe10bdbd69f870ca950e95df96847f487a13ae473002c95fb7e6` |
| Rebuilt SPIR-V | `0f1d7519a1c0eb1e23e6bd770feaf5383a28db18221dbc02d007bf4bb9bd2c85` |
| glslang executable | `0148c5f3673bcb25dbecd96d701731824d71e45b95ca7d4c61bab75609530371` |

## Executed validation

The opt-in [C++ harness](../tests/ShaderRepairIntegration.cpp) consumes the
prepared source edit/build record; it does not edit source or autonomously
diagnose the defect. It verifies the one-line edit, unchanged other shader inputs,
compiler identity/options, actual compiler input/output hashes, and frozen
application identity before running the capture workflow. Diagnosis and editing
are the separate evidence-backed development actions described above.

The executed command shape is:

```sh
cmake --build --preset linux-gcc-debug --target ngm_shader_repair_integration
build/linux-gcc-debug/tests/ngm_shader_repair_integration \
  build/linux-gcc-debug/nsight-graphics-mcp \
  build/linux-gcc-debug/ngm-vulkan-fixture \
  build/shader-repair-validation/inputs/original-shaders \
  build/shader-repair-validation/inputs/repaired-shaders \
  build/shader-repair-validation/inputs/record \
  /opt/nsight-graphics/NVIDIA-Nsight-Graphics-2026.3 \
  artifacts/nsight-repair-evidence \
  build/shader-repair-validation/nsight-2026.3
```

The second run selected `build/nsight-2026.2/installation` and a separate
`nsight-2026.2` run directory. Exact absolute argv and exit records are retained
under those run roots. These paths describe local evidence preparation, not a
clean-checkout installation recipe; R-014 still owns that delivery check.
The executable is excluded from default builds, CTest, and CPU aggregates.

Both runs use seed 42, 192×128 pixels, C++ capture wait count 2, final application
frame 120, and no SDK control. Generated source retrieved through MCP identifies
`frame.2`. Each run first uses `ngm::run_experiment` for three independent frame-2
application readbacks with synchronization validation. Each Nsight BMP exactly
matches its corresponding application PPM through `artifact_compare_images`.
The evidence sources remain separately labelled.

On both releases the faulty/reference comparison finds **7,337 differing pixels**,
maximum channel difference **140**, and mean absolute channel difference
**8.852064344618055** RGB8 steps. The rebuilt shader, still selected through
`shader-error`, has **zero differing pixels** and zero maximum/mean error against
the reference at tolerance 0. All capture jobs confirm cleanup and release their
GPU reservation. Pinned indexes and the repaired comparison remain identical
after server restart without desktop variables or an Nsight override.

The host is RTX 3080 Ti / driver 615.71.09, KDE Wayland through Xwayland/XCB,
GCC Debug **16.2.1 20260810**, and source-built glslang **16.4.0**. Each retained
application run records its GPU, desktop, compiler, shader, and executable inputs.
Both runs used server SHA-256
`991d392c96b310161256aa3c051f63ec86f2c95517ae81a92ea6d706bebaddc7`, fixture
`b6f78356951264d4a29115618a117712899d95c5fd622b5c7c20f39f973b343f`, and harness
`27ce70ad82ee9dfda14efb518f41ac362d3c30f4ab15dd81207568dc034c49e6`.
A subsequent capability-description wording correction changes the server binary
but not capture or comparison behavior; its focused MCP regression is recorded
in [BUILD_VALIDATION.md](BUILD_VALIDATION.md).

## Retained evidence

All bundles below are explicitly pinned in the separate
**`artifacts/nsight-repair-evidence`** store. Its separation avoids adding these
large frozen inputs to the existing store's protected-data quota.
Report imports retain source/build records, frozen inputs, three application
baselines, complete MCP transcripts, shutdown results, and comparison reports.
Report imports have caller-import provenance; they do not masquerade as captures.

| Role | 2026.3 bundle | 2026.2 bundle |
| --- | --- | --- |
| Complete run report | `bundle-8c4bde6534439d155ebd27efd5d0764f` | `bundle-3f67d4137c5db7386090b01c934a1a0d` |
| Reference capture | `bundle-10af9199c5194f650980d5d896ed07bf` | `bundle-a0ba5be3776dad31848947012e7410fd` |
| Faulty capture | `bundle-71416297cc50c9366d24e207b18dd195` | `bundle-d156ac636dcc5365d6e5a77203d46049` |
| Repaired capture | `bundle-4f1f82bd71d3413d4fdbe5383282a9da` | `bundle-d6fcd6944b92e75bdc84233ce82b78a3` |

The six independently imported application-baseline IDs are recorded in each
run's `raw/imported/report.json` and are pinned too. The raw reports distinguish
observed executable/tool identities from application-provided shader provenance.
The new captures do not perform fresh selected-resource SPIR-V extraction.
This validates the supplied basic repair and fresh captured output, not generic
resource decoding, arbitrary event state, image previews, standalone GPU replay,
or a new autonomous Codex-client diagnosis run. R-007 and R-015 remain in progress.

Independent fresh-context review decodes all six screenshots with Pillow, checks
SPIR-V embedded source and vector-shuffle instructions, verifies the generated
draw/pipeline/module association and prior captured faulty shader bytes, and
checks recorded cleanup, pins, and restart transcripts. Its script/results and
the exact run argv/exit records are pinned together as
**`bundle-ecb441b86e6c43c544cd2b1103363353`** in the repair store. The review
finds no blocker for this basic case; it does not rerun the compiler or extract
new captured shader bytes.
