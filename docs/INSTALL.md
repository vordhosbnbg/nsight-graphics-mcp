# Install and connect

## Prerequisites

- Linux x86-64, C++20 compiler/standard library, CMake 3.25+, Ninja, Python 3,
  Git, binutils, pkg-config and XCB development files.
- For GPU work: a compatible NVIDIA driver, Vulkan 1.3 loader/device and a
  separately installed Nsight Graphics release. Windowed capture needs DISPLAY
  in an existing X11/Xwayland session. See [Compatibility](COMPATIBILITY.md).
- Optional resource/descriptor workers require Linux UAPI headers and a running
  kernel supporting Landlock ABI 3 and seccomp. Optional descriptor builds also need diff.

Vendored code is built from source and linked statically. OS/compiler runtimes,
XCB, the Vulkan loader and GPU/desktop libraries remain external dependencies.
The server needs no Python runtime after building.

## Build

```sh
git clone https://github.com/vordhosbnbg/nsight-graphics-mcp.git
cd nsight-graphics-mcp
git submodule update --init --recursive
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --target ngm_check
build/linux-gcc-debug/nsight-graphics-mcp --version
```

Configure/build use initialized dependency sources without downloading anything.
Other presets are linux-clang-debug and linux-gcc-release. Change parallelism
with --parallel N and keep machine overrides in ignored CMakeUserPresets.json.
CPU checks do not qualify GPU compatibility.

## Stdio client

Configure your MCP client to launch the server with absolute paths. For Codex:

```toml
[mcp_servers.nsight_graphics]
command = "/absolute/checkout/build/linux-gcc-debug/nsight-graphics-mcp"
args = ["--artifact-root", "/absolute/managed-artifacts", "--nsight-root", "/absolute/Nsight-installation"]
env_vars = ["DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "XDG_RUNTIME_DIR", "XDG_SESSION_TYPE", "XDG_CURRENT_DESKTOP", "DBUS_SESSION_BUS_ADDRESS", "XDG_DATA_DIRS"]
```

Use a writable, user-owned artifact directory separate from source/application
files. Reconnect after changing configuration and call capabilities. Discovery
can run without an artifact root; capture/storage tools need one. Select one
Nsight installation explicitly so capture/replay tools cannot be mixed.

[Optional helpers](DEVELOPMENT.md#optional-nsight-helpers) enable qualified byte
and descriptor reads. Add the matching --resource-worker-2026-3,
--resource-worker-2026-2 or --descriptor-worker-2026-3 executable path to server
arguments. Source listing works without workers; typed draw values need both
matching resource and descriptor workers.

## Local HTTP service

HTTP is optional and exposes the same tools. Create a private token file:

```sh
python3 - <<'TOKEN'
import os, secrets
fd = os.open('http-token', os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
with os.fdopen(fd, 'w') as output:
    output.write(secrets.token_hex(32) + '\n')
TOKEN
build/linux-gcc-debug/nsight-graphics-mcp \
  --transport http --http-port 18080 \
  --http-token-file /absolute/path/to/http-token \
  --artifact-root /absolute/managed-artifacts \
  --nsight-root /absolute/Nsight-installation
```

The token file must be a regular file owned by the server user, inaccessible to
others and neither a symlink nor hard-linked. Tokens contain 32–128 ASCII
letters/digits/hyphens/underscores. The file is read at startup; rotate by restarting.

Connect to http://127.0.0.1:18080/mcp with that bearer token. For Codex:

```toml
[mcp_servers.nsight_http]
url = "http://127.0.0.1:18080/mcp"
bearer_token_env_var = "NGM_HTTP_TOKEN"
```

Supply NGM_HTTP_TOKEN to the client from the private file. All clients act as
one trusted user and share jobs/artifacts; do not expose the endpoint through
an untrusted proxy/tunnel. Only IPv4 loopback is supported. Origin headers are
refused. Session deletion/disconnect preserves jobs; SIGINT/SIGTERM shuts down
and cleans up owned jobs. [MCP.md](MCP.md#http) describes the wire contract.

## Retention and packaging

--artifact-max-bytes defaults to 2147483648 and --artifact-max-age-seconds to
2592000; zero disables the respective limit. Pins and active leases prevent
pruning. Ordinary development output is temporary; see [artifact handling](MCP.md#artifacts).
One live server owns a store at a time.

For native Arch packaging, initialize submodules and run:

```sh
cmake --preset package
cmake --build --preset package --target ngm_arch_package
```

Arch packaging also requires Python 3.11+ and the usual base-devel tools.
Run makepkg as a normal user. Outputs under build/package/arch/ include a package,
source archive, PKGBUILD, .SRCINFO and SHA256SUMS. ngm_arch_sources creates only
the source/recipe outputs. The package installs the server, capture CLI, docs and
licenses; fixtures and proprietary helpers stay separate. Review and install the
specific package with pacman -U. Packaging does not configure services or GPU permissions.
