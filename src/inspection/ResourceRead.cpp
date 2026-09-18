#include "ngm/ResourceRead.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Inspection.hpp"
#include "ngm/Process.hpp"
#include "ngm/Version.hpp"

#include <algorithm>
#include <fstream>
#include <set>

namespace ngm {
namespace {
using Json = nlohmann::json;
constexpr std::size_t blob_limit = 16U * 1024U * 1024U;
void require(bool ok, const char* reason) {
    if(!ok)
        throw std::runtime_error(reason);
}
Json parse(const std::string& input) {
    std::vector<std::set<std::string>> keys;
    return Json::parse(input, [&](int depth, Json::parse_event_t event, Json& value) {
        if(event == Json::parse_event_t::object_start || event == Json::parse_event_t::array_start) {
            require(depth < 8, "Worker JSON exceeds nesting limit");
            keys.emplace_back();
        } else if(event == Json::parse_event_t::object_end || event == Json::parse_event_t::array_end)
            keys.pop_back();
        else if(event == Json::parse_event_t::key)
            require(keys.back().insert(value.get<std::string>()).second, "Worker JSON has duplicate keys");
        return true;
    });
}
std::uint64_t number(const Json& object, const char* key) {
    const auto& value = object.at(key);
    require(value.is_number_integer() && (value.is_number_unsigned() || value.get<std::int64_t>() >= 0),
            "Worker header requires unsigned integer fields");
    return value.get<std::uint64_t>();
}
void save(const std::filesystem::path& path, const Json& value) {
    std::ofstream file(path, std::ios::binary);
    file.exceptions(std::ios::failbit | std::ios::badbit);
    file << value.dump(2) << '\n';
    file.close();
}
Json process_record(const ProcessResult& result) {
    return {{"exit_code", result.exit_code ? Json(*result.exit_code) : Json(nullptr)},
            {"signal", result.signal ? Json(*result.signal) : Json(nullptr)},
            {"timed_out", result.timed_out},
            {"cancelled", result.cancelled},
            {"cleanup_confirmed", result.cleanup_confirmed},
            {"error", result.error}};
}
bool succeeded(const ProcessResult& result) {
    return result.exit_code == 0 && !result.signal && !result.timed_out && !result.cancelled &&
           result.cleanup_confirmed && result.error.empty();
}
std::string hex(std::string_view value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string output;
    output.reserve(value.size() * 2);
    for(const unsigned char byte : value) {
        output += digits[byte >> 4];
        output += digits[byte & 15];
    }
    return output;
}
std::string digest(std::string_view value) {
    return sha256(std::as_bytes(std::span(value.data(), value.size())));
}
} // namespace

Json read_cpp_resource(ArtifactStore& store, const ResourceReadRequest& request) {
    if(request.worker.empty())
        throw InspectionError(InspectionErrorCode::ExportUnavailable,
                              "No resource worker configured for this producer");
    if(!request.worker.is_absolute() || request.offset > blob_limit || request.length == 0 || request.length > 65536)
        throw std::invalid_argument("Resource worker path/range exceeds limits");
    const auto handle = number(request.reference, "handle");
    const auto expected =
        request.reference.at("expected_bytes").is_null() ? 0 : number(request.reference, "expected_bytes");
    require(handle <= 2147483647 && expected <= blob_limit && request.reference.at("readable") == true,
            "Resource reference is not readable");
    auto lease = store.lease(request.capture_id);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    Json report{{"schema_version", 1},
                {"project_version", project_version()},
                {"evidence_origin", "nsight_generated_resource_read"},
                {"capture_id", request.capture_id},
                {"reference", request.reference},
                {"offset", request.offset},
                {"length", request.length},
                {"helper_profile", request.helper_profile},
                {"worker_path", request.worker.string()},
                {"status", "preparing"}};
    auto writer = store.begin({{"evidence_origin", "nsight_generated_resource_read"},
                               {"capture_id", request.capture_id},
                               {"report_path", "raw/resource-read.json"}},
                              {"raw/resource-read.json", "raw/read.stdout"}, request.pin);
    const auto root = writer.raw_directory();
    bool cleanup = true;
    const auto flush = [&] { save(root / "resource-read.json", report); };
    try {
        flush();
        // Verify every helper from the retained inventory, even though only its
        // fingerprint-qualified compiled counterpart is executed.
        const std::set<std::string> names{"ReadOnlyDatabase.cpp", "ReadOnlyDatabase.h", "DataScope.cpp", "DataScope.h",
                                          "DllCommon.h"};
        require(request.helper_profile.at("files").is_object() &&
                    request.helper_profile.at("files").size() == names.size(),
                "Helper profile closure is incomplete");
        for(const auto& name : names) {
            const auto content = store.read(request.capture_id, request.project_directory + '/' + name, 65536);
            require(digest(content) == request.helper_profile.at("files").at(name).get<std::string>(),
                    "Capture helper fingerprint is not qualified");
        }
        report["worker_sha256"] = sha256_regular_file(request.worker, deadline);
        const auto run = [&](const std::string& stage, std::vector<std::string> arguments) {
            ProcessOptions options;
            options.executable = request.worker;
            options.arguments = std::move(arguments);
            options.working_directory = root;
            options.stdout_path = root / (stage + ".stdout");
            options.stderr_path = root / (stage + ".stderr");
            options.timeout =
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
            require(options.timeout.count() > 0, "Resource read deadline exceeded");
            report[stage + "_arguments"] = options.arguments;
            cleanup = false;
            const auto result = run_process(options);
            cleanup = result.cleanup_confirmed;
            report[stage + "_process"] = process_record(result);
            flush();
            require(succeeded(result), "Resource worker did not complete successfully; inspect retained logs");
            require(read_regular_file(options.stderr_path, 131072).empty(),
                    "Resource worker emitted unexpected diagnostics");
            return read_regular_file(options.stdout_path, stage == "profile" ? 8192 : 131072);
        };
        const auto profile = parse(run("profile", {"--profile"}));
        require(profile.is_object() && profile.size() == 4 && number(profile, "schema_version") == 1 &&
                    profile.at("profile") == request.helper_profile.at("profile") &&
                    profile.at("files") == request.helper_profile.at("files") &&
                    profile.at("project_version").is_string() &&
                    profile.at("project_version").get_ref<const std::string&>().size() <= 64,
                "Worker profile does not match the capture helper closure");
        report["worker_profile"] = profile;
        Json inputs = Json::object();
        for(const auto& name : {"data.bin", "data.bin.rec"}) {
            const std::size_t maximum = std::string_view(name) == "data.bin" ? 256U * 1024U * 1024U : blob_limit;
            const auto copied = store.snapshot_file(request.capture_id, request.project_directory + '/' + name, writer,
                                                    std::string("raw/input/") + name, maximum, deadline);
            inputs[name] = {{"bytes", copied.bytes},
                            {"sha256", sha256_regular_file(writer.directory() / copied.path, deadline)}};
        }
        report["inputs"] = inputs;
        report["status"] = "running";
        flush();
        const auto output =
            run("read", {(root / "input").string(), std::to_string(handle), std::to_string(request.offset),
                         std::to_string(request.length), std::to_string(expected)});
        require(sha256_regular_file(request.worker, deadline) == report.at("worker_sha256").get<std::string>(),
                "Worker changed during resource read");
        for(const auto& name : {"data.bin", "data.bin.rec"})
            require(sha256_regular_file(root / "input" / name, deadline) ==
                        inputs.at(name).at("sha256").get<std::string>(),
                    "Worker input changed during resource read");
        const auto newline = output.find('\n');
        require(newline != std::string::npos && newline <= 1024, "Missing or excessive worker header");
        const auto header = parse(output.substr(0, newline));
        require(header.is_object() && header.size() == 6 && number(header, "schema_version") == 1 &&
                    header.at("profile") == request.helper_profile.at("profile") &&
                    number(header, "handle") == handle && number(header, "offset") == request.offset,
                "Worker header/request identity mismatch");
        const auto total = number(header, "total_bytes");
        const auto count = number(header, "returned_bytes");
        require(total > 0 && total <= blob_limit && (expected == 0 || total == expected) && request.offset <= total &&
                    count == std::min<std::uint64_t>(request.length, total - request.offset) &&
                    output.size() - newline - 1 == count,
                "Worker response size/range mismatch");
        const std::string_view bytes(output.data() + newline + 1, static_cast<std::size_t>(count));
        Json result{{"capture_id", request.capture_id},
                    {"resource_ref", request.reference.at("resource_ref")},
                    {"reference", request.reference},
                    {"helper_profile", request.helper_profile.at("profile")},
                    {"worker_sha256", report.at("worker_sha256")},
                    {"inputs", inputs},
                    {"offset", request.offset},
                    {"total_bytes", total},
                    {"returned_bytes", count},
                    {"next_offset", request.offset + count < total ? Json(request.offset + count) : Json(nullptr)},
                    {"encoding", "hex"},
                    {"data", hex(bytes)},
                    {"sha256", digest(bytes)},
                    {"evidence_origin", "nsight_generated_resource_read"},
                    {"scope", "Opaque serialized bytes from a source-referenced handle; not executed GPU state"},
                    {"artifact_id", writer.id()},
                    {"report_path", "raw/resource-read.json"}};
        report["status"] = "succeeded";
        report["result"] = result;
        // Keep the exact inputs through publication, including quota/fsync
        // failures. Each read bundle is self-contained and charged to retention.
        // Source capture retention/pinning remains independent.
        flush();
        store.publish_success(writer);
        return result;
    } catch(const std::exception& error) {
        report["status"] = "failed";
        report["error"] = std::string(error.what()).substr(0, 4096);
        try {
            flush();
            if(cleanup)
                store.publish_failure(writer, "Resource extraction failed; inspect raw/resource-read.json");
            else
                store.quarantine(writer, "Resource worker cleanup unconfirmed");
        } catch(...) { /* The writer destructor protects interrupted publication. */
        }
        throw InspectionError(InspectionErrorCode::ExportUnavailable,
                              std::string(error.what()).substr(0, 2048) + "; evidence artifact " + writer.id());
    }
}
} // namespace ngm
