#pragma once

#include "McpClient.hpp"
#include "ShaderTraceFixture.hpp"

#include <algorithm>

namespace ngm::check::shader_trace {
using mcp::Client;
using namespace std::chrono_literals;
inline void initialize(Client& client) {
    require(client
                .request(1, "initialize",
                         {{"protocolVersion", "2025-11-25"},
                          {"capabilities", Json::object()},
                          {"clientInfo", {{"name", "shader-trace-check"}, {"version", "1"}}}})
                .contains("result"),
            "initialize shader trace MCP session");
    client.send({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
}
inline Json call(Client& client, const char* tool, Json arguments = Json::object()) {
    const auto response = client.request(20, "tools/call", {{"name", tool}, {"arguments", std::move(arguments)}});
    require(response.contains("result") && !response.at("result").value("isError", false),
            std::string(tool) + " succeeds: " + response.dump());
    const auto& result = response.at("result");
    require(result.contains("structuredContent") &&
                Json::parse(result.at("content")[0].at("text").get<std::string>()) == result.at("structuredContent"),
            "shader trace structured and text content agree");
    return result.at("structuredContent");
}
inline void error(Client& client, const char* tool, Json arguments, const std::string& code) {
    const auto response = client.request(21, "tools/call", {{"name", tool}, {"arguments", std::move(arguments)}});
    require(response.contains("result") && response.at("result").value("isError", false) &&
                response.at("result").at("content")[0].at("text").get<std::string>().starts_with(code + ":"),
            "shader trace returns typed refusal " + code + ": " + response.dump());
}
inline Json completed(Client& client, const Json& submission, std::chrono::seconds timeout = 20s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while(std::chrono::steady_clock::now() < deadline) {
        auto status = call(client, "job_status", {{"job_id", submission.at("identity").at("job_id")}});
        const auto state = status.at("state").get<std::string>();
        if(state != "queued" && state != "running" && !status.at("worker_running").get<bool>() &&
           !status.at("finalization_pending").get<bool>()) {
            require(status.at("cleanup_confirmed") == true && status.at("gpu_reserved") == false &&
                        status.at("identity") == submission.at("identity"),
                    "terminal trace capture has exact identity and confirmed cleanup");
            return status;
        }
        std::this_thread::sleep_for(10ms);
    }
    throw std::runtime_error("trace capture exceeded bounded test deadline");
}
inline void schema_members(const Json& value, const Json& schema) {
    if(schema.contains("const"))
        require(value == schema.at("const"), "result matches schema constant");
    if(schema.contains("enum"))
        require(std::find(schema.at("enum").begin(), schema.at("enum").end(), value) != schema.at("enum").end(),
                "result matches schema discriminant");
    if(value.is_object()) {
        for(const auto& key : schema.value("required", Json::array()))
            require(value.contains(key.get<std::string>()), "schema-required result member exists");
        for(const auto& [name, member] : value.items()) {
            require(schema.at("properties").contains(name), "result member belongs to closed output schema: " + name);
            schema_members(member, schema.at("properties").at(name));
        }
    } else if(value.is_array()) {
        require(!schema.contains("maxItems") || value.size() <= schema.at("maxItems").get<std::size_t>(),
                "result array fits schema bound");
        for(const auto& item : value)
            schema_members(item, schema.at("items"));
    } else if(value.is_number_integer()) {
        require(!schema.contains("minimum") || value >= schema.at("minimum"), "result meets schema minimum");
        require(!schema.contains("maximum") || value <= schema.at("maximum"), "result meets schema maximum");
    }
}
} // namespace ngm::check::shader_trace
