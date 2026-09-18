#include "Server.hpp"
#include "Workflow.hpp"

#include "ngm/Capabilities.hpp"
#include "ngm/Version.hpp"

#include <fastmcpp/mcp/handler.hpp>
#include <fastmcpp/tools/manager.hpp>

#include <algorithm>
#include <csignal>
#include <functional>
#include <iostream>
#include <string>

namespace ngm {
namespace {
using Json = fastmcpp::Json;

struct JsonNestingLimit {};

bool limit_json_nesting(int depth, Json::parse_event_t event, Json&) {
    // Callback depth counts already-open containers, starting at zero. Throw
    // before opening the 65th container so parsing, copies, and destruction all
    // operate on a bounded DOM, including when the parser unwinds this failure.
    if(depth >= 64 && (event == Json::parse_event_t::object_start || event == Json::parse_event_t::array_start)) {
        throw JsonNestingLimit{};
    }
    return true;
}

Json capability_report(const ServerOptions& options, const std::string& protocol_version) {
    const auto observations = discover_prerequisites(options.nsight_root);
    Json executables = Json::array();
    for(const auto& executable : observations.executables) {
        executables.push_back({{"name", executable.name},
                               {"executable_found", executable.path.has_value()},
                               {"path", executable.path ? Json(executable.path->string()) : Json(nullptr)}});
    }
    const auto unavailable = [](const char* reason) {
        return Json{{"available", false}, {"status", "not_implemented"}, {"reason", reason}};
    };
    const bool storage = !options.artifacts.root.empty();
    const bool tool_paths = std::all_of(observations.executables.begin(), observations.executables.end(),
                                        [](const auto& item) { return item.path.has_value(); });
    const bool desktop = observations.display_present || observations.wayland_display_present;
    const bool capture_ready = storage && tool_paths && desktop;
    const Json capture{{"available", capture_ready},
                       {"status", capture_ready ? "prerequisites_observed" : "missing_prerequisites"},
                       {"reason",
                        "Implemented asynchronous fresh-process capture. Requires --artifact-root, matching "
                        "Nsight tools, a working desktop, and a compatible GPU/driver. Paths/environment "
                        "only establish observable prerequisites; each job checks tools and retains results. "
                        "Captures one delimiter interval, using presentation by default. Optional "
                        "graphics_capture_api boundaries require application SDK initialization and calls; "
                        "qualification is specific to the application, SDK, and matching Nsight release. "
                        "vk_frame_boundary requires an application-enabled VK_EXT_frame_boundary feature and "
                        "frame-end submissions; the measured no-presentation profile is Nsight 2026.3.1.0 only. "
                        "capture_cpp selects documented Generate C++ Capture with its separate wait_frames "
                        "control; it retains generated source/data on qualified 2026.2/2026.3 builds. "
                        "That mode has no SDK delimiter or automatic replay. Resource reads are separate requests."}};
    const Json store{{"available", storage},
                     {"status", storage ? "configured" : "missing_prerequisites"},
                     {"reason", storage ? "Implemented; storage is opened lazily by a workflow call. "
                                          "The configured path has not been validated by this query."
                                        : "Launch with --artifact-root ABS_PATH to enable workflow tools."}};
    return {
        {"server",
         {{"name", "nsight-graphics-mcp"},
          {"version", project_version()},
          {"transport", "stdio"},
          {"protocol_version", protocol_version}}},
        {"implemented_tools", implemented_tool_names()},
        {"operations",
         {{"capture", capture},
          {"jobs", store},
          {"artifacts", store},
          {"inspection",
           {{"available", storage},
            {"status", storage ? "retained_exports_only" : "missing_prerequisites"},
            {"reason", "Implemented bounded metadata/event/object inventories for complete retained server captures "
                       "from observed Nsight 2026.3.1.0 build 38722833 or 2026.2.0.0 build 37991608 Vulkan exports. "
                       "Also compares and previews retained P6/PNG/BMP images without inferring workload equivalence. "
                       "Requires --artifact-root; "
                       "inventory queries validate their bundle and producer. Generated C++ queries return numbered "
                       "source and literal draw/pipeline/shader relationships for qualified recording forms, with "
                       "explicit unsupported coverage. Literal resource references can be listed without a reader. "
                       "Separate resource byte queries require a configured qualified worker for the producer. "
                       "Source relationships and serialized bytes do not establish executed GPU state or descriptor "
                       "selection."}}},
          {"resource_bytes",
           {{"available", storage && (!options.resource_workers.nsight_2026_3.empty() ||
                                      !options.resource_workers.nsight_2026_2.empty())},
            {"status", "configuration_observed"},
            {"reason", "Each read requires a matching configured worker, exact qualified capture helpers and Linux "
                       "confinement. Configuration does not verify the binary or kernel support; the read checks both. "
                       "Each read retains database snapshots, output and logs as a quota-managed artifact."}}},
          {"profiling", unavailable("GPU profiling and metric extraction are pending.")},
          {"fixture_via_mcp",
           unavailable("No fixture-specific tool; the generic capture tool accepts its absolute executable path.")}}},
        {"prerequisites",
         {{"platform", "linux"},
          {"artifact_store",
           {{"configured", storage},
            {"root", storage ? Json(options.artifacts.root.string()) : Json(nullptr)},
            {"max_bytes", options.artifacts.max_bytes},
            {"max_age_seconds", options.artifacts.max_age.count()}}},
          {"nsight",
           {{"discovery_source", observations.discovery_source},
            {"root_override", observations.nsight_root ? Json(observations.nsight_root->string()) : Json(nullptr)},
            {"executables", executables},
            {"version", nullptr},
            {"compatibility", "not_verified"},
            {"note", "Paths are executable-file observations only; this query runs no Nsight command or capture. "
                     "PATH entries may belong to different releases; select --nsight-root for one installation."}}},
          {"resource_readers",
           {{"nsight_2026_3", options.resource_workers.nsight_2026_3.empty()
                                  ? Json(nullptr)
                                  : Json(options.resource_workers.nsight_2026_3.string())},
            {"nsight_2026_2", options.resource_workers.nsight_2026_2.empty()
                                  ? Json(nullptr)
                                  : Json(options.resource_workers.nsight_2026_2.string())}}},
          {"desktop",
           {{"display_environment_present", observations.display_present},
            {"wayland_display_environment_present", observations.wayland_display_present},
            {"connection", "not_verified"}}},
          {"gpu", {{"status", "not_probed"}, {"name", nullptr}, {"driver", nullptr}}}}},
        {"next_step", "Use --nsight-root with an existing absolute installation directory if tool paths are "
                      "missing. Configure --artifact-root, submit capture with absolute application paths, then poll "
                      "job_status and inspect retained artifact files. Detailed state queries, diagnosis, and real "
                      "release compatibility require separately recorded validation."}};
}

Json output_schema() {
    auto schema = Json::parse(R"json({
        "type": "object",
        "additionalProperties": false,
        "required": ["server", "implemented_tools", "operations", "prerequisites", "next_step"],
        "properties": {
            "server": {
                "type": "object", "additionalProperties": false,
                "required": ["name", "version", "transport", "protocol_version"],
                "properties": {
                    "name": {"type": "string"}, "version": {"type": "string"},
                    "transport": {"type": "string", "enum": ["stdio"]},
                    "protocol_version": {"type": "string"}
                }
            },
            "implemented_tools": {"type": "array", "items": {"type": "string"}},
            "operations": {
                "type": "object", "additionalProperties": false,
                "required": ["capture", "inspection", "profiling", "fixture_via_mcp"],
                "properties": {
                    "capture": {"$ref": "#/$defs/operation"},
                    "inspection": {"$ref": "#/$defs/operation"},
                    "profiling": {"$ref": "#/$defs/operation"},
                    "fixture_via_mcp": {"$ref": "#/$defs/operation"}
                }
            },
            "prerequisites": {
                "type": "object", "additionalProperties": false,
                "required": ["platform", "nsight", "desktop", "gpu"],
                "properties": {
                    "platform": {"type": "string", "enum": ["linux"]},
                    "nsight": {
                        "type": "object", "additionalProperties": false,
                        "required": ["discovery_source", "root_override", "executables", "version", "compatibility", "note"],
                        "properties": {
                            "discovery_source": {"type": "string"},
                            "root_override": {"type": ["string", "null"]},
                            "version": {"type": ["string", "null"]},
                            "compatibility": {"type": "string"},
                            "note": {"type": "string"},
                            "executables": {"type": "array", "items": {
                                "type": "object", "additionalProperties": false,
                                "required": ["name", "executable_found", "path"],
                                "properties": {
                                    "name": {"type": "string"},
                                    "executable_found": {"type": "boolean"},
                                    "path": {"type": ["string", "null"]}
                                }
                            }}
                        }
                    },
                    "desktop": {
                        "type": "object", "additionalProperties": false,
                        "required": ["display_environment_present", "wayland_display_environment_present", "connection"],
                        "properties": {
                            "display_environment_present": {"type": "boolean"},
                            "wayland_display_environment_present": {"type": "boolean"},
                            "connection": {"type": "string"}
                        }
                    },
                    "gpu": {
                        "type": "object", "additionalProperties": false,
                        "required": ["status", "name", "driver"],
                        "properties": {
                            "status": {"type": "string"},
                            "name": {"type": ["string", "null"]},
                            "driver": {"type": ["string", "null"]}
                        }
                    }
                }
            },
            "next_step": {"type": "string"}
        },
        "$defs": {"operation": {
            "type": "object", "additionalProperties": false,
            "required": ["available", "status", "reason"],
            "properties": {
                "available": {"type": "boolean"},
                "status": {"type": "string"},
                "reason": {"type": "string"}
            }
        }}
    })json");
    for(const auto* operation : {"jobs", "artifacts", "resource_bytes"}) {
        schema["properties"]["operations"]["required"].push_back(operation);
        schema["properties"]["operations"]["properties"][operation] = {{"$ref", "#/$defs/operation"}};
    }
    schema["properties"]["prerequisites"]["required"].push_back("artifact_store");
    schema["properties"]["prerequisites"]["properties"]["artifact_store"] = {
        {"type", "object"},
        {"additionalProperties", false},
        {"required", {"configured", "root", "max_bytes", "max_age_seconds"}},
        {"properties",
         {{"configured", {{"type", "boolean"}}},
          {"root", {{"type", {"string", "null"}}}},
          {"max_bytes", {{"type", "integer"}, {"minimum", 0}}},
          {"max_age_seconds", {{"type", "integer"}, {"minimum", 0}}}}}};
    schema["properties"]["prerequisites"]["required"].push_back("resource_readers");
    schema["properties"]["prerequisites"]["properties"]["resource_readers"] = {
        {"type", "object"},
        {"additionalProperties", false},
        {"required", {"nsight_2026_3", "nsight_2026_2"}},
        {"properties",
         {{"nsight_2026_3", {{"type", {"string", "null"}}}}, {"nsight_2026_2", {{"type", {"string", "null"}}}}}}};
    return schema;
}

Json rpc_error(const Json& request, int code, const char* message) {
    const bool usable_id = request.is_object() && request.contains("id") &&
                           (request["id"].is_string() || request["id"].is_number_integer());
    return {{"jsonrpc", "2.0"},
            {"id", usable_id ? request["id"] : Json(nullptr)},
            {"error", {{"code", code}, {"message", message}}}};
}

// fastmcpp handles tool routing and results. Validate envelope types here because
// its generic handler classifies nlohmann type errors as internal errors.
std::optional<Json> validate_request(const Json& request) {
    if(!request.is_object() || !request.contains("jsonrpc") || request["jsonrpc"] != "2.0" ||
       !request.contains("method") || !request["method"].is_string() ||
       (request.contains("id") && !request["id"].is_string() && !request["id"].is_number_integer())) {
        return rpc_error(request, -32600, "Expected a JSON-RPC 2.0 request with a string method.");
    }
    if(request.contains("params") && !request["params"].is_object()) {
        return rpc_error(request, -32602, "Method params must be an object.");
    }
    const auto params = request.value("params", Json::object());
    if(params.contains("_meta") && !params["_meta"].is_object()) {
        return rpc_error(request, -32602, "Request _meta must be an object.");
    }
    if(request["method"] == "initialize") {
        if(!params.contains("protocolVersion") || !params["protocolVersion"].is_string() ||
           !params.contains("capabilities") || !params["capabilities"].is_object() || !params.contains("clientInfo") ||
           !params["clientInfo"].is_object() || !params["clientInfo"].contains("name") ||
           !params["clientInfo"]["name"].is_string() || !params["clientInfo"].contains("version") ||
           !params["clientInfo"]["version"].is_string()) {
            return rpc_error(request, -32602, "initialize requires protocolVersion, capabilities, and clientInfo.");
        }
    }
    if(request["method"] == "tools/call") {
        if(!params.contains("name") || !params["name"].is_string() || params["name"].get<std::string>().empty() ||
           (params.contains("arguments") && !params["arguments"].is_object())) {
            return rpc_error(request, -32602, "tools/call requires a string name and object arguments.");
        }
        for(const auto& [key, value] : params.items()) {
            (void)value;
            if(key != "name" && key != "arguments" && key != "_meta") {
                return rpc_error(request, -32602, "Unknown tools/call parameter.");
            }
        }
    }
    return std::nullopt;
}

// The pinned StdioServerWrapper maps parse failures to internal errors and has
// no input-size limit. Keep framing here, and use the documented fastmcpp handler
// for negotiation, tool listing/invocation, and MCP result construction.
int run_stdio(const std::function<Json(const Json&)>& handler, std::string& protocol_version) {
    constexpr std::size_t maximum_line_bytes = 64 * 1024;
    enum class Phase { Initialize, Initialized, Ready };
    auto phase = Phase::Initialize;
    std::signal(SIGPIPE, SIG_IGN);
    const auto respond = [](const Json& response) {
        std::cout << response.dump() << '\n';
        std::cout.flush();
        return static_cast<bool>(std::cout);
    };
    const auto process = [&](const std::string& line, bool oversized) {
        if(oversized) {
            return respond(rpc_error(nullptr, -32600, "Request exceeds the 65536-byte line limit."));
        }
        if(line.empty()) {
            return true;
        }
        // The pinned JSON lexer treats raw NUL as end-of-input. Do not let a
        // valid prefix of a malformed line execute or change lifecycle state.
        if(line.find('\0') != std::string::npos) {
            return respond(rpc_error(nullptr, -32700, "Parse error: raw NUL bytes are not valid JSON."));
        }
        Json request;
        try {
            request = Json::parse(line, limit_json_nesting, false);
        } catch(const JsonNestingLimit&) {
            return respond(rpc_error(nullptr, -32600, "Request exceeds the 64-container JSON nesting limit."));
        }
        if(request.is_discarded()) {
            return respond(rpc_error(nullptr, -32700, "Parse error: expected one JSON request per line."));
        }
        if(auto error = validate_request(request)) {
            // Valid JSON-RPC notifications never receive replies, including
            // invalid method parameters. Invalid envelopes are not notifications.
            if(error->at("error").at("code") != -32600 && !request.contains("id")) {
                return true;
            }
            return respond(*error);
        }
        if(!request.contains("id")) {
            if(request["method"] == "notifications/initialized" && phase == Phase::Initialized) {
                phase = Phase::Ready;
            }
            return true;
        }
        const auto& method = request["method"];
        if(method != "initialize" && method != "ping" && method != "tools/list" && method != "tools/call") {
            return respond(rpc_error(request, -32601, "Method not found."));
        }
        if(method == "initialize" && phase != Phase::Initialize) {
            return respond(rpc_error(request, -32600, "This connection has already initialized."));
        }
        if(method != "initialize" && method != "ping" && phase != Phase::Ready) {
            return respond(rpc_error(request, -32002, "Initialize and send notifications/initialized first."));
        }
        auto response = handler(request);
        if(request["method"] == "initialize" && response.contains("result")) {
            // fastmcpp also recognizes older revisions, including one requiring
            // JSON-RPC batches. This adapter only supports the two revisions
            // below. Offer the latest supported revision for any other request,
            // as MCP version negotiation requires; the client can disconnect.
            protocol_version = request["params"]["protocolVersion"] == "2025-06-18" ? "2025-06-18" : "2025-11-25";
            response["result"]["protocolVersion"] = protocol_version;
            phase = Phase::Initialized;
            std::cerr << "nsight-graphics-mcp " << project_version() << ": negotiated MCP " << protocol_version << '\n';
        }
        return respond(response);
    };
    std::string line;
    line.reserve(4096);
    bool oversized = false;
    char character;
    while(std::cin.get(character)) {
        if(character == '\n') {
            if(!process(line, oversized)) {
                return 1;
            }
            line.clear();
            oversized = false;
        } else if(line.size() < maximum_line_bytes) {
            line.push_back(character);
        } else {
            oversized = true;
        }
    }
    if((!line.empty() || oversized) && !process(line, oversized)) {
        return 1;
    }
    if(std::cin.bad()) {
        std::cerr << "nsight-graphics-mcp: error reading stdin\n";
        return 1;
    }
    std::cerr << "nsight-graphics-mcp " << project_version() << ": stdio closed\n";
    return 0;
}
} // namespace

int serve_stdio(const ServerOptions& options) {
    std::string protocol_version;
    WorkflowTools workflow(options);
    fastmcpp::tools::ToolManager tools;
    fastmcpp::tools::Tool capabilities(
        "capabilities", {{"type", "object"}, {"properties", Json::object()}, {"additionalProperties", false}},
        output_schema(), [&options, &protocol_version](const Json& arguments) {
            // The pinned validator checks types/required fields, but ignores
            // additionalProperties. Enforce the exact public contract ourselves.
            if(!arguments.is_object() || !arguments.empty()) {
                // A valid CallToolRequest with invalid tool input is a tool
                // error, distinct from malformed protocol params rejected above.
                return Json{{"isError", true},
                            {"content", Json::array({{{"type", "text"},
                                                      {"text", "capabilities accepts only an empty object (no "
                                                               "arguments). Remove all argument properties."}}})}};
            }
            return capability_report(options, protocol_version);
        });
    capabilities.set_description("Report implemented MCP operations and local prerequisite observations. "
                                 "Does not launch applications or verify Nsight/GPU compatibility.");
    capabilities.set_validate_args(true);
    capabilities.set_annotations({{"readOnlyHint", true}, {"destructiveHint", false}, {"openWorldHint", false}});
    tools.register_tool(capabilities);
    workflow.register_tools(tools);
    const auto handler = fastmcpp::mcp::make_mcp_handler(
        "nsight-graphics-mcp", std::string(project_version()), tools, {}, {},
        "Call capabilities to discover implemented operations and prerequisite observations. "
        "Capture requires --artifact-root and starts a fresh application for each asynchronous job. Poll job_status "
        "and use artifact tools to inspect bounded retained evidence; explicitly pin important baselines. "
        "Use capture_metadata/capture_events/capture_objects for supported retained export inventories. "
        "Use capture_cpp_source/capture_cpp_draws for qualified generated-source relationships and coverage limits. "
        "Found executable paths are not evidence of Nsight or GPU compatibility. Generated-source relationships "
        "do not establish GPU state or extracted resource bytes; profiling is not implemented.");
    const auto result = run_stdio(handler, protocol_version);
    if(!workflow.shutdown()) {
        std::cerr << "nsight-graphics-mcp: job shutdown did not confirm all owned-process cleanup\n";
        return 1;
    }
    return result;
}
} // namespace ngm
