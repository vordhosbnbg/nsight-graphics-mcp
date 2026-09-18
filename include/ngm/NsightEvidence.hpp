#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ngm {
// Analysis limits are intentionally narrower than the raw-export storage limit.
// Apply maximum_bytes when reading a file, before passing its text to a parser.
struct NsightEvidenceLimits {
    static constexpr std::size_t maximum_bytes = 16U * 1024U * 1024U;
    static constexpr std::size_t maximum_records = 100000;
    static constexpr std::size_t maximum_depth = 16;
    static constexpr std::size_t maximum_string_bytes = 16384;
    static constexpr std::size_t maximum_json_values = 1000000;
    static constexpr std::size_t maximum_container_entries = 100000;
    static constexpr std::size_t maximum_metadata_entries = 256;
    static constexpr std::size_t maximum_metadata_string_bytes = 64U * 1024U;
};

enum class NsightEvidenceErrorCode {
    MalformedJson,
    LimitExceeded,
    InvalidSchema,
    MissingField,
    InvalidField,
    DuplicateIdentifier,
    DuplicateKey,
    UnsupportedVersion
};

std::string_view nsight_evidence_error_name(NsightEvidenceErrorCode code);

class NsightEvidenceError : public std::runtime_error {
public:
    NsightEvidenceError(NsightEvidenceErrorCode code, std::string message);
    NsightEvidenceErrorCode code() const noexcept;

private:
    NsightEvidenceErrorCode code_;
};

struct NsightEvent {
    // Identity is meaningful only within the caller's capture. Retained order
    // and function names do not establish arguments, state, or relationships.
    std::uint64_t event_index = 0;
    std::string function_name;
    std::uint64_t thread_index = 0;
    // Opaque observed integer, not a globally unique or contiguous identity.
    std::optional<std::uint64_t> sequence_id;
    // Observed on some indirect-workload records; no parent or argument
    // relationship is inferred from its presence, value, or export position.
    std::optional<std::uint64_t> indirect_index;
};

struct NsightObject {
    // Capture-scoped object identity, not a Vulkan handle or cross-run key.
    std::uint64_t uid = 0;
    std::string api;
    std::string object_name;
    std::string type_name;
    // Opaque observed integer. No Vulkan access-bit semantics are assumed.
    std::uint64_t access_flags = 0;
};

using NsightMetadataStringLists = std::map<std::string, std::vector<std::string>>;

struct NsightMetadataCollection {
    std::optional<std::vector<std::string>> info;
    std::optional<std::vector<std::string>> warnings;
};

struct NsightMetadata {
    std::uint32_t metadata_version = 0;
    std::optional<std::string> uuid;
    std::optional<std::string> nsight_version;
    std::optional<std::string> nsight_version_build_id;
    std::optional<std::string> primary_api;
    std::optional<std::string> primary_gpu;
    std::optional<std::string> driver_vendor;
    std::optional<std::string> driver_version;
    std::optional<std::string> os_information;
    std::optional<std::string> process_name;
    std::optional<std::string> request_time;
    // These fields are strings in the observed export; do not normalize them
    // into a frame number, dimensions, boolean, or timestamp without a contract.
    std::optional<std::string> captured_frame;
    std::optional<std::string> resolution;
    std::optional<std::string> non_portable;
    std::optional<std::string> d3d12_core_version;
    std::optional<bool> has_unsupported_operation;
    std::optional<NsightMetadataStringLists> graphics_apis;
    std::optional<NsightMetadataStringLists> graphics_features;
    std::optional<NsightMetadataCollection> collection;
};

// Pure parsers for observed Nsight 2026.3.1/2026.2.0 JSON exports. These functions own no
// files, artifact leases, processes, or capture identities. The caller associates
// each result with its capture and retains the raw export/tool provenance.
//
// Throw NsightEvidenceError on malformed/schema-invalid/over-limit input. Limits
// and duplicate JSON keys are checked before building a JSON tree. Additional
// fields are bounded and ignored. Present optional fields must have the observed
// type; absence remains nullopt. No text or numeric coercion is performed.
// Metadata requires version 1 and excludes process_environment and command line.
NsightMetadata parse_nsight_metadata(std::string_view text);
std::vector<NsightEvent> parse_nsight_functions(std::string_view text);
std::vector<NsightObject> parse_nsight_objects(std::string_view text);
} // namespace ngm
