#pragma once

#include "ngm/Artifacts.hpp"

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

    explicit InspectionService(ArtifactStore& artifacts);
    nlohmann::json metadata(const std::string& capture_id) const;
    nlohmann::json events(const std::string& capture_id, std::size_t offset = 0, std::size_t limit = 50) const;
    nlohmann::json objects(const std::string& capture_id, std::size_t offset = 0, std::size_t limit = 50) const;

private:
    ArtifactStore& artifacts_;
};
} // namespace ngm
