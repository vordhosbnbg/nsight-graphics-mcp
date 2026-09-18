#include "Check.hpp"
#include "McpClient.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Image.hpp"
#include "ngm/Version.hpp"

#include <csignal>
#include <fastmcpp/util/pagination.hpp>
#include <fstream>
#include <iostream>
#include <span>
#include <unistd.h>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using ngm::check::require;
void save(const fs::path& path, const Json& value) {
    std::ofstream stream(path);
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    stream << value.dump(2) << '\n';
}
} // namespace

int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);
    if(argc != 5) {
        std::cerr << "SERVER ARTIFACT_ROOT CASES_JSON RUN_ROOT required\n";
        return 2;
    }
    try {
        const auto server = fs::canonical(argv[1]);
        const auto store = fs::canonical(argv[2]);
        const auto cases_path = fs::canonical(argv[3]);
        const auto cases = Json::parse(ngm::read_regular_file(cases_path, 65536));
        require(cases.is_array() && !cases.empty() && cases.size() <= 20, "one to twenty explicit retained images");
        const auto root = fs::absolute(argv[4]);
        fs::create_directories(root);
        auto pattern = (root / "image-preview-XXXXXX").string();
        const auto* allocated = mkdtemp(pattern.data());
        require(allocated, "allocate isolated retained-image validation output");
        const fs::path run(allocated);
        fs::copy_file(__FILE__, run / "RetainedImageIntegration.cpp");
        fs::copy_file(fs::path(__FILE__).parent_path() / "McpClient.hpp", run / "McpClient.hpp");
        fs::copy_file(fs::path(__FILE__).parent_path() / "Check.hpp", run / "Check.hpp");
        save(run / "cases.json", cases);
        Json report{{"status", "running"},
                    {"project_version", ngm::project_version()},
                    {"evidence_origin", "retained_image_mcp_validation"},
                    {"store", store.string()},
                    {"server_sha256", ngm::sha256_file(server)},
                    {"harness_sha256", ngm::sha256_file("/proc/self/exe")},
                    {"cases", Json::array()},
                    {"scope", "Retained files only; no fresh capture, GPU or Nsight invocation"}};
        std::ofstream transcript(run / "transcript.ndjson");
        transcript.exceptions(std::ios::badbit | std::ios::failbit);
        std::cout << run << '\n' << std::flush;
        ngm::check::mcp::Client client(server.string(), "/nonexistent", {"--artifact-root", store.string()}, false, {},
                                       {std::chrono::seconds(30), std::chrono::seconds(30)});
        std::int64_t request_id = 1;
        const auto request = [&](const char* method, const Json& params) {
            transcript
                << Json{{"direction", "request"}, {"id", request_id}, {"method", method}, {"params", params}}.dump()
                << '\n';
            const auto reply = client.request(request_id++, method, params);
            transcript << Json{{"direction", "response"}, {"message", reply}}.dump() << '\n';
            transcript.flush();
            require(reply.contains("result") && !reply.at("result").value("isError", false),
                    "MCP request succeeds: " + reply.dump());
            return reply.at("result");
        };
        try {
            const auto initialized = request(
                "initialize",
                {{"protocolVersion", "2025-11-25"},
                 {"capabilities", Json::object()},
                 {"clientInfo", {{"name", "ngm-retained-image-integration"}, {"version", ngm::project_version()}}}});
            require(initialized.at("serverInfo").at("version") == ngm::project_version(), "matching server version");
            client.send({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
            for(std::size_t i = 0; i < cases.size(); ++i) {
                const auto& item = cases.at(i);
                const Json base{{"artifact_id", item.at("artifact_id")}, {"path", item.at("path")}};
                const auto info = request("tools/call", {{"name", "artifact_info"},
                                                         {"arguments", {{"artifact_id", item.at("artifact_id")}}}});
                require(info.at("structuredContent").at("pinned") == true, "retained baseline is explicitly pinned");
                const auto source_path =
                    store / "bundles" / item.at("artifact_id").get<std::string>() / item.at("path").get<std::string>();
                const auto original = ngm::decode_image(ngm::read_regular_file(source_path, 16 * 1024 * 1024));
                Json result{{"input", item}, {"previews", Json::array()}};
                for(bool cropped : {false, true}) {
                    auto arguments = base;
                    ngm::ImageRegion region{0, 0, original.width, original.height};
                    if(cropped) {
                        region = {original.width / 4, original.height / 4, std::max(1U, original.width / 2),
                                  std::max(1U, original.height / 2)};
                        arguments["region"] = {
                            {"x", region.x}, {"y", region.y}, {"width", region.width}, {"height", region.height}};
                        arguments["max_edge"] = 24;
                    }
                    const auto preview =
                        request("tools/call", {{"name", "artifact_preview_image"}, {"arguments", arguments}});
                    require(preview.dump().size() < 1024 * 1024 - 1024 && preview.at("content").size() == 2,
                            "bounded metadata and image content");
                    const auto& metadata = preview.at("structuredContent");
                    require(Json::parse(preview.at("content").at(0).at("text").get<std::string>()) == metadata &&
                                preview.at("content").at(1).at("mimeType") == "image/png",
                            "MCP content agrees with metadata");
                    const auto png = fastmcpp::util::pagination::base64_decode(
                        preview.at("content").at(1).at("data").get<std::string>());
                    require(ngm::sha256(std::as_bytes(std::span(png))) == metadata.at("png_sha256").get<std::string>(),
                            "actual PNG payload identity");
                    const auto expected = ngm::preview_image(original, region, cropped ? 24 : 384);
                    require(ngm::compare_images(ngm::decode_image(png), expected, 0).differing_pixels == 0,
                            "transport preserves selected RGB pixels");
                    const auto filename = "image-" + std::to_string(i) + (cropped ? "-crop.png" : "-full.png");
                    std::ofstream image(run / filename, std::ios::binary);
                    image.exceptions(std::ios::badbit | std::ios::failbit);
                    image << png;
                    result["previews"].push_back({{"file", filename}, {"metadata", metadata}});
                }
                const auto compare = request("tools/call", {{"name", "artifact_compare_images"},
                                                            {"arguments", {{"reference", base}, {"candidate", base}}}});
                require(compare.at("structuredContent").at("matches_within_tolerance") == true,
                        "new PNG profile supports original-image comparison");
                result["comparison"] = compare.at("structuredContent");
                report["cases"].push_back(result);
                save(run / "report.json", report);
            }
            require(client.finish() == 0, "server exits cleanly on EOF");
            report["status"] = "pass";
        } catch(const std::exception& error) {
            report["status"] = "fail";
            report["error"] = error.what();
        }
        std::ofstream(run / "server.stderr.txt") << client.diagnostics();
        save(run / "report.json", report);
        std::cout << report.at("status") << '\n';
        return report.at("status") == "pass" ? 0 : 1;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
