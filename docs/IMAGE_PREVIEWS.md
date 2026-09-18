# Retained image previews

At **0.2.5**, `artifact_preview_image` returns an actual MCP PNG image block plus
structured/text metadata identifying its retained source, crop, sampling, and
pixel hashes. This is the eighteenth tool. It uses the same leased, inventoried
file reads as image comparison, without launching Nsight or using a GPU.
Imports and readable failed bundles are allowed and retain their explicit status.

```json
{
  "artifact_id": "bundle-…",
  "path": "raw/exports/screenshot.png",
  "max_edge": 384,
  "region": {"x": 20, "y": 16, "width": 96, "height": 64}
}
```

Use an actual returned bundle ID and inventoried path. `region` is optional and
defaults to the complete source; all four integer fields are required when it is
present. Coordinates are zero-based from the decoded image's top left. Width and
height must be positive, and the whole region must fit the image. `max_edge` is
an integer from 1 to 384, default 384. Input files are limited to 16 MiB and source
dimensions to 4096×4096. Expired, staging, uninventoried, escaping, and symlinked
inputs fail through the artifact read contract.

Supported sources are P6 RGB8 PPM, the observed 24/32-bit BI_RGB BMP profile, and
static opaque RGB/RGBA8 PNG, including interlacing. PNG is decoded with pinned,
source-built LodePNG. Transparent PNG, 16-bit/palette/grayscale PNG, APNG, JPEG,
and other BMP layouts fail explicitly. [IMAGE_COMPARISON.md](IMAGE_COMPARISON.md)
records the format and alpha rules. Stored RGB values are preserved; color,
physical, and orientation metadata is ignored. No gamma/profile correction,
alpha compositing, tone mapping, or image registration occurs. A PNG preview
contains no source ancillary metadata, so it does not promise colorimetric
agreement with a color-managed viewer of the original.

## Crop and sampling contract

For a crop of width `w`, height `h`, and requested edge `e`, let
`d = max(w, h, e)`. Output width is `max(1, floor(w*e/d))`, and output height is
`max(1, floor(h*e/d))`. This preserves aspect ratio subject to integer rounding,
never enlarges a crop, and fits within `e` on each axis.
For output pixel `(u,v)`, the original source coordinate is:

```text
x = region.x + floor(u * region.width  / output.width)
y = region.y + floor(v * region.height / output.height)
```

The declared sampling name is `nearest_neighbor_top_left`. It is deliberately
specified by coordinates: other image libraries may use center-based nearest
sampling. A downsampled preview can omit thin defects. Request an appropriate
crop to examine its pixels, and use `artifact_compare_images` on the original
files for full-resolution verification. Preview equality does not prove original
image equality or workload equivalence.

The result has one JSON text block, the equivalent `structuredContent`, and one
`image` content block with base64 PNG and MIME type `image/png`. Metadata includes
source artifact/path/status, encoded source size/hash, full decoded source RGB
hash, original dimensions, selected region, output dimensions, requested edge,
`resampled`, PNG byte count/hash, and preview RGB hash. Neither the PNG nor its
base64 representation is duplicated in structured/text metadata.

The PNG encoder emits RGB8, filter-zero rows, and stored deflate blocks. The
384×384 cap keeps the maximum encoded image below 512 KiB and the complete MCP
reply below the existing 1 MiB limit, including base64 and text fallback. The
source stays on disk unchanged; the preview is a derived in-memory response.

## Executed retained-file validation

The opt-in C++ `ngm_retained_image_integration` target is excluded from default
builds, CTest, and the CPU aggregate because it requires explicit retained
artifacts. It runs without desktop variables and with `PATH=/nonexistent`:

```sh
cmake --build --preset linux-gcc-debug --target ngm_retained_image_integration
build/linux-gcc-debug/tests/ngm_retained_image_integration \
  build/linux-gcc-debug/nsight-graphics-mcp \
  artifacts/nsight-evidence \
  build/image-preview-validation/graphics-cases.json \
  build/image-preview-validation/graphics
```

A second invocation uses `artifacts/nsight-repair-evidence`, `repair-cases.json`,
and the `repair` run root. The case files contain explicit artifact IDs, relative
image paths, and labels. They refer to the already pinned two-release evidence
from [NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md) and
[SHADER_REPAIR.md](SHADER_REPAIR.md); they are retained validation inputs, not
clean-checkout fixtures. Every referenced source remains explicitly pinned in
its original store.

Both invocations pass: **18 retained originals** (4 PNG graphics screenshots,
6 BMP C++ screenshots, and 8 application PPMs) produce **36 previews**, one full
and one cropped/downsampled per image. The ten Nsight captures belong to matching
2026.3.1.0/build 38722833 or 2026.2.0.0/build 37991608 producers. This exercises
new retained-image tools on existing evidence; no new capture, replay, GPU run,
or autonomous Codex-client invocation occurs.

The harness records 72 retained-read tool calls and two initialize requests, source hashes, complete MCP
responses, PNG outputs, and clean EOF. Its original-image comparisons compare
an image to itself and establish transport/decoding behavior only. Separate
fresh-context review uses Pillow and independent crop/index calculations to
check all 36 output PNGs against their original files and metadata. It also
performs 18 independent full-resolution cross-image checks, confirming retained
application/capture correspondence and the already established basic repair
results. These checks do not create additional repair qualification.

CPU coverage includes independent synthetic PNG fixtures generated with Python
standard-library zlib (the generator is not a test prerequisite), alpha and
`tRNS`, interlacing, corrupt ancillary CRC, APNG, inflated-data limits, dimensions,
truncation/trailing bytes, known-pixel crop/sampling, response bounds, invalid MCP
arguments, failed/import status, and identical preview responses after restart.
See [BUILD_VALIDATION.md](BUILD_VALIDATION.md) for executed logs and build scope.

The complete validation snapshot is explicitly pinned as
**`bundle-f9e131446027c6dd2a10f64c5d4a1d2b`** in
**`artifacts/nsight-repair-evidence`**. Its 70 files retain both reports, exact
invocation/exit records, all PNGs/transcripts, harness source, implementation
source snapshot, LodePNG pin, and the independent audit script/results. Source
artifacts retain their existing pins in the two original stores.
