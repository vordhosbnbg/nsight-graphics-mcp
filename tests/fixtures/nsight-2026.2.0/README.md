# Nsight Graphics 2026.2.0 metadata fixtures

These are sanitized real exports from explicitly pinned bundles read on
2026-09-18. Capture and replay tools report **2026.2.0.0/build 37991608**;
metadata reports version integer **1**, `nsight_version: "2026.2.0"`, build
string `"37991608"`, and primary API `"Vulkan"`.

| Prefix | Scenario | Pinned source bundle |
| --- | --- | --- |
| `reference-` | `reference` | `bundle-9c1f4095228eaf248119d63c9604ee86` |
| `shader-error-` | `shader-error` | `bundle-49d90463ab1842ecb147e4c511fd4a91` |

The windowed fixture used RTX 3080 Ti, driver 615.71.09, KDE Wayland/Xwayland,
seed 42, 192x128, final application frame 20, and Nsight capture frame 2.
The complete matrix and provenance are in [NSIGHT_VALIDATION.md](../../../docs/NSIGHT_VALIDATION.md).

Each input comes from `raw/exports/<kind>.raw`. JSON was parsed and serialized
with two-space indentation and a trailing newline. Functions and objects retain
all entries, fields, values, labels, and order. Only metadata process context
was sanitized: executable, `--output`, and `--shader-dir` paths became
`/fixture/ngm-vulkan-fixture`, `/fixture/artifacts`, and `/fixture/shaders`;
`process_environment` became an empty array. The remaining arguments and their
order, capture UUID/time, platform text, and metadata collection are unchanged.

Each capture contains 22 events (10 with sequence IDs) and 32 objects. Function
exports match the corresponding 2026.3 source bytes. The 2026.2 metadata
collection has six info entries and no warnings member; absence must remain
explicit. Semaphore UID 14 and fence UIDs 40/42 have access value 0, where the
2026.3 fixtures contain 524288. Those integers remain opaque. Shader names differ
between correct/faulty samples without supplying bytes or event associations.

| Fixture | Original raw SHA-256 |
| --- | --- |
| `reference-metadata.json` | `fb973a3dd9250c24678c539a4debad1e5b1e0e15618102363b46410d2ee13219` |
| `reference-functions.json` | `5331b6dface10342f0d46cc8f5190ac89003436d931eafc2e2bd361c23b34a02` |
| `reference-objects.json` | `6372889ad50a07649a5e6479a2cef287b420805eaebb33017a95e6b9bcec138a` |
| `shader-error-metadata.json` | `84d4704b6e87916c8ae71a3eb7177b3e409c725523d64e6ee3248779a8dcff42` |
| `shader-error-functions.json` | `5331b6dface10342f0d46cc8f5190ac89003436d931eafc2e2bd361c23b34a02` |
| `shader-error-objects.json` | `42182cca7fe533f3774dafdc54e057fa9c00eae92ae46af067491805a6b64799` |

These small fixtures contain no proprietary captures, images, environment values,
local host paths, or shader sources. They establish observed parsing inputs;
GPU replay, detailed state, source repair, and advanced workload support require
separate evidence. Synthetic malformed/profile-mismatch cases belong in the C++
checks, not in files attributed to real exports.

## Advanced indirect inventory samples

The following real exports are from the 0.2.1 workload matrix on 2026-09-18.
Each source is complete and explicitly pinned. Workload inputs remain seed 42,
192x128, and capture frame 2; the fixture now retains seven shader sources/SPIR-V.
Metadata uses the same sanitization described above, additionally replacing any
`--shader-dir` path with `/fixture/shaders`. Functions and objects are unchanged
apart from JSON whitespace.

| Prefix | Pinned source bundle |
| --- | --- |
| `indirect-reference-` | `bundle-066edc64d62bf21ce511090365d494ea` |
| `indirect-parameter-error-` | `bundle-274143de5c12b175a612dc20287fdf8c` |

| Fixture | Original raw SHA-256 |
| --- | --- |
| `indirect-reference-metadata.json` | `7dd29ddd8702f2696c77fa2edb1f6f92908742eb3abc04810ab9cf3d23a30aa2` |
| `indirect-reference-functions.json` | `140518f3c64ab83f59cd05c6f3a579dbbbd51fc26850cd91ba313b4818bce294` |
| `indirect-reference-objects.json` | `0f5cf66168533be5a4bbab8b80363bd7cf3a77ee7e59aef46307e5bdf5e168cd` |
| `indirect-parameter-error-metadata.json` | `f589fc3b16216c610fef8ff94eac177496c9f64bd3f665a788ba4bcee08e8342` |
| `indirect-parameter-error-functions.json` | `140518f3c64ab83f59cd05c6f3a579dbbbd51fc26850cd91ba313b4818bce294` |
| `indirect-parameter-error-objects.json` | `0f5cf66168533be5a4bbab8b80363bd7cf3a77ee7e59aef46307e5bdf5e168cd` |

Each functions array contains 23 records, including `vkCmdDrawIndirect` at
event 14 and `vkCmdDraw` at event 15. The 2026.3.1 producer additionally emits
`indirect_index: 0` on event 15; 2026.2.0 omits that member. Retain its presence
and unsigned value without inferring parent/child links, arguments, buffer
contents, or command expansion semantics. Both object arrays contain 34 entries.
