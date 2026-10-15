#include "Check.hpp"
#include "PushState.hpp"
#include "PushStateFixture.hpp"
#include "ngm/DrawState.hpp"

namespace {
using ngm::check::require;
using Json = nlohmann::json;
namespace state = ngm::check::state;
namespace pc = ngm::check::push;
using namespace ngm::check::state;
Json inspect(const std::vector<ngm::CppSource>& sources) {
    auto result = ngm::inspect_draw_executions(sources);
    require(!result.at("executions").empty(), "submitted push cases: " + result.at("unsupported").dump());
    return result.at("executions");
}
Json push_state(const std::vector<ngm::CppSource>& sources) {
    return inspect(sources)[0].at("state").at("push_constants");
}
} // namespace
int main() {
    return ngm::check::run([] {
        const auto full = push_state(pc::sources<ngm::CppSource>());
        require(full.at("status") == "unavailable" && full.at("value").is_null() &&
                    full.at("coverage").at("input_coverage_complete") == true,
                "source coverage never fabricates unread bytes");
        const auto& first = full.at("coverage").at("ranges")[0].at("segments")[0];
        require(first.at("status") == "not_read" && first.at("offset") == 0 && first.at("length") == 16 &&
                    first.at("write").at("handle") == 18,
                "full recorded input reference");
        const auto middle = state::replace(draw, "Event #11", "Event #15");
        const auto right = state::replace(draw, "Event #11", "Event #17");
        const auto rows = inspect(pc::sources<ngm::CppSource>(begin + bind + pc::write(4, 37, 0, 16, 18) + draw +
                                                              pc::write(14, 48, 4, 8, 19) + middle +
                                                              pc::write(16, 37, 8, 8, 20) + right + end));
        require(rows.size() == 3, "three selected push snapshots");
        const auto segments = [&](unsigned n) {
            return rows[n].at("state").at("push_constants").at("coverage").at("ranges")[0].at("segments");
        };
        require(segments(1).size() == 3 && segments(1)[0].at("length") == 4 &&
                    segments(1)[1].at("write").at("handle") == 19 && segments(1)[2].at("write").at("handle") == 18 &&
                    segments(1)[2].at("write").at("resource_offset") == 12,
                "middle update preserves earlier prefix and suffix");
        require(segments(2).size() == 3 && segments(2)[1].at("length") == 4 &&
                    segments(2)[1].at("write").at("handle") == 19 && segments(2)[2].at("length") == 8 &&
                    segments(2)[2].at("write").at("handle") == 20,
                "later overlapping update changes only its bytes");
        const auto resolved = [&](Json field) {
            std::map<std::string, Json> reads;
            for(auto& range : field["coverage"]["ranges"])
                for(auto& part : range["segments"])
                    if(part.at("status") == "not_read") {
                        const auto handle = part.at("write").at("handle").get<unsigned>();
                        const auto ref = std::string(64, static_cast<char>('a' + handle - 18));
                        part["resource_ref"] = ref;
                        const auto data =
                            handle == 18
                                ? std::string(part.at("write").at("declared_bytes").get<std::size_t>() * 2, '0')
                            : handle == 19 ? std::string("0100000001000000")
                                           : std::string("0200000002000000");
                        reads[ref] = {{"data", data},
                                      {"sha256", std::string(64, 'f')},
                                      {"artifact_id", "bundle-" + std::string(32, 'a')}};
                    }
            ngm::detail::resolve_push_data(field, reads);
            return field;
        };
        require(resolved(rows[1].at("state").at("push_constants")).at("value")[0].at("data") ==
                        "00000000010000000100000000000000" &&
                    resolved(rows[2].at("state").at("push_constants")).at("value")[0].at("data") ==
                        "00000000010000000200000002000000",
                "independent incremental byte-bank oracle");
        auto partial = push_state(pc::sources<ngm::CppSource>(begin + bind + pc::write(4, 37, 4, 4, 18) + draw + end));
        require(partial.at("coverage").at("input_coverage_complete") == false &&
                    partial.at("coverage").at("ranges")[0].at("segments")[0].at("status") == "unavailable",
                "unwritten stage bytes remain undefined");
        require(resolved(partial).at("status") == "unavailable" && resolved(partial).at("value").is_null(),
                "reading known segments does not initialize missing bytes");
        auto broken = full;
        broken["coverage"]["ranges"][0]["segments"][0]["resource_ref"] = std::string(64, 'a');
        const auto before = broken;
        bool failed = false;
        try {
            ngm::detail::resolve_push_data(broken, {{std::string(64, 'a'),
                                                     {{"data", "00"},
                                                      {"sha256", std::string(64, 'f')},
                                                      {"artifact_id", "bundle-" + std::string(32, 'a')}}}});
        } catch(const std::runtime_error&) {
            failed = true;
        }
        require(failed && broken == before, "truncated reader data refuses without committing a partial value");
        const auto incompatible =
            push_state(pc::sources<ngm::CppSource>(begin + bind + pc::write(4, 49, 0, 16, 18) + draw + end));
        require(incompatible.at("coverage").at("input_coverage_complete") == false &&
                    incompatible.at("coverage").at("ranges")[0].at("segments")[0].at("status") == "unsupported",
                "noncompatible push layout cannot satisfy bound layout");
        const auto rebound = push_state(pc::sources<ngm::CppSource>(
            begin + pc::write(4, 48, 0, 16, 18) +
            state::replace(state::replace(bind, "VkPipeline_uid_38", "VkPipeline_uid_999"), "Event #3", "Event #20") +
            bind + draw + end));
        require(rebound.at("coverage").at("input_coverage_complete") == true,
                "binding another pipeline and binding back does not disturb push values");
        auto invalid = pc::sources<ngm::CppSource>();
        invalid[0].text = state::replace(invalid[0].text, "16u,NV_GET_RESOURCE_STATIC", "12u,NV_GET_RESOURCE_STATIC");
        require(push_state(invalid).at("status") == "unsupported", "push command and source size mismatch refuse");
        invalid = pc::sources<ngm::CppSource>();
        invalid[1].text = state::replace(invalid[1].text, "VK_SHADER_STAGE_FRAGMENT_BIT),0u,16u",
                                         "VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT),0u,16u");
        require(push_state(invalid).at("status") == "unsupported",
                "push mask must cover all overlapping layout stages");
        invalid = pc::sources<ngm::CppSource>();
        invalid[1].text =
            state::replace(invalid[1].text, "0u,nullptr,1u,VkPushConstantRange_p37", "0u,nullptr,0u,nullptr");
        invalid[0].text = ngm::check::cpp::recording(begin + bind + draw + end);
        require(push_state(invalid).at("status") == "not_applicable",
                "empty push layout differs from unread or missing writes");
    });
}
