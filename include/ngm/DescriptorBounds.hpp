#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace ngm {
inline void descriptor_assert(bool condition) {
    if(!condition)
        throw std::runtime_error("Descriptor hydration assertion failed");
}

// The generated helper owns its structure grammar. This window only validates
// the byte ranges and alignment of every pointer/copy operation it performs.
// Use after confinement; this is not a replacement for the process boundary.
class DescriptorBounds {
public:
    DescriptorBounds(const void* input, std::size_t input_bytes, const void* output, std::size_t output_bytes) :
        input_(reinterpret_cast<std::uintptr_t>(input)), output_(reinterpret_cast<std::uintptr_t>(output)),
        input_bytes_(input_bytes), output_bytes_(output_bytes) {
        if(!input || !output || input_bytes == 0 || input_bytes > 16U * 1024U * 1024U || output_bytes == 0 ||
           output_bytes > 65536 || input_bytes > maximum - input_ || output_bytes > maximum - output_)
            throw std::runtime_error("Descriptor hydration window exceeds bounds");
        if(input_ < output_ + output_bytes_ && output_ < input_ + input_bytes_)
            throw std::runtime_error("Descriptor hydration windows overlap");
    }

    template <typename T>
    const T* pointer(const void* parent, const T* offset) {
        operation();
        const auto base = reinterpret_cast<std::uintptr_t>(parent);
        const auto relative = reinterpret_cast<std::uintptr_t>(offset);
        if(parent)
            input_range(base, 1, 1);
        if(relative > maximum - base)
            throw std::runtime_error("Descriptor hydration pointer overflow");
        const auto address = base + relative;
        input_range(address, sizeof(T), alignof(T));
        return reinterpret_cast<const T*>(address);
    }

    template <typename T>
    void copy(std::uint32_t count, const T* input, std::size_t offset) {
        operation();
        if(count == 0 || count > 4096 || count > std::numeric_limits<std::size_t>::max() / sizeof(T))
            throw std::runtime_error("Descriptor hydration copy count exceeds bounds");
        const auto bytes = sizeof(T) * count;
        input_range(reinterpret_cast<std::uintptr_t>(input), bytes, alignof(T));
        if(offset > output_bytes_ || bytes > output_bytes_ - offset || (output_ + offset) % alignof(T) != 0)
            throw std::runtime_error("Descriptor hydration output exceeds bounds or alignment");
    }

private:
    static constexpr auto maximum = std::numeric_limits<std::uintptr_t>::max();
    std::uintptr_t input_, output_;
    std::size_t input_bytes_, output_bytes_, operations_ = 0;
    void operation() {
        if(++operations_ > 1024)
            throw std::runtime_error("Descriptor hydration operation limit exceeded");
    }
    void input_range(std::uintptr_t address, std::size_t bytes, std::size_t alignment) const {
        if(address < input_ || address - input_ > input_bytes_ || bytes > input_bytes_ - (address - input_))
            throw std::runtime_error("Descriptor hydration input exceeds bounds");
        if(address % alignment != 0)
            throw std::runtime_error("Descriptor hydration input alignment is invalid");
    }
};
} // namespace ngm
