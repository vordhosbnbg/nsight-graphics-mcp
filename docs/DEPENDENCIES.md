# Source dependencies and runtime boundary

R-013 source pins, selected and built on 2026-09-17. Gitlinks are authoritative;
tags below describe the selected revisions, not moving update policies.
All sources are unmodified submodules under `external/`.

| Path | Upstream/version | Exact commit | Purpose and linkage |
| --- | --- | --- | --- |
| `external/fastmcpp` | [0xeb/fastmcpp](https://github.com/0xeb/fastmcpp), 3.4.7.1 | `29144985f51f41247584efe0c9cd2064de01b6fa` | Static MCP library; build validated, client interoperability pending R-003. |
| `external/json` | [nlohmann/json](https://github.com/nlohmann/json), v3.11.3 | `9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03` | Header-only JSON used by fastmcpp and first-party checks. |
| `external/cpp-httplib` | [yhirose/cpp-httplib](https://github.com/yhirose/cpp-httplib), v0.15.3 | `5c00bbf36ba8ff47b4fb97712fc38cb2884e5b98` | Header-only transitive fastmcpp dependency; no optional TLS/compression libraries. |
| `external/glslang` | [KhronosGroup/glslang](https://github.com/KhronosGroup/glslang), 16.4.0 | `168d452a4f460d24b588fed08477a81c44ee27a1` | Static compiler libraries plus source-built `glslang` executable. |
| `external/vulkan-headers` | [KhronosGroup/Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers), vulkan-sdk-1.4.350.1 | `8864cdc896bbc2a9b6eb36b3218fc9ef57908d77` | Vulkan C headers; no loader or driver binary. |
| `external/volk` | [zeux/volk](https://github.com/zeux/volk), vulkan-sdk-1.4.350.1 | `3ca312a4f38baa63d8006b6905abbeeb89c8087d` | Static Vulkan entry-point loader, using the pinned headers. |

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
GLSL 460 source, and line instructions. This target version is a build probe;
R-005 must validate its actual rendering features, and R-006 must establish useful
Nsight source inspection. No system `glslangValidator` is consulted.

Vulkan-Headers and volk use matching SDK tags. volk's system Vulkan discovery is
disabled; it links to `Vulkan::Headers` from the local tree. This establishes the
native build boundary without choosing a window-system library for R-005.

## Acquisition and configuration

```sh
git submodule update --init --recursive
git submodule status --recursive
```

The six selected revisions have no nested submodules. Required transitive sources
are explicit top-level submodules. There are no project package-manager inputs or
network FetchContent fallbacks. Missing markers are checked before configuring any
dependency, and FetchContent is forced into disconnected mode as an additional
guard. A clean checkout was built in a network namespace without network access;
see [BUILD_VALIDATION.md](BUILD_VALIDATION.md).

Dependency licenses remain in their source trees: fastmcpp's Apache-2.0 license,
JSON/httplib/volk MIT licenses, glslang's collected notices in `LICENSE.txt`, and
Vulkan-Headers' `LICENSE.md`/`LICENSES/`. Consult those exact files when distributing
the corresponding sources or compiled code.

## Actual linkage and external runtimes

The current ELF audit covers both entry points, the dependency check (which
actually calls fastmcpp and volk), and the shader compiler. It inspects `DT_NEEDED`
with `readelf`, checks the static archives with `ar`, and rejects shared libraries
outside the documented compiler/OS runtime families. It does not merely check
`BUILD_SHARED_LIBS=OFF`. Header-only dependencies become part of their consumers.
The bootstrap entry points do not yet exercise MCP or Vulkan; the dependency check
ensures that real library calls also link successfully.

On the tested x86-64 GNU/Linux builds, runtime requirements are glibc, libstdc++,
libgcc, libm, and the ELF dynamic loader (`ld-linux-x86-64.so.2`). Older glibc setups
may expose `libpthread`, `libdl`, or `librt` separately; these are allowed OS
exceptions. No vendored shared library, OpenSSL, curl, or installed glslang library
appeared in `DT_NEEDED`. Runtime file-open tracing of the version executable and
dependency check loaded only these OS/compiler libraries.

Runtime-loaded libraries also matter: volk's Linux implementation opens
`libvulkan.so.1` (falling back to `libvulkan.so`) when `volkInitialize()` is called.
That is the separately installed Vulkan loader, which in turn discovers installed
driver ICDs and any enabled layers. The current executables/checks never call that
initializer; this is a source-verified future runtime requirement, not a successful
GPU run. Nsight's own binaries/injection libraries and the NVIDIA driver are also
separate runtime inputs, not vendored sources.

Desktop libraries, the actual window-system path, driver/layer libraries, and
Nsight injection must be audited again when R-005/R-001 exercise them. These CPU
checks cannot qualify that runtime closure or two-release Nsight compatibility.
The binaries are not fully static or independent of system/runtime/GPU software.
