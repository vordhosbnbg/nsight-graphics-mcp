#pragma once

#include "ngm/Artifacts.hpp"
#include "ngm/ResourceRead.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace ngm {
struct ServerOptions {
    std::optional<std::filesystem::path> nsight_root;
    // An empty root leaves workflow tools unavailable. Merely configuring the
    // root never opens or creates it during the handshake/capability query.
    ArtifactOptions artifacts;
    ResourceWorkers resource_workers;
    bool http = false;
    unsigned http_port = 18080;
    std::filesystem::path http_token_file;
};

struct RpcSession {
    enum class Phase { Initialize, Initialized, Ready };
    Phase phase = Phase::Initialize;
    std::string protocol_version;
};
struct ParsedRpc {
    std::optional<nlohmann::json> request;
    std::optional<nlohmann::json> error;
};
ParsedRpc parse_rpc(std::string_view input);
// One shared tool/workflow core, serialized calls and independent client lifecycle.
class ProtocolService {
public:
    explicit ProtocolService(const ServerOptions& options);
    ~ProtocolService();
    std::optional<nlohmann::json> respond(const nlohmann::json& request, RpcSession& session);
    bool shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
int serve_http(const ServerOptions& options);
int serve_stdio(const ServerOptions& options);
} // namespace ngm
