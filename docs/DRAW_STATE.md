# Recorded draw state

At **0.4.5**, `capture_draw_executions` and `capture_draw_state` add a bounded
recorded-state interpreter for generated Vulkan C++ captures from **Nsight
2026.3.1.0/build 38722833**. The shared stdio/HTTP surface has **28 tools**.
This is the first state slice of R-021; the broader effective-state objective
remains open. Existing `capture_cpp_draws` retains its source-relationship contract.

## Select and inspect

Capture with `capture_cpp`, then list submitted draw occurrences:

```json
{"capture_id":"bundle-…","limit":20}
```

Pass a returned `execution_ref` to `capture_draw_state`:

```json
{"capture_id":"bundle-…","execution_ref":"<listed 64-character reference>"}
```

Selection uses the `generated_cpp_recorded_submission` domain: generated frame,
queue, submission, submit-info and command-buffer array occurrence, recording
revision and annotated source event. Repeated submissions have separate
references. These annotated events are distinct from graphics function-inventory
IDs. Integer equality cannot join those domains or separate captures.

The response always declares `captured_gpu_execution_correspondence: false` and
`full_effective_state_coverage: false`. It interprets the recorded submission
stream and API inputs. It does not observe the GPU at an event or prove that every
recorded command executed successfully. Debug labels help compare the fixture's
private witness; labels alone do not establish GPU execution.

Each field carries a nullable value, reason, status and creation/bind/setter
source references. The response's `input_files` supply their capture-time hashes.

| Field | Qualified recorded values |
| --- | --- |
| `pipeline` | Unique bound graphics pipeline, layout, render-pass symbol and subpass zero. |
| `shaders` | Unique stage/module declarations, entry-point expressions and serialized SPIR-V resource references. |
| `depth_test`, `depth_write`, `depth_compare` | Static values or applicable core dynamic setters; a subpass without a depth attachment is `not_applicable`. |
| `cull_mode`, `front_face` | Static raster values or applicable core dynamic setters. |
| `viewports` | Arrays of `[x,y,width,height,minDepth,maxDepth]`. |
| `scissors` | Arrays of `[x,y,width,height]`. |

Available fields use `qualified_recorded_state`. Missing dynamic writes use
`unavailable`; unqualified creation/lifetime forms use `unsupported`. The
interpreter never substitutes ignored static creation values for missing dynamic
setters. An intervening static bind invalidates earlier dynamic values. Setters
after a static bind cannot qualify that pipeline's static draw. The interpreter
follows the [Khronos dynamic-state lifetime rules](https://docs.vulkan.org/guide/latest/dynamic_state.html).

Shader `serialized_byte_read` starts as `not_read`, with a null byte hash. Its
listed `resource_ref` can be passed to the existing `capture_cpp_resource` tool
for independently confined bytes/hash and retained provenance. No byte read or
new artifact is hidden inside the state query. A SPIR-V identity does not identify
driver machine code. Existing worker/database/range limits remain in force;
see [RESOURCE_QUERIES.md](RESOURCE_QUERIES.md).

## Grammar, refusals and bounds

The qualified execution path is one generated `Frame0Part00.cpp`, primary
command-buffer allocations, explicit recording/reset/submission calls, and
straight-line single-part recordings. Direct and indexed draws in an inline
render pass are supported. Creation and active render-pass symbols must match;
compatibility between distinct render passes is unqualified. Pipeline extensions,
libraries, shader objects, specialization, rasterizer discard, secondary command
buffers, multipart recordings, indirect draws and unknown relevant commands
receive explicit refusals or unsupported coverage. Unknown recording commands
cannot leave earlier draws in that recording available. Unknown frame commands
prevent execution association. One-time recordings cannot be submitted again
without rerecording.

Static state and these observed core dynamic forms are qualified:
`CmdSetDepthTestEnable`, `CmdSetDepthWriteEnable`, `CmdSetDepthCompareOp`,
`CmdSetCullMode`, `CmdSetFrontFace`, `CmdSetViewport`, `CmdSetScissor`, and the
`ViewportWithCount`/`ScissorWithCount` variants. EXT aliases and other setters are
not advertised. Partial viewport/scissor writes require every applicable array
element. The qualified count bound is 16. The extended commands are part of
[Vulkan 1.3](https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_extended_dynamic_state.html).

New C++ captures retain `recorded_state_inputs` fingerprints in their existing
schema-1 index: all indexed C++ sources, metadata, `Resources.h`, `data.bin` and
`data.bin.rec`. State queries require those capture-time anchors. Older captures
remain usable with existing inspection tools; recapture for the new state tools.
Imported directories are not promoted to service captures.

Under a usage lease, queries validate every fingerprint using streamed,
no-symlink, inventoried-file reads. The bound is 256 MiB per fingerprinted file,
512 MiB total and a ten-second hashing deadline. Parsed syntax is limited to
64 files, 4 MiB per file and 16 MiB total. Changed source/payload files refuse;
foreign, stale or guessed execution references refuse. References bind the
capture ID, input hashes and full selection and remain stable after restart.
These checks detect damaged/changed evidence; they do not authenticate a producer.

Lists page either `executions` or `unsupported`, at most 100 records/256 KiB.
Selected state is at most 256 KiB; escaped MCP fallback/framing stays within
1 MiB. Pure syntax and interpreted results have separate internal budgets.
Queries use the existing pin/lease/retention policy. A reference alone never pins
its capture; explicitly pin important source captures and separate byte-read
artifacts.

## Qualification and remaining work

Four fresh 0.4.5 development-build captures and independent normal launches pass
on Nsight **2026.3.1.0/build 38722833**, RTX **3080 Ti**, capture-export driver
`61571` and independent Vulkan driver observation **615.71.09**. The host uses
Linux **7.2.7-zen1-1-zen**, KDE Wayland with the fixture's XCB/Xwayland surface,
GCC **16.2.1**, Debug diagnostic GLSL/SPIR-V builds. Inputs are seed 42,
192×128, wait-frames 2, final application frame 20. The cases are `state-static`,
`state-dynamic`, `state-indexed` and `state-count`. Dynamic creation values
deliberately disagree with the setters. Each capture selects the witnessed
`frame.2` submission; seven raster/depth fields match independently recorded
API inputs, with separately audited pipeline/stage relationships. Native
fixture handles prove its own create/bind association; they are not joined to
Nsight symbols by integer equality. Eight exact vertex/fragment byte reads match retained application
SPIR-V, including capture-time database identities. MCP references/fields and
all capture/read pins survive restart. See [BUILD_VALIDATION.md](BUILD_VALIDATION.md)
for evidence identities and exact validation commands.

The C++ runner is opt-in and absent from default CTest:

```sh
cmake --build --preset linux-gcc-debug --target ngm_draw_state_integration
build/linux-gcc-debug/tests/ngm_draw_state_integration \
  /absolute/ngm-vulkan-fixture /absolute/shaders/fixture \
  /absolute/matching/nsight-install /absolute/validation-parent \
  /absolute/launch-environment.json
build/linux-gcc-debug/tests/ngm_draw_state_integration --verify-mcp \
  /absolute/nsight-graphics-mcp /absolute/ngm-resource-worker-2026_3 \
  /absolute/returned/draw-state-run
```

The first command collects independent baselines, captures and private API
witnesses; it explicitly does not qualify a query. The second compares the
private witness, actual MCP state and exact shader reads, then restarts the
server and checks stable fields/references/pins. `--inspect-retained RUN` runs
only the pure witness comparison. Private `--state-witness` fixture output stays
outside captured application evidence and never becomes a general product API.

`descriptors`, `push_constants` and `uniform_material_values` remain unavailable.
Serialized initial resources cannot prove host/GPU-updated contents at a selected
execution or actual bindless element access. Descriptor compatibility/write
history, partial constant writes, typed layouts and captured-execution
correspondence remain R-021 follow-up. No wider proprietary hydration closure is
introduced. New state support on Nsight 2026.2 is unqualified and its state query
refuses explicitly; historical two-release capabilities retain their original
scope. R-022's correlated timing/counters and I-038's graphics-format timeouts
also remain separate work. No machine permissions, clocks or display settings
are changed by this capability.
