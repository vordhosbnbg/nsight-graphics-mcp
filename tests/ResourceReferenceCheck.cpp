#include "Check.hpp"
#include "ngm/CppEvidence.hpp"
#include "ngm/Inspection.hpp"

namespace {
using ngm::check::require;
auto inspect(const std::string& text, const std::string& path = "Frame0.cpp") {
    return ngm::inspect_cpp_resources({{path, text}});
}
} // namespace
int main() {
    return ngm::check::run([] {
        const std::string text = "// NV_GET_RESOURCE(int*,99)\n"
                                 "const char* decoy = \"NV_GET_RESOURCE(int*,98)\";\n"
                                 "NV_GET_RESOURCE(char*,7);\nNV_GET_RESOURCE_CHECKED(char*,7,8ull);\n"
                                 "#if PLATFORM\nNV_GET_RESOURCE_STATIC(char*,8,4);\n#endif\n";
        const auto result = inspect(text);
        require(result.at("resources").size() == 3 && result.at("unsupported").empty(), "literal references/decoys");
        const auto& rows = result.at("resources");
        require(rows[0].at("expected_bytes") == 8 && rows[0].at("declared_bytes").is_null(), "shared declaration");
        require(rows[2].at("conditional") == true, "conditional literal must remain labelled");
        require(inspect(text) == result, "stable identities");
        require(inspect(text, "Frame1.cpp")["resources"][0]["resource_ref"] != rows[0]["resource_ref"],
                "path identity");
        require(inspect(text + "\n")["resources"][0]["resource_ref"] != rows[0]["resource_ref"],
                "full source identity");
        const auto span = rows[1].at("source");
        require(text.substr(span.at("byte_offset").get<std::size_t>(), span.at("byte_count").get<std::size_t>()) ==
                    "NV_GET_RESOURCE_CHECKED(char*,7,8ull)",
                "exact macro span");
        for(const auto& extra : {"NV_GET_RESOURCE_CHECKED(char*,7,9);", "NV_GET_RESOURCE_CHECKED(char*,7,0);",
                                 "NV_GET_RESOURCE_CHECKED(char*,7,16777217);"}) {
            const auto conflict =
                inspect("NV_GET_RESOURCE(char*,7);NV_GET_RESOURCE_CHECKED(char*,7,8);" + std::string(extra));
            require(conflict["resources"][0]["readable"] == false, "invalid/conflicting size did not propagate");
        }
        for(const auto* invalid :
            {"NV_GET_RESOURCE_CHECKED(char*,7,8,)", "NV_GET_RESOURCE(char*,7,)", "NV_GET_RESOURCE(char*,2147483648)",
             "NV_GET_RESOURCE(char*,07)", "NV_GET_RESOURCE(char*,variable)", "NV_GET_RESOURCE_FUTURE(char*,7)",
             "NV_GET_RESOURCE_CHECKED(char*,7,sizeof(int))"}) {
            const auto rejected = inspect(invalid);
            require(rejected.at("resources").empty() && rejected.at("unsupported").size() == 1,
                    "unsupported spelling silently accepted");
        }
        require(inspect("#if NV_GET_RESOURCE_FUTURE(char*,7)\n#endif")["unsupported"].size() == 1,
                "preprocessor occurrence lost");
        const auto unknown = inspect("NV_GET_RESOURCE(char*,7)")["resources"][0];
        require(unknown["expected_bytes"].is_null() && unknown["readable"] == true, "unknown size must stay explicit");
        for(const auto& invalid : {"NV_GET_RESOURCE(char*,7", "/* truncated", "#define X 7\n", "// splice\\\n"}) {
            bool failed = false;
            try {
                (void)inspect(invalid);
            } catch(const ngm::InspectionError&) {
                failed = true;
            }
            require(failed, "malformed source accepted");
        }
        bool duplicate = false;
        try {
            (void)ngm::inspect_cpp_resources({{"Frame.cpp", text}, {"Frame.cpp", text}});
        } catch(const ngm::InspectionError&) {
            duplicate = true;
        }
        require(duplicate, "duplicate source accepted");
        std::string many;
        for(unsigned i = 0; i < 10001; ++i)
            many += "NV_GET_RESOURCE(char*,7);\n";
        bool bounded = false;
        try {
            (void)inspect(many);
        } catch(const ngm::InspectionError& error) {
            bounded = error.code() == ngm::InspectionErrorCode::LimitExceeded;
        }
        require(bounded, "resource result count unbounded");
    });
}
