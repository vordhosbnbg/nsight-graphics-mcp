// Optional exact-helper CPU check; no database reader, Vulkan call or GPU work.
#include "Check.hpp"
#include "VulkanStructHydrator.h"

#include <cstring>
#include <vector>

namespace {
template <typename T>
T handle(std::uint64_t id) {
    return reinterpret_cast<T>(static_cast<std::uintptr_t>(id));
}
struct Input {
    VkWriteDescriptorSet write{};
    VkDescriptorBufferInfo buffer{};
};
static_assert(sizeof(Input) == 88);
} // namespace

int main() {
    return ngm::check::run([] {
        using ngm::check::require;
        Input input;
        input.write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        input.write.dstSet = handle<VkDescriptorSet>(101);
        input.write.dstBinding = 3;
        input.write.dstArrayElement = 2;
        input.write.descriptorCount = 1;
        input.write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        input.write.pBufferInfo = reinterpret_cast<VkDescriptorBufferInfo*>(64);
        input.buffer.buffer = handle<VkBuffer>(202);
        input.buffer.offset = 32;
        input.buffer.range = 16;
        const auto decode = [](VkObjectType type, std::uint64_t id) -> std::uint64_t {
            require((type == VK_OBJECT_TYPE_DESCRIPTOR_SET && id == 101) ||
                        (type == VK_OBJECT_TYPE_BUFFER && id == 202),
                    "callback checks serialized object type and identity");
            return id + 0x10000;
        };
        const auto run = [&](const Input& data, std::size_t input_bytes, std::size_t output_bytes) {
            std::vector<std::uint8_t> output(output_bytes);
            NV::Vulkan::StructHydrator hydrator(output, decode, &data, input_bytes);
            const auto* writes = hydrator.Rehydrate(nullptr, 1, &data.write);
            require(writes && writes->dstSet == handle<VkDescriptorSet>(0x10000 + 101) && writes->dstBinding == 3 &&
                        writes->dstArrayElement == 2 && writes->descriptorCount == 1 && writes->pBufferInfo &&
                        writes->pBufferInfo->buffer == handle<VkBuffer>(0x10000 + 202) &&
                        writes->pBufferInfo->offset == 32 && writes->pBufferInfo->range == 16 && hydrator.Free() == 0,
                    "real generated helper preserves fields and translates distinct tokens");
        };
        run(input, sizeof(input), sizeof(input));
        const auto refuses = [&](auto mutation, std::size_t input_bytes = sizeof(Input),
                                 std::size_t output_bytes = sizeof(Input)) {
            auto data = input;
            mutation(data);
            bool failed = false;
            try {
                run(data, input_bytes, output_bytes);
            } catch(const std::runtime_error&) {
                failed = true;
            }
            require(failed, "malformed helper input refuses in checked boundary");
        };
        refuses([](auto&) {}, 64);
        refuses([](auto&) {}, 87);
        refuses([](auto&) {}, sizeof(Input), 64);
        refuses([](auto& d) { d.write.pBufferInfo = reinterpret_cast<VkDescriptorBufferInfo*>(UINTPTR_MAX); });
        refuses([](auto& d) { d.write.pBufferInfo = reinterpret_cast<VkDescriptorBufferInfo*>(65); });
        refuses([](auto& d) { d.write.descriptorCount = UINT32_MAX; });
        refuses([](auto& d) { d.write.pNext = reinterpret_cast<void*>(64); });
        refuses([](auto& d) { d.write.dstSet = handle<VkDescriptorSet>(303); });

        struct ImageInput {
            VkWriteDescriptorSet write;
            VkDescriptorImageInfo images[2];
        } images{};
        static_assert(sizeof(images) == 112);
        images.write = input.write;
        images.write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        images.write.descriptorCount = 2;
        images.write.pBufferInfo = nullptr;
        images.write.pImageInfo = reinterpret_cast<VkDescriptorImageInfo*>(64);
        for(std::size_t i = 0; i < 2; ++i) {
            images.images[i].sampler = handle<VkSampler>(400 + i);
            images.images[i].imageView = handle<VkImageView>(500 + i);
            images.images[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        std::vector<std::uint8_t> output(sizeof(images));
        NV::Vulkan::StructHydrator hydrator(
            output, [](VkObjectType, std::uint64_t id) { return id + 0x20000; }, &images, sizeof(images));
        const auto* result = hydrator.Rehydrate(nullptr, 1, &images.write);
        require(result && result->descriptorCount == 2 && result->pImageInfo && hydrator.Free() == 0,
                "multiple image elements hydrate within bounds");
        for(std::size_t i = 0; i < 2; ++i)
            require(result->pImageInfo[i].sampler == handle<VkSampler>(0x20000 + 400 + i) &&
                        result->pImageInfo[i].imageView == handle<VkImageView>(0x20000 + 500 + i) &&
                        result->pImageInfo[i].imageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    "image-array fields preserve independent expected values");

        struct WritesInput {
            VkWriteDescriptorSet writes[2];
            VkDescriptorBufferInfo buffers[2];
        } multiple{};
        static_assert(sizeof(multiple) == 176);
        for(std::size_t i = 0; i < 2; ++i) {
            multiple.writes[i] = input.write;
            multiple.writes[i].dstArrayElement = static_cast<std::uint32_t>(i);
            multiple.writes[i].pBufferInfo = reinterpret_cast<VkDescriptorBufferInfo*>(128 + 24 * i);
            multiple.buffers[i] = input.buffer;
            multiple.buffers[i].buffer = handle<VkBuffer>(600 + i);
        }
        std::vector<std::uint8_t> multi_output(sizeof(multiple));
        NV::Vulkan::StructHydrator multi(
            multi_output, [](VkObjectType, std::uint64_t id) { return id + 0x30000; }, &multiple, sizeof(multiple));
        const auto* multi_result = multi.Rehydrate(nullptr, 2, multiple.writes);
        require(multi_result && multi.Free() == 0, "multiple top-level writes fill the checked output");
        for(std::size_t i = 0; i < 2; ++i)
            require(multi_result[i].dstArrayElement == i && multi_result[i].pBufferInfo &&
                        multi_result[i].pBufferInfo->buffer == handle<VkBuffer>(0x30000 + 600 + i),
                    "nested offsets are relative to the generated array base");
    });
}
