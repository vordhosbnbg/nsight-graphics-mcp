#include "ngm/Artifacts.hpp"
#include "ngm/Version.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <sys/file.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace ngm {
namespace {
using Json = nlohmann::json;
constexpr std::size_t manifest_limit = 4 * 1024 * 1024;
constexpr std::size_t provenance_limit = 64 * 1024;
constexpr std::size_t file_limit = 4096;
constexpr std::size_t entry_limit = 16384;
constexpr std::size_t directory_depth_limit = 64;
constexpr std::size_t path_limit = 512;
constexpr std::size_t reason_limit = 4096;

class Descriptor {
public:
    explicit Descriptor(int value = -1) : value_(value) {}
    Descriptor(Descriptor&& other) noexcept : value_(std::exchange(other.value_, -1)) {}
    Descriptor& operator=(Descriptor&& other) noexcept {
        if(this != &other) {
            if(value_ >= 0) {
                close(value_);
            }
            value_ = std::exchange(other.value_, -1);
        }
        return *this;
    }
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    ~Descriptor() {
        if(value_ >= 0) {
            close(value_);
        }
    }
    int get() const {
        return value_;
    }

private:
    int value_;
};

[[noreturn]] void fail_errno(const std::string& action, int error = errno) {
    const auto code = error == ENOSPC || error == EDQUOT ? ArtifactErrorCode::DiskFull : ArtifactErrorCode::Filesystem;
    throw ArtifactError(
        code, action + ": " + std::strerror(error) +
                  (code == ArtifactErrorCode::DiskFull ? "; free space or increase the filesystem quota" : ""));
}
[[noreturn]] void corrupt(const std::string& message) {
    throw ArtifactError(ArtifactErrorCode::Corrupt, message);
}
bool valid_id(std::string_view id) {
    return id.size() == 39 && id.substr(0, 7) == "bundle-" && std::all_of(id.begin() + 7, id.end(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}
void check_id(const std::string& id) {
    if(!valid_id(id)) {
        throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Invalid artifact ID: " + id.substr(0, 100));
    }
}
std::string unique_suffix() {
    std::array<unsigned char, 16> bytes{};
    std::size_t offset = 0;
    while(offset < bytes.size()) {
        const auto count = getrandom(bytes.data() + offset, bytes.size() - offset, 0);
        if(count < 0 && errno == EINTR) {
            continue;
        }
        if(count <= 0) {
            fail_errno("Generate artifact identity");
        }
        offset += static_cast<std::size_t>(count);
    }
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    for(const auto byte : bytes) {
        result += hex[byte >> 4];
        result += hex[byte & 15];
    }
    return result;
}
void sync_directory(int fd) {
    if(fsync(fd) != 0) {
        fail_errno("Synchronize artifact directory");
    }
}
Descriptor directory_at(int parent, const std::string& name, bool create = false) {
    if(create && mkdirat(parent, name.c_str(), 0700) != 0 && errno != EEXIST) {
        fail_errno("Create artifact directory " + name);
    }
    Descriptor result(openat(parent, name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if(result.get() < 0) {
        fail_errno("Open directory without following links: " + name);
    }
    return result;
}
Descriptor absolute_directory(const std::filesystem::path& input, bool create) {
    if(input.empty()) {
        throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Artifact directory must not be empty");
    }
    const auto path = std::filesystem::absolute(input).lexically_normal();
    Descriptor result(open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    if(result.get() < 0) {
        fail_errno("Open filesystem root");
    }
    for(const auto& component : path.relative_path()) {
        result = directory_at(result.get(), component.string(), create);
    }
    return result;
}
std::vector<std::string> directory_names(int fd) {
    const auto duplicate = openat(fd, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if(duplicate < 0) {
        fail_errno("Enumerate artifact directory");
    }
    DIR* opened = fdopendir(duplicate);
    if(!opened) {
        close(duplicate);
        fail_errno("Enumerate artifact directory");
    }
    struct Directory {
        DIR* value;
        ~Directory() {
            closedir(value);
        }
    } dir{opened};
    std::vector<std::string> names;
    errno = 0;
    while(const auto* entry = readdir(dir.value)) {
        const std::string name = entry->d_name;
        if(name != "." && name != "..") {
            names.push_back(name);
        }
        errno = 0;
    }
    if(errno != 0) {
        fail_errno("Enumerate artifact directory");
    }
    std::sort(names.begin(), names.end());
    return names;
}
std::optional<struct stat> attributes_at(int fd, const std::string& name) {
    struct stat attributes{};
    if(fstatat(fd, name.c_str(), &attributes, AT_SYMLINK_NOFOLLOW) != 0) {
        if(errno == ENOENT) {
            return {};
        }
        fail_errno("Inspect artifact path " + name);
    }
    return attributes;
}
void regular_attributes(const struct stat& attributes, const std::string& name) {
    if(!S_ISREG(attributes.st_mode) || attributes.st_nlink != 1 || attributes.st_size < 0) {
        corrupt("Expected a regular artifact file with one link: " + name);
    }
}
std::string read_at(int parent, const std::string& name, std::size_t limit) {
    Descriptor fd(openat(parent, name.c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC));
    if(fd.get() < 0) {
        fail_errno("Read artifact file " + name);
    }
    struct stat attributes{};
    if(fstat(fd.get(), &attributes) != 0) {
        fail_errno("Inspect opened artifact file " + name);
    }
    regular_attributes(attributes, name);
    if(static_cast<std::uint64_t>(attributes.st_size) > limit) {
        throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Artifact file exceeds the read limit: " + name);
    }
    std::string data(static_cast<std::size_t>(attributes.st_size), '\0');
    std::size_t offset = 0;
    while(offset < data.size()) {
        const auto count = ::read(fd.get(), data.data() + offset, data.size() - offset);
        if(count < 0 && errno == EINTR) {
            continue;
        }
        if(count <= 0) {
            corrupt("Artifact file changed while reading: " + name);
        }
        offset += static_cast<std::size_t>(count);
    }
    char extra;
    ssize_t count;
    do {
        count = ::read(fd.get(), &extra, 1);
    } while(count < 0 && errno == EINTR);
    if(count != 0) {
        corrupt("Artifact file changed while reading: " + name);
    }
    return data;
}
void write_all(int fd, std::string_view data) {
    std::size_t offset = 0;
    while(offset < data.size()) {
        const auto count = ::write(fd, data.data() + offset, data.size() - offset);
        if(count < 0 && errno == EINTR) {
            continue;
        }
        if(count <= 0) {
            fail_errno("Write artifact metadata");
        }
        offset += static_cast<std::size_t>(count);
    }
}
void atomic_json(int parent, const std::string& name, const Json& value) {
    const auto data = value.dump(2) + '\n';
    if(data.size() > manifest_limit) {
        throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Artifact metadata exceeds the 4 MiB limit");
    }
    const auto temporary = ".tmp-" + unique_suffix();
    Descriptor fd(openat(parent, temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
    if(fd.get() < 0) {
        fail_errno("Create artifact metadata");
    }
    try {
        write_all(fd.get(), data);
        if(fsync(fd.get()) != 0) {
            fail_errno("Synchronize artifact metadata");
        }
        if(renameat(parent, temporary.c_str(), parent, name.c_str()) != 0) {
            fail_errno("Publish artifact metadata " + name);
        }
        sync_directory(parent);
    } catch(...) {
        unlinkat(parent, temporary.c_str(), 0);
        throw;
    }
}
Json read_json(int fd, const std::string& name) {
    try {
        return Json::parse(read_at(fd, name, manifest_limit));
    } catch(const Json::exception& error) {
        corrupt("Invalid artifact JSON in " + name + ": " + error.what());
    }
}
void remove_file(int fd, const std::string& name) {
    if(unlinkat(fd, name.c_str(), 0) != 0 && errno != ENOENT) {
        fail_errno("Remove managed artifact file " + name);
    }
}
bool temporary_name(const std::string& name) {
    return name.size() == 37 && name.starts_with(".tmp-") && std::all_of(name.begin() + 5, name.end(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}
void clean_temporaries(int fd) {
    for(const auto& name : directory_names(fd)) {
        if(temporary_name(name)) {
            const auto attributes = attributes_at(fd, name);
            if(attributes && S_ISREG(attributes->st_mode) && attributes->st_nlink == 1) {
                remove_file(fd, name);
            } else {
                corrupt("Invalid temporary artifact metadata: " + name);
            }
        }
    }
}
void check_relative(const std::string& path, bool require_partition = true) {
    const std::filesystem::path value(path);
    if(path.empty() || path.size() > path_limit || path.find('\0') != std::string::npos || value.is_absolute() ||
       value.generic_string() != path || path.back() == '/') {
        throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Invalid artifact relative path");
    }
    for(const auto& component : value) {
        if(component == "." || component == ".." || component.empty()) {
            throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Artifact paths cannot traverse directories");
        }
    }
    if(require_partition && (!path.starts_with("raw/") && !path.starts_with("derived/"))) {
        throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Artifact outputs must be below raw/ or derived/");
    }
}
void validate_inputs(const Json& provenance, const std::vector<std::string>& required) {
    if(!provenance.is_object() || provenance.dump().size() > provenance_limit) {
        throw ArtifactError(ArtifactErrorCode::InvalidArgument,
                            "Artifact provenance must be an object no larger than 64 KiB");
    }
    if(required.size() > 128) {
        throw ArtifactError(ArtifactErrorCode::InvalidArgument, "At most 128 required artifact outputs are supported");
    }
    std::set<std::string> seen;
    for(const auto& path : required) {
        check_relative(path);
        if(!seen.insert(path).second) {
            throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Duplicate required artifact output: " + path);
        }
    }
}
void add_bytes(std::uint64_t& total, std::uint64_t bytes) {
    if(bytes > std::numeric_limits<std::uint64_t>::max() - total) {
        corrupt("Artifact byte accounting overflow");
    }
    total += bytes;
}
std::uint64_t tree_bytes(int fd) {
    // Accounting must remain usable even when an active or quarantined writer
    // has exceeded publication limits. Walk iteratively instead of imposing a
    // second, inconsistent nesting limit or consuming the C++ call stack.
    struct Frame {
        Descriptor directory;
        std::vector<std::string> names;
        std::size_t next = 0;
    };
    std::vector<Frame> frames;
    frames.push_back({directory_at(fd, "."), directory_names(fd), 0});
    std::uint64_t total = 0;
    while(!frames.empty()) {
        auto& frame = frames.back();
        if(frame.next == frame.names.size()) {
            frames.pop_back();
            continue;
        }
        const auto name = frame.names[frame.next++];
        const auto attributes = attributes_at(frame.directory.get(), name);
        if(!attributes) {
            continue;
        }
        if(S_ISDIR(attributes->st_mode)) {
            auto child = directory_at(frame.directory.get(), name);
            auto names = directory_names(child.get());
            frames.push_back({std::move(child), std::move(names), 0});
        } else if(attributes->st_size > 0) {
            // Count the link itself; never dereference an unvalidated writer's links.
            add_bytes(total, static_cast<std::uint64_t>(attributes->st_size));
        }
    }
    return total;
}
void inventory_tree(int fd, const std::string& prefix, std::vector<ArtifactFile>& files, std::size_t& entries,
                    bool synchronize, std::size_t depth) {
    for(const auto& name : directory_names(fd)) {
        if(++entries > entry_limit) {
            throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Artifact bundle exceeds 16384 directory entries");
        }
        const auto path = prefix + '/' + name;
        check_relative(path);
        const auto attributes = attributes_at(fd, name);
        if(!attributes) {
            corrupt("Artifact output disappeared during publication: " + path);
        }
        if(S_ISDIR(attributes->st_mode)) {
            if(depth == directory_depth_limit) {
                throw ArtifactError(ArtifactErrorCode::InvalidArgument,
                                    "Artifact directories exceed 64 levels below the bundle root");
            }
            const auto child = directory_at(fd, name);
            inventory_tree(child.get(), path, files, entries, synchronize, depth + 1);
        } else {
            regular_attributes(*attributes, path);
            if(files.size() == file_limit) {
                throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Artifact bundle exceeds 4096 files");
            }
            files.push_back({path, static_cast<std::uint64_t>(attributes->st_size)});
            if(synchronize) {
                Descriptor file(openat(fd, name.c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC));
                if(file.get() < 0 || fsync(file.get()) != 0) {
                    fail_errno("Synchronize artifact output " + path);
                }
            }
        }
    }
    if(synchronize) {
        sync_directory(fd);
    }
}
std::vector<ArtifactFile> inventory(int bundle, bool synchronize) {
    std::vector<ArtifactFile> files;
    std::size_t entries = 0;
    for(const auto* partition : {"raw", "derived"}) {
        const auto dir = directory_at(bundle, partition);
        inventory_tree(dir.get(), partition, files, entries, synchronize, 1);
    }
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.path < b.path; });
    return files;
}
void validate_required(const Json& manifest, const std::vector<ArtifactFile>& files) {
    for(const auto& required : manifest.at("required_outputs")) {
        const auto path = required.get<std::string>();
        const auto found =
            std::find_if(files.begin(), files.end(), [&](const auto& file) { return file.path == path; });
        if(found == files.end() || found->bytes == 0) {
            throw ArtifactError(ArtifactErrorCode::InvalidArgument,
                                "Missing or empty required artifact output: " + path);
        }
    }
}
std::uint64_t json_unsigned(const Json& value) {
    if(!(value.is_number_unsigned() || (value.is_number_integer() && value.get<std::int64_t>() >= 0))) {
        corrupt("Invalid unsigned artifact metadata value");
    }
    return value.get<std::uint64_t>();
}
std::int64_t timestamp(const Json& value) {
    const auto result = json_unsigned(value);
    if(result > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        corrupt("Invalid artifact timestamp");
    }
    return static_cast<std::int64_t>(result);
}
void validate_manifest(const Json& manifest, const std::string& id) {
    try {
        if(!manifest.is_object() || manifest.at("schema_version") != 1 || manifest.at("id") != id ||
           !manifest.at("project_version").is_string() ||
           manifest.at("project_version").get<std::string>().size() > 100) {
            corrupt("Invalid artifact manifest identity: " + id);
        }
        const auto status = manifest.at("status").get<std::string>();
        if(status != "staging" && status != "complete" && status != "failed") {
            corrupt("Invalid artifact manifest status: " + id);
        }
        const auto created = timestamp(manifest.at("created_at_ms"));
        if(status == "staging") {
            if(!manifest.at("completed_at_ms").is_null()) {
                corrupt("Staging artifact has a completion timestamp: " + id);
            }
        } else if(timestamp(manifest.at("completed_at_ms")) < created) {
            corrupt("Artifact completion precedes creation: " + id);
        }
        const auto reason = manifest.at("reason").get<std::string>();
        if(reason.size() > reason_limit || (status == "failed" && reason.empty()) ||
           !manifest.at("quarantined").is_boolean() || (status == "complete" && manifest.at("quarantined") == true)) {
            corrupt("Invalid artifact failure or quarantine state: " + id);
        }
        validate_inputs(manifest.at("provenance"), manifest.at("required_outputs").get<std::vector<std::string>>());
        const auto& files = manifest.at("files");
        if(!files.is_array() || files.size() > file_limit) {
            corrupt("Invalid artifact file inventory: " + id);
        }
        std::string previous;
        for(const auto& file : files) {
            const auto path = file.at("path").get<std::string>();
            check_relative(path);
            if(path <= previous) {
                corrupt("Artifact inventory is not unique and ordered: " + id);
            }
            previous = path;
            json_unsigned(file.at("bytes"));
        }
    } catch(const Json::exception& error) {
        corrupt("Malformed artifact manifest " + id + ": " + error.what());
    }
}
void validate_inventory(const Json& manifest, const std::vector<ArtifactFile>& files) {
    if(manifest.at("files") != Json(files)) {
        corrupt("Artifact output inventory differs from its published manifest: " +
                manifest.at("id").get<std::string>());
    }
    if(manifest.at("status") == "complete") {
        validate_required(manifest, files);
    }
}
void remove_tree(int parent, const std::string& name) {
    const auto attributes = attributes_at(parent, name);
    if(!attributes) {
        return;
    }
    if(!S_ISDIR(attributes->st_mode)) {
        remove_file(parent, name);
        return;
    }
    const auto dir = directory_at(parent, name);
    for(const auto& child : directory_names(dir.get())) {
        remove_tree(dir.get(), child);
    }
    if(unlinkat(parent, name.c_str(), AT_REMOVEDIR) != 0 && errno != ENOENT) {
        fail_errno("Delete claimed artifact directory " + name);
    }
}
bool path_contains(const std::filesystem::path& parent, const std::filesystem::path& child) {
    auto a = parent.begin();
    auto b = child.begin();
    for(; a != parent.end(); ++a, ++b) {
        if(b == child.end() || *a != *b) {
            return false;
        }
    }
    return true;
}
void copy_tree(int source, int destination, const std::string& prefix, std::size_t& entries, std::size_t& files,
               std::uint64_t& copied, std::uint64_t maximum_bytes, std::size_t depth) {
    for(const auto& name : directory_names(source)) {
        if(++entries > entry_limit) {
            throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Imported evidence exceeds 16384 entries");
        }
        check_relative("raw/imported/" + prefix + name);
        const auto attributes = attributes_at(source, name);
        if(!attributes) {
            corrupt("Imported evidence changed while copying: " + name);
        }
        if(S_ISDIR(attributes->st_mode)) {
            // Reject before mkdir: a partial import must itself satisfy the
            // publication limits so its available evidence can publish failure.
            if(depth == directory_depth_limit) {
                throw ArtifactError(ArtifactErrorCode::InvalidArgument,
                                    "Imported directories exceed 64 levels below the bundle root");
            }
            const auto from = directory_at(source, name);
            const auto to = directory_at(destination, name, true);
            copy_tree(from.get(), to.get(), prefix + name + '/', entries, files, copied, maximum_bytes, depth + 1);
            sync_directory(to.get());
            continue;
        }
        regular_attributes(*attributes, name);
        if(files == file_limit) {
            throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Imported evidence exceeds 4096 files");
        }
        ++files;
        Descriptor from(openat(source, name.c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC));
        Descriptor to(openat(destination, name.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
        if(from.get() < 0 || to.get() < 0) {
            fail_errno("Copy imported evidence " + name);
        }
        struct stat opened{};
        if(fstat(from.get(), &opened) != 0) {
            fail_errno("Inspect imported evidence " + name);
        }
        regular_attributes(opened, name);
        std::array<char, 65536> buffer;
        while(true) {
            const auto count = ::read(from.get(), buffer.data(), buffer.size());
            if(count < 0 && errno == EINTR) {
                continue;
            }
            if(count < 0) {
                fail_errno("Read imported evidence " + name);
            }
            if(count == 0) {
                break;
            }
            add_bytes(copied, static_cast<std::uint64_t>(count));
            if(maximum_bytes && copied > maximum_bytes) {
                throw ArtifactError(ArtifactErrorCode::QuotaExceeded,
                                    "Imported evidence exceeds the artifact byte budget");
            }
            write_all(to.get(), std::string_view(buffer.data(), static_cast<std::size_t>(count)));
        }
        if(fsync(to.get()) != 0) {
            fail_errno("Synchronize imported evidence " + name);
        }
    }
}
} // namespace

struct ArtifactStoreState {
    struct Entry {
        Json manifest;
        bool pinned = false;
        bool staged = true;
        std::size_t leases = 0;
        bool writer_active = false;
    };
    ArtifactOptions options;
    Descriptor root;
    Descriptor ownership_lock;
    Descriptor staging;
    Descriptor bundles;
    Descriptor expired;
    Descriptor trash;
    mutable std::mutex mutex;
    std::mutex deletion_mutex;
    std::map<std::string, Entry> entries;
    std::map<std::string, ArtifactSummary> expirations;
    // Bytes remain conservatively charged while deletion runs outside mutex.
    std::map<std::string, std::uint64_t> trash_usage;

    std::int64_t now() const {
        const auto value =
            std::chrono::duration_cast<std::chrono::milliseconds>(options.clock().time_since_epoch()).count();
        if(value < 0) {
            throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Artifact clock must be after the Unix epoch");
        }
        return value;
    }
    Entry& entry(const std::string& id) {
        check_id(id);
        const auto found = entries.find(id);
        if(found != entries.end()) {
            return found->second;
        }
        if(expirations.contains(id)) {
            throw ArtifactError(ArtifactErrorCode::Expired,
                                "Artifact " + id + " expired: " + expirations.at(id).reason);
        }
        throw ArtifactError(ArtifactErrorCode::NotFound, "Artifact does not exist: " + id);
    }
    int parent(const Entry& entry) const {
        return entry.staged ? staging.get() : bundles.get();
    }
    ArtifactInfo info(const std::string& id, const Entry& entry) const {
        ArtifactInfo result;
        auto& summary = result.summary;
        const auto& manifest = entry.manifest;
        summary.id = id;
        summary.status = manifest.at("status").get<std::string>();
        summary.created_at_ms = timestamp(manifest.at("created_at_ms"));
        if(!manifest.at("completed_at_ms").is_null()) {
            summary.completed_at_ms = timestamp(manifest.at("completed_at_ms"));
        }
        summary.pinned = entry.pinned;
        summary.in_use = entry.leases != 0 || entry.writer_active;
        summary.quarantined = manifest.at("quarantined").get<bool>();
        summary.reason = manifest.at("reason").get<std::string>();
        const auto dir = directory_at(parent(entry), id);
        summary.bytes = tree_bytes(dir.get());
        result.provenance = manifest.at("provenance");
        result.required_outputs = manifest.at("required_outputs").get<std::vector<std::string>>();
        result.file_count = manifest.at("files").size();
        return result;
    }
    ArtifactUsage usage_locked() const {
        ArtifactUsage usage;
        usage.max_bytes = options.max_bytes;
        for(const auto& [id, entry] : entries) {
            const auto dir = directory_at(parent(entry), id);
            const auto bytes = tree_bytes(dir.get());
            add_bytes(entry.staged ? usage.staging_bytes : usage.completed_bytes, bytes);
            if(entry.pinned) {
                add_bytes(usage.pinned_bytes, bytes);
            }
            if(entry.leases || entry.writer_active) {
                add_bytes(usage.in_use_bytes, bytes);
            }
        }
        for(const auto& [id, bytes] : trash_usage) {
            (void)id;
            add_bytes(usage.trash_bytes, bytes);
        }
        usage.metadata_bytes = tree_bytes(expired.get());
        for(const auto& name : directory_names(root.get())) {
            const auto attributes = attributes_at(root.get(), name);
            if(attributes && S_ISREG(attributes->st_mode)) {
                add_bytes(usage.metadata_bytes, static_cast<std::uint64_t>(attributes->st_size));
            }
        }
        add_bytes(usage.total_bytes, usage.completed_bytes);
        add_bytes(usage.total_bytes, usage.staging_bytes);
        add_bytes(usage.total_bytes, usage.trash_bytes);
        add_bytes(usage.total_bytes, usage.metadata_bytes);
        usage.quota_exceeded = options.max_bytes && usage.total_bytes > options.max_bytes;
        return usage;
    }
    Json initial_manifest(const std::string& id, std::int64_t created, Json provenance,
                          std::vector<std::string> required) const {
        return {{"schema_version", 1},
                {"id", id},
                {"project_version", project_version()},
                {"status", "staging"},
                {"created_at_ms", created},
                {"completed_at_ms", nullptr},
                {"reason", ""},
                {"quarantined", false},
                {"provenance", std::move(provenance)},
                {"required_outputs", std::move(required)},
                {"files", Json::array()}};
    }
    void write_pin(int dir, const std::string& id, bool pinned) const {
        atomic_json(dir, "pin.json", {{"schema_version", 1}, {"id", id}, {"pinned", pinned}});
    }
    bool read_pin(int dir, const std::string& id) const {
        const auto value = read_json(dir, "pin.json");
        try {
            if(!value.is_object() || value.at("schema_version") != 1 || value.at("id") != id ||
               !value.at("pinned").is_boolean()) {
                corrupt("Invalid artifact pin state: " + id);
            }
            return value.at("pinned").get<bool>();
        } catch(const Json::exception& error) {
            corrupt("Malformed artifact pin state " + id + ": " + error.what());
        }
    }
    void quarantine_locked(const std::string& id, std::string reason, std::int64_t completed) {
        auto& value = entry(id);
        if(!value.staged) {
            throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Published artifacts cannot be quarantined");
        }
        if(reason.empty() || reason.size() > reason_limit) {
            throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Quarantine reason must contain 1 to 4096 bytes");
        }
        value.pinned = true;
        value.manifest["quarantined"] = true;
        value.manifest["status"] = "failed";
        value.manifest["completed_at_ms"] = std::max(completed, timestamp(value.manifest.at("created_at_ms")));
        value.manifest["reason"] = std::move(reason);
        const auto dir = directory_at(staging.get(), id);
        write_pin(dir.get(), id, true);
        atomic_json(dir.get(), "manifest.json", value.manifest);
    }
    Json prepare_locked(const std::string& id, bool success, std::string reason, std::int64_t completed,
                        bool synchronize) {
        auto& value = entry(id);
        if(!value.staged) {
            throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Artifact is already published: " + id);
        }
        if(success && value.manifest.at("quarantined") == true) {
            throw ArtifactError(ArtifactErrorCode::InvalidArgument,
                                "Quarantined attempts can only publish failure after confirmed cleanup");
        }
        if(!success && (reason.empty() || reason.size() > reason_limit)) {
            throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Failure reason must contain 1 to 4096 bytes");
        }
        const auto dir = directory_at(staging.get(), id);
        clean_temporaries(dir.get());
        for(const auto& name : directory_names(dir.get())) {
            if(name != "manifest.json" && name != "pin.json" && name != "raw" && name != "derived") {
                throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Unexpected file outside raw/derived: " + name);
            }
        }
        const auto files = inventory(dir.get(), synchronize);
        if(success) {
            validate_required(value.manifest, files);
        }
        auto manifest = value.manifest;
        manifest["status"] = success ? "complete" : "failed";
        manifest["quarantined"] = false;
        manifest["completed_at_ms"] = std::max(completed, timestamp(manifest.at("created_at_ms")));
        manifest["reason"] = std::move(reason);
        manifest["files"] = files;
        validate_manifest(manifest, id);
        return manifest;
    }
    std::uint64_t publication_growth_locked(const std::string& id, const Json& manifest) const {
        const auto bytes = manifest.dump(2).size() + 1;
        if(bytes > manifest_limit) {
            throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Artifact metadata exceeds the 4 MiB limit");
        }
        const auto dir = directory_at(staging.get(), id);
        const auto previous = attributes_at(dir.get(), "manifest.json");
        const auto previous_bytes = previous ? static_cast<std::uint64_t>(previous->st_size) : 0;
        return bytes > previous_bytes ? bytes - previous_bytes : 0;
    }
    bool exceeds_budget(std::uint64_t current_bytes, std::uint64_t additional_bytes = 0) const {
        return options.max_bytes &&
               (additional_bytes > options.max_bytes || current_bytes > options.max_bytes - additional_bytes);
    }
    ArtifactInfo finalize_locked(const std::string& id, bool success, std::string reason, std::int64_t completed) {
        auto manifest = prepare_locked(id, success, std::move(reason), completed, true);
        auto& value = entry(id);
        const auto dir = directory_at(staging.get(), id);
        if(success && options.max_bytes) {
            auto predicted = usage_locked().total_bytes;
            const auto previous = attributes_at(dir.get(), "manifest.json");
            if(previous) {
                predicted -= static_cast<std::uint64_t>(previous->st_size);
            }
            add_bytes(predicted, manifest.dump(2).size() + 1);
            if(predicted > options.max_bytes) {
                throw ArtifactError(ArtifactErrorCode::QuotaExceeded,
                                    "Artifact final manifest exceeds the storage budget; staged evidence is retained");
            }
        }
        atomic_json(dir.get(), "manifest.json", manifest);
        if(renameat(staging.get(), id.c_str(), bundles.get(), id.c_str()) != 0) {
            fail_errno("Publish artifact bundle " + id);
        }
        // The writer remains protective until its coordinator releases it.
        value.manifest = std::move(manifest);
        value.staged = false;
        sync_directory(bundles.get());
        sync_directory(staging.get());
        return info(id, value);
    }
    void release_lease(const std::string& id) noexcept {
        std::lock_guard lock(mutex);
        const auto found = entries.find(id);
        if(found != entries.end() && found->second.leases) {
            --found->second.leases;
        }
    }
    void release_writer(const std::string& id) noexcept {
        try {
            const auto completed = now();
            std::lock_guard lock(mutex);
            const auto found = entries.find(id);
            if(found == entries.end() || !found->second.writer_active) {
                return;
            }
            found->second.writer_active = false;
            if(found->second.staged && found->second.manifest.at("quarantined") != true) {
                quarantine_locked(id,
                                  "Writer closed without confirmed publication; owned-process cleanup must be verified",
                                  completed);
            }
        } catch(...) {
            // No destructor removes evidence or reports false success. Restart
            // pins and quarantines any incomplete staging metadata.
        }
    }
    void initialize();
    void recover();
    ArtifactPruneResult prune(std::uint64_t additional_bytes = 0);
};

void ArtifactStoreState::initialize() {
    if(options.root.empty() || !options.clock || options.max_age.count() < 0) {
        throw ArtifactError(ArtifactErrorCode::InvalidArgument,
                            "Artifact root, clock, and nonnegative age are required");
    }
    options.root = std::filesystem::absolute(options.root).lexically_normal();
    root = absolute_directory(options.root, true);
    if(!attributes_at(root.get(), ".ngm-store.json")) {
        // Reject unrelated data before creating even the ownership lock. Check
        // again after locking because another initializer may race this preflight.
        for(const auto& name : directory_names(root.get())) {
            if(name != ".lock") {
                throw ArtifactError(ArtifactErrorCode::InvalidArgument,
                                    "Refusing to adopt nonempty unmarked artifact root: " + options.root.string());
            }
        }
    }
    ownership_lock = Descriptor(openat(root.get(), ".lock", O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600));
    if(ownership_lock.get() < 0) {
        fail_errno("Open artifact ownership lock");
    }
    struct stat lock_attributes{};
    if(fstat(ownership_lock.get(), &lock_attributes) != 0) {
        fail_errno("Inspect artifact ownership lock");
    }
    regular_attributes(lock_attributes, ".lock");
    if(flock(ownership_lock.get(), LOCK_EX | LOCK_NB) != 0) {
        if(errno == EWOULDBLOCK || errno == EAGAIN) {
            throw ArtifactError(ArtifactErrorCode::Busy,
                                "Artifact root is already owned by another store: " + options.root.string());
        }
        fail_errno("Acquire artifact ownership lock");
    }
    if(!attributes_at(root.get(), ".ngm-store.json")) {
        for(const auto& name : directory_names(root.get())) {
            if(name != ".lock") {
                throw ArtifactError(ArtifactErrorCode::InvalidArgument,
                                    "Refusing to adopt nonempty unmarked artifact root: " + options.root.string());
            }
        }
        atomic_json(root.get(), ".ngm-store.json", {{"schema_version", 1}, {"owner", "nsight-graphics-mcp"}});
    } else {
        const auto marker = read_json(root.get(), ".ngm-store.json");
        if(marker != Json{{"schema_version", 1}, {"owner", "nsight-graphics-mcp"}}) {
            corrupt("Unrecognized artifact store marker");
        }
    }
    clean_temporaries(root.get());
    for(const auto& name : directory_names(root.get())) {
        if(name != ".lock" && name != ".ngm-store.json" && name != "staging" && name != "bundles" &&
           name != "expired" && name != "trash") {
            corrupt("Unexpected artifact root entry; refusing automatic cleanup: " + name);
        }
    }
    staging = directory_at(root.get(), "staging", true);
    bundles = directory_at(root.get(), "bundles", true);
    expired = directory_at(root.get(), "expired", true);
    trash = directory_at(root.get(), "trash", true);
    sync_directory(root.get());
    recover();
}

void ArtifactStoreState::recover() {
    clean_temporaries(expired.get());
    for(const auto& name : directory_names(expired.get())) {
        if(!name.ends_with(".json") || !valid_id(name.substr(0, name.size() - 5))) {
            corrupt("Unexpected expiration record: " + name);
        }
        const auto value = read_json(expired.get(), name);
        const auto id = name.substr(0, name.size() - 5);
        try {
            if(value.at("schema_version") != 1 || value.at("id") != id || value.at("status") != "expired" ||
               value.at("pinned") != false || value.at("in_use") != false || value.at("quarantined") != false) {
                corrupt("Invalid expiration record: " + name);
            }
            ArtifactSummary summary;
            summary.id = id;
            summary.status = "expired";
            summary.created_at_ms = timestamp(value.at("created_at_ms"));
            summary.completed_at_ms = timestamp(value.at("completed_at_ms"));
            summary.bytes = json_unsigned(value.at("bytes"));
            summary.reason = value.at("reason").get<std::string>();
            if(summary.reason.empty() || summary.reason.size() > reason_limit) {
                corrupt("Invalid expiration reason: " + name);
            }
            timestamp(value.at("expired_at_ms"));
            expirations.emplace(id, std::move(summary));
        } catch(const Json::exception& error) {
            corrupt("Malformed expiration record " + name + ": " + error.what());
        }
    }
    for(const auto& id : directory_names(bundles.get())) {
        check_id(id);
        const auto dir = directory_at(bundles.get(), id);
        clean_temporaries(dir.get());
        const auto manifest = read_json(dir.get(), "manifest.json");
        validate_manifest(manifest, id);
        if(manifest.at("status") == "staging" || manifest.at("quarantined") == true) {
            corrupt("Unfinished manifest in a published artifact: " + id);
        }
        validate_inventory(manifest, inventory(dir.get(), false));
        entries.emplace(id, Entry{manifest, read_pin(dir.get(), id), false});
        if(expirations.erase(id)) {
            // A receipt preceding an uncommitted rename does not erase visible
            // evidence. A committed claim resides exclusively in trash.
            remove_file(expired.get(), id + ".json");
            sync_directory(expired.get());
        }
    }
    for(const auto& id : directory_names(staging.get())) {
        check_id(id);
        if(entries.contains(id) || expirations.contains(id)) {
            corrupt("Duplicate artifact identity during recovery: " + id);
        }
        const auto dir = directory_at(staging.get(), id);
        clean_temporaries(dir.get());
        directory_at(dir.get(), "raw", true);
        directory_at(dir.get(), "derived", true);
        Json manifest;
        bool valid = false;
        if(attributes_at(dir.get(), "manifest.json")) {
            try {
                manifest = read_json(dir.get(), "manifest.json");
                validate_manifest(manifest, id);
                valid = true;
            } catch(const ArtifactError&) {
                const auto attributes = attributes_at(dir.get(), "manifest.json");
                if(!attributes) {
                    throw;
                }
                regular_attributes(*attributes, "manifest.json");
                const auto raw = directory_at(dir.get(), "raw");
                const auto retained = "recovery-manifest-" + unique_suffix() + ".json";
                if(renameat(dir.get(), "manifest.json", raw.get(), retained.c_str()) != 0) {
                    fail_errno("Retain invalid staging manifest");
                }
            }
        }
        if(!valid) {
            manifest = initial_manifest(id, now(), {{"recovery", "interrupted_initialization"}}, {});
        }
        const bool prepared = valid && manifest.at("status") != "staging" && manifest.at("quarantined") == false;
        if(prepared) {
            const bool pinned = read_pin(dir.get(), id);
            validate_inventory(manifest, inventory(dir.get(), true));
            if(renameat(staging.get(), id.c_str(), bundles.get(), id.c_str()) != 0) {
                fail_errno("Recover artifact publication " + id);
            }
            entries.emplace(id, Entry{manifest, pinned, false});
            sync_directory(bundles.get());
            sync_directory(staging.get());
        } else {
            entries.emplace(id, Entry{manifest, true, true});
            quarantine_locked(
                id,
                valid && manifest.at("quarantined") == true
                    ? manifest.at("reason").get<std::string>()
                    : "Interrupted before publication; verify owned-process cleanup before releasing quarantine",
                now());
        }
    }
    for(const auto& id : directory_names(trash.get())) {
        check_id(id);
        if(entries.contains(id) || !expirations.contains(id)) {
            corrupt("Claimed artifact lacks an unambiguous expiration record: " + id);
        }
        const auto dir = directory_at(trash.get(), id);
        trash_usage[id] = tree_bytes(dir.get());
        try {
            remove_tree(trash.get(), id);
            trash_usage.erase(id);
        } catch(const ArtifactError&) {
            // Retained trash remains included in usage and a prune retry.
        }
    }
    sync_directory(trash.get());
}

ArtifactPruneResult ArtifactStoreState::prune(std::uint64_t additional_bytes) {
    std::lock_guard deletion_lock(deletion_mutex);
    ArtifactPruneResult result;
    std::vector<std::pair<std::int64_t, std::string>> candidates;
    {
        std::lock_guard lock(mutex);
        for(const auto& [id, value] : entries) {
            if(!value.staged) {
                candidates.emplace_back(timestamp(value.manifest.at("completed_at_ms")), id);
            }
        }
    }
    std::sort(candidates.begin(), candidates.end());
    // Candidate ordering and clock sampling do not own the protection mutex.
    // Every pin/lease is rechecked atomically with the actual deletion claim.
    const auto time = now();
    const auto delete_claimed = [&](const std::string& id) {
        try {
            remove_tree(trash.get(), id);
            sync_directory(trash.get());
            std::lock_guard lock(mutex);
            trash_usage.erase(id);
        } catch(const ArtifactError& error) {
            result.errors.push_back(id + ": " + error.what());
        }
    };
    std::vector<std::string> previous_claims;
    {
        std::lock_guard lock(mutex);
        for(const auto& [id, bytes] : trash_usage) {
            (void)bytes;
            previous_claims.push_back(id);
        }
    }
    for(const auto& id : previous_claims) {
        delete_claimed(id);
    }
    for(const auto& [completed, id] : candidates) {
        bool claimed = false;
        {
            std::lock_guard lock(mutex);
            const auto found = entries.find(id);
            if(found == entries.end() || found->second.staged || found->second.pinned || found->second.leases ||
               found->second.writer_active) {
                continue;
            }
            const auto age_seconds = time >= completed ? (time - completed) / 1000 : 0;
            const bool aged = options.max_age.count() && age_seconds >= options.max_age.count();
            if(!aged && !exceeds_budget(usage_locked().total_bytes, additional_bytes)) {
                continue;
            }
            auto summary = info(id, found->second).summary;
            summary.status = "expired";
            summary.reason =
                aged ? "Maximum retention age reached" : "Storage budget required oldest eligible evidence removal";
            Json record = summary;
            record["schema_version"] = 1;
            record["expired_at_ms"] = time;
            try {
                atomic_json(expired.get(), id + ".json", record);
                if(renameat(bundles.get(), id.c_str(), trash.get(), id.c_str()) != 0) {
                    const int error = errno;
                    remove_file(expired.get(), id + ".json");
                    sync_directory(expired.get());
                    fail_errno("Claim artifact for deletion " + id, error);
                }
                trash_usage[id] = summary.bytes;
                expirations[id] = std::move(summary);
                entries.erase(found);
                claimed = true;
                result.expired_ids.push_back(id);
                sync_directory(trash.get());
                sync_directory(bundles.get());
            } catch(const ArtifactError& error) {
                result.errors.push_back(id + ": " + error.what());
            }
        }
        if(claimed) {
            delete_claimed(id);
        }
    }
    {
        std::lock_guard lock(mutex);
        result.usage = usage_locked();
    }
    return result;
}

ArtifactError::ArtifactError(ArtifactErrorCode code, std::string message) :
    std::runtime_error(std::move(message)), code_(code) {}
ArtifactErrorCode ArtifactError::code() const noexcept {
    return code_;
}
std::string_view artifact_error_name(ArtifactErrorCode code) {
    switch(code) {
        case ArtifactErrorCode::InvalidArgument:
            return "invalid_argument";
        case ArtifactErrorCode::NotFound:
            return "not_found";
        case ArtifactErrorCode::Expired:
            return "expired";
        case ArtifactErrorCode::Busy:
            return "busy";
        case ArtifactErrorCode::QuotaExceeded:
            return "quota_exceeded";
        case ArtifactErrorCode::DiskFull:
            return "disk_full";
        case ArtifactErrorCode::Corrupt:
            return "corrupt";
        case ArtifactErrorCode::Filesystem:
            return "filesystem";
    }
    return "unknown";
}

ArtifactLease::ArtifactLease(std::shared_ptr<ArtifactStoreState> state, std::string id) :
    state_(std::move(state)), id_(std::move(id)) {}
ArtifactLease::ArtifactLease(ArtifactLease&& other) noexcept = default;
ArtifactLease& ArtifactLease::operator=(ArtifactLease&& other) noexcept {
    if(this != &other) {
        if(state_) {
            state_->release_lease(id_);
        }
        state_ = std::move(other.state_);
        id_ = std::move(other.id_);
    }
    return *this;
}
ArtifactLease::~ArtifactLease() {
    if(state_) {
        state_->release_lease(id_);
    }
}
const std::string& ArtifactLease::id() const noexcept {
    return id_;
}
std::filesystem::path ArtifactLease::directory() const {
    if(!state_) {
        throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Artifact lease is empty");
    }
    return state_->options.root / "bundles" / id_;
}

ArtifactWriter::ArtifactWriter(std::shared_ptr<ArtifactStoreState> state, std::string id) :
    state_(std::move(state)), id_(std::move(id)) {}
ArtifactWriter::ArtifactWriter(ArtifactWriter&& other) noexcept = default;
ArtifactWriter& ArtifactWriter::operator=(ArtifactWriter&& other) noexcept {
    if(this != &other) {
        if(state_) {
            state_->release_writer(id_);
        }
        state_ = std::move(other.state_);
        id_ = std::move(other.id_);
    }
    return *this;
}
ArtifactWriter::~ArtifactWriter() {
    if(state_) {
        state_->release_writer(id_);
    }
}
const std::string& ArtifactWriter::id() const noexcept {
    return id_;
}
std::filesystem::path ArtifactWriter::directory() const {
    if(!state_) {
        throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Artifact writer is closed");
    }
    std::lock_guard lock(state_->mutex);
    return state_->options.root / (state_->entry(id_).staged ? "staging" : "bundles") / id_;
}
std::filesystem::path ArtifactWriter::raw_directory() const {
    return directory() / "raw";
}
std::filesystem::path ArtifactWriter::derived_directory() const {
    return directory() / "derived";
}

ArtifactStore::ArtifactStore(ArtifactOptions options) : state_(std::make_shared<ArtifactStoreState>()) {
    state_->options = std::move(options);
    state_->initialize();
}
ArtifactWriter ArtifactStore::begin(Json provenance, std::vector<std::string> required_outputs, bool pinned) {
    validate_inputs(provenance, required_outputs);
    const auto cleanup = state_->prune();
    if(cleanup.usage.quota_exceeded) {
        throw ArtifactError(ArtifactErrorCode::QuotaExceeded,
                            "Artifact budget exhausted by protected, active, or pending-deletion data; unpin unused "
                            "evidence or increase max_bytes");
    }
    const auto created = state_->now();
    std::lock_guard lock(state_->mutex);
    if(state_->usage_locked().quota_exceeded) {
        throw ArtifactError(ArtifactErrorCode::QuotaExceeded,
                            "Artifact byte budget was exhausted by another active writer");
    }
    const auto id = "bundle-" + unique_suffix();
    if(state_->entries.contains(id) || state_->expirations.contains(id) ||
       mkdirat(state_->staging.get(), id.c_str(), 0700) != 0) {
        fail_errno("Allocate unique artifact staging directory");
    }
    const auto dir = directory_at(state_->staging.get(), id);
    const auto manifest = state_->initial_manifest(id, created, std::move(provenance), std::move(required_outputs));
    state_->entries.emplace(id, ArtifactStoreState::Entry{manifest, pinned, true});
    try {
        directory_at(dir.get(), "raw", true);
        directory_at(dir.get(), "derived", true);
        state_->write_pin(dir.get(), id, pinned);
        atomic_json(dir.get(), "manifest.json", manifest);
        sync_directory(state_->staging.get());
        state_->entries.at(id).writer_active = true;
    } catch(const ArtifactError& error) {
        throw ArtifactError(error.code(), std::string(error.what()) + "; incomplete evidence retained as " + id);
    }
    return ArtifactWriter(state_, id);
}
void ArtifactStore::update_provenance(ArtifactWriter& writer, Json provenance) {
    if(writer.state_ != state_ || !writer.state_) {
        throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Writer does not belong to this artifact store");
    }
    std::lock_guard lock(state_->mutex);
    auto& entry = state_->entry(writer.id_);
    if(!entry.staged) {
        throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Published artifact provenance is immutable");
    }
    validate_inputs(provenance, entry.manifest.at("required_outputs").get<std::vector<std::string>>());
    auto manifest = entry.manifest;
    manifest["provenance"] = std::move(provenance);
    const auto dir = directory_at(state_->staging.get(), writer.id_);
    atomic_json(dir.get(), "manifest.json", manifest);
    entry.manifest = std::move(manifest);
}
void ArtifactStore::quarantine(ArtifactWriter& writer, std::string reason) {
    if(writer.state_ != state_ || !writer.state_) {
        throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Writer does not belong to this artifact store");
    }
    const auto completed = state_->now();
    std::lock_guard lock(state_->mutex);
    state_->quarantine_locked(writer.id_, std::move(reason), completed);
}
ArtifactWriter ArtifactStore::resume_quarantined(const std::string& id) {
    std::lock_guard lock(state_->mutex);
    auto& entry = state_->entry(id);
    if(!entry.staged || entry.manifest.at("quarantined") != true || entry.writer_active) {
        throw ArtifactError(ArtifactErrorCode::Busy, "Artifact is not an available quarantined attempt");
    }
    entry.writer_active = true;
    return ArtifactWriter(state_, id);
}
ArtifactInfo ArtifactStore::publish_success(ArtifactWriter& writer) {
    if(writer.state_ != state_ || !writer.state_) {
        throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Writer does not belong to this artifact store");
    }
    const auto completed = state_->now();
    std::uint64_t additional_bytes = 0;
    {
        std::lock_guard lock(state_->mutex);
        const auto manifest = state_->prepare_locked(writer.id_, true, {}, completed, false);
        additional_bytes = state_->publication_growth_locked(writer.id_, manifest);
    }
    // Include the final inventory/metadata growth when choosing old bundles to
    // prune. Current on-disk usage alone can fit while final publication cannot.
    const auto cleanup = state_->prune(additional_bytes);
    if(state_->exceeds_budget(cleanup.usage.total_bytes, additional_bytes)) {
        throw ArtifactError(ArtifactErrorCode::QuotaExceeded,
                            "Artifact budget exceeded; evidence remains staged. Publish failure, unpin unused "
                            "evidence, or increase max_bytes");
    }
    std::lock_guard lock(state_->mutex);
    return state_->finalize_locked(writer.id_, true, {}, completed);
}
ArtifactInfo ArtifactStore::publish_failure(ArtifactWriter& writer, std::string reason) {
    if(writer.state_ != state_ || !writer.state_) {
        throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Writer does not belong to this artifact store");
    }
    const auto completed = state_->now();
    std::lock_guard lock(state_->mutex);
    return state_->finalize_locked(writer.id_, false, std::move(reason), completed);
}
ArtifactInfo ArtifactStore::inspect(const std::string& id) const {
    check_id(id);
    std::lock_guard lock(state_->mutex);
    if(const auto found = state_->expirations.find(id); found != state_->expirations.end()) {
        return {found->second, Json::object(), {}, 0};
    }
    return state_->info(id, state_->entry(id));
}
ArtifactPage ArtifactStore::list(const std::string& after_id, std::size_t limit) const {
    if(limit == 0 || limit > 100) {
        throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Artifact page size must be between 1 and 100");
    }
    if(!after_id.empty()) {
        check_id(after_id);
    }
    std::lock_guard lock(state_->mutex);
    auto live = state_->entries.upper_bound(after_id);
    auto expired = state_->expirations.upper_bound(after_id);
    ArtifactPage result;
    while(result.artifacts.size() < limit && (live != state_->entries.end() || expired != state_->expirations.end())) {
        if(expired == state_->expirations.end() || (live != state_->entries.end() && live->first < expired->first)) {
            result.artifacts.push_back(state_->info(live->first, live->second).summary);
            ++live;
        } else {
            result.artifacts.push_back(expired->second);
            ++expired;
        }
    }
    if(live != state_->entries.end() || expired != state_->expirations.end()) {
        result.next_after = result.artifacts.back().id;
    }
    return result;
}
ArtifactFilePage ArtifactStore::files(const std::string& id, std::size_t offset, std::size_t limit) const {
    if(limit == 0 || limit > 100) {
        throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Artifact file page size must be between 1 and 100");
    }
    std::lock_guard lock(state_->mutex);
    const auto& entry = state_->entry(id);
    if(entry.staged) {
        throw ArtifactError(ArtifactErrorCode::Busy, "Artifact outputs are not published yet");
    }
    const auto& files = entry.manifest.at("files");
    ArtifactFilePage result;
    for(auto index = std::min(offset, files.size()); index < files.size() && result.files.size() < limit; ++index) {
        result.files.push_back({files[index].at("path").get<std::string>(), json_unsigned(files[index].at("bytes"))});
    }
    if(offset < files.size() && result.files.size() < files.size() - offset) {
        result.next_offset = offset + result.files.size();
    }
    return result;
}
ArtifactInfo ArtifactStore::pin(const std::string& id, bool pinned) {
    std::lock_guard lock(state_->mutex);
    auto& entry = state_->entry(id);
    if(!pinned && entry.manifest.at("quarantined") == true) {
        throw ArtifactError(ArtifactErrorCode::Busy,
                            "Quarantined evidence requires confirmed cleanup and failed publication before unpinning");
    }
    const auto dir = directory_at(state_->parent(entry), id);
    try {
        state_->write_pin(dir.get(), id, pinned);
        entry.pinned = pinned;
    } catch(...) {
        // A rename may have succeeded before fsync failed. Preserve protection
        // in memory until restart has read the durable pin state.
        entry.pinned = true;
        throw;
    }
    return state_->info(id, entry);
}
ArtifactLease ArtifactStore::lease(const std::string& id) const {
    std::lock_guard lock(state_->mutex);
    auto& entry = state_->entry(id);
    if(entry.staged) {
        throw ArtifactError(ArtifactErrorCode::Busy, "Artifact outputs are not published yet");
    }
    ++entry.leases;
    return ArtifactLease(state_, id);
}
std::string ArtifactStore::read(const std::string& id, const std::string& relative_path,
                                std::size_t maximum_bytes) const {
    check_relative(relative_path);
    if(maximum_bytes > 16U * 1024U * 1024U) {
        throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Artifact read limit cannot exceed 16 MiB");
    }
    auto protection = lease(id);
    std::uint64_t expected_bytes = 0;
    {
        std::lock_guard lock(state_->mutex);
        const auto& inventory = state_->entry(id).manifest.at("files");
        const auto found = std::find_if(inventory.begin(), inventory.end(),
                                        [&](const auto& value) { return value.at("path") == relative_path; });
        if(found == inventory.end()) {
            throw ArtifactError(ArtifactErrorCode::NotFound,
                                "File is not in the published artifact inventory: " + relative_path);
        }
        expected_bytes = json_unsigned(found->at("bytes"));
    }
    auto dir = directory_at(state_->bundles.get(), id);
    const auto path = std::filesystem::path(relative_path);
    for(const auto& component : path.parent_path()) {
        dir = directory_at(dir.get(), component.string());
    }
    auto result = read_at(dir.get(), path.filename().string(), maximum_bytes);
    if(result.size() != expected_bytes) {
        corrupt("Artifact file size changed after publication: " + relative_path);
    }
    return result;
}
ArtifactUsage ArtifactStore::usage() const {
    std::lock_guard lock(state_->mutex);
    return state_->usage_locked();
}
ArtifactFile ArtifactStore::snapshot_file(const std::string& id, const std::string& relative_path,
                                          ArtifactWriter& destination, const std::string& destination_path,
                                          std::size_t maximum_bytes, std::chrono::steady_clock::time_point deadline) {
    check_relative(relative_path);
    check_relative(destination_path);
    if(std::chrono::steady_clock::now() >= deadline)
        throw ArtifactError(ArtifactErrorCode::Busy, "Snapshot deadline exceeded");
    if(maximum_bytes > 256U * 1024U * 1024U || destination.state_ != state_ || !destination.state_)
        throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Invalid snapshot writer or byte limit");
    auto protection = lease(id);
    std::uint64_t expected = 0;
    {
        std::lock_guard lock(state_->mutex);
        if(!state_->entry(destination.id_).staged)
            throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Snapshot destination is already published");
        const auto& files = state_->entry(id).manifest.at("files");
        const auto found =
            std::find_if(files.begin(), files.end(), [&](const auto& f) { return f.at("path") == relative_path; });
        if(found == files.end())
            throw ArtifactError(ArtifactErrorCode::NotFound, "Snapshot source is not inventoried");
        expected = json_unsigned(found->at("bytes"));
        if(expected > maximum_bytes)
            throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Snapshot source exceeds the byte limit");
    }
    auto from_dir = directory_at(state_->bundles.get(), id);
    const std::filesystem::path source(relative_path), target(destination_path);
    for(const auto& part : source.parent_path())
        from_dir = directory_at(from_dir.get(), part.string());
    auto to_dir = directory_at(state_->staging.get(), destination.id_);
    for(const auto& part : target.parent_path())
        to_dir = directory_at(to_dir.get(), part.string(), true);
    Descriptor from(openat(from_dir.get(), source.filename().c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC));
    if(from.get() < 0)
        fail_errno("Open snapshot source");
    struct stat before{};
    if(fstat(from.get(), &before) != 0)
        fail_errno("Inspect snapshot source");
    regular_attributes(before, relative_path);
    if(static_cast<std::uint64_t>(before.st_size) != expected)
        corrupt("Snapshot inventory size changed");
    Descriptor to(
        openat(to_dir.get(), target.filename().c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
    if(to.get() < 0)
        fail_errno("Create exclusive snapshot file");
    std::array<char, 65536> buffer;
    std::uint64_t copied = 0;
    while(copied < expected) {
        if(std::chrono::steady_clock::now() >= deadline)
            throw ArtifactError(ArtifactErrorCode::Busy, "Snapshot deadline exceeded");
        const auto count = ::read(from.get(), buffer.data(), std::min<std::uint64_t>(buffer.size(), expected - copied));
        if(count < 0 && errno == EINTR)
            continue;
        if(count <= 0)
            corrupt("Snapshot source changed or failed while reading");
        write_all(to.get(), std::string_view(buffer.data(), static_cast<std::size_t>(count)));
        copied += static_cast<std::uint64_t>(count);
    }
    char extra;
    ssize_t count;
    do {
        count = ::read(from.get(), &extra, 1);
    } while(count < 0 && errno == EINTR);
    struct stat after{};
    if(count != 0 || fstat(from.get(), &after) != 0 || before.st_size != after.st_size ||
       before.st_mtim.tv_sec != after.st_mtim.tv_sec || before.st_mtim.tv_nsec != after.st_mtim.tv_nsec ||
       before.st_ctim.tv_sec != after.st_ctim.tv_sec || before.st_ctim.tv_nsec != after.st_ctim.tv_nsec)
        corrupt("Snapshot source changed while copying");
    if(fchmod(to.get(), 0400) != 0 || fsync(to.get()) != 0)
        fail_errno("Finalize snapshot file");
    sync_directory(to_dir.get());
    if(std::chrono::steady_clock::now() >= deadline)
        throw ArtifactError(ArtifactErrorCode::Busy, "Snapshot deadline exceeded");
    return {destination_path, copied};
}
ArtifactPruneResult ArtifactStore::prune() {
    return state_->prune();
}
ArtifactInfo ArtifactStore::import_directory(const std::filesystem::path& source, Json provenance,
                                             std::vector<std::string> required_outputs, bool pinned) {
    if(source.empty()) {
        throw ArtifactError(ArtifactErrorCode::InvalidArgument, "Import source must not be empty");
    }
    const auto path = std::filesystem::absolute(source).lexically_normal();
    if(path_contains(path, state_->options.root) || path_contains(state_->options.root, path)) {
        throw ArtifactError(ArtifactErrorCode::InvalidArgument,
                            "Import source must not contain or reside within the artifact store");
    }
    const auto from = absolute_directory(path, false);
    for(auto& required : required_outputs) {
        check_relative(required, false);
        required = "raw/imported/" + required;
    }
    auto writer = begin(std::move(provenance), std::move(required_outputs), pinned);
    try {
        const auto dir = directory_at(state_->staging.get(), writer.id());
        const auto raw = directory_at(dir.get(), "raw");
        const auto to = directory_at(raw.get(), "imported", true);
        // The imported/ wrapper itself is an inventoried directory entry.
        std::size_t entries = 1;
        std::size_t files = 0;
        std::uint64_t copied = 0;
        copy_tree(from.get(), to.get(), "", entries, files, copied, state_->options.max_bytes, 2);
        return publish_success(writer);
    } catch(const ArtifactError& error) {
        const auto id = writer.id();
        try {
            publish_failure(writer, std::string(error.what()).substr(0, reason_limit));
        } catch(...) {
            // The writer/recovery path protects an attempt if metadata cannot
            // be published, including actual disk-full conditions.
        }
        throw ArtifactError(error.code(), std::string(error.what()) + "; imported attempt retained as " + id);
    }
}
const std::filesystem::path& ArtifactStore::root() const noexcept {
    return state_->options.root;
}
void to_json(Json& json, const ArtifactSummary& value) {
    json = {{"id", value.id},
            {"status", value.status},
            {"created_at_ms", value.created_at_ms},
            {"completed_at_ms", value.completed_at_ms ? Json(*value.completed_at_ms) : Json(nullptr)},
            {"bytes", value.bytes},
            {"pinned", value.pinned},
            {"in_use", value.in_use},
            {"quarantined", value.quarantined},
            {"reason", value.reason}};
}
void to_json(Json& json, const ArtifactInfo& value) {
    json = value.summary;
    json["provenance"] = value.provenance;
    json["required_outputs"] = value.required_outputs;
    json["file_count"] = value.file_count;
}
void to_json(Json& json, const ArtifactPage& value) {
    json = {{"artifacts", value.artifacts}, {"next_after", value.next_after ? Json(*value.next_after) : Json(nullptr)}};
}
void to_json(Json& json, const ArtifactFile& value) {
    json = {{"path", value.path}, {"bytes", value.bytes}};
}
void to_json(Json& json, const ArtifactFilePage& value) {
    json = {{"files", value.files}, {"next_offset", value.next_offset ? Json(*value.next_offset) : Json(nullptr)}};
}
void to_json(Json& json, const ArtifactUsage& value) {
    json = {{"completed_bytes", value.completed_bytes}, {"staging_bytes", value.staging_bytes},
            {"trash_bytes", value.trash_bytes},         {"metadata_bytes", value.metadata_bytes},
            {"total_bytes", value.total_bytes},         {"pinned_bytes", value.pinned_bytes},
            {"in_use_bytes", value.in_use_bytes},       {"max_bytes", value.max_bytes},
            {"quota_exceeded", value.quota_exceeded}};
}
void to_json(Json& json, const ArtifactPruneResult& value) {
    json = {{"expired_ids", value.expired_ids}, {"errors", value.errors}, {"usage", value.usage}};
}
} // namespace ngm
