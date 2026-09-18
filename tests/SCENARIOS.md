# Private fixture expectations

This file is harness/developer data. Do not include it, its expected answers, or
the analytic oracle in MCP capture/inspection responses or diagnosis prompts.
All scenarios use valid Vulkan API operations and deterministic inputs.

| Scenario | Expected visual symptom | Deliberate cause | Correct reference |
| --- | --- | --- | --- |
| `reference` | Interpolated red/green/blue triangle, dark background, seed/frame tint. | None. | Independent analytic image calculation in `FixtureIntegration.cpp`. |
| `shader-error` | Red/blue spatial contributions exchanged within the triangle. | `shader-error.frag` swaps interpolated channels before multiplying by the tint. | `reference` with identical seed, dimensions, and selected frame. |
| `binding-error` | Triangle tint differs despite the same geometry and shader calculation. | Descriptor set binds the alternate, valid uniform buffer. | `reference` with identical inputs. |
| `pipeline-error` | Red writes are missing inside the triangle; clear red remains. | Pipeline color-write mask omits red. | `reference` with identical inputs. |

| Advanced scenario | Expected visual symptom | Deliberate cause | Correct reference |
| --- | --- | --- | --- |
| `multipass-reference` | Triangle and background transformed by the post-processing pass. | None; RGBA8 offscreen scene followed by exact per-pixel fetch and color transform. | Independent two-pass analytic oracle. |
| `pass-output-error` | Red/blue input contributions change in the final pass, including the background. | The post-pass `channel_order` push constant selects BGR input before the transform. | `multipass-reference` with identical inputs. |
| `bindless-reference` | Alternating eight-pixel vertical palette bands within the triangle, with seed/frame phase. | None; runtime storage-buffer descriptor array indexed nonuniformly per fragment. | Independent palette-selection oracle. |
| `resource-selection-error` | The two palette bands exchange their tint. | The resource selection push constant XORs the valid index with one. | `bindless-reference` with identical inputs. |
| `indirect-reference` | Two half-width triangles, separately positioned left and right. | None; one indirect draw command requests two instances. | Independent instanced-geometry oracle. |
| `indirect-parameter-error` | The right triangle is absent. | The valid indirect command sets `instanceCount` to one instead of two. | `indirect-reference` with identical inputs. |
| `combined-reference` | Two banded triangles rendered offscreen then post-processed. | None; indirect geometry, bindless palette access, and the two-pass path all execute. | Independent combined oracle. |
| `combined-pass-error` | Post-process red/blue input contributions change. | Same channel-order push-constant variation as `pass-output-error`, with all combined paths enabled. | `combined-reference` with identical inputs. |
| `combined-resource-error` | Palette bands exchange tint before post-processing. | Same bounded resource-index XOR as `resource-selection-error`, with all combined paths enabled. | `combined-reference` with identical inputs. |
| `combined-indirect-error` | The right triangle is absent from the post-processed scene. | Same valid one-instance indirect command as `indirect-parameter-error`, with all combined paths enabled. | `combined-reference` with identical inputs. |

The primary tint is `(0.65 + 0.35 * seedByte / 255) * (0.8 + 0.02 * (frame % 11))`
per channel. The alternate tint is `(primary.b * 0.25, primary.r, primary.g * 0.5)`.
The bindless palette index is `((pixelX / 8) ^ ((seed ^ frame) & 1) ^ resourceXor) & 1`.
Indirect geometry scales the original triangle's X by 0.5 and translates it by
`-0.5 + instance`. The post pass samples the quantized offscreen pixel and applies
scale `(0.75, 0.875, 0.5)` plus bias `(0.03125, 0.015625, 0.0625)`.
These equations are harness data, never an application result or MCP diagnosis.

Every comparison records its tolerance and input tuple. Exact fresh-launch
repeatability covers all pixels; the independent analytic comparison allows one
RGB8 channel step for single-pass output and two for the two-pass path, accounting
for its intermediate and final UNORM conversions. It excludes pixels whose
minimum barycentric coordinate is within `2 / min(width, height)` of zero for a
rendered triangle. Fault scenarios
must differ from their corresponding reference on more than one tenth of pixels.
The shader failure check separately proves a failed compilation removes previous
SPIR-V output. A repair test must edit/rebuild the responsible source and obtain
new evidence; choosing `reference` is not a source repair.

The hardware harness defaults to `--suite basic`, preserving the 17-launch basic
matrix. `--suite advanced` covers the ten advanced scenarios twice at each of two
input tuples (40 launches). `--suite all` covers fourteen scenarios plus the
shader override (57 launches). A prerequisite failure remains `unsupported` in its
row and fails the aggregate; it is never treated as a passing substitute workload.
Advanced cases are pending real build/review/GPU validation when introduced; this
file defines their contract, not an observed compatibility claim.
