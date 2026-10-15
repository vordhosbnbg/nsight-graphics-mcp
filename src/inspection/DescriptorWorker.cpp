#include "DescriptorWorkerBuild.hpp"
#include "ReadOnlyDatabase.h"
#include "VulkanStructHydrator.h"
#include "ngm/Hash.hpp"
#include "ngm/WorkerConfinement.hpp"

#include <charconv>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <span>
#include <stdexcept>
#include <type_traits>

namespace {
using Json = nlohmann::json;
void require(bool ok, const char* reason) {
    if(!ok)
        throw std::runtime_error(reason);
}
std::uint64_t number(std::string_view text) {
    std::uint64_t value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    require(parsed.ec == std::errc() && parsed.ptr == text.data() + text.size(), "Expected unsigned integer");
    return value;
}
template <typename T>
std::uint64_t bits(T value) {
    if constexpr(std::is_pointer_v<T>)
        return reinterpret_cast<std::uintptr_t>(value);
    else
        return static_cast<std::uint64_t>(value);
}
} // namespace

int main(int argc, char** argv) {
    const char* phase = "arguments";
    try {
        if(argc == 2 && std::string_view(argv[1]) == "--profile") {
            std::cout << ngm::descriptor_worker_build_json << '\n';
            return 0;
        }
        require(argc == 5, "Expected SNAPSHOT HANDLE WRITE_COUNT OUTPUT_BYTES");
        const std::filesystem::path root(argv[1]);
        const auto handle = number(argv[2]), count = number(argv[3]), capacity = number(argv[4]);
        require(root.is_absolute() && handle <= INT32_MAX && count > 0 && count <= 64 && capacity > 0 &&
                    capacity <= 65536 && count * sizeof(VkWriteDescriptorSet) <= capacity,
                "Descriptor request exceeds bounds");
        require(sizeof(void*) == 8 && sizeof(VkWriteDescriptorSet) == 64 && sizeof(VkDescriptorBufferInfo) == 24 &&
                    sizeof(VkDescriptorImageInfo) == 24,
                "Unqualified Vulkan ABI");
        phase = "confinement";
        ngm::confine_resource_worker(root / "data.bin", root / "data.bin.rec");
        phase = "database";
        Serialization::ReadOnlyDatabase database;
        require(database.Init((root / "data.bin").c_str()) == Serialization::ReadOnlyDatabase::InitResult::Ok,
                "Generated database initialization failed");
        const Serialization::DATABASE_HANDLE id(static_cast<std::int32_t>(handle));
        const auto input_bytes = database.GetSize(id);
        require(input_bytes >= count * sizeof(VkWriteDescriptorSet) && input_bytes <= 16U * 1024U * 1024U,
                "Missing or excessive descriptor resource");
        Serialization::DataScope scope(Serialization::DataScopeTracker::Instance());
        auto resource = database.Read<const VkWriteDescriptorSet*>(id);
        require(resource.Get() != nullptr, "Generated descriptor resource read failed");
        // Database blocks need not start at a Vulkan structure's alignment.
        // Relocate the opaque bytes before the helper follows relative offsets.
        // Their interpretation and extent remain entirely the helper's contract.
        std::vector<std::uint64_t> aligned_input((input_bytes + sizeof(std::uint64_t) - 1) / sizeof(std::uint64_t));
        std::memcpy(aligned_input.data(), resource.Get(), input_bytes);
        const auto* input = reinterpret_cast<const VkWriteDescriptorSet*>(aligned_input.data());
        phase = "hydration";
        using Key = std::pair<VkObjectType, std::uint64_t>;
        std::map<Key, std::uint64_t> tokens;
        std::vector<Key> objects;
        constexpr std::uint64_t token_base = 0x100000;
        std::vector<std::uint8_t> output(capacity);
        NV::Vulkan::StructHydrator hydrator(
            output,
            [&](VkObjectType type, std::uint64_t value) {
                require(type == VK_OBJECT_TYPE_DESCRIPTOR_SET || type == VK_OBJECT_TYPE_BUFFER ||
                            type == VK_OBJECT_TYPE_IMAGE_VIEW || type == VK_OBJECT_TYPE_SAMPLER,
                        "Unqualified descriptor object type");
                const Key key{type, value};
                if(const auto found = tokens.find(key); found != tokens.end())
                    return found->second;
                require(value != 0 && objects.size() < 1024, "Descriptor object limit exceeded");
                const auto token = token_base + objects.size();
                tokens.emplace(key, token);
                objects.push_back(key);
                return token;
            },
            input, input_bytes);
        // The documented generated helper owns serialization interpretation.
        // Checked windows instrument its copies/pointers; no GPU calls occur.
        const auto* writes = hydrator.Rehydrate(nullptr, static_cast<std::uint32_t>(count), input);
        require(writes != nullptr && hydrator.Free() <= capacity, "Missing hydrated writes");
        const auto object = [&](VkObjectType type, auto value) -> Json {
            const auto token = bits(value);
            if(token == 0)
                return nullptr;
            require(token >= token_base && token - token_base < objects.size(), "Unknown hydrated object token");
            const auto& key = objects[token - token_base];
            require(key.first == type, "Hydrated object token type mismatch");
            return {{"object_type", static_cast<std::uint32_t>(type)}, {"serialized_id", key.second}};
        };
        phase = "output_validation";
        Json rows = Json::array();
        std::size_t descriptors = 0;
        for(std::size_t i = 0; i < count; ++i) {
            const auto& write = writes[i];
            require(write.sType == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET && write.dstSet != VK_NULL_HANDLE &&
                        write.pNext == nullptr && write.pTexelBufferView == nullptr && write.descriptorCount > 0 &&
                        write.descriptorCount <= 128 && descriptors + write.descriptorCount <= 512,
                    "Unqualified hydrated descriptor write");
            descriptors += write.descriptorCount;
            Json row{{"dst_set", object(VK_OBJECT_TYPE_DESCRIPTOR_SET, write.dstSet)},
                     {"binding", write.dstBinding},
                     {"array_element", write.dstArrayElement},
                     {"descriptor_type", static_cast<std::uint32_t>(write.descriptorType)},
                     {"descriptor_count", write.descriptorCount},
                     {"buffers", Json::array()},
                     {"images", Json::array()}};
            if(write.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
               write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
               write.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
               write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) {
                require(write.pBufferInfo != nullptr && write.pImageInfo == nullptr, "Unqualified buffer descriptor");
                for(std::uint32_t j = 0; j < write.descriptorCount; ++j) {
                    const auto& buffer = write.pBufferInfo[j];
                    row["buffers"].push_back({{"buffer", object(VK_OBJECT_TYPE_BUFFER, buffer.buffer)},
                                              {"offset", buffer.offset},
                                              {"range", buffer.range}});
                }
            } else {
                require(write.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && write.pImageInfo &&
                            !write.pBufferInfo,
                        "Unqualified image descriptor");
                for(std::uint32_t j = 0; j < write.descriptorCount; ++j) {
                    const auto& image = write.pImageInfo[j];
                    row["images"].push_back({{"image_view", object(VK_OBJECT_TYPE_IMAGE_VIEW, image.imageView)},
                                             {"sampler", object(VK_OBJECT_TYPE_SAMPLER, image.sampler)},
                                             {"image_layout", static_cast<std::int32_t>(image.imageLayout)}});
                }
            }
            rows.push_back(std::move(row));
        }
        const auto result = Json{
            {"schema_version", 1},
            {"profile", ngm::descriptor_worker_profile},
            {"evidence_origin", "generated_helper_descriptor_writes"},
            {"scope", "Source-scoped descriptor writes; not execution-time values or shader selection"},
            {"handle", handle},
            {"write_count", count},
            {"input_bytes", input_bytes},
            {"source_alignment_remainder",
             reinterpret_cast<std::uintptr_t>(resource.Get()) % alignof(VkWriteDescriptorSet)},
            {"input_sha256", ngm::sha256(std::span(reinterpret_cast<const std::byte*>(resource.Get()),
                                                   static_cast<std::size_t>(input_bytes)))},
            {"output_capacity", capacity},
            {"output_used_bytes", capacity - hydrator.Free()},
            {"writes", rows}}.dump();
        require(result.size() < 65536, "Descriptor worker output exceeds limit");
        std::cout << result << '\n';
        std::cout.flush();
        require(bool(std::cout), "Descriptor worker output failed");
        return 0;
    } catch(const std::exception& error) {
        std::cerr << "Descriptor worker " << phase << ": " << error.what() << '\n';
        return 2;
    }
}
