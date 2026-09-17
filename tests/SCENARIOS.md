# Private fixture expectations

This file is harness/developer data. Do not include it, its expected answers, or
the analytic oracle in MCP capture/inspection responses or diagnosis prompts.
The initial scenarios use valid Vulkan API operations and deterministic inputs.

| Scenario | Expected visual symptom | Deliberate cause | Correct reference |
| --- | --- | --- | --- |
| `reference` | Interpolated red/green/blue triangle, dark background, seed/frame tint. | None. | Independent analytic image calculation in `FixtureIntegration.cpp`. |
| `shader-error` | Red/blue spatial contributions exchanged within the triangle. | `shader-error.frag` swaps interpolated channels before multiplying by the tint. | `reference` with identical seed, dimensions, and selected frame. |
| `binding-error` | Triangle tint differs despite the same geometry and shader calculation. | Descriptor set binds the alternate, valid uniform buffer. | `reference` with identical inputs. |
| `pipeline-error` | Red writes are missing inside the triangle; clear red remains. | Pipeline color-write mask omits red. | `reference` with identical inputs. |

Every comparison records its tolerance and input tuple. Exact fresh-launch
repeatability covers all pixels; the independent analytic comparison allows one
RGB8 channel step and excludes a narrow rasterization-edge band. Fault scenarios
must differ from their corresponding reference on more than one tenth of pixels.
The shader failure check separately proves a failed compilation removes previous
SPIR-V output. A repair test must edit/rebuild the responsible source and obtain
new evidence; choosing `reference` is not a source repair.
