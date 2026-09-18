# Retained image comparison

Product 0.2.4 adds `artifact_compare_images` (17 current MCP tools). It compares
caller-selected image files already retained in managed artifacts. It does not
infer that two captures used equivalent workloads or that a source fix succeeded.
Those conclusions require the surrounding capture/build evidence.

```json
{
  "reference": {"artifact_id": "bundle-…", "path": "raw/cpp/CppCaptures/project/screenshot.bmp"},
  "candidate": {"artifact_id": "bundle-…", "path": "raw/imported/output/image.ppm"},
  "channel_tolerance": 0
}
```

Use actual 39-character bundle IDs and inventoried paths. `channel_tolerance`
is an integer from 0 through 255, default 0. Both usage leases remain held while
files are read, decoded, hashed, and compared; automatic retention cannot remove
either input during the operation. Imports and readable failed bundles are
allowed, and each returned reference includes the artifact's status. Staging,
expired, uninventoried, escaping, symlinked, or otherwise unsafe files fail through
the normal artifact read checks. Each encoded file is limited to 16 MiB.

Supported encodings are P6 RGB8 PPM and the observed Nsight BMP profile: a 14-byte
file header followed by a 40-byte BITMAPINFOHEADER, pixel offset 54, BI_RGB 24 or 32
bits per pixel, no palette, reserved fields zero, and exact file/pixel lengths.
Both top-down and bottom-up BMP rows are supported, including four-byte row
padding. Dimensions are 1–4096 on each axis, subject to the encoded byte cap.
Other BMP headers, masks, compression, palettes, PNG/JPEG, and differing image
dimensions are rejected explicitly. This is a bounded export reader, not a
universal image library.

The BMP rules follow Microsoft's [BITMAPFILEHEADER](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapfileheader)
and [BITMAPINFOHEADER](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader)
contracts. Decoding produces top-down RGB8. It reverses BMP BGR channel storage,
ignores the unused fourth byte in 32-bit BI_RGB, and performs no alpha blending,
resizing, color-space conversion, tone mapping, or registration.

The result includes input paths/statuses, encoded sizes/SHA-256 values, decoded
RGB SHA-256 values, width/height, tolerance, differing-pixel count, maximum channel
difference, and mean absolute channel difference. A pixel differs when **any**
channel exceeds the inclusive tolerance. Maximum and mean differences always
include all channel errors, even those within tolerance. Units are RGB8 channel
steps 0–255. `matches_within_tolerance` means only that the differing-pixel count
is zero. Results identify their origin as `caller_selected_artifact_images`.

CPU checks cover whitespace-like PPM pixels, BMP row order/channel order/padding,
multiple columns, 32-bit unused bytes, malformed headers, dimensions and signed
height overflow, truncation/trailing bytes, byte limits, differing dimensions,
failed-source status, retention/path errors, MCP argument validation, tolerance,
and identical results after restart without desktop/Nsight configuration.
Real repair qualification is recorded separately in [SHADER_REPAIR.md](SHADER_REPAIR.md).
Image previews and generic deep-state/resource queries remain separate R-007 work.
