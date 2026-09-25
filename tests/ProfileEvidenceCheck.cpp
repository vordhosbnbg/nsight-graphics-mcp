#include "Check.hpp"
#include "ngm/File.hpp"
#include "ngm/NsightEvidence.hpp"
#include "ngm/ProfileEvidence.hpp"

#include <filesystem>

namespace {
using ngm::check::require;
using Kind = ngm::ProfileTableKind;
using Code = ngm::NsightEvidenceErrorCode;
using Limits = ngm::ProfileEvidenceLimits;
template <typename Function>
void rejected(Code code, Function function) {
    try {
        function();
    } catch(const ngm::NsightEvidenceError& error) {
        require(error.code() == code, "rejection has expected error code");
        return;
    }
    require(false, "invalid profile export was accepted");
}
std::string read(const std::filesystem::path& root, const std::string& name) {
    return ngm::read_regular_file(root / name, Limits::bytes);
}
} // namespace

int main(int argc, char** argv) {
    return ngm::check::run([&] {
        require(argc == 3, "two observed profile fixture directories");
        for(int release = 1; release < argc; ++release) {
            const std::filesystem::path root = argv[release];
            for(const auto local : {1, 64}) {
                const auto prefix = std::to_string(local) + '-';
                const auto reproduction = ngm::parse_profile_reproduction(read(root, prefix + "REPRO_INFO.xls.tsv"));
                require(reproduction.product_version.find(release == 1 ? "2026.3.1.0" : "2026.2.0.0") == 0,
                        "producer identity retained");
                require(reproduction.settings.at("GPU Clocks") == "Unaltered" &&
                            reproduction.settings.at("Multi-Pass Metrics") == "Disabled",
                        "actual exported profiling settings retained");
                require(!reproduction.settings.contains("Command Line"), "process context is excluded");
                const auto frame = ngm::parse_profile_table(read(root, prefix + "FRAME.xls.tsv"), Kind::FrameDuration);
                require(frame.rows.size() == 1 && frame.rows[0].values.size() == 1, "observed single positional value");
                const auto metrics =
                    ngm::parse_profile_table(read(root, prefix + "GPUTRACE_FRAME.xls.tsv"), Kind::FrameMetrics);
                require(metrics.rows.size() == 4 && metrics.columns.empty(), "headerless frame metric subset");
                bool ratio = false;
                for(const auto& row : metrics.rows) {
                    if(row.label == "smsp__thread_inst_executed_per_inst_executed.ratio") {
                        ratio = true;
                        require(row.values.size() == 1 && row.values[0].value == (local == 1 ? 1 : 32),
                                "real lane metric retained without conversion");
                    }
                }
                require(ratio, "expected observed metric exists");
                const auto events =
                    ngm::parse_profile_table(read(root, prefix + "D3DPERF_EVENTS.xls.tsv"), Kind::EventDurations);
                require(events.columns == std::vector<std::string>{"time_ms"} && events.rows.size() == 3,
                        "three real event rows retain explicit time header");
                require(events.rows[0].label == events.rows[1].label, "duplicate event names retain separate rows");
                const auto regimes =
                    ngm::parse_profile_table(read(root, prefix + "GPUTRACE_REGIMES.xls.tsv"), Kind::RegimeMetrics);
                require(regimes.rows.size() == 3 && regimes.columns.size() == 2, "real selected regime metrics");
            }
        }
        const auto repeated =
            ngm::parse_profile_table(read(argv[1], "graphics-GPUTRACE_REGIMES.xls.tsv"), Kind::RegimeMetrics);
        require(repeated.columns.size() == 6 && repeated.columns[0] == repeated.columns[2] &&
                    repeated.rows[0].values[0].value != repeated.rows[0].values[1].value,
                "duplicate headers and unequal positional values are preserved");
        const auto graphics =
            ngm::parse_profile_table(read(argv[1], "graphics-GPUTRACE_FRAME.xls.tsv"), Kind::FrameMetrics);
        require(graphics.rows.size() == 2 && graphics.rows[0].values.size() == 3, "three raw frame positions");
        const auto exact = ngm::parse_profile_table("metric\t1.2500e-03\r\n", Kind::FrameMetrics);
        require(exact.rows[0].values[0].text == "1.2500e-03" && exact.rows[0].values[0].value == .00125,
                "text precision and CRLF preserved/accepted");
        const auto unicode = ngm::parse_profile_table("event_text\ttime_ms\n\xCE\xBB\t2", Kind::EventDurations);
        require(unicode.rows[0].label == "\xCE\xBB", "UTF-8 event labels are accepted");
        for(const auto invalid : {"", "nan", "inf", "-inf", "1e999", "1e-999", "1junk", " 1", "1 "}) {
            rejected(Code::InvalidField,
                     [&] { ngm::parse_profile_table(std::string("metric\t") + invalid, Kind::FrameMetrics); });
        }
        for(const auto invalid : {"", "metric", "metric\t1\n\n", "metric\t1\nother\t2\t3", "\t1", "metric\t1\rX"}) {
            rejected(Code::InvalidSchema, [&] { ngm::parse_profile_table(invalid, Kind::FrameMetrics); });
        }
        rejected(Code::InvalidField, [&] { ngm::parse_profile_table(std::string("m\0x\t1", 5), Kind::FrameMetrics); });
        rejected(Code::InvalidField, [&] { ngm::parse_profile_table("\xFF\t1", Kind::FrameMetrics); });
        rejected(Code::DuplicateIdentifier, [&] { ngm::parse_profile_table("m\t1\nm\t2", Kind::FrameMetrics); });
        rejected(Code::InvalidSchema, [&] { ngm::parse_profile_table("wrong\ttime_ms\nx\t1", Kind::EventDurations); });
        rejected(Code::InvalidField,
                 [&] { ngm::parse_profile_table("event_text\tmilliseconds\nx\t1", Kind::EventDurations); });
        rejected(Code::InvalidSchema, [&] { ngm::parse_profile_table("unknown\t1", Kind::FrameDuration); });
        rejected(Code::InvalidSchema,
                 [&] { ngm::parse_profile_table("flattened_event_name\tm", Kind::RegimeMetrics); });
        rejected(Code::LimitExceeded,
                 [&] { ngm::parse_profile_table(std::string(Limits::bytes + 1, 'x'), Kind::FrameMetrics); });
        rejected(Code::LimitExceeded, [&] {
            ngm::parse_profile_table(std::string(Limits::field_bytes + 1, 'x') + "\t1", Kind::FrameMetrics);
        });
        std::string rows;
        for(std::size_t i = 0; i <= Limits::rows; ++i)
            rows += "m\t1\n";
        rejected(Code::LimitExceeded, [&] { ngm::parse_profile_table(rows, Kind::FrameMetrics); });
        std::string columns = "m";
        for(std::size_t i = 0; i < Limits::columns; ++i)
            columns += "\t1";
        rejected(Code::LimitExceeded, [&] { ngm::parse_profile_table(columns, Kind::FrameMetrics); });
        std::string many_cells, row = "m";
        for(int i = 0; i < 1023; ++i)
            row += "\t1";
        for(int i = 0; i < 257; ++i)
            many_cells += row + '\n';
        rejected(Code::LimitExceeded, [&] { ngm::parse_profile_table(many_cells, Kind::RegimeMetrics); });
        const auto repro = read(argv[1], "1-REPRO_INFO.xls.tsv");
        const auto filtered = ngm::parse_profile_reproduction(repro + "Command Line\tprivate-argument\n");
        require(!filtered.settings.contains("Command Line"), "supplied private process context is excluded");
        auto missing_setting = repro;
        const auto clock_start = missing_setting.find("GPU Clocks\t");
        missing_setting.erase(clock_start, missing_setting.find('\n', clock_start) - clock_start + 1);
        rejected(Code::MissingField, [&] { ngm::parse_profile_reproduction(missing_setting); });
        auto entries = repro;
        std::size_t entry_count = 0;
        for(const char c : entries)
            if(c == '\n')
                ++entry_count;
        for(; entry_count < 128; ++entry_count)
            entries += "ignored-" + std::to_string(entry_count) + "\tx\n";
        require(ngm::parse_profile_reproduction(entries).gpu == filtered.gpu, "128 reproduction entries accepted");
        rejected(Code::LimitExceeded, [&] { ngm::parse_profile_reproduction(entries + "overflow\tx\n"); });
        rejected(Code::DuplicateKey, [&] { ngm::parse_profile_reproduction(repro + "API\tVulkan\n"); });
        rejected(Code::MissingField, [&] { ngm::parse_profile_reproduction("API\tVulkan"); });
        auto unknown = repro;
        unknown.replace(unknown.find("2026.3.1.0"), 10, "2026.9.0.0");
        rejected(Code::UnsupportedVersion, [&] { ngm::parse_profile_reproduction(unknown); });
        auto wrong_api = repro;
        wrong_api.replace(wrong_api.find("API\tVulkan"), 10, "API\tOpenGL");
        rejected(Code::InvalidField, [&] { ngm::parse_profile_reproduction(wrong_api); });
    });
}
