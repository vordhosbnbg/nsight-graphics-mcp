#pragma once

#include "ngm/Artifacts.hpp"

#include <filesystem>
#include <optional>

namespace ngm {
struct ServerOptions {
    std::optional<std::filesystem::path> nsight_root;
    // An empty root leaves workflow tools unavailable. Merely configuring the
    // root never opens or creates it during the handshake/capability query.
    ArtifactOptions artifacts;
};

int serve_stdio(const ServerOptions& options);
} // namespace ngm
