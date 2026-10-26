#include "ngm/ShaderTrace.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace ngm {
namespace {
using Json = nlohmann::json;
void require(bool condition, const char* reason) {
    if(!condition)
        throw std::invalid_argument(std::string("Invalid application shader trace: ") + reason);
}
void keys(const Json& value, std::initializer_list<const char*> names) {
    require(value.is_object() && value.size() == names.size(), "unexpected object fields");
    for(const auto* name : names)
        require(value.contains(name), "missing object field");
}
std::uint32_t number(const Json& value) {
    require(value.is_number_integer() && (value.is_number_unsigned() || value.get<std::int64_t>() >= 0) &&
                value.get<std::uint64_t>() <= UINT32_MAX,
            "expected uint32");
    return value.get<std::uint32_t>();
}
std::string text(const Json& value, std::size_t maximum) {
    require(value.is_string(), "expected string");
    const auto result = value.get<std::string>();
    require(!result.empty() && result.size() <= maximum && result.find('\0') == std::string::npos,
            "text exceeds bounds");
    return result;
}
void validate(const Json& trace) {
    keys(trace, {"schema_version", "evidence_origin", "original_execution", "capture_event_correspondence", "shader",
                 "extent", "selection", "capacity", "attempted_records", "dropped_records", "probes", "records"});
    require(number(trace.at("schema_version")) == 1 && trace.at("evidence_origin") == shader_trace_origin &&
                trace.at("original_execution") == false && trace.at("capture_event_correspondence") == false,
            "unsupported schema or evidence claims");
    const auto& shader = trace.at("shader");
    keys(shader, {"path", "sha256", "entry_point", "stage"});
    const auto path = text(shader.at("path"), 512);
    require(path.front() != '/' && path.back() != '/' && path.find('\\') == std::string::npos,
            "shader path must be application-relative");
    for(std::size_t start = 0; start < path.size();) {
        const auto end = path.find('/', start);
        const auto part = path.substr(start, (end == std::string::npos ? path.size() : end) - start);
        require(!part.empty() && part != "." && part != "..", "unsafe shader path");
        start = end == std::string::npos ? path.size() : end + 1;
    }
    const auto hash = text(shader.at("sha256"), 64);
    require(hash.size() == 64 && std::all_of(hash.begin(), hash.end(),
                                             [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }),
            "shader sha256 must be lowercase hexadecimal");
    (void)text(shader.at("entry_point"), 128);
    require(shader.at("stage") == "fragment", "only fragment traces are supported");
    keys(trace.at("extent"), {"width", "height"});
    const auto width = number(trace.at("extent").at("width"));
    const auto height = number(trace.at("extent").at("height"));
    require(width > 0 && width <= 65536 && height > 0 && height <= 65536, "extent exceeds bounds");
    keys(trace.at("selection"), {"pixel_x", "pixel_y"});
    const auto x = number(trace.at("selection").at("pixel_x"));
    const auto y = number(trace.at("selection").at("pixel_y"));
    require(x < width && y < height, "selected pixel is outside extent");
    const auto capacity = number(trace.at("capacity"));
    const auto attempted = number(trace.at("attempted_records"));
    const auto dropped = number(trace.at("dropped_records"));
    require(capacity > 0 && capacity <= shader_trace_maximum_records, "capacity exceeds bounds");
    const auto& records = trace.at("records");
    require(records.is_array() && records.size() == std::min(attempted, capacity) &&
                dropped == attempted - records.size(),
            "record counts are inconsistent");
    const auto& probes = trace.at("probes");
    require(probes.is_array() && !probes.empty() && probes.size() <= 64, "probe count exceeds bounds");
    std::map<std::uint32_t, std::uint32_t> components;
    for(const auto& probe : probes) {
        keys(probe, {"id", "name", "type", "components"});
        const auto count = number(probe.at("components"));
        require(count > 0 && count <= 4 && components.emplace(number(probe.at("id")), count).second,
                "invalid or duplicate probe ID/components");
        (void)text(probe.at("name"), 128);
        require(probe.at("type") == "float32" || probe.at("type") == "int32" || probe.at("type") == "uint32",
                "unsupported probe type");
    }
    for(const auto& record : records) {
        keys(record, {"frame", "draw", "pixel_x", "pixel_y", "sample", "primitive", "probe", "bits"});
        for(const auto* field : {"frame", "draw", "sample"})
            (void)number(record.at(field));
        require(number(record.at("pixel_x")) == x && number(record.at("pixel_y")) == y,
                "record pixel differs from selection");
        if(!record.at("primitive").is_null())
            (void)number(record.at("primitive"));
        const auto probe = number(record.at("probe"));
        const auto& bits = record.at("bits");
        require(components.contains(probe) && bits.is_array() && bits.size() == components.at(probe),
                "record probe or component count differs from declaration");
        for(const auto& value : bits)
            (void)number(value);
    }
}
Json selection_json(const ShaderTraceSelection& selection) {
    Json result = Json::object();
    for(const auto& [name, value] :
        std::initializer_list<std::pair<const char*, std::optional<std::uint32_t>>>{{"frame", selection.frame},
                                                                                    {"draw", selection.draw},
                                                                                    {"pixel_x", selection.pixel_x},
                                                                                    {"pixel_y", selection.pixel_y},
                                                                                    {"sample", selection.sample},
                                                                                    {"probe", selection.probe}})
        if(value)
            result[name] = *value;
    return result;
}
} // namespace

Json parse_shader_trace(std::string_view input) {
    require(input.size() <= shader_trace_maximum_bytes && input.find('\0') == std::string_view::npos,
            "payload exceeds byte bounds or contains NUL");
    try {
        std::vector<std::set<std::string>> names;
        auto result = Json::parse(input, [&](int depth, Json::parse_event_t event, Json& value) {
            if(event == Json::parse_event_t::object_start || event == Json::parse_event_t::array_start) {
                require(depth < 16, "nesting exceeds bounds");
                names.emplace_back();
            } else if(event == Json::parse_event_t::object_end || event == Json::parse_event_t::array_end)
                names.pop_back();
            else if(event == Json::parse_event_t::key)
                require(names.back().insert(value.get<std::string>()).second, "duplicate JSON field");
            return true;
        });
        validate(result);
        return result;
    } catch(const Json::exception&) {
        throw std::invalid_argument("Invalid application shader trace JSON");
    }
}

Json shader_trace_page(const Json& trace, const ShaderTraceSelection& selection, std::size_t offset,
                       std::size_t limit) {
    require(offset <= 100000 && limit > 0 && limit <= 100, "invalid pagination");
    // The composing owner admits the immutable payload once before paging it.
    const auto selected = selection_json(selection);
    std::map<std::uint32_t, const Json*> probes;
    for(const auto& probe : trace.at("probes"))
        probes.emplace(probe.at("id").get<std::uint32_t>(), &probe);
    Json result{{"evidence_origin", shader_trace_origin},
                {"original_execution", false},
                {"capture_event_correspondence", false},
                {"producer_claims_verified", false},
                {"shader", trace.at("shader")},
                {"extent", trace.at("extent")},
                {"reported_selection", trace.at("selection")},
                {"selection", selected},
                {"capacity", trace.at("capacity")},
                {"attempted_records", trace.at("attempted_records")},
                {"dropped_records", trace.at("dropped_records")},
                {"probes", trace.at("probes")},
                {"records_total", trace.at("records").size()},
                {"matched_records", 0},
                {"offset", offset},
                {"next_offset", nullptr},
                {"records", Json::array()}};
    std::size_t matched = 0;
    for(const auto& record : trace.at("records")) {
        bool matches = true;
        for(const auto& [key, value] : selected.items())
            matches = matches && record.at(key) == value;
        if(!matches)
            continue;
        const auto index = matched++;
        if(index < offset)
            continue;
        if(result.at("records").size() >= limit) {
            result["next_offset"] = offset + result.at("records").size();
            continue;
        }
        auto row = record;
        const auto& probe = *probes.at(record.at("probe").get<std::uint32_t>());
        row["probe_name"] = probe.at("name");
        row["type"] = probe.at("type");
        row["values"] = Json::array();
        row["bits_hex"] = Json::array();
        for(const auto& value : record.at("bits")) {
            const auto bits = value.get<std::uint32_t>();
            std::string hex(8, '0');
            constexpr auto digits = "0123456789abcdef";
            for(unsigned n = 0; n < 8; ++n)
                hex[7 - n] = digits[(bits >> (n * 4)) & 15];
            row["bits_hex"].push_back("0x" + hex);
            if(probe.at("type") == "float32") {
                const auto number = std::bit_cast<float>(bits);
                row["values"].push_back(std::isfinite(number) ? Json(number) : Json(nullptr));
            } else if(probe.at("type") == "int32")
                row["values"].push_back(std::bit_cast<std::int32_t>(bits));
            else
                row["values"].push_back(bits);
        }
        result["records"].push_back(std::move(row));
    }
    result["matched_records"] = matched;
    require(result.dump().size() <= 256U * 1024U, "page exceeds response budget");
    return result;
}

Json make_shader_trace(const ShaderTraceBufferHeader& header, std::span<const ShaderTraceBufferRecord> records,
                       Json shader, std::uint32_t width, std::uint32_t height, Json probes) {
    require(header.reserved == 0 && header.capacity > 0 && header.capacity <= shader_trace_maximum_records &&
                records.size() == std::min(header.capacity, header.attempted_records),
            "readback buffer bounds differ from header");
    Json result{{"schema_version", 1},
                {"evidence_origin", shader_trace_origin},
                {"original_execution", false},
                {"capture_event_correspondence", false},
                {"shader", std::move(shader)},
                {"extent", {{"width", width}, {"height", height}}},
                {"selection", {{"pixel_x", header.pixel_x}, {"pixel_y", header.pixel_y}}},
                {"capacity", header.capacity},
                {"attempted_records", header.attempted_records},
                {"dropped_records", header.dropped_records},
                {"probes", std::move(probes)},
                {"records", Json::array()}};
    for(const auto& record : records) {
        require(record.selector[3] > 0 && record.selector[3] <= 4, "readback components exceed bounds");
        result["records"].push_back(
            {{"frame", record.invocation[0]},
             {"draw", record.invocation[1]},
             {"pixel_x", record.invocation[2]},
             {"pixel_y", record.invocation[3]},
             {"sample", record.selector[0]},
             {"primitive", record.selector[1] == UINT32_MAX ? Json(nullptr) : Json(record.selector[1])},
             {"probe", record.selector[2]},
             {"bits", std::vector<std::uint32_t>(record.bits.begin(), record.bits.begin() + record.selector[3])}});
    }
    validate(result);
    require(result.dump().size() <= shader_trace_maximum_bytes, "producer payload exceeds byte bounds");
    return result;
}
} // namespace ngm
