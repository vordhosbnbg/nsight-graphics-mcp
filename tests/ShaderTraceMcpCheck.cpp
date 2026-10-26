#include "ShaderTraceMcp.hpp"

namespace {
using namespace ngm::check::shader_trace;
using ngm::check::require;
void checks(const fs::path& standin, const fs::path& server) {
    Scratch scratch;
    const auto root = installation(standin, scratch.path);
    Client client(server.string(), "/usr/bin:/bin",
                  {"--nsight-root", root.string(), "--artifact-root", (scratch.path / "store").string()}, true);
    initialize(client);
    const auto tools = client.request(2, "tools/list").at("result").at("tools");
    const auto tool = std::find_if(tools.begin(), tools.end(),
                                   [](const auto& row) { return row.at("name") == "capture_shader_trace"; });
    require(tool != tools.end() && tool->at("inputSchema").at("additionalProperties") == false,
            "instrumented trace tool advertises a closed input contract");
    require(call(client, "capabilities").at("operations").at("shader_trace").at("status") ==
                "application_reported_instrumented_subset",
            "capabilities describe the actual trace evidence scope");
    auto collect = [&](const char* mode) {
        return call(client, "capture",
                    {{"executable", fs::read_symlink("/proc/self/exe").string()},
                     {"working_directory", scratch.path.string()},
                     {"arguments", {"--produce", mode}},
                     {"application_output_option", "--output"},
                     {"timeout_ms", 15000}});
    };
    const auto submission = collect("slow");
    const auto id = submission.at("artifact_id");
    error(client, "capture_shader_trace", {{"capture_id", id}}, "busy");
    require(completed(client, submission).at("state") == "succeeded", "trace-producing capture succeeds");
    const auto first = call(client, "capture_shader_trace", {{"capture_id", id}, {"limit", 1}, {"frame", 10}});
    schema_members(first, tool->at("outputSchema"));
    require(first.at("records_total") == 4 && first.at("matched_records") == 2 && first.at("next_offset") == 1 &&
                first.at("producer_claims_verified") == false && first.at("records")[0].at("primitive").is_null(),
            "MCP selection/page retains unverified invocation and unavailable primitive semantics");
    const auto last =
        call(client, "capture_shader_trace", {{"capture_id", id}, {"frame", 10}, {"offset", 1}, {"limit", 1}});
    schema_members(last, tool->at("outputSchema"));
    require(last.at("next_offset").is_null() && last.at("records")[0].at("probe") == 1,
            "MCP selection offsets page filtered rows");
    error(client, "capture_shader_trace", {{"capture_id", id}, {"frame", std::uint64_t{UINT32_MAX} + 1}},
          "invalid_argument");
    error(client, "capture_shader_trace", {{"capture_id", id}, {"limit", 101}}, "invalid_argument");
    error(client, "capture_shader_trace", {{"capture_id", id}, {"extra", 1}}, "invalid_argument");
    for(const auto* mode : {"missing", "bad-hash", "not-spirv"}) {
        const auto job = collect(mode);
        require(completed(client, job).at("state") == "succeeded", "capture success is separate from trace validity");
        error(client, "capture_shader_trace", {{"capture_id", job.at("artifact_id")}},
              std::string_view(mode) == "missing" ? "export_unavailable" : "invalid_export");
    }
    // A fresh target deliberately remains alive long enough for cancellation.
    const auto cancelled = collect("slow");
    call(client, "job_cancel", {{"job_id", cancelled.at("identity").at("job_id")}});
    require(completed(client, cancelled).at("state") == "cancelled", "cancelled trace capture confirms cleanup");
    error(client, "capture_shader_trace", {{"capture_id", cancelled.at("artifact_id")}}, "incomplete_capture");
    require(client.finish() == 0, "MCP EOF drains jobs and exits normally");
}
} // namespace
int main(int argc, char** argv) {
    if(argc > 1 && std::string_view(argv[1]) == "--produce")
        return ngm::check::run([&] { require(produce(argc, argv) == 0, "synthetic producer succeeds"); });
    return ngm::check::run([&] {
        require(argc == 3, "Nsight stand-in and actual MCP server required");
        checks(fs::absolute(argv[1]), fs::absolute(argv[2]));
    });
}
