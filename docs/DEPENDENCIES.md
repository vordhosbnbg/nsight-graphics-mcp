# Source dependencies and runtime boundary

Initial R-013 source pins were selected and built on 2026-09-17; R-007 adds
LodePNG at 0.2.5 on 2026-09-18. Gitlinks are authoritative;
tags below describe the selected revisions, not moving update policies.
All vendored sources are unmodified submodules under `external/`. The optional
NGFX SDK and generated resource-reader helpers are separately supplied toolchain
sources, described below.

| Path | Upstream/version | Exact commit | Purpose and linkage |
| --- | --- | --- | --- |
| `external/fastmcpp` | [0xeb/fastmcpp](https://github.com/0xeb/fastmcpp), 3.4.7.1 | `29144985f51f41247584efe0c9cd2064de01b6fa` | Static MCP library; source build and real Codex capability query validated; see MCP.md. |
| `external/json` | [nlohmann/json](https://github.com/nlohmann/json), v3.11.3 | `9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03` | Header-only JSON used by fastmcpp and first-party checks. |
| `external/cpp-httplib` | [yhirose/cpp-httplib](https://github.com/yhirose/cpp-httplib), v0.15.3 | `5c00bbf36ba8ff47b4fb97712fc38cb2884e5b98` | Header-only transitive fastmcpp dependency; no optional TLS/compression libraries. |
| `external/glslang` | [KhronosGroup/glslang](https://github.com/KhronosGroup/glslang), 16.4.0 | `168d452a4f460d24b588fed08477a81c44ee27a1` | Static compiler libraries plus source-built `glslang` executable. |
| `external/vulkan-headers` | [KhronosGroup/Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers), vulkan-sdk-1.4.350.1 | `8864cdc896bbc2a9b6eb36b3218fc9ef57908d77` | Vulkan C headers; no loader or driver binary. |
| `external/volk` | [zeux/volk](https://github.com/zeux/volk), vulkan-sdk-1.4.350.1 | `3ca312a4f38baa63d8006b6905abbeeb89c8087d` | Static Vulkan entry-point loader, using the pinned headers. |
| `external/lodepng` | [lvandeve/lodepng](https://github.com/lvandeve/lodepng), header version 20260119 | `ed6fe5825c6a4fbb7f58ab35a4231c7543cd452a` | Static C++ PNG decoder/encoder with included deflate/zlib implementation; no transitive sources. |

The fastmcpp revision includes local-target support and an option to disable its
CLI. JSON/httplib match the versions requested by its fallback configuration.
Our CMake supplies `nlohmann_json::nlohmann_json` and `httplib::httplib` first, so
those fallbacks never run. The httplib target follows upstream's header-only
consumption interface instead of executing optional system-package discovery.
[Pinned fastmcpp build interface](../external/fastmcpp/CMakeLists.txt).

TLS, curl transports, curl fetching, HTTP sampling handlers, dependency tests,
examples, and fastmcpp's CLI are disabled. fastmcpp's upstream target still compiles
its HTTP implementation into the static archive; it has no switch to omit those
source files. This does not enable an HTTP service in this project. System threads
remain a required platform facility.

glslang builds GLSL and SPIR-V generation, with HLSL, optimization, external
dependencies, tests, installation, and JS support disabled. Its pinned tree
contains the generator's SPIR-V headers. SPIRV-Tools and googletest are not required
by this selected configuration and are neither discovered nor downloaded. Python 3
is required by `StandAlone/CMakeLists.txt` to generate the intrinsic header, even
though upstream's general README describes Python as optional in some builds.
[Pinned compiler build interface](../external/glslang/CMakeLists.txt),
[standalone generator](../external/glslang/StandAlone/CMakeLists.txt).

The build invokes `$<TARGET_FILE:glslang-standalone>` with
`-V --target-env vulkan1.3 -g -Od`. The CPU probe confirms SPIR-V 1.6, embedded
GLSL 460 source, and line instructions. R-005's basic renderer also exercises these
shader artifacts on the GPU; R-006 must establish useful Nsight source inspection.
No system `glslangValidator` is consulted.

Vulkan-Headers and volk use matching SDK tags. volk's system Vulkan discovery is
disabled; it links to `Vulkan::Headers` from the local tree. R-005 selects the
system XCB library for X11/Xwayland presentation as a desktop runtime exception.

LodePNG supplies PNG screenshot decoding and bounded preview encoding. Only its
`lodepng.cpp` is compiled; disk helpers, ancillary metadata decoding, tools,
examples, and upstream tests are disabled. The codec's built-in deflate/zlib code
is compiled into `ngm_lodepng`, with no system zlib lookup or download. A per-codec
allocation limit of 128 MiB complements first-party input/dimension/chunk bounds;
this is not a claim that total process memory is limited to 128 MiB. All consumers
share the same public compile definitions. `ngm_core` links the static archive,
and the ELF/archive audit includes it. The PNG profile and color/alpha limits are
in [IMAGE_COMPARISON.md](IMAGE_COMPARISON.md) and [IMAGE_PREVIEWS.md](IMAGE_PREVIEWS.md).

## Acquisition and configuration

```sh
git submodule update --init --recursive
git submodule status --recursive
```

The seven selected revisions have no nested submodules. Required transitive sources
are explicit top-level submodules. There are no project package-manager inputs or
network FetchContent fallbacks. Missing markers are checked before configuring any
dependency, and FetchContent is forced into disconnected mode as an additional
guard. A clean checkout was built in a network namespace without network access;
see [BUILD_VALIDATION.md](BUILD_VALIDATION.md).

Dependency licenses remain in their source trees: fastmcpp's Apache-2.0 license,
JSON/httplib/volk MIT licenses, glslang's collected notices in `LICENSE.txt`, and
Vulkan-Headers' `LICENSE.md`/`LICENSES/`, and LodePNG's zlib license in `LICENSE`. Consult those exact files when distributing
the corresponding sources or compiled code.

## Actual linkage and external runtimes

The ELF audit covers the server, fixture, experiment runner, dependency check
(which actually calls fastmcpp and volk), and shader compiler. It inspects `DT_NEEDED`
with `readelf`, checks the static archives with `ar`, and rejects shared libraries
outside the documented compiler/OS and fixture-only XCB runtime families. It does not merely check
`BUILD_SHARED_LIBS=OFF`. Header-only dependencies become part of their consumers.
The actual server and renderer now use their respective libraries; CPU dependency
checks still avoid Vulkan initialization.

On the tested x86-64 GNU/Linux builds, runtime requirements are glibc, libstdc++,
libgcc, libm, and the ELF dynamic loader (`ld-linux-x86-64.so.2`). Older glibc setups
may expose `libpthread`, `libdl`, or `librt` separately; these are allowed OS
exceptions. No vendored shared library, OpenSSL, curl, or installed glslang library
appeared in `DT_NEEDED`. R-013 runtime file-open tracing loaded only these
OS/compiler libraries. The fixture now also links `libxcb.so.1`; local `ldd`
inspection identifies its transitive `libXau.so.6` and `libXdmcp.so.6` desktop
dependencies. pkg-config and XCB development headers are required at build time.

Runtime-loaded libraries also matter: volk's Linux implementation opens
`libvulkan.so.1` (falling back to `libvulkan.so`) when `volkInitialize()` is called.
That is the separately installed Vulkan loader, which in turn discovers installed
driver ICDs and any enabled layers. The fixture now exercises this path during
explicit GPU runs; ordinary CPU checks continue to avoid it. Nsight's own binaries/injection libraries and the NVIDIA driver are also
separate runtime inputs, not vendored sources.

The fixture uses the development machine's existing KDE Wayland session through
Xwayland, with system XCB 1.17.0. GPU validation and provenance are recorded in
[FIXTURE.md](FIXTURE.md). Nsight injection remains to be audited with R-001.
CPU checks cannot qualify GPU runtime behavior or two-release Nsight compatibility.
The binaries are not fully static or independent of system/runtime/GPU software.

The optional R-002 fixture build can consume the header-only NGFX SDK from an
explicitly selected, separately installed Nsight toolchain. This is a GPU-toolchain
exception, not another downloaded/prebuilt vendored library. The default build
has no SDK dependency, and the MCP server does not include the SDK. Exact accepted
source-bundle fingerprints, compilation/provenance boundaries, and the distinction
between build availability and runtime qualification are in
[SDK_CONTROL.md](SDK_CONTROL.md). Nsight runtime libraries remain separately
installed inputs; no vendor binary is committed or linked as a project library.

On 2026-09-18, a successful reference run under `strace -f -e trace=openat`
recorded the actual fixture runtime path at product version 0.1.0. Besides the
OS/compiler libraries above, it opened Vulkan loader 1.4.357, NVIDIA 615.71.09
libraries (`libGLX_nvidia`, `libnvidia-glcore`, `libnvidia-glsi`,
`libnvidia-glvkspirv`, `libnvidia-gpucomp`, `libnvidia-rtcore`, `libnvidia-tls`, and
`libnvidia-allocator`), X11/XCB presentation libraries (including DRI3, GLX,
present, randr, sync, and xfixes), and driver/runtime dependencies `libdrm`,
`libdbus-1`, and `libsystemd`. These are observed system/desktop/driver runtime
inputs; they are not linked vendored project libraries or portable minimum
versions. The runner disabled implicit layers for this fixture-only run.

Trace: `build/linux-gcc-debug/fixture-runtime-0.1.0.log`. Successful run report:
`artifacts/runtime-validation/run-962c37cc1de3490c370464fd72c3960c/report.json`.
Reproduction from the repository root:

```sh
strace -f -e trace=openat -o build/linux-gcc-debug/fixture-runtime.log build/linux-gcc-debug/ngm-experiment --fixture build/linux-gcc-debug/ngm-vulkan-fixture --output-root artifacts/runtime-validation --scenario reference --seed 42 --width 192 --height 128 --frame 2
```

## Optional generated resource-reader helpers

At 0.2.11, `NGM_RESOURCE_HELPERS_2026_3` and `NGM_RESOURCE_HELPERS_2026_2` may select
local C++ capture directories produced by the two qualified Nsight releases.
`cmake/ResourceWorker.cmake` pins each of the five helper files by SHA-256 and
copies only that verified closure into the build tree. Two helper translation
units build as static libraries; no proprietary binary, generated CMake script,
replay application, network acquisition or runtime compiler is used. The worker's
`--profile` output retains exact compiled source hashes and producer identity.
These generated toolchain outputs remain outside version control and outside the
vendored submodule graph, under the scoped exception in AGENTS.md.

Optional worker builds require Linux x86-64 and UAPI headers with Landlock ABI 3
(Linux 6.2+). Default builds with older headers compile an unavailable stub;
explicit optional-worker configuration refuses those headers. Runtime confinement
requires Landlock ABI 3 and seccomp, with no unconfined fallback. Workers retain
normal libc/libstdc++/libgcc/libm and loader dependencies; the generated reader
has no Vulkan or GPU dependency. [RESOURCE_WORKER.md](RESOURCE_WORKER.md) records
commands, limits, validation and the still-unfinished MCP integration.
