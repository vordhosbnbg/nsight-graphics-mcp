# Arch Linux package

Build an x86-64 Arch package containing the MCP server, the `ngm-capture` CLI,
documentation, and license notices. Vendored libraries are linked statically;
the installed executables depend on `glibc` and `gcc-libs`.

## Build

Install the build prerequisites through your usual Arch setup: `base-devel`,
`cmake`, `ninja`, `python` (3.11+), `git`, and `libxcb`. Start from a Git checkout
with its submodules initialized:

```sh
git submodule update --init --recursive
cmake --preset package
cmake --build --preset package --target ngm_arch_package
```

Run as your normal user. The target creates a source archive, then invokes
`makepkg` to configure, build, run the CPU checks, and stage the runtime files.
Dependency acquisition is confined to the explicit submodule step. Packaging
uses those sources without downloading dependencies. It does not require a GPU,
desktop session, or Nsight installation.

Outputs are in `build/package/arch/`:

| File | Purpose |
| --- | --- |
| `nsight-graphics-mcp-<version>-1-x86_64.pkg.tar.zst` | Installable package |
| `nsight-graphics-mcp-<version>.tar.gz` | Complete source snapshot, including pinned dependency sources |
| `PKGBUILD` and `.SRCINFO` | Versioned recipe and package metadata |
| `SHA256SUMS` | Source archive checksum, also embedded in the recipe |
| `*.log` | `makepkg` build, check, and packaging logs |

The version comes from the root CMake `project()` declaration. The source archive
includes current first-party edits and new, non-ignored source files; dependencies
must be clean and match their exact gitlinks. `SOURCE_PROVENANCE.json` records
the source revision, dirty state, dependency revisions, and archive timestamp.
Git metadata, builds, captures, and local configuration are excluded.

`makepkg` supplies the host's compiler and linker flags. Compilation defaults to
four jobs; set `CMAKE_BUILD_PARALLEL_LEVEL` to change that. Output paths are kept
under `build/package/arch/`, including when `makepkg.conf` specifies another
destination. Repeating the package target replaces its prior package and starts
a fresh source build.

## Install and use

After reviewing the package, install the specific output with `pacman -U`:

```sh
sudo pacman -U "build/package/arch/nsight-graphics-mcp-<version>-1-x86_64.pkg.tar.zst"
```

Replace `<version>` with the generated version. Configure your MCP client to run
`/usr/bin/nsight-graphics-mcp` with a writable, user-owned `--artifact-root` and
the appropriate `--nsight-root`. See [client setup](INSTALL.md#connect-codex-and-capture).
Documentation is installed under `/usr/share/doc/nsight-graphics-mcp/` and license
notices under `/usr/share/licenses/nsight-graphics-mcp/`.

Nsight Graphics is an optional package dependency for live capture and profiling;
retained-artifact inspection can run without it. Supply a compatible Nsight
installation, GPU driver, and the desktop prerequisites for the intended workload.
The package does not configure a service, authentication token, GPU permissions,
or a shared artifact directory.

## Share or rebuild the sources

To prepare the archive and recipe without compiling the package:

```sh
cmake --build --preset package --target ngm_arch_sources
```

Copy the archive, `PKGBUILD`, `.SRCINFO`, and `SHA256SUMS` together into a separate
directory on an Arch build host. From that directory, run `sha256sum -c SHA256SUMS`
and `makepkg --check`. The recipe builds from the archive without requiring the
original checkout or its Git metadata. It uses standard
[PKGBUILD source and checksum fields](https://man.archlinux.org/man/PKGBUILD.5.en).
The checked-in template is `packaging/arch/PKGBUILD.in`.

## Scope

This is a local package workflow; no AUR entry or hosted source release is
published. Only native Linux x86-64 builds are supported. The Vulkan fixture,
experiment runner, and shader compiler are built for validation and remain in
the build tree. Optional SDK integration and proprietary resource-reader helpers
are excluded. Build a matching [resource worker](RESOURCE_WORKER.md) separately
and pass its absolute path to the installed server when byte inspection is needed.
The existing [capture limitations](../README.md#limitations) still apply.
