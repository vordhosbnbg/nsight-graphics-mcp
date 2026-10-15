# Consecutive capture intervals

The **0.5.3** R-025 foundation exposes the existing documented Graphics Capture
interval count through the shared core, MCP and native CLI. **R-025 remains In
Progress**: this capture input support does not complete draw-timing correlation.
The shared surface remains **30 tools**.

`capture` accepts optional `frame_count`, an integer **1..600**, default **1**.
`capture_frame` still chooses the first delimiter ordinal, minimum 2, default 2.
For example, add `"capture_frame": 7, "frame_count": 3` to a normal capture
request to retain three consecutive selected delimiter intervals in one file.
The CLI spelling is `ngm-capture --frame-count 3`. No original application reuse
is introduced: each capture job still launches a fresh target.

Counts above one require matching **Nsight 2026.3.1.0/build 38722833**; older
producers refuse before launch. Single-interval historical behavior remains
available. C++ capture and ordinary profile requests cannot silently consume a
Graphics Capture interval count. The bound is a project policy; deadline,
file-size and artifact quotas still apply. The measured multi-interval GPU case
uses count three, not every count or Vulkan workload in the allowed range.

The report and compact provenance retain the exact requested count, and the
backend passes it as `--frame-count`. Publication, source SHA-256, export checks,
owned cleanup, pins and retention use the existing capture path. This does not
assert that arbitrary original application frame numbers match delimiter ordinals.

`capture_metadata` now preserves both optional producer strings:

- `captured_frame`: the single-interval field, observed as `"7"`;
- `captured_frames`: the multi-interval field, observed as `"7 - 9"`.

Missing fields remain null. The server does not split range text into IDs or
create a profile timing-column join. Replay validates both fields against a
fresh matching header, alongside its existing input snapshot/hash checks.

Serial actual MCP/CLI qualification on RTX 3080 Ti / 615.71.09 / KDE
Wayland-Xwayland uses the existing explicit matching-driver overlay. Default-one
and three-interval captures complete with all exports. Their embedded images
match independently preselected application frame-7 and frame-9 RGB8 readbacks,
respectively; CLI and MCP three-interval images are equal. Standalone replay of
the multi-interval source completes, and pins/metadata survive two restarts.
The private label fixture is a separately identified 0.5.2 experiment, not a new
public fixture mode. No installed server, helper closure or machine settings change.

[BUILD_VALIDATION.md](BUILD_VALIDATION.md) records exact source/build identities,
CPU checks and pinned qualification. I-053–I-060 preserve the separate timing
export, trigger, startup, orchestration and typed range findings. A future
source-validated replay-profile collection is implemented separately at 0.5.4;
see [REPLAY_TIMING.md](REPLAY_TIMING.md). Draw correlation still needs its own
query, independent draw timestamps and acceptance evidence.

The final serial CPU aggregate passes **37/37** checks in **368.46 seconds**.
Serial collection and final metadata/replay verification validate **154** actual
successful tool results against their respective advertised schemas. Complete/
pinned **bundle-487017236ba14e318c20a04bbc66dd5a** retains all **2,082** verified
payloads and **21** independently pinned native bundles. Collection/final code
and compressed binary identities stay separate across the additive parser change.
