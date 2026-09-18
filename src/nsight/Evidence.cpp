#include "ngm/NsightEvidence.hpp"

#include <nlohmann/json.hpp>

#include <set>
#include <utility>

namespace ngm {
namespace {
using Json = nlohmann::json;
using Error = NsightEvidenceErrorCode;
using Limits = NsightEvidenceLimits;

[[noreturn]] void fail(Error code, const std::string& message) {
    throw NsightEvidenceError(code, "Nsight evidence: " + message);
}

// A SAX pass rejects depth/size/duplicate-key problems before any JSON tree is
// allocated. In particular, ignored extension fields obey the same limits.
class BoundedJson : public nlohmann::json_sax<Json> {
public:
    bool null() override {
        return value();
    }
    bool boolean(bool) override {
        return value();
    }
    bool number_integer(number_integer_t) override {
        return value();
    }
    bool number_unsigned(number_unsigned_t) override {
        return value();
    }
    bool number_float(number_float_t, const string_t&) override {
        return value();
    }
    bool string(string_t& text) override {
        string_size(text);
        return value();
    }
    bool binary(binary_t&) override {
        fail(Error::MalformedJson, "binary values are not JSON text");
    }
    bool start_object(std::size_t) override {
        return start();
    }
    bool key(string_t& text) override {
        string_size(text);
        if(!stack_.back().keys.insert(text).second) {
            fail(Error::DuplicateKey, "duplicate JSON object key");
        }
        return true;
    }
    bool end_object() override {
        stack_.pop_back();
        return true;
    }
    bool start_array(std::size_t) override {
        return start();
    }
    bool end_array() override {
        stack_.pop_back();
        return true;
    }
    bool parse_error(std::size_t position, const std::string&, const nlohmann::detail::exception&) override {
        // The upstream exception/token may contain raw environment or paths.
        fail(Error::MalformedJson, "malformed JSON near byte " + std::to_string(position));
    }

private:
    struct Container {
        std::size_t entries = 0;
        std::set<std::string> keys;
    };
    std::vector<Container> stack_;
    std::size_t values_ = 0;

    static void string_size(std::string_view text) {
        if(text.size() > Limits::maximum_string_bytes) {
            fail(Error::LimitExceeded, "JSON string exceeds the decoded-byte limit");
        }
    }

    bool value() {
        if(++values_ > Limits::maximum_json_values) {
            fail(Error::LimitExceeded, "JSON value count exceeds the limit");
        }
        if(!stack_.empty() && ++stack_.back().entries > Limits::maximum_container_entries) {
            fail(Error::LimitExceeded, "JSON container entry count exceeds the limit");
        }
        return true;
    }

    bool start() {
        value();
        if(stack_.size() >= Limits::maximum_depth) {
            fail(Error::LimitExceeded, "JSON nesting exceeds the depth limit");
        }
        stack_.push_back(Container{});
        return true;
    }
};

Json bounded_json(std::string_view text) {
    if(text.size() > Limits::maximum_bytes) {
        fail(Error::LimitExceeded, "JSON text exceeds the byte limit");
    }
    if(text.empty()) {
        fail(Error::MalformedJson, "JSON text is empty");
    }
    // The upstream lexer treats a raw NUL as end-of-input. Reject it explicitly
    // so a valid prefix cannot hide trailing bytes. Escaped \u0000 remains data.
    if(const auto position = text.find('\0'); position != std::string_view::npos) {
        fail(Error::MalformedJson, "raw NUL byte at position " + std::to_string(position));
    }
    BoundedJson guard;
    if(!Json::sax_parse(text.begin(), text.end(), &guard)) {
        fail(Error::MalformedJson, "malformed JSON");
    }
    // Only a bounded, strictly valid input reaches DOM construction. No schema
    // helper reparses, serializes, or copies ignored JSON subtrees.
    return Json::parse(text.begin(), text.end());
}

void require_object(const Json& value, std::string_view path) {
    if(!value.is_object()) {
        fail(Error::InvalidSchema, std::string(path) + " must be a JSON object");
    }
}

void require_array(const Json& value, std::string_view path, std::size_t limit) {
    if(!value.is_array()) {
        fail(Error::InvalidSchema, std::string(path) + " must be a JSON array");
    }
    if(value.size() > limit) {
        fail(Error::LimitExceeded, std::string(path) + " exceeds its entry limit");
    }
}

const Json& required(const Json& object, std::string_view field, const std::string& path) {
    const auto found = object.find(field);
    if(found == object.end()) {
        fail(Error::MissingField, path + '.' + std::string(field) + " is required");
    }
    return *found;
}

std::uint64_t unsigned_integer(const Json& value, const std::string& path) {
    if(value.is_number_unsigned()) {
        return value.get<std::uint64_t>();
    }
    if(value.is_number_integer()) {
        const auto result = value.get<std::int64_t>();
        if(result >= 0) {
            return static_cast<std::uint64_t>(result);
        }
    }
    fail(Error::InvalidField, path + " must be an unsigned 64-bit integer");
}

void metadata_string_size(std::string_view text, std::size_t& bytes) {
    if(text.size() > Limits::maximum_metadata_string_bytes - bytes) {
        fail(Error::LimitExceeded, "selected metadata strings exceed the total decoded-byte limit");
    }
    bytes += text.size();
}

std::string string_value(const Json& value, const std::string& path, bool allow_empty = true,
                         std::size_t* metadata_bytes = nullptr) {
    if(!value.is_string()) {
        fail(Error::InvalidField, path + " must be a string");
    }
    const auto& result = value.get_ref<const std::string&>();
    if(!allow_empty && result.empty()) {
        fail(Error::InvalidField, path + " must not be empty");
    }
    if(metadata_bytes != nullptr) {
        metadata_string_size(result, *metadata_bytes);
    }
    return result;
}

std::optional<std::string> optional_string(const Json& object, std::string_view field, std::size_t& bytes) {
    const auto found = object.find(field);
    if(found == object.end()) {
        return std::nullopt;
    }
    return string_value(*found, "metadata." + std::string(field), true, &bytes);
}

std::optional<std::vector<std::string>> optional_strings(const Json& object, std::string_view field,
                                                         const std::string& parent, std::size_t& bytes) {
    const auto found = object.find(field);
    if(found == object.end()) {
        return std::nullopt;
    }
    const auto path = parent + '.' + std::string(field);
    require_array(*found, path, Limits::maximum_metadata_entries);
    std::vector<std::string> result;
    result.reserve(found->size());
    for(const auto& item : *found) {
        result.push_back(string_value(item, path + " entry", true, &bytes));
    }
    return result;
}

std::optional<NsightMetadataStringLists> optional_string_lists(const Json& object, std::string_view field,
                                                               std::size_t& bytes) {
    const auto found = object.find(field);
    if(found == object.end()) {
        return std::nullopt;
    }
    const auto path = "metadata." + std::string(field);
    require_object(*found, path);
    if(found->size() > Limits::maximum_metadata_entries) {
        fail(Error::LimitExceeded, path + " exceeds its entry limit");
    }
    NsightMetadataStringLists result;
    for(const auto& [key, values] : found->items()) {
        metadata_string_size(key, bytes);
        require_array(values, path + " member", Limits::maximum_metadata_entries);
        std::vector<std::string> strings;
        strings.reserve(values.size());
        for(const auto& item : values) {
            strings.push_back(string_value(item, path + " member entry", true, &bytes));
        }
        result.emplace(key, std::move(strings));
    }
    return result;
}
} // namespace

std::string_view nsight_evidence_error_name(NsightEvidenceErrorCode code) {
    switch(code) {
        case Error::MalformedJson:
            return "malformed_json";
        case Error::LimitExceeded:
            return "limit_exceeded";
        case Error::InvalidSchema:
            return "invalid_schema";
        case Error::MissingField:
            return "missing_field";
        case Error::InvalidField:
            return "invalid_field";
        case Error::DuplicateIdentifier:
            return "duplicate_identifier";
        case Error::DuplicateKey:
            return "duplicate_key";
        case Error::UnsupportedVersion:
            return "unsupported_version";
    }
    return "unknown";
}

NsightEvidenceError::NsightEvidenceError(NsightEvidenceErrorCode code, std::string message) :
    std::runtime_error(std::move(message)), code_(code) {}

NsightEvidenceErrorCode NsightEvidenceError::code() const noexcept {
    return code_;
}

NsightMetadata parse_nsight_metadata(std::string_view text) {
    const auto document = bounded_json(text);
    require_object(document, "metadata");
    const auto version =
        unsigned_integer(required(document, "metadata_version", "metadata"), "metadata.metadata_version");
    if(version != 1) {
        fail(Error::UnsupportedVersion, "only metadata_version 1 is supported");
    }
    NsightMetadata result;
    result.metadata_version = 1;
    std::size_t string_bytes = 0;
    result.uuid = optional_string(document, "uuid", string_bytes);
    result.nsight_version = optional_string(document, "nsight_version", string_bytes);
    result.nsight_version_build_id = optional_string(document, "nsight_version_build_id", string_bytes);
    result.primary_api = optional_string(document, "primary_api", string_bytes);
    result.primary_gpu = optional_string(document, "primary_gpu", string_bytes);
    result.driver_vendor = optional_string(document, "driver_vendor", string_bytes);
    result.driver_version = optional_string(document, "driver_version", string_bytes);
    result.os_information = optional_string(document, "os_information", string_bytes);
    result.process_name = optional_string(document, "process_name", string_bytes);
    result.request_time = optional_string(document, "request_time", string_bytes);
    result.captured_frame = optional_string(document, "captured_frame", string_bytes);
    result.resolution = optional_string(document, "resolution", string_bytes);
    result.non_portable = optional_string(document, "non_portable", string_bytes);
    result.d3d12_core_version = optional_string(document, "d3d12_core_version", string_bytes);
    if(const auto found = document.find("has_unsupported_operation"); found != document.end()) {
        if(!found->is_boolean()) {
            fail(Error::InvalidField, "metadata.has_unsupported_operation must be a boolean");
        }
        result.has_unsupported_operation = found->get<bool>();
    }
    result.graphics_apis = optional_string_lists(document, "graphics_apis", string_bytes);
    result.graphics_features = optional_string_lists(document, "graphics_features", string_bytes);
    if(const auto found = document.find("_metadata_collection_"); found != document.end()) {
        require_object(*found, "metadata._metadata_collection_");
        NsightMetadataCollection collection;
        collection.info = optional_strings(*found, "info", "metadata._metadata_collection_", string_bytes);
        collection.warnings = optional_strings(*found, "warnings", "metadata._metadata_collection_", string_bytes);
        result.collection = std::move(collection);
    }
    return result;
}

NsightCppMetadata parse_nsight_cpp_metadata(std::string_view text) {
    const auto document = bounded_json(text);
    require_object(document, "cpp_metadata");
    if(unsigned_integer(required(document, "metadata_version", "cpp_metadata"), "metadata_version") != 1) {
        fail(Error::UnsupportedVersion, "unsupported C++ capture metadata version");
    }
    NsightCppMetadata result;
    result.nsight_version = string_value(required(document, "nsight_version", "cpp_metadata"), "nsight_version", false);
    result.build_id =
        unsigned_integer(required(document, "nsight_version_build_id", "cpp_metadata"), "nsight_version_build_id");
    result.primary_api = string_value(required(document, "primary_api", "cpp_metadata"), "primary_api", false);
    result.primary_gpu = string_value(required(document, "primary_gpu", "cpp_metadata"), "primary_gpu", false);
    result.project_filename =
        string_value(required(document, "project_filename", "cpp_metadata"), "project_filename", false);
    const auto unsupported = required(document, "has_unsupported_operation", "cpp_metadata");
    if(!unsupported.is_boolean()) {
        fail(Error::InvalidField, "has_unsupported_operation must be a boolean");
    }
    result.has_unsupported_operation = unsupported.get<bool>();
    return result;
}

std::vector<NsightEvent> parse_nsight_functions(std::string_view text) {
    const auto document = bounded_json(text);
    require_array(document, "functions", Limits::maximum_records);
    std::set<std::uint64_t> identities;
    std::vector<NsightEvent> result;
    result.reserve(document.size());
    for(std::size_t index = 0; index < document.size(); ++index) {
        const auto& record = document[index];
        const auto path = "functions[" + std::to_string(index) + ']';
        require_object(record, path);
        NsightEvent event;
        event.event_index = unsigned_integer(required(record, "event_index", path), path + ".event_index");
        if(!identities.insert(event.event_index).second) {
            fail(Error::DuplicateIdentifier, path + " duplicates an event_index in this capture");
        }
        event.function_name = string_value(required(record, "function_name", path), path + ".function_name", false);
        event.thread_index = unsigned_integer(required(record, "thread_index", path), path + ".thread_index");
        if(const auto found = record.find("sequence_id"); found != record.end()) {
            event.sequence_id = unsigned_integer(*found, path + ".sequence_id");
        }
        if(const auto found = record.find("indirect_index"); found != record.end()) {
            event.indirect_index = unsigned_integer(*found, path + ".indirect_index");
        }
        result.push_back(std::move(event));
    }
    return result;
}

std::vector<NsightObject> parse_nsight_objects(std::string_view text) {
    const auto document = bounded_json(text);
    require_array(document, "objects", Limits::maximum_records);
    std::set<std::uint64_t> identities;
    std::vector<NsightObject> result;
    result.reserve(document.size());
    for(std::size_t index = 0; index < document.size(); ++index) {
        const auto& record = document[index];
        const auto path = "objects[" + std::to_string(index) + ']';
        require_object(record, path);
        NsightObject object;
        object.uid = unsigned_integer(required(record, "uid", path), path + ".uid");
        if(!identities.insert(object.uid).second) {
            fail(Error::DuplicateIdentifier, path + " duplicates a uid in this capture");
        }
        object.api = string_value(required(record, "api", path), path + ".api", false);
        object.object_name = string_value(required(record, "object_name", path), path + ".object_name");
        object.type_name = string_value(required(record, "type_name", path), path + ".type_name", false);
        object.access_flags = unsigned_integer(required(record, "access_flags", path), path + ".access_flags");
        result.push_back(std::move(object));
    }
    return result;
}
} // namespace ngm
