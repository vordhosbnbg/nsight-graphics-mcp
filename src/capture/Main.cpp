#include "ngm/CaptureService.hpp"
#include "ngm/File.hpp"
#include "ngm/Version.hpp"

#include <charconv>
#include <iostream>
#include <set>
#include <string_view>

namespace {
std::uint64_t integer(std::string_view text, const char* name) {
    std::uint64_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if(result.ec != std::errc() || result.ptr != text.data() + text.size()) {
        throw std::invalid_argument(std::string(name) + " requires an unsigned integer");
    }
    return value;
}
} // namespace

int main(int argc, char** argv) {
    if(argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "ngm-capture " << ngm::project_version() << '\n';
        return 0;
    }
    if(argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout
            << "Usage: ngm-capture --artifact-root DIR --executable PATH --working-directory DIR\n"
               "  [--nsight-root DIR] [--argument TEXT ...] [--capture-frame N] [--timeout-ms N]\n"
               "  [--application-output-option NAME] [--provenance-json PATH] [--pin]\n"
               "Launch a fresh application with documented Nsight CLI, export evidence, and retain a managed bundle.\n"
               "Arguments are subject to the documented adapter's safe-token limit. --pin protects retained "
               "evidence.\n";
        return 0;
    }
    try {
        ngm::CaptureServiceOptions options;
        options.environment = ngm::capture_environment();
        ngm::CaptureRequest request;
        std::set<std::string> seen;
        for(int index = 1; index < argc; ++index) {
            const std::string key(argv[index]);
            if(key != "--argument" && !seen.insert(key).second) {
                throw std::invalid_argument("Duplicate option " + key);
            }
            if(key == "--pin") {
                request.pin = true;
                continue;
            }
            if(index + 1 == argc) {
                throw std::invalid_argument("Missing value for " + key);
            }
            const std::string value(argv[++index]);
            if(key == "--artifact-root") {
                options.artifacts.root = value;
            } else if(key == "--nsight-root") {
                options.nsight_root = value;
            } else if(key == "--executable") {
                request.executable = value;
            } else if(key == "--working-directory") {
                request.working_directory = value;
            } else if(key == "--argument") {
                request.arguments.push_back(value);
            } else if(key == "--application-output-option") {
                request.application_output_option = value;
            } else if(key == "--capture-frame") {
                request.capture_frame = integer(value, "--capture-frame");
            } else if(key == "--timeout-ms") {
                const auto timeout = integer(value, "--timeout-ms");
                if(timeout < 1 || timeout > 600000) {
                    throw std::invalid_argument("--timeout-ms must be in 1..600000");
                }
                request.timeout = std::chrono::milliseconds(timeout);
            } else if(key == "--provenance-json") {
                const auto provenance_text = ngm::read_regular_file(value, 65536);
                if(provenance_text.find('\0') != std::string::npos) {
                    throw std::invalid_argument("Provenance JSON must not contain raw NUL bytes");
                }
                request.application_provenance = nlohmann::json::parse(
                    provenance_text, [](int depth, nlohmann::json::parse_event_t event, nlohmann::json&) {
                        if(depth >= 64 && (event == nlohmann::json::parse_event_t::object_start ||
                                           event == nlohmann::json::parse_event_t::array_start)) {
                            throw std::invalid_argument("Provenance JSON exceeds 64 nested containers");
                        }
                        return true;
                    });
            } else {
                throw std::invalid_argument("Unknown option " + key);
            }
        }
        for(const auto* key : {"--artifact-root", "--executable", "--working-directory"}) {
            if(!seen.contains(key)) {
                throw std::invalid_argument(std::string("Missing required option ") + key);
            }
        }
        ngm::CaptureService service(std::move(options));
        const auto timeout = request.timeout;
        const auto submitted = service.capture(std::move(request));
        const auto completed = service.wait(submitted.identity.job_id, timeout + std::chrono::seconds(5));
        nlohmann::json report{{"submission", submitted},
                              {"job", completed ? nlohmann::json(*completed) : nlohmann::json(nullptr)},
                              {"artifact", service.artifacts().inspect(submitted.artifact_id)}};
        std::cout << report.dump() << '\n';
        return completed && completed->state == ngm::JobState::succeeded && completed->cleanup_confirmed ? 0 : 1;
    } catch(const std::exception& error) {
        std::cerr << "ngm-capture: " << error.what() << '\n';
        return 2;
    }
}
