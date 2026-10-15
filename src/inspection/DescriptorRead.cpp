#include "ngm/DescriptorRead.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Inspection.hpp"
#include "ngm/Process.hpp"
#include "ngm/Version.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <map>
#include <set>

namespace ngm {
namespace {
using Json = nlohmann::json;
void require(bool ok, const char* reason) {
    if(!ok)
        throw std::runtime_error(reason);
}
void keys(const Json& object, std::initializer_list<const char*> names) {
    require(object.is_object() && object.size() == names.size(), "Descriptor response has unexpected fields");
    for(const auto* name : names)
        require(object.contains(name), "Descriptor response is missing a field");
}
std::uint64_t number(const Json& object, const char* name) {
    const auto& value = object.at(name);
    require(value.is_number_integer() && (value.is_number_unsigned() || value.get<std::int64_t>() >= 0),
            "Descriptor response requires unsigned integer fields");
    return value.get<std::uint64_t>();
}
std::string text(const Json& object, const char* name, std::size_t maximum) {
    const auto& value = object.at(name);
    require(value.is_string(), "Descriptor response requires text fields");
    const auto& result = value.get_ref<const std::string&>();
    require(!result.empty() && result.size() <= maximum && result.find('\0') == std::string::npos,
            "Descriptor text exceeds bounds");
    return result;
}
std::string hash_text(const Json& object, const char* name) {
    const auto result = text(object, name, 64);
    require(result.size() == 64 && result.find_first_not_of("0123456789abcdef") == std::string::npos,
            "Descriptor fingerprint is not a lowercase SHA-256");
    return result;
}
std::string digest(std::string_view value) {
    return sha256(std::as_bytes(std::span(value.data(), value.size())));
}
Json parse(const std::string& bytes) {
    require(bytes.find('\0') == std::string::npos, "Descriptor JSON contains NUL");
    std::vector<std::set<std::string>> stack;
    return Json::parse(bytes, [&](int depth, Json::parse_event_t event, Json& value) {
        if(event == Json::parse_event_t::object_start || event == Json::parse_event_t::array_start) {
            require(depth < 12, "Descriptor JSON nesting exceeds limit");
            stack.emplace_back();
        } else if(event == Json::parse_event_t::object_end || event == Json::parse_event_t::array_end)
            stack.pop_back();
        else if(event == Json::parse_event_t::key)
            require(stack.back().insert(value.get<std::string>()).second, "Descriptor JSON has duplicate fields");
        return true;
    });
}
void save(const std::filesystem::path& path, const Json& value) {
    std::ofstream file(path, std::ios::binary);
    file.exceptions(std::ios::failbit | std::ios::badbit);
    file << value.dump(2) << '\n';
    file.close();
}
Json process_record(const ProcessResult& r) {
    return {{"exit_code", r.exit_code ? Json(*r.exit_code) : Json(nullptr)},
            {"signal", r.signal ? Json(*r.signal) : Json(nullptr)},
            {"timed_out", r.timed_out},
            {"cancelled", r.cancelled},
            {"cleanup_confirmed", r.cleanup_confirmed},
            {"error", r.error}};
}

Json qualify_writes(const Json& response, const Json& registrations, std::uint64_t count) {
    using Key = std::pair<std::uint64_t, std::uint64_t>;
    std::map<Key, Json> registry;
    require(registrations.is_array() && registrations.size() <= 4096, "Source registration list exceeds bounds");
    const std::map<std::uint64_t, std::pair<std::string, std::string>> objects{
        {9, {"VK_OBJECT_TYPE_BUFFER", "VkBuffer_uid_"}},
        {14, {"VK_OBJECT_TYPE_IMAGE_VIEW", "VkImageView_uid_"}},
        {21, {"VK_OBJECT_TYPE_SAMPLER", "VkSampler_uid_"}},
        {23, {"VK_OBJECT_TYPE_DESCRIPTOR_SET", "VkDescriptorSet_uid_"}}};
    for(const auto& r : registrations) {
        keys(r, {"object_type", "type_name", "serialized_id", "symbol", "sources"});
        const auto type = number(r, "object_type"), id = number(r, "serialized_id");
        require(objects.contains(type) && id != 0 && r.at("type_name") == objects.at(type).first,
                "Source registration object type is unqualified");
        const auto symbol = text(r, "symbol", 128);
        require(symbol.starts_with(objects.at(type).second) && symbol.size() > objects.at(type).second.size() &&
                    symbol.find_first_not_of("0123456789", objects.at(type).second.size()) == std::string::npos &&
                    r.at("sources").is_array() && !r.at("sources").empty() && r.at("sources").size() <= 1024,
                "Source registration symbol/evidence is unqualified");
        require(registry.emplace(Key{type, id}, r).second, "Ambiguous typed source registration");
    }
    const auto object = [&](const Json& value, std::uint64_t expected, bool nullable) -> Json {
        if(value.is_null()) {
            require(nullable, "Descriptor destination set cannot be null");
            return nullptr;
        }
        keys(value, {"object_type", "serialized_id"});
        const auto type = number(value, "object_type"), id = number(value, "serialized_id");
        require(type == expected && id != 0 && registry.contains({type, id}),
                "Descriptor object is absent or mismatched in source registrations");
        return registry.at({type, id});
    };
    const auto& writes = response.at("writes");
    require(writes.is_array() && writes.size() == count, "Descriptor write count does not match source request");
    Json normalized = Json::array();
    std::size_t total = 0;
    const std::map<std::uint64_t, std::string> types{{1, "VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER"},
                                                     {6, "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER"},
                                                     {7, "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER"},
                                                     {8, "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC"},
                                                     {9, "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC"}};
    for(const auto& write : writes) {
        keys(write,
             {"dst_set", "binding", "array_element", "descriptor_type", "descriptor_count", "buffers", "images"});
        const auto type = number(write, "descriptor_type"), extent = number(write, "descriptor_count");
        const auto first = number(write, "array_element"), binding = number(write, "binding");
        require(types.contains(type) && extent > 0 && extent <= 128 && total + extent <= 512 && binding <= UINT32_MAX &&
                    first <= UINT32_MAX && extent <= 4294967296ull - first && write.at("buffers").is_array() &&
                    write.at("images").is_array(),
                "Unqualified descriptor write type/extent");
        total += extent;
        auto row = write;
        row["descriptor_type_name"] = types.at(type);
        row["dst_set"] = object(write.at("dst_set"), 23, false);
        if(type == 1) {
            require(write.at("buffers").empty() && write.at("images").size() == extent,
                    "Image descriptor count/payload mismatch");
            row["images"] = Json::array();
            for(const auto& image : write.at("images")) {
                keys(image, {"image_view", "sampler", "image_layout"});
                const auto& layout = image.at("image_layout");
                require(layout.is_number_integer() &&
                            (layout.is_number_unsigned()
                                 ? layout.get<std::uint64_t>() <= INT32_MAX
                                 : layout.get<std::int64_t>() >= INT32_MIN && layout.get<std::int64_t>() <= INT32_MAX),
                        "Image layout exceeds its signed ABI field");
                row["images"].push_back({{"image_view", object(image.at("image_view"), 14, true)},
                                         {"sampler", object(image.at("sampler"), 21, true)},
                                         {"image_layout", layout}});
            }
        } else {
            require(write.at("images").empty() && write.at("buffers").size() == extent,
                    "Buffer descriptor count/payload mismatch");
            row["buffers"] = Json::array();
            for(const auto& buffer : write.at("buffers")) {
                keys(buffer, {"buffer", "offset", "range"});
                const auto offset = number(buffer, "offset"), range = number(buffer, "range");
                require(range > 0 && (range == UINT64_MAX || offset <= UINT64_MAX - range),
                        "Descriptor buffer range is empty or overflows");
                row["buffers"].push_back(
                    {{"buffer", object(buffer.at("buffer"), 9, true)}, {"offset", offset}, {"range", range}});
            }
        }
        normalized.push_back(std::move(row));
    }
    require(normalized.dump().size() <= 768U * 1024U, "Qualified descriptor rows exceed response budget");
    return normalized;
}
} // namespace

Json read_cpp_descriptor_writes(ArtifactStore& store, const DescriptorReadRequest& request) {
    if(request.worker.empty())
        throw InspectionError(InspectionErrorCode::ExportUnavailable,
                              "No descriptor worker configured for this producer");
    if(!request.worker.is_absolute())
        throw std::invalid_argument("Descriptor worker path must be absolute");
    const auto handle = number(request.reference, "handle"), count = number(request.reference, "write_count"),
               capacity = number(request.reference, "output_bytes");
    require(handle <= INT32_MAX && count > 0 && count <= 64 && capacity >= count * 64 && capacity <= 65536 &&
                request.reference.at("readable") == true && request.reference.at("conditional") == false &&
                number(request.reference, "copy_count") == 0,
            "Descriptor update reference is not readable");
    (void)hash_text(request.reference, "update_ref");
    auto lease = store.lease(request.capture_id);
    const auto deadline = std::min(std::chrono::steady_clock::now() + std::chrono::seconds(20),
                                   request.deadline.value_or(std::chrono::steady_clock::time_point::max()));
    Json report{{"schema_version", 1},
                {"project_version", project_version()},
                {"evidence_origin", "nsight_generated_descriptor_read"},
                {"capture_id", request.capture_id},
                {"reference", request.reference},
                {"helper_profile", request.helper_profile},
                {"worker_path", request.worker.string()},
                {"status", "preparing"}};
    auto writer = store.begin({{"evidence_origin", "nsight_generated_descriptor_read"},
                               {"capture_id", request.capture_id},
                               {"report_path", "raw/descriptor-read.json"}},
                              {"raw/descriptor-read.json", "raw/read.stdout"}, request.pin);
    const auto root = writer.raw_directory();
    bool cleanup = true;
    const auto flush = [&] { save(root / "descriptor-read.json", report); };
    try {
        flush();
        const std::set<std::string> names{"ReadOnlyDatabase.cpp",  "ReadOnlyDatabase.h", "DataScope.cpp",
                                          "DataScope.h",           "DllCommon.h",        "VulkanStructHydrator.cpp",
                                          "VulkanStructHydrator.h"};
        const auto& originals = request.helper_profile.at("original_files");
        require(originals.is_object() && originals.size() == names.size() && request.helper_paths.is_object() &&
                    request.helper_paths.size() == names.size(),
                "Descriptor helper closure is incomplete");
        for(const auto& name : names) {
            const auto path = text(request.helper_paths, name.c_str(), 4096);
            require(path == request.project_directory + '/' + name ||
                        (name == "VulkanStructHydrator.h" && path == request.project_directory + "/include/" + name),
                    "Descriptor helper source path is outside the qualified project closure");
            require(digest(store.read(request.capture_id, path, 65536)) == originals.at(name).get<std::string>(),
                    "Capture descriptor helper fingerprint is unqualified");
        }
        report["helper_paths"] = request.helper_paths;
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
            require(options.timeout.count() > 0, "Descriptor read deadline exceeded");
            report[stage + "_arguments"] = options.arguments;
            cleanup = false;
            const auto r = run_process(options);
            cleanup = r.cleanup_confirmed;
            report[stage + "_process"] = process_record(r);
            flush();
            require(r.exit_code == 0 && !r.signal && !r.timed_out && !r.cancelled && r.cleanup_confirmed &&
                        r.error.empty(),
                    "Descriptor worker did not complete successfully; inspect retained logs");
            require(read_regular_file(options.stderr_path, 131072).empty(),
                    "Descriptor worker emitted unexpected diagnostics");
            return read_regular_file(options.stdout_path, stage == "profile" ? 8192 : 131072);
        };
        auto profile = parse(run("profile", {"--profile"}));
        report["worker_profile"] = profile;
        (void)text(profile, "project_version", 64);
        profile.erase("project_version");
        require(profile == request.helper_profile,
                "Descriptor worker profile does not match qualified instrumentation");
        keys(request.expected_inputs, {"data.bin", "data.bin.rec"});
        Json inputs = Json::object();
        for(const auto* name : {"data.bin", "data.bin.rec"}) {
            const auto maximum = std::string_view(name) == "data.bin" ? 256U * 1024U * 1024U : 16U * 1024U * 1024U;
            const auto copied = store.snapshot_file(request.capture_id, request.project_directory + '/' + name, writer,
                                                    std::string("raw/input/") + name, maximum, deadline);
            inputs[name] = {{"bytes", copied.bytes},
                            {"sha256", sha256_regular_file(writer.directory() / copied.path, deadline)}};
            require(number(request.expected_inputs.at(name), "bytes") == copied.bytes &&
                        hash_text(request.expected_inputs.at(name), "sha256") ==
                            inputs[name].at("sha256").get<std::string>(),
                    "Descriptor input differs from its capture-time identity");
        }
        report["inputs"] = inputs;
        report["status"] = "running";
        flush();
        const auto response = parse(run("read", {(root / "input").string(), std::to_string(handle),
                                                 std::to_string(count), std::to_string(capacity)}));
        report["worker_result"] = response;
        require(sha256_regular_file(request.worker, deadline) == report.at("worker_sha256").get<std::string>(),
                "Descriptor worker changed during read");
        for(const auto* name : {"data.bin", "data.bin.rec"})
            require(sha256_regular_file(root / "input" / name, deadline) ==
                        inputs[name].at("sha256").get<std::string>(),
                    "Descriptor input changed during read");
        keys(response,
             {"schema_version", "profile", "evidence_origin", "scope", "handle", "write_count", "input_bytes",
              "source_alignment_remainder", "input_sha256", "output_capacity", "output_used_bytes", "writes"});
        require(number(response, "schema_version") == 1 && response.at("profile") == profile.at("profile") &&
                    response.at("evidence_origin") == "generated_helper_descriptor_writes" &&
                    number(response, "handle") == handle && number(response, "write_count") == count &&
                    number(response, "input_bytes") >= count * 64 &&
                    number(response, "input_bytes") <= 16U * 1024U * 1024U &&
                    number(response, "source_alignment_remainder") < 8 &&
                    number(response, "output_capacity") == capacity &&
                    number(response, "output_used_bytes") >= count * 64 &&
                    number(response, "output_used_bytes") <= capacity,
                "Descriptor response/source request identity or extent mismatch");
        (void)text(response, "scope", 256);
        (void)hash_text(response, "input_sha256");
        auto writes = qualify_writes(response, request.registrations, count);
        Json result{{"capture_id", request.capture_id},
                    {"update_ref", request.reference.at("update_ref")},
                    {"reference", request.reference},
                    {"helper_profile", profile.at("profile")},
                    {"worker_sha256", report.at("worker_sha256")},
                    {"worker_profile", report.at("worker_profile")},
                    {"inputs", inputs},
                    {"input_bytes", response.at("input_bytes")},
                    {"input_sha256", response.at("input_sha256")},
                    {"output_used_bytes", response.at("output_used_bytes")},
                    {"writes", std::move(writes)},
                    {"artifact_id", writer.id()},
                    {"report_path", "raw/descriptor-read.json"},
                    {"evidence_origin", "nsight_generated_descriptor_read"},
                    {"scope",
                     "Source-scoped descriptor writes with typed registered symbols; no selected-draw visibility, "
                     "effective buffer address, GPU-written resource contents or shader value is inferred"}};
        require(result.dump().size() <= 1024U * 1024U, "Descriptor result exceeds the full response limit");
        report["status"] = "succeeded";
        report["result"] = result;
        flush();
        store.publish_success(writer);
        return result;
    } catch(const std::exception& error) {
        report["status"] = "failed";
        report["error"] = std::string(error.what()).substr(0, 4096);
        try {
            flush();
            if(cleanup)
                store.publish_failure(writer, "Descriptor extraction failed; inspect raw/descriptor-read.json");
            else
                store.quarantine(writer, "Descriptor worker cleanup unconfirmed");
        } catch(...) { /* Interrupted publication stays protected by the writer. */
        }
        throw InspectionError(InspectionErrorCode::ExportUnavailable,
                              std::string(error.what()).substr(0, 2048) + "; evidence artifact " + writer.id());
    }
}
} // namespace ngm
