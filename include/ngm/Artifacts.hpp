#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ngm {
enum class ArtifactErrorCode { InvalidArgument, NotFound, Expired, Busy, QuotaExceeded, DiskFull, Corrupt, Filesystem };

class ArtifactError : public std::runtime_error {
public:
    ArtifactError(ArtifactErrorCode code, std::string message);
    ArtifactErrorCode code() const noexcept;

private:
    ArtifactErrorCode code_;
};

std::string_view artifact_error_name(ArtifactErrorCode code);

struct ArtifactOptions {
    std::filesystem::path root;
    // Zero disables the respective limit. Age starts at completion, not last access.
    std::chrono::seconds max_age{30 * 24 * 60 * 60};
    std::uint64_t max_bytes = 2ULL * 1024 * 1024 * 1024;
    std::function<std::chrono::system_clock::time_point()> clock = std::chrono::system_clock::now;
};

struct ArtifactSummary {
    std::string id;
    std::string status; // staging, complete, failed, expired
    std::int64_t created_at_ms = 0;
    std::optional<std::int64_t> completed_at_ms;
    std::uint64_t bytes = 0;
    bool pinned = false;
    bool in_use = false;
    bool quarantined = false;
    std::string reason;
};

struct ArtifactInfo {
    ArtifactSummary summary;
    nlohmann::json provenance = nlohmann::json::object();
    std::vector<std::string> required_outputs;
    std::size_t file_count = 0;
};

struct ArtifactPage {
    std::vector<ArtifactSummary> artifacts;
    std::optional<std::string> next_after;
};

struct ArtifactFile {
    std::string path;
    std::uint64_t bytes = 0;
};

struct ArtifactFilePage {
    std::vector<ArtifactFile> files;
    std::optional<std::size_t> next_offset;
};

struct ArtifactUsage {
    std::uint64_t completed_bytes = 0;
    std::uint64_t staging_bytes = 0;
    std::uint64_t trash_bytes = 0;
    std::uint64_t metadata_bytes = 0;
    std::uint64_t total_bytes = 0;
    // Subsets of completed/staging bytes, not additional usage.
    std::uint64_t pinned_bytes = 0;
    std::uint64_t in_use_bytes = 0;
    std::uint64_t max_bytes = 0;
    bool quota_exceeded = false;
};

struct ArtifactPruneResult {
    std::vector<std::string> expired_ids;
    std::vector<std::string> errors;
    ArtifactUsage usage;
};

struct ArtifactStoreState;

class ArtifactLease {
public:
    ArtifactLease() = default;
    ArtifactLease(ArtifactLease&& other) noexcept;
    ArtifactLease& operator=(ArtifactLease&& other) noexcept;
    ArtifactLease(const ArtifactLease&) = delete;
    ArtifactLease& operator=(const ArtifactLease&) = delete;
    ~ArtifactLease();
    const std::string& id() const noexcept;
    std::filesystem::path directory() const;

private:
    friend class ArtifactStore;
    ArtifactLease(std::shared_ptr<ArtifactStoreState> state, std::string id);
    std::shared_ptr<ArtifactStoreState> state_;
    std::string id_;
};

class ArtifactWriter {
public:
    ArtifactWriter() = default;
    ArtifactWriter(ArtifactWriter&& other) noexcept;
    ArtifactWriter& operator=(ArtifactWriter&& other) noexcept;
    ArtifactWriter(const ArtifactWriter&) = delete;
    ArtifactWriter& operator=(const ArtifactWriter&) = delete;
    // An unfinished writer becomes a pinned failed/quarantined attempt. Published
    // writers retain a usage lease until destruction, including after publication.
    ~ArtifactWriter();
    const std::string& id() const noexcept;
    std::filesystem::path directory() const;
    std::filesystem::path raw_directory() const;
    std::filesystem::path derived_directory() const;

private:
    friend class ArtifactStore;
    ArtifactWriter(std::shared_ptr<ArtifactStoreState> state, std::string id);
    std::shared_ptr<ArtifactStoreState> state_;
    std::string id_;
};

class ArtifactStore {
public:
    // Owns an exclusive advisory lock until all store/writer/lease handles close.
    // Never adopts a nonempty directory lacking the store marker.
    explicit ArtifactStore(ArtifactOptions options);
    ArtifactStore(const ArtifactStore&) = delete;
    ArtifactStore& operator=(const ArtifactStore&) = delete;
    ArtifactStore(ArtifactStore&&) noexcept = default;
    ArtifactStore& operator=(ArtifactStore&&) noexcept = default;
    ~ArtifactStore() = default;

    ArtifactWriter begin(nlohmann::json provenance, std::vector<std::string> required_outputs = {},
                         bool pinned = false);
    void update_provenance(ArtifactWriter& writer, nlohmann::json provenance);
    // Persist protection when subprocess cleanup is unconfirmed. Publishing a
    // failure subsequently asserts that cleanup is confirmed and clears quarantine.
    void quarantine(ArtifactWriter& writer, std::string reason);
    // Reacquires a quarantined writer after restart; the caller must confirm
    // owned-process cleanup before publish_failure. Success remains forbidden.
    ArtifactWriter resume_quarantined(const std::string& id);
    // A successful publication freezes raw/derived files. Callers must close all
    // output descriptors before publishing and never mutate published bundles.
    // Quota failure leaves the writer usable for explicit failed publication.
    ArtifactInfo publish_success(ArtifactWriter& writer);
    ArtifactInfo publish_failure(ArtifactWriter& writer, std::string reason);
    ArtifactInfo inspect(const std::string& id) const;
    ArtifactPage list(const std::string& after_id = {}, std::size_t limit = 50) const;
    ArtifactFilePage files(const std::string& id, std::size_t offset = 0, std::size_t limit = 100) const;
    ArtifactInfo pin(const std::string& id, bool pinned);
    ArtifactLease lease(const std::string& id) const;
    // Reads only inventoried raw/derived regular files; maximum_bytes <= 16 MiB.
    // The default remains 1 MiB. Transport tools impose their own smaller caps.
    std::string read(const std::string& id, const std::string& relative_path,
                     std::size_t maximum_bytes = 1024 * 1024) const;
    ArtifactUsage usage() const;
    ArtifactPruneResult prune();
    // Copies a source directory below raw/imported without following links.
    // Required output paths are relative to the source directory. Import errors
    // retain a failed bundle with the copied evidence; the source is never changed.
    ArtifactInfo import_directory(const std::filesystem::path& source, nlohmann::json provenance,
                                  std::vector<std::string> required_outputs = {}, bool pinned = true);
    const std::filesystem::path& root() const noexcept;

private:
    std::shared_ptr<ArtifactStoreState> state_;
};

void to_json(nlohmann::json& json, const ArtifactSummary& value);
void to_json(nlohmann::json& json, const ArtifactInfo& value);
void to_json(nlohmann::json& json, const ArtifactPage& value);
void to_json(nlohmann::json& json, const ArtifactFile& value);
void to_json(nlohmann::json& json, const ArtifactFilePage& value);
void to_json(nlohmann::json& json, const ArtifactUsage& value);
void to_json(nlohmann::json& json, const ArtifactPruneResult& value);
} // namespace ngm
