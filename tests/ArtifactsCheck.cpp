#include "Check.hpp"
#include "ngm/Artifacts.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <latch>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>

namespace {
std::atomic<bool> fail_write{false};
std::atomic<bool> fail_bundle_rename{false};
std::atomic<int> fail_sync_after{-1};
std::atomic<bool> fail_unlink{false};
std::latch* deletion_entered = nullptr;
std::latch* deletion_continue = nullptr;
} // namespace
extern "C" ssize_t __real_write(int, const void*, size_t);
extern "C" int __real_fsync(int);
extern "C" int __real_renameat(int, const char*, int, const char*);
extern "C" int __real_unlinkat(int, const char*, int);
extern "C" ssize_t __wrap_write(int fd, const void* buffer, size_t bytes) {
    if(fail_write.exchange(false)) {
        errno = ENOSPC;
        return -1;
    }
    return __real_write(fd, buffer, bytes);
}
extern "C" int __wrap_fsync(int fd) {
    const auto previous = fail_sync_after.load();
    if(previous >= 0 && fail_sync_after.fetch_sub(1) == 0) {
        errno = EIO;
        return -1;
    }
    return __real_fsync(fd);
}
extern "C" int __wrap_renameat(int from, const char* source, int to, const char* destination) {
    if(std::string_view(source).starts_with("bundle-") && fail_bundle_rename.exchange(false)) {
        errno = EIO;
        return -1;
    }
    return __real_renameat(from, source, to, destination);
}
extern "C" int __wrap_unlinkat(int parent, const char* name, int flags) {
    if(std::string_view(name) == "evidence.txt") {
        if(deletion_entered) {
            deletion_entered->count_down();
            deletion_continue->wait();
            deletion_entered = nullptr;
        }
        if(fail_unlink.exchange(false)) {
            errno = EACCES;
            return -1;
        }
    }
    return __real_unlinkat(parent, name, flags);
}

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using ngm::ArtifactErrorCode;
using ngm::check::require;
using namespace std::chrono_literals;

struct FakeClock {
    std::chrono::system_clock::time_point current{1000000ms};
    std::function<void()> on_next;
    auto now() {
        if(on_next) {
            auto callback = std::exchange(on_next, {});
            callback();
        }
        return current;
    }
};
struct Scratch {
    fs::path path;
    ~Scratch() {
        std::error_code error;
        fs::permissions(path, fs::perms::owner_all, fs::perm_options::add, error);
        fs::remove_all(path, error);
    }
};
ngm::ArtifactOptions options(const fs::path& root, FakeClock& clock, std::chrono::seconds age = 0s,
                             std::uint64_t bytes = 0) {
    return {.root = root, .max_age = age, .max_bytes = bytes, .clock = [&clock] { return clock.now(); }};
}
void write(const fs::path& path, const std::string& data) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << data;
    output.close();
    require(output.good(), "write test evidence");
}
std::string read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}
template <typename Function>
void expect(ArtifactErrorCode code, Function&& function) {
    try {
        function();
    } catch(const ngm::ArtifactError& error) {
        require(error.code() == code, std::string("Expected ") + std::string(ngm::artifact_error_name(code)) +
                                          ", received " + std::string(ngm::artifact_error_name(error.code())) + ": " +
                                          error.what());
        require(std::string(error.what()).size() > 10, "errors carry actionable context");
        return;
    }
    throw std::runtime_error("Expected artifact operation failure");
}
std::string publish(ngm::ArtifactStore& store, FakeClock& clock, std::size_t bytes = 100, bool pinned = false) {
    auto writer = store.begin({{"evidence_origin", "test_only"}}, {"raw/evidence.txt"}, pinned);
    write(writer.raw_directory() / "evidence.txt", std::string(bytes, 'x'));
    clock.current += 1s;
    const auto result = store.publish_success(writer);
    require(result.summary.status == "complete" && result.summary.in_use, "publisher retains a gap-free usage lease");
    return result.summary.id;
}

void basic(const fs::path& base) {
    FakeClock clock;
    const auto root = base / "basic";
    std::string id;
    std::int64_t created = 0;
    std::int64_t completed = 0;
    {
        ngm::ArtifactStore store(options(root, clock));
        expect(ArtifactErrorCode::Busy, [&] { ngm::ArtifactStore duplicate(options(root, clock)); });
        auto writer = store.begin({{"evidence_origin", "application_readback"}}, {"raw/output.txt"});
        id = writer.id();
        const auto staging = writer.directory();
        require(store.inspect(id).summary.status == "staging", "staging is explicitly incomplete");
        require(!fs::exists(root / "bundles" / id), "partial bundle is absent from published namespace");
        expect(ArtifactErrorCode::Busy, [&] { store.lease(id); });
        expect(ArtifactErrorCode::Busy, [&] { store.read(id, "raw/output.txt"); });
        expect(ArtifactErrorCode::InvalidArgument, [&] { store.publish_success(writer); });
        write(writer.raw_directory() / "output.txt", "actual evidence");
        write(writer.derived_directory() / "index.json", "{}");
        store.update_provenance(writer, {{"evidence_origin", "application_readback"}, {"build", "test-build"}});
        clock.current += 5s;
        const auto info = store.publish_success(writer);
        created = info.summary.created_at_ms;
        completed = *info.summary.completed_at_ms;
        require(info.file_count == 2 && completed - created == 5000, "immutable timestamps and validated file count");
        require(!fs::exists(staging) && writer.directory() == root / "bundles" / id,
                "atomic same-filesystem publication");
        require(store.read(id, "raw/output.txt") == "actual evidence", "bounded evidence read");
        require(store.read(id, "derived/index.json") == "{}", "derived index is separate");
        expect(ArtifactErrorCode::InvalidArgument, [&] { store.read(id, "raw/output.txt", 3); });
        expect(ArtifactErrorCode::InvalidArgument, [&] { store.read(id, "raw/output.txt", 16U * 1024U * 1024U + 1); });
        expect(ArtifactErrorCode::NotFound, [&] { store.read(id, "raw/unknown.txt"); });
        expect(ArtifactErrorCode::InvalidArgument, [&] { store.update_provenance(writer, Json::object()); });
        expect(ArtifactErrorCode::InvalidArgument, [&] { store.publish_failure(writer, "already published"); });
        const auto first = store.files(id, 0, 1);
        require(first.files.size() == 1 && first.next_offset == 1, "file list is paginated");
        require(store.files(id, 1, 1).files.size() == 1 && !store.files(id, 1, 1).next_offset,
                "file pagination terminates");
        require(store.files(id, 9999).files.empty(), "file pagination handles large offsets");
        expect(ArtifactErrorCode::InvalidArgument, [&] { store.files(id, 0, 101); });
        expect(ArtifactErrorCode::InvalidArgument, [&] { store.list({}, 0); });
        store.pin(id, true);
        writer = {};
        require(!store.inspect(id).summary.in_use, "publisher lease releases when writer closes");
        fs::last_write_time(root / "bundles" / id, fs::file_time_type::min());
    }
    {
        ngm::ArtifactStore store(options(root, clock));
        const auto info = store.inspect(id);
        require(info.summary.pinned && info.summary.created_at_ms == created &&
                    info.summary.completed_at_ms == completed,
                "restart preserves pin and timestamps independently of directory mtime");
        require(info.provenance.at("build") == "test-build", "restart retains provenance");
        store.pin(id, false);
    }
    {
        ngm::ArtifactStore store(options(root, clock));
        require(!store.inspect(id).summary.pinned, "unpin persists across restart");
        const auto second = publish(store, clock);
        require(second != id, "bundle IDs are unique");
        const auto first = store.list({}, 1);
        require(first.artifacts.size() == 1 && first.next_after, "bounded artifact page has cursor");
        const auto last = store.list(*first.next_after, 1);
        require(last.artifacts.size() == 1 && !last.next_after, "cursor visits each artifact once");
        Json serialized = store.inspect(id);
        require(serialized.at("status") == "complete" && serialized.at("file_count") == 2,
                "MCP-compatible bounded metadata");
    }
}

void explicit_read_bound(const fs::path& base) {
    FakeClock clock;
    ngm::ArtifactStore store(options(base / "read-bound", clock));
    const auto id = publish(store, clock, 1024U * 1024U + 1);
    expect(ArtifactErrorCode::InvalidArgument, [&] { store.read(id, "raw/evidence.txt"); });
    require(store.read(id, "raw/evidence.txt", 16U * 1024U * 1024U).size() == 1024U * 1024U + 1,
            "larger analysis reads require an explicit bound; the default remains 1 MiB");
    expect(ArtifactErrorCode::InvalidArgument, [&] { store.read(id, "raw/evidence.txt", 16U * 1024U * 1024U + 1); });
}

void protection(const fs::path& base) {
    FakeClock clock;
    const auto root = base / "protection";
    auto store = std::make_unique<ngm::ArtifactStore>(options(root, clock, 10s));
    const auto pinned_id = publish(*store, clock, 50, true);
    const auto leased_id = publish(*store, clock);
    const auto late_pin_id = publish(*store, clock);
    const auto late_lease_id = publish(*store, clock);
    const auto expired_id = publish(*store, clock);
    auto lease = store->lease(leased_id);
    std::optional<ngm::ArtifactLease> late_lease;
    clock.current += 20s;
    std::latch choosing(1);
    std::latch protected_now(1);
    clock.on_next = [&] {
        choosing.count_down();
        protected_now.wait();
    };
    ngm::ArtifactPruneResult result;
    std::exception_ptr failure;
    std::jthread prune([&] {
        try {
            result = store->prune();
        } catch(...) {
            failure = std::current_exception();
        }
    });
    choosing.wait();
    store->pin(late_pin_id, true);
    late_lease = store->lease(late_lease_id);
    protected_now.count_down();
    prune.join();
    if(failure) {
        std::rethrow_exception(failure);
    }
    require(result.expired_ids == std::vector<std::string>{expired_id},
            "pin/lease acquired during selection prevents deletion claim");
    require(store->inspect(expired_id).summary.status == "expired", "expired reference retains explanation");
    expect(ArtifactErrorCode::Expired, [&] { store->read(expired_id, "raw/evidence.txt"); });
    expect(ArtifactErrorCode::Expired, [&] { store->pin(expired_id, true); });
    require(store->read(pinned_id, "raw/evidence.txt").size() == 50, "pinned evidence survives age pruning");
    require(store->usage().in_use_bytes > 0 && store->usage().pinned_bytes > 0, "protection bytes are reported");
    store->pin(pinned_id, false);
    store->pin(late_pin_id, false);
    lease = {};
    late_lease.reset();
    require(store->prune().expired_ids.size() == 4, "released evidence becomes eligible as whole bundles");
    const auto metadata = store->usage().metadata_bytes;
    require(metadata > 0 && store->usage().completed_bytes == 0, "small expiration records outlive removed evidence");
    store.reset();
    ngm::ArtifactStore restarted(options(root, clock));
    require(restarted.inspect(expired_id).summary.status == "expired", "expired status survives restart");

    const auto held_root = base / "lease-lifetime";
    auto owner = std::make_unique<ngm::ArtifactStore>(options(held_root, clock));
    const auto held_id = publish(*owner, clock);
    auto held = owner->lease(held_id);
    owner.reset();
    expect(ArtifactErrorCode::Busy, [&] { ngm::ArtifactStore other(options(held_root, clock)); });
    require(fs::exists(held.directory() / "raw/evidence.txt"), "lease keeps store ownership alive");
    held = {};
    ngm::ArtifactStore other(options(held_root, clock));
}

void writer_protection_and_quarantine(const fs::path& base) {
    FakeClock clock;
    const auto root = base / "writer";
    std::string abandoned;
    {
        ngm::ArtifactStore store(options(root, clock, 1s));
        auto writer = store.begin(Json::object(), {"raw/evidence.txt"});
        write(writer.raw_directory() / "evidence.txt", "evidence");
        const auto id = store.publish_success(writer).summary.id;
        clock.current += 5s;
        require(store.prune().expired_ids.empty(), "publication preserves writer protection");
        writer = {};
        require(store.prune().expired_ids == std::vector<std::string>{id}, "coordinator release permits expiration");
        auto attempt = store.begin(Json::object());
        abandoned = attempt.id();
        write(attempt.raw_directory() / "partial.log", "partial");
        store.quarantine(attempt, "cleanup unconfirmed");
        require(store.inspect(abandoned).summary.quarantined && store.inspect(abandoned).summary.pinned,
                "quarantine is explicit and pinned");
        expect(ArtifactErrorCode::Busy, [&] { store.pin(abandoned, false); });
        expect(ArtifactErrorCode::InvalidArgument, [&] { store.publish_success(attempt); });
    }
    {
        ngm::ArtifactStore store(options(root, clock, 1s));
        const auto info = store.inspect(abandoned);
        require(info.summary.status == "failed" && info.summary.quarantined && info.summary.pinned,
                "quarantine survives restart as failed protected staging");
        require(info.summary.reason == "cleanup unconfirmed", "quarantine preserves the worker's recorded reason");
        require(store.usage().staging_bytes > 0 && store.prune().expired_ids.empty(),
                "quarantine is charged and never auto-pruned");
        expect(ArtifactErrorCode::Busy, [&] { store.lease(abandoned); });
        auto recovered = store.resume_quarantined(abandoned);
        require(read(recovered.raw_directory() / "partial.log") == "partial", "recovery retains partial logs");
        store.publish_failure(recovered, "cleanup subsequently confirmed");
        require(!store.inspect(abandoned).summary.quarantined, "explicit failure publication clears quarantine");
        store.pin(abandoned, false);
        clock.current += 5s;
        require(store.prune().expired_ids.empty(), "failure publication also keeps writer protection");
        recovered = {};
        require(store.prune().expired_ids == std::vector<std::string>{abandoned}, "released failed bundle is prunable");
    }
}

void budgets(const fs::path& base) {
    FakeClock clock;
    const auto root = base / "budgets";
    std::string oldest;
    std::string middle;
    std::string pinned;
    std::uint64_t total = 0;
    std::uint64_t first_bytes = 0;
    {
        ngm::ArtifactStore store(options(root, clock));
        oldest = publish(store, clock, 10000);
        middle = publish(store, clock, 10000);
        pinned = publish(store, clock, 10000, true);
        total = store.usage().total_bytes;
        first_bytes = store.inspect(oldest).summary.bytes;
    }
    {
        ngm::ArtifactStore store(options(root, clock, 0s, total - first_bytes + 1000));
        const auto result = store.prune();
        require(result.expired_ids == std::vector<std::string>{oldest},
                "budget removes oldest eligible complete bundle first");
        require(!result.usage.quota_exceeded && store.inspect(middle).summary.status == "complete",
                "pruning stops when budget is met");
    }
    {
        ngm::ArtifactStore store(options(root, clock, 0s, 1000));
        const auto result = store.prune();
        require(result.expired_ids == std::vector<std::string>{middle},
                "unpinned evidence is eligible before pinned bytes");
        require(result.usage.quota_exceeded && result.usage.pinned_bytes > 10000,
                "protected evidence can exceed configured quota");
        expect(ArtifactErrorCode::QuotaExceeded, [&] { store.begin(Json::object()); });
        require(store.read(pinned, "raw/evidence.txt").size() == 10000, "quota exhaustion preserves pinned data");
    }
    {
        ngm::ArtifactStore store(options(base / "active-budget", clock, 0s, 5000));
        auto writer = store.begin(Json::object());
        write(writer.raw_directory() / "oversize", std::string(6000, 'q'));
        const auto usage = store.usage();
        require(usage.staging_bytes > 6000 && usage.in_use_bytes > 6000 && usage.quota_exceeded,
                "active bytes are honestly accounted");
        expect(ArtifactErrorCode::QuotaExceeded, [&] { store.publish_success(writer); });
        const auto result = store.publish_failure(writer, "quota exhausted");
        require(result.summary.status == "failed" && result.summary.in_use,
                "oversize failed attempt retains available evidence");
        require(store.prune().usage.quota_exceeded, "protected failed data is never deleted to satisfy quota");
        writer = {};
        require(store.prune().expired_ids.size() == 1, "released oversize failure can be pruned");
    }
}

void publication_growth(const fs::path& base) {
    for(const bool pinned : {false, true}) {
        FakeClock clock;
        ngm::ArtifactStore store(options(base / (pinned ? "growth-protected" : "growth"), clock, 0s, 110000));
        const auto old = publish(store, clock, 100000, pinned);
        auto writer = store.begin(Json::object());
        for(int index = 0; index < 100; ++index) {
            write(writer.raw_directory() / ("file-" + std::to_string(index)), std::string(75, 'x'));
        }
        require(!store.usage().quota_exceeded, "reproducer fits before final manifest growth");
        if(pinned) {
            expect(ArtifactErrorCode::QuotaExceeded, [&] { store.publish_success(writer); });
            require(store.inspect(old).summary.pinned, "metadata reservation never removes protected data");
            store.pin(old, false);
        }
        const auto info = store.publish_success(writer);
        require(info.summary.status == "complete" && info.file_count == 100,
                "publication prunes enough space for final inventory metadata");
        require(store.inspect(old).summary.status == "expired" && !store.usage().quota_exceeded,
                "oldest eligible bundle covers manifest growth without a retry loop");
    }
}

void depth_limits(const fs::path& base) {
    FakeClock clock;
    const auto root = base / "direct-depth";
    std::string rejected_id;
    {
        ngm::ArtifactStore store(options(root, clock));
        auto writer = store.begin(Json::object());
        rejected_id = writer.id();
        auto deepest = writer.raw_directory();
        for(int level = 0; level < 65; ++level) {
            deepest /= "d";
        }
        write(deepest / "evidence", "deep evidence");
        write(writer.raw_directory() / "producer.log", "retained producer log");
        expect(ArtifactErrorCode::InvalidArgument, [&] { store.publish_success(writer); });
        require(store.inspect(rejected_id).summary.status == "staging" && !fs::exists(root / "bundles" / rejected_id),
                "over-depth output is rejected before publication");
        require(store.usage().staging_bytes > 0 && store.list().artifacts.size() == 1,
                "over-depth staging remains inspectable and accounted");
        publish(store, clock);
        require(store.list().artifacts.size() == 2, "invalid depth does not block subsequent admission");
        store.quarantine(writer, "external writer exceeded directory depth");
    }
    {
        ngm::ArtifactStore store(options(root, clock));
        require(store.inspect(rejected_id).summary.quarantined && store.usage().staging_bytes > 0,
                "over-depth writer remains recoverable after restart");
        auto writer = store.resume_quarantined(rejected_id);
        // The owning backend has confirmed cleanup and explicitly removes its
        // unsupported output, retaining the independent producer log.
        fs::remove_all(writer.raw_directory() / "d");
        store.publish_failure(writer, "unsupported nested output removed after confirmed cleanup");
        require(store.read(rejected_id, "raw/producer.log") == "retained producer log",
                "repaired quarantine can publish retained failed evidence");
    }
    {
        ngm::ArtifactStore store(options(base / "boundary-depth", clock));
        auto writer = store.begin(Json::object());
        auto deepest = writer.raw_directory();
        for(int level = 0; level < 63; ++level) {
            deepest /= "d";
        }
        write(deepest / "evidence", "boundary");
        const auto info = store.publish_success(writer);
        require(info.file_count == 1 && store.usage().completed_bytes > 0,
                "64 directory levels including raw are publishable and accountable");
    }
    for(const int source_depth : {62, 65}) {
        const auto source = base / ("depth-source-" + std::to_string(source_depth));
        auto deepest = source;
        for(int level = 0; level < source_depth; ++level) {
            deepest /= "d";
        }
        write(deepest / "evidence", "source evidence");
        write(source / "00-log.txt", "source log");
        const auto imported_root = base / ("depth-import-" + std::to_string(source_depth));
        std::string id;
        {
            ngm::ArtifactStore store(options(imported_root, clock));
            if(source_depth == 62) {
                const auto info = store.import_directory(source, Json::object(), {}, false);
                id = info.summary.id;
                require(info.summary.status == "complete" && info.file_count == 2,
                        "import includes raw/imported in the shared directory depth");
            } else {
                expect(ArtifactErrorCode::InvalidArgument,
                       [&] { store.import_directory(source, Json::object(), {}, false); });
                const auto info = store.list().artifacts.at(0);
                id = info.id;
                require(info.status == "failed" && !info.quarantined && !info.pinned,
                        "depth overflow retains a valid unpinned failed import");
                require(store.read(id, "raw/imported/00-log.txt") == "source log",
                        "depth overflow retains already copied evidence");
            }
            require(store.usage().staging_bytes == 0, "import depth failures do not leave poisoned staging");
        }
        ngm::ArtifactStore restarted(options(imported_root, clock, 1s));
        require(restarted.inspect(id).file_count > 0, "bounded import remains valid after restart");
        clock.current += 5s;
        require(restarted.prune().expired_ids == std::vector<std::string>{id},
                "unprotected bounded import remains normally prunable");
        require(read(deepest / "evidence") == "source evidence", "depth rejection never changes source evidence");
    }
}

void import_file_limit(const fs::path& base) {
    FakeClock clock;
    const auto source = base / "many-files-source";
    for(int index = 0; index < 4097; ++index) {
        write(source / ("file-" + std::to_string(index)), "x");
    }
    const auto root = base / "many-files-import";
    std::string id;
    {
        ngm::ArtifactStore store(options(root, clock));
        expect(ArtifactErrorCode::InvalidArgument, [&] { store.import_directory(source, Json::object(), {}, false); });
        const auto summary = store.list().artifacts.at(0);
        id = summary.id;
        require(summary.status == "failed" && !summary.pinned && !summary.quarantined,
                "4097th imported file is rejected before copying an unpublishable bundle");
        require(store.inspect(id).file_count == 4096 && store.usage().staging_bytes == 0,
                "partial import satisfies the publication file bound");
        const auto first = store.files(id, 0, 1).files.at(0);
        require(store.read(id, first.path) == "x", "failed bounded import retains copied evidence");
        store.pin(id, false);
        publish(store, clock);
        require(store.list().artifacts.size() == 2, "file overflow does not block later admission");
    }
    ngm::ArtifactStore restarted(options(root, clock, 1s));
    require(restarted.inspect(id).file_count == 4096 && !restarted.inspect(id).summary.quarantined,
            "failed bounded import is valid after restart");
    clock.current += 5s;
    const auto result = restarted.prune();
    require(std::find(result.expired_ids.begin(), result.expired_ids.end(), id) != result.expired_ids.end(),
            "failed import can expire normally without quarantine repair");
    require(std::distance(fs::directory_iterator(source), fs::directory_iterator{}) == 4097,
            "file overflow leaves the entire source untouched");
}

void confinement_and_import(const fs::path& base) {
    FakeClock clock;
    const auto source = base / "external";
    write(source / "nested/log.txt", "outside remains");
    write(source / "report.json", "{\"evidence_origin\":\"application_readback\"}");
    ngm::ArtifactStore store(options(base / "confinement", clock));
    for(const auto& path : {"../report.json", "/report.json", "raw/../report.json", "raw//x", "manifest.json"}) {
        expect(ArtifactErrorCode::InvalidArgument, [&] { store.begin(Json::object(), {path}); });
    }
    expect(ArtifactErrorCode::InvalidArgument, [&] { store.inspect("../external"); });
    expect(ArtifactErrorCode::InvalidArgument, [&] { store.begin(Json::array()); });
    expect(ArtifactErrorCode::InvalidArgument, [&] { store.begin({{"large", std::string(65537, 'x')}}); });
    expect(ArtifactErrorCode::InvalidArgument, [&] { store.begin(Json::object(), {"raw/x", "raw/x"}); });
    const auto imported =
        store.import_directory(source, {{"evidence_origin", "application_readback"}}, {"report.json"});
    require(imported.summary.pinned && imported.summary.status == "complete",
            "baseline import is explicitly pinned by default");
    require(store.read(imported.summary.id, "raw/imported/nested/log.txt") == "outside remains",
            "source copied into distinct raw partition");
    require(read(source / "nested/log.txt") == "outside remains", "import does not alter source evidence");
    expect(ArtifactErrorCode::InvalidArgument, [&] { store.import_directory(store.root(), Json::object()); });
    expect(ArtifactErrorCode::InvalidArgument, [&] { store.import_directory(base, Json::object()); });
    auto writer = store.begin(Json::object());
    fs::create_symlink(source / "nested/log.txt", writer.raw_directory() / "link");
    expect(ArtifactErrorCode::Corrupt, [&] { store.publish_success(writer); });
    fs::remove(writer.raw_directory() / "link");
    fs::create_directory_symlink(source, writer.raw_directory() / "directory-link");
    expect(ArtifactErrorCode::Corrupt, [&] { store.publish_success(writer); });
    fs::remove(writer.raw_directory() / "directory-link");
    require(mkfifo((writer.raw_directory() / "fifo").c_str(), 0600) == 0, "create FIFO probe");
    expect(ArtifactErrorCode::Corrupt, [&] { store.publish_success(writer); });
    fs::remove(writer.raw_directory() / "fifo");
    fs::create_hard_link(source / "report.json", writer.raw_directory() / "hard-link");
    expect(ArtifactErrorCode::Corrupt, [&] { store.publish_success(writer); });
    fs::remove(writer.raw_directory() / "hard-link");
    store.publish_failure(writer, "invalid outputs rejected");
    require(read(source / "nested/log.txt") == "outside remains", "no rejected link is followed or deleted");
    fs::create_directory_symlink(source, base / "root-link");
    expect(ArtifactErrorCode::Filesystem,
           [&] { ngm::ArtifactStore invalid(options(base / "root-link" / "store", clock)); });
    expect(ArtifactErrorCode::InvalidArgument, [&] { ngm::ArtifactStore invalid(options(source, clock)); });
    require(!fs::exists(source / ".ngm-store.json") && !fs::exists(source / ".lock"),
            "unmarked nonempty directories are never adopted or modified");
    fs::create_symlink(source / "report.json", source / "unsafe-link");
    expect(ArtifactErrorCode::Corrupt, [&] { store.import_directory(source, Json::object()); });
    require(store.list().artifacts.size() == 3, "failed import retains its explicitly failed attempt");
    fs::remove(source / "unsafe-link");
}

void filesystem_failures(const fs::path& base) {
    FakeClock clock;
    const auto root = base / "faults";
    std::string id;
    {
        ngm::ArtifactStore store(options(root, clock, 1s));
        id = publish(store, clock);
        fail_write = true;
        expect(ArtifactErrorCode::DiskFull, [&] { store.pin(id, true); });
        require(store.inspect(id).summary.pinned, "failed pin write conservatively retains protection");
        store.pin(id, false);
        clock.current += 5s;
        fail_write = true;
        const auto result = store.prune();
        require(!result.errors.empty() && result.expired_ids.empty(),
                "disk-full expiration metadata failure leaves evidence intact");
        require(store.read(id, "raw/evidence.txt").size() == 100, "failed pruning preserves source bundle");
        fail_sync_after = 1;
        expect(ArtifactErrorCode::Filesystem, [&] { store.pin(id, true); });
        require(store.inspect(id).summary.pinned, "rename-before-sync failure protects pin in memory");
    }
    {
        ngm::ArtifactStore store(options(root, clock, 1s));
        require(store.inspect(id).summary.pinned && store.prune().expired_ids.empty(),
                "pin rename survives restart despite reported sync failure");
        fail_write = true;
        expect(ArtifactErrorCode::DiskFull, [&] { store.begin(Json::object()); });
        require(store.usage().staging_bytes == 0, "empty failed allocation contains no logical payload bytes");
    }
    {
        ngm::ArtifactStore store(options(root, clock, 1s));
        const auto page = store.list();
        require(page.artifacts.size() == 2, "failed allocation is discovered on restart");
        const auto quarantined = std::find_if(page.artifacts.begin(), page.artifacts.end(),
                                              [](const auto& info) { return info.quarantined; });
        require(quarantined != page.artifacts.end() && quarantined->pinned, "incomplete initialization is protected");
    }
}

void deletion_failures(const fs::path& base) {
    FakeClock clock;
    const auto root = base / "deletion-failure";
    ngm::ArtifactStore store(options(root, clock, 10s));
    const auto first = publish(store, clock);
    const auto second = publish(store, clock);
    clock.current += 20s;
    std::latch entered(1);
    std::latch proceed(1);
    deletion_entered = &entered;
    deletion_continue = &proceed;
    fail_unlink = true;
    ngm::ArtifactPruneResult result;
    std::exception_ptr failure;
    std::jthread prune([&] {
        try {
            result = store.prune();
        } catch(...) {
            failure = std::current_exception();
        }
    });
    entered.wait();
    // This would deadlock if recursive deletion held the protection mutex.
    store.pin(second, true);
    require(store.usage().trash_bytes > 0, "claimed data remains charged during slow deletion");
    proceed.count_down();
    prune.join();
    if(failure) {
        std::rethrow_exception(failure);
    }
    require(result.expired_ids == std::vector<std::string>{first} && result.errors.size() == 1,
            "failed deletion keeps its expiration and error");
    require(result.usage.trash_bytes > 0 && fs::exists(root / "trash" / first),
            "failed deletion remains conservatively charged");
    require(store.inspect(first).summary.status == "expired" && store.inspect(second).summary.pinned,
            "pin succeeds concurrently with deletion outside the protection lock");
    const auto retry = store.prune();
    require(retry.errors.empty() && retry.usage.trash_bytes == 0 && !fs::exists(root / "trash" / first),
            "prune retries previously claimed deletion");
    deletion_continue = nullptr;
}

void child_crash(const fs::path& root, bool prepared) {
    const auto child = fork();
    require(child >= 0, "fork crash simulation");
    if(child == 0) {
        try {
            FakeClock clock;
            ngm::ArtifactStore store(options(root, clock));
            auto writer =
                store.begin({{"crash_probe", prepared ? "prepared" : "unfinished"}}, {"raw/evidence.txt"}, true);
            write(writer.raw_directory() / "evidence.txt", "crash evidence");
            if(prepared) {
                fail_bundle_rename = true;
                try {
                    store.publish_success(writer);
                } catch(const ngm::ArtifactError& error) {
                    if(error.code() == ArtifactErrorCode::Filesystem) {
                        _exit(0);
                    }
                }
                _exit(2);
            }
            _exit(0);
        } catch(...) {
            _exit(3);
        }
    }
    int status = 0;
    require(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0,
            "child created the requested crash window");
}
void recovery(const fs::path& base) {
    FakeClock clock;
    const auto prepared_root = base / "prepared-crash";
    child_crash(prepared_root, true);
    {
        ngm::ArtifactStore store(options(prepared_root, clock));
        const auto info = store.list().artifacts.at(0);
        require(info.status == "complete" && info.pinned,
                "validated prepared publication finishes atomic rename after restart");
        require(store.read(info.id, "raw/evidence.txt") == "crash evidence",
                "recovered prepared publication retains output");
    }
    const auto unfinished_root = base / "unfinished-crash";
    child_crash(unfinished_root, false);
    {
        ngm::ArtifactStore store(options(unfinished_root, clock));
        const auto info = store.list().artifacts.at(0);
        require(info.status == "failed" && info.quarantined && info.pinned, "unfinished process death is quarantined");
        require(!fs::exists(unfinished_root / "bundles" / info.id), "interrupted work never appears as complete");
        auto writer = store.resume_quarantined(info.id);
        store.publish_failure(writer, "crash test process cleanup confirmed");
        require(store.read(info.id, "raw/evidence.txt") == "crash evidence",
                "failed recovery retains available evidence");
    }
    const auto deletion_root = base / "deletion-crash";
    std::string before;
    std::string after;
    {
        ngm::ArtifactStore store(options(deletion_root, clock));
        before = publish(store, clock);
        after = publish(store, clock);
        for(const auto& id : {before, after}) {
            auto summary = store.inspect(id).summary;
            summary.status = "expired";
            summary.reason = "simulated interrupted deletion";
            Json tombstone = summary;
            tombstone["schema_version"] = 1;
            tombstone["expired_at_ms"] = 1009000;
            write(deletion_root / "expired" / (id + ".json"), tombstone.dump());
        }
    }
    fs::rename(deletion_root / "bundles" / after, deletion_root / "trash" / after);
    {
        ngm::ArtifactStore store(options(deletion_root, clock));
        require(store.inspect(before).summary.status == "complete",
                "uncommitted deletion receipt does not erase visible evidence");
        require(!fs::exists(deletion_root / "expired" / (before + ".json")), "stale pre-rename receipt removed");
        require(store.inspect(after).summary.status == "expired" && !fs::exists(deletion_root / "trash" / after),
                "claimed deletion resumes from trash after restart");
    }
}
} // namespace

int main(int argc, char** argv) {
    return ngm::check::run([&] {
        require(argc == 2, "ArtifactsCheck requires a scratch parent directory");
        Scratch scratch{fs::absolute(argv[1]) / ("run-" + std::to_string(getpid()))};
        require(!fs::exists(scratch.path), "test scratch directory must be fresh");
        fs::create_directories(scratch.path);
        basic(scratch.path);
        explicit_read_bound(scratch.path);
        protection(scratch.path);
        writer_protection_and_quarantine(scratch.path);
        budgets(scratch.path);
        publication_growth(scratch.path);
        depth_limits(scratch.path);
        import_file_limit(scratch.path);
        confinement_and_import(scratch.path);
        filesystem_failures(scratch.path);
        deletion_failures(scratch.path);
        recovery(scratch.path);
    });
}
