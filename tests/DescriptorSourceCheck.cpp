#include "Check.hpp"
#include "ngm/CppEvidence.hpp"
#include "ngm/Inspection.hpp"

namespace {
const char* text = R"cpp(
void SetInitialFrameState00() {
    // Numeric serialized IDs deliberately differ from source UID suffixes.
    NV_VK_REPLAY(VulkanHelper_RegisterObject(VK_OBJECT_TYPE_BUFFER, uint64_t(VkBuffer_uid_7), 900ull));
    NV_VK_REPLAY(VulkanHelper_RegisterObject(VK_OBJECT_TYPE_DESCRIPTOR_SET, uint64_t(VkDescriptorSet_uid_12), 901ull));
    NV_VK_REPLAY(VulkanHelper_RegisterObject(VK_OBJECT_TYPE_DEVICE, uint64_t(VkDevice_uid_4), 4ull));
    NV_VK_REPLAY(VulkanHelper_UpdateDescriptorSetsFromDatabase(VkDevice_uid_4, 2u, Serialization::DATABASE_HANDLE(91), 176ull, 0, nullptr));
}
)cpp";
} // namespace

int main() {
    return ngm::check::run([] {
        using ngm::check::require;
        const ngm::CppSource source{"raw/cpp/project/FrameSetup00.cpp", text};
        const auto parsed = ngm::inspect_cpp_descriptor_updates({source});
        require(parsed.at("updates").size() == 1 && parsed.at("registrations").size() == 2 &&
                    parsed.at("unsupported").empty() && parsed.at("registration_coverage_complete") == true,
                "source update and only the worker-supported object types are listed");
        const auto& update = parsed.at("updates").at(0);
        require(update.at("handle") == 91 && update.at("write_count") == 2 && update.at("output_bytes") == 176 &&
                    update.at("copy_count") == 0 && update.at("readable") == true && update.at("conditional") == false,
                "literal source request fields preserve the helper calling contract");
        require(parsed.at("registrations").at(0).at("serialized_id") == 900 &&
                    parsed.at("registrations").at(0).at("symbol") == "VkBuffer_uid_7" &&
                    parsed.at("registrations").at(1).at("serialized_id") == 901 &&
                    parsed.at("registrations").at(1).at("symbol") == "VkDescriptorSet_uid_12",
                "typed registration identity does not assume UID suffix equals serialized ID");
        const auto ref = update.at("source_ref").get<std::string>();
        require(ref.size() == 64 && ref.find_first_not_of("0123456789abcdef") == std::string::npos,
                "source update has a bounded content reference");
        const auto location = update.at("source");
        const auto statement = source.text.substr(location.at("byte_offset"), location.at("byte_count"));
        require(statement.starts_with("NV_VK_REPLAY(") && statement.ends_with(";"), "span covers the complete wrapper");
        auto changed = source;
        changed.text += "// unrelated source edit\n";
        require(ngm::inspect_cpp_descriptor_updates({changed}).at("updates").at(0).at("source_ref") != ref,
                "the full source content participates in reference identity");

        const auto unsupported = [&](std::string spelling, std::string replacement) {
            auto altered = source;
            const auto at = altered.text.find(spelling);
            require(at != std::string::npos, "test mutation anchor exists");
            altered.text.replace(at, spelling.size(), replacement);
            const auto result = ngm::inspect_cpp_descriptor_updates({altered});
            require(result.at("updates").empty() && result.at("unsupported").size() == 1,
                    "unsupported source call never yields a readable request");
        };
        unsupported("2u, Serialization", "count, Serialization");
        unsupported("2u, Serialization", "0u, Serialization");
        unsupported("176ull, 0, nullptr", "127ull, 0, nullptr");
        unsupported("176ull, 0, nullptr", "65537ull, 0, nullptr");
        unsupported("176ull, 0, nullptr", "176ull, 1, copies");
        unsupported("176ull, 0, nullptr", "176ull, 0, nullptr,");
        unsupported("DATABASE_HANDLE(91)", "DATABASE_HANDLE(handle)");
        unsupported("DATABASE_HANDLE(91)", "DATABASE_HANDLE(2147483648)");
        unsupported("DATABASE_HANDLE(91)", "DATABASE_HANDLE(091)");

        auto conditional = source;
        conditional.text = "#if OPTIONAL\n" + conditional.text + "#endif\n";
        const auto guarded = ngm::inspect_cpp_descriptor_updates({conditional});
        require(guarded.at("updates").size() == 1 && guarded.at("updates").at(0).at("readable") == false &&
                    guarded.at("updates").at(0).at("conditional") == true &&
                    guarded.at("registration_coverage_complete") == false,
                "conditional registrations and updates cannot establish unconditional source writes");
        const ngm::CppSource conflict{
            "raw/cpp/project/Resources00.cpp",
            "NV_VK_REPLAY(VulkanHelper_RegisterObject(VK_OBJECT_TYPE_BUFFER, uint64_t(VkBuffer_uid_88), 900ull));"};
        require(ngm::inspect_cpp_descriptor_updates({source, conflict}).at("registration_coverage_complete") == false,
                "conflicting typed registrations do not choose an arbitrary source symbol");
        auto repeated = conflict;
        repeated.text =
            "NV_VK_REPLAY(VulkanHelper_RegisterObject(VK_OBJECT_TYPE_BUFFER, uint64_t(VkBuffer_uid_7), 900ull));";
        require(
            ngm::inspect_cpp_descriptor_updates({source, repeated}).at("registrations").at(0).at("sources").size() == 2,
            "repeated identical symbol registration retains every source occurrence");
        const auto reordered = ngm::inspect_cpp_descriptor_updates({repeated, source});
        require(reordered.at("updates").at(0).at("source_ref") == ref,
                "source input order does not define descriptor identity");
        const ngm::CppSource comments{"FrameSetup00.cpp", R"cpp(
// VulkanHelper_UpdateDescriptorSetsFromDatabase(...)
const char* s = "VulkanHelper_RegisterObject(...)";
)cpp"};
        require(ngm::inspect_cpp_descriptor_updates({comments}).at("updates").empty() &&
                    ngm::inspect_cpp_descriptor_updates({comments}).at("registrations").empty(),
                "comments and string literals do not create source writes");
        bool invalid = false;
        try {
            (void)ngm::inspect_cpp_descriptor_updates({source, source});
        } catch(const ngm::InspectionError& error) {
            invalid = error.code() == ngm::InspectionErrorCode::InvalidExport;
        }
        require(invalid, "duplicate source paths refuse before listing evidence");
    });
}
