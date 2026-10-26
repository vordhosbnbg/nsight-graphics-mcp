#pragma once

#include "ngm/Artifacts.hpp"
#include "ngm/ResourceRead.hpp"
#include "ngm/ShaderTrace.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ngm {
enum class InspectionErrorCode {
    NotCapture,
    IncompleteCapture,
    InvalidReport,
    ExportUnavailable,
    UnsupportedProducer,
    InvalidExport,
    LimitExceeded
};

std::string_view inspection_error_name(InspectionErrorCode code);

class InspectionError : public std::runtime_error {
public:
    InspectionError(InspectionErrorCode code, std::string message);
    InspectionErrorCode code() const noexcept;

private:
    InspectionErrorCode code_;
};

// Retained export queries, independent of the MCP transport. Each query leases
// one published bundle and validates its service report, producer observations,
// and same-bundle metadata before parsing an inventory. Imported directories are
// not service captures. These checks preserve recorded provenance; they do not
// authenticate a producer or establish that a GPU replay loop succeeded.
class InspectionService {
public:
    static constexpr std::size_t maximum_page_records = 100;
    // Leaves room for both MCP structured content and its escaped text fallback.
    static constexpr std::size_t maximum_page_bytes = 256U * 1024U;
    static constexpr std::size_t maximum_result_bytes = 1024U * 1024U;

    explicit InspectionService(ArtifactStore& artifacts, ResourceWorkers workers = {});
    nlohmann::json metadata(const std::string& capture_id) const;
    nlohmann::json events(const std::string& capture_id, std::size_t offset = 0, std::size_t limit = 50) const;
    nlohmann::json objects(const std::string& capture_id, std::size_t offset = 0, std::size_t limit = 50) const;
    // Optional application-reported instrumented fragment observations. This
    // validates retained content/launch provenance, not GPU execution claims.
    nlohmann::json shader_trace(const std::string& capture_id, const ShaderTraceSelection& selection = {},
                                std::size_t offset = 0, std::size_t limit = 50) const;

    // Generated C++ capture evidence uses a separate producer/index contract.
    nlohmann::json cpp_source(const std::string& capture_id, const std::string& source_path, std::size_t start_line = 1,
                              std::size_t max_lines = 100) const;
    nlohmann::json cpp_draws(const std::string& capture_id, const std::string& section = "draws",
                             std::size_t offset = 0, std::size_t limit = 50) const;
    nlohmann::json draw_executions(const std::string& capture_id, const std::string& section = "executions",
                                   std::size_t offset = 0, std::size_t limit = 50) const;
    nlohmann::json draw_state(const std::string& capture_id, const std::string& execution_ref,
                              bool read_push_constants = false, bool pin = false) const;
    nlohmann::json cpp_resources(const std::string& capture_id, const std::string& section = "resources",
                                 std::size_t offset = 0, std::size_t limit = 50) const;
    nlohmann::json cpp_resource(const std::string& capture_id, const std::string& resource_ref, std::size_t offset = 0,
                                std::size_t length = 65536, bool pin = false) const;
    // Without update_ref, list updates or unsupported source forms. A listed
    // capture-bound update_ref decodes source writes with the configured worker.
    nlohmann::json cpp_descriptor_writes(const std::string& capture_id, const std::string& update_ref = {},
                                         const std::string& section = "updates", std::size_t offset = 0,
                                         std::size_t limit = 50, bool pin = false) const;
    // Worker-free candidate listing, or an explicit selected source-value read.
    // The selected path authenticates all helpers, source/database identities,
    // submission lifetimes and shader effects before returning typed values.
    nlohmann::json draw_values(const std::string& capture_id, const std::string& execution_ref = {},
                               const std::string& section = "executions", std::size_t offset = 0,
                               std::size_t limit = 50, bool pin = false) const;

private:
    ArtifactStore& artifacts_;
    ResourceWorkers workers_;
};
} // namespace ngm
