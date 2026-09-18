// Trusted, fixed-input qualification only. Never register this as an MCP tool.
#include "ReadOnlyDatabase.h"
#include "VulkanStructHydrator.h"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"

#include <filesystem>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
void require(bool condition, const std::string& message) {
    if(!condition)
        throw std::runtime_error(message);
}
std::string type_name(VkObjectType type) {
    switch(type) {
        case VK_OBJECT_TYPE_DESCRIPTOR_SET:
            return "VK_OBJECT_TYPE_DESCRIPTOR_SET";
        case VK_OBJECT_TYPE_BUFFER:
            return "VK_OBJECT_TYPE_BUFFER";
        case VK_OBJECT_TYPE_IMAGE_VIEW:
            return "VK_OBJECT_TYPE_IMAGE_VIEW";
        case VK_OBJECT_TYPE_SAMPLER:
            return "VK_OBJECT_TYPE_SAMPLER";
        default:
            throw std::runtime_error("Unqualified descriptor object type");
    }
}
template <typename T>
std::uint64_t bits(T value) {
    if constexpr(std::is_pointer_v<T>)
        return reinterpret_cast<std::uintptr_t>(value);
    else
        return static_cast<std::uint64_t>(value);
}
void source_line(const fs::path& root, const Json& reference) {
    std::istringstream input(ngm::read_regular_file(root / reference.at("source").get<std::string>(), 4 * 1024 * 1024));
    std::string line;
    for(std::size_t i = 0; i < reference.at("line").get<std::size_t>(); ++i)
        require(bool(std::getline(input, line)), "Source reference exists");
    const auto start = line.find_first_not_of(" \t\r");
    require(start != std::string::npos && line.substr(start) == reference.at("text").get<std::string>(),
            "Exact source reference");
}
} // namespace

int main(int argc, char** argv) {
    std::string phase = "input_identity";
    try {
        require(argc == 2, "SNAPSHOT required");
        const auto root = fs::canonical(argv[1]);
        require(ngm::sha256_file(root / "case.json") == NGM_DESCRIPTOR_CASE_SHA256, "Fixed case identity changed");
        const auto item = Json::parse(ngm::read_regular_file(root / "case.json", 65536));
        for(const auto& [name, identity] : item.at("files").items()) {
            const auto path = root / name;
            require(fs::symlink_status(path).type() == fs::file_type::regular &&
                        fs::file_size(path) == identity.at("bytes") &&
                        ngm::sha256_file(path) == identity.at("sha256").get<std::string>(),
                    "Fixed generated input identity changed: " + name);
        }
        std::map<std::uint64_t, Json> registrations;
        for(const auto& registration : item.at("registrations")) {
            source_line(root, registration);
            require(registrations.emplace(registration.at("id"), registration).second, "Unique registration ID");
        }
        for(const auto& update : item.at("updates"))
            source_line(root, update);
        Json report{{"status", "running"},
                    {"capture_id", item.at("capture_id")},
                    {"producer", item.at("producer")},
                    {"build", item.at("build")},
                    {"evidence_origin", "generated_helper_setup_descriptor_writes"},
                    {"scope", "Fixed trusted capture only; setup writes, not executed GPU or shader selection state"},
                    {"sizeof_write_descriptor_set", sizeof(VkWriteDescriptorSet)},
                    {"sizeof_descriptor_buffer_info", sizeof(VkDescriptorBufferInfo)},
                    {"sizeof_descriptor_image_info", sizeof(VkDescriptorImageInfo)},
                    {"updates", Json::array()}};
        phase = "generated_helper";
        Serialization::ReadOnlyDatabase database;
        const auto initialized = database.Init((root / "data.bin").c_str());
        require(initialized == Serialization::ReadOnlyDatabase::InitResult::Ok,
                Serialization::ReadOnlyDatabase::InitResultToString(initialized));
        for(const auto& update : item.at("updates")) {
            Serialization::DataScope scope(Serialization::DataScopeTracker::Instance());
            const Serialization::DATABASE_HANDLE handle(update.at("handle").get<int32_t>());
            const auto input_bytes = database.GetSize(handle);
            const auto output_bytes = update.at("output_bytes").get<std::size_t>();
            const auto count = update.at("write_count").get<std::uint32_t>();
            require(input_bytes > 0 && input_bytes <= 1024 * 1024 && output_bytes > 0 && output_bytes <= 1024 * 1024 &&
                        count > 0 && count <= 16,
                    "Fixed probe resource limits");
            auto resource = database.Read<const VkWriteDescriptorSet*>(handle);
            require(resource.Get() != nullptr, "Generated database resource read");
            std::vector<std::uint8_t> output(output_bytes);
            Json callbacks = Json::array();
            // Tokens deliberately differ from serialized IDs. They never reach Vulkan.
            constexpr std::uint64_t token_base = 0x100000;
            NV::Vulkan::StructHydrator hydrator(output, [&](VkObjectType type, std::uint64_t id) {
                const auto found = registrations.find(id);
                require(found != registrations.end() && found->second.at("type").get<std::string>() == type_name(type),
                        "Source registration matches callback ID and type");
                callbacks.push_back({{"type", type_name(type)},
                                     {"serialized_id", id},
                                     {"symbol", found->second.at("symbol")},
                                     {"probe_token", token_base + id}});
                return token_base + id;
            });
            // The same call as exported VulkanReplay.cpp. The vendor helper owns
            // all packed-structure interpretation; no private format decoder here.
            const auto* writes = hydrator.Rehydrate(nullptr, count, resource.Get());
            require(writes && hydrator.Free() <= output_bytes, "Hydrated output exists");
            const auto symbol = [&](auto object) -> Json {
                const auto token = bits(object);
                if(token == 0)
                    return nullptr;
                require(token >= token_base && registrations.contains(token - token_base), "Hydrated token mapped");
                return registrations.at(token - token_base).at("symbol");
            };
            Json rows = Json::array();
            for(std::uint32_t i = 0; i < count; ++i) {
                const auto& write = writes[i];
                require(write.sType == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET && !write.pNext &&
                            !write.pTexelBufferView && write.descriptorCount > 0 && write.descriptorCount <= 16,
                        "Qualified output write structure");
                Json row{{"dst_set", symbol(write.dstSet)},
                         {"binding", write.dstBinding},
                         {"array_element", write.dstArrayElement},
                         {"descriptor_count", write.descriptorCount},
                         {"buffers", Json::array()},
                         {"images", Json::array()}};
                if(write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
                   write.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
                    row["descriptor_type"] = write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
                                                 ? "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER"
                                                 : "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER";
                    require(write.pBufferInfo && !write.pImageInfo, "Buffer descriptor payload");
                    for(std::uint32_t j = 0; j < write.descriptorCount; ++j) {
                        const auto& buffer = write.pBufferInfo[j];
                        row["buffers"].push_back(
                            {{"buffer", symbol(buffer.buffer)}, {"offset", buffer.offset}, {"range", buffer.range}});
                    }
                } else {
                    require(write.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && write.pImageInfo &&
                                !write.pBufferInfo,
                            "Qualified combined image sampler");
                    row["descriptor_type"] = "VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER";
                    for(std::uint32_t j = 0; j < write.descriptorCount; ++j) {
                        const auto& image = write.pImageInfo[j];
                        row["images"].push_back({{"image_view", symbol(image.imageView)},
                                                 {"sampler", symbol(image.sampler)},
                                                 {"image_layout", static_cast<int>(image.imageLayout)}});
                    }
                }
                rows.push_back(std::move(row));
            }
            report["updates"].push_back(
                {{"source", update},
                 {"input_bytes", input_bytes},
                 {"input_sha256",
                  ngm::sha256(std::span(reinterpret_cast<const std::byte*>(resource.Get()), input_bytes))},
                 {"output_used_bytes", output_bytes - hydrator.Free()},
                 {"callbacks", callbacks},
                 {"writes", rows}});
        }
        report["status"] = "pass";
        std::cout << report.dump(2) << '\n';
        return 0;
    } catch(const std::exception& error) {
        std::cerr << Json{{"status", "fail"}, {"phase", phase}, {"error", error.what()}}.dump() << '\n';
        return 2;
    }
}
