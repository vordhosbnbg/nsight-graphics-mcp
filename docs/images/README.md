# README screenshots

`shader-before.png` and `shader-after.png` are unmodified PNG responses from
`artifact_preview_image`, showing the repository's Vulkan fixture before and
after the [basic shader repair](../SHADER_REPAIR.md). They contain only the
rendered application image, with no desktop or personal information.

The original captures used product 0.2.4 and Nsight Graphics
2026.3.1.0 / build 38722833, at 192×128 pixels, seed 42, frame 2. The previews
were validated at product 0.2.5; these images do not represent a new capture run.
The repaired screenshot's RGB pixels exactly match the reference capture.

Both PNGs are copied byte for byte from the pinned preview snapshot
`bundle-f9e131446027c6dd2a10f64c5d4a1d2b` in the local
`artifacts/nsight-repair-evidence` store. Paths below are relative to that
bundle's `raw/imported/repair/image-preview-xRg6w2/` directory:

| Documentation image | Preview file | Original capture bundle |
| --- | --- | --- |
| `shader-before.png` | `image-2-full.png` | `bundle-71416297cc50c9366d24e207b18dd195` |
| `shader-after.png` | `image-4-full.png` | `bundle-4f1f82bd71d3413d4fdbe5383282a9da` |

The source capture bundles and preview snapshot are explicitly pinned. The
snapshot's `repair-cases.json` and preview report retain the source paths and
hashes. [Image preview validation](../IMAGE_PREVIEWS.md) describes the original
checks; the small copies here make the README independent of the local store.
