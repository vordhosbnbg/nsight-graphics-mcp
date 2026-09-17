# R-013 build validation

Date: 2026-09-17. Product version: `0.0.1`.
Scope: the source-build foundation, with CPU-only library/compiler probes and
version-reporting entry points. No capture, rendering, or Codex interoperability
result is claimed here. The [source pins](DEPENDENCIES.md) identify all dependency
inputs; first-party files are the accompanying R-013 change.

## Toolchain policy and tested configurations

First-party C++20 is selected for standard lifetime/concurrency/data utilities,
including `std::span` and the check harness's `std::source_location`.
Vendored fastmcpp/glslang use their upstream C++17 settings. CMake 3.25 is the
minimum for preset schema 6 and `add_subdirectory(... SYSTEM)`; 3.25.3 was tested
explicitly. The tested compiler baseline is GCC 15.3 / Clang 22.1 with libstdc++;
older versions are not qualified, rather than assigned an untested compatibility
claim. CMake also enforces the C++20 feature requirement for first-party targets.

Host: Arch Linux x86-64; Ninja 1.13.2; GNU binutils; Git 2.55.0. No system
dependency packages, installed shader compiler, display, GPU, or Nsight were used
by the build/check paths.

| Configuration | Compiler | CMake | Python selected by glslang | Result |
| --- | --- | --- | --- | --- |
| `linux-gcc-debug` | GCC 16.2.1 (20260810) | 4.4.3 | 3.14.7 | Build and all 7 checks passed. |
| Clean `linux-clang-debug` snapshot | Clang 22.1.8, libstdc++ | 3.25.3 | 3.12.14 | Network-isolated configure/build and all 7 checks passed. |
| `linux-gcc-release`, explicit `gcc-15`/`g++-15` | GCC 15.3.0 | 4.4.3 | 3.14.7 | Network-isolated build and all 7 checks passed. |

GCC 15.3 Release emitted upstream glslang `-Wmaybe-uninitialized` warnings in
`TType::shallowCopy` during qualifier parsing. Vendored sources were not changed
or warnings suppressed; first-party warnings remain errors. The build and shader
checks passed. This is not a claim that all upstream shader paths were tested.

The older CMake host tool was obtained from Kitware's official 3.25.3 release and
verified against its published SHA-256 list. The x86-64 archive hash was
`d4d2ba83301b215857d3b6590cd4434a414fa151c5807693abe587bd6c03581e`.
This is a host build tool, not a substitute for source-built project dependencies.

## Commands and evidence

The working-tree GCC debug build used:

```sh
git submodule update --init --recursive
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --target ngm_check
cmake --build --preset linux-gcc-debug --target ngm_dependency_check_run
ctest --preset linux-gcc-debug
```

Because the repository started with no commits, clean-checkout validation used a
temporary local snapshot repository containing the first-party build inputs and
the six exact gitlinks. A fresh clone recursively initialized those submodules
from local source mirrors; its `git status --porcelain` was empty. This avoided
committing the user's working tree and did not reuse any build outputs.
Configuration, compilation, shader generation, and aggregate checks then ran with
networking unavailable, using the CMake 3.25.3 executable:

```sh
unshare --user --map-root-user --net cmake --preset linux-clang-debug
unshare --user --map-root-user --net cmake --build --preset linux-clang-debug
unshare --user --map-root-user --net cmake --build --preset linux-clang-debug --target ngm_check
```

`cmake` in this block denotes the verified 3.25.3 host tool. Network isolation is
a validation technique, not an ordinary build prerequisite. Normal source builds
need only the instructions in [README.md](../README.md).

The Release configuration used the second installed GCC toolchain, with the
network namespace applied to configure, build, and aggregate checks:

```sh
unshare --user --map-root-user --net cmake --preset linux-gcc-release -DCMAKE_C_COMPILER=gcc-15 -DCMAKE_CXX_COMPILER=g++-15
unshare --user --map-root-user --net cmake --build --preset linux-gcc-release
unshare --user --map-root-user --net cmake --build --preset linux-gcc-release --target ngm_check
```

The bootstrap versions both reported `0.0.1`. glslang reported 16.4.0, GLSL 4.60,
and SPIR-V 1.6. `ngm_shader_check` inspected actual generated instructions for
retained source/line data. A malformed shader was compiled over an earlier good
output; the build helper failed and removed that output, then recovered with good
source. None of this establishes Nsight shader-source inspection.

`ngm_linkage_check` inspected both entry points, the dependency-call consumer,
glslang, and the fastmcpp/volk/glslang/resource-limits archives. The observed shared
libraries were `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6`, and the
ELF loader, with no vendored shared dependencies. `ldd` and file-open tracing also
confirmed the current runtime boundary:

```sh
ldd build/linux-gcc-debug/nsight-graphics-mcp
strace -f -e trace=openat -o build/linux-gcc-debug/version-runtime.log build/linux-gcc-debug/nsight-graphics-mcp --version
strace -f -e trace=openat -o build/linux-gcc-debug/dependency-runtime.log build/linux-gcc-debug/tests/ngm_dependency_check
```

Raw CTest/linkage/runtime logs are disposable ignored build outputs. These are
build checks, not Nsight investigation or visual-verification evidence bundles.
The reproducible commands and source pins are the retained record. Runtime-loaded
Vulkan/driver/desktop/Nsight libraries are discussed separately in
[DEPENDENCIES.md](DEPENDENCIES.md); their live validation remains pending.
