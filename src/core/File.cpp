#include "ngm/File.hpp"

#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace ngm {
std::string read_regular_file(const std::filesystem::path& path, std::size_t maximum_bytes) {
    struct File {
        int descriptor;
        ~File() {
            if(descriptor >= 0) {
                close(descriptor);
            }
        }
    } file{open(path.c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC)};
    struct stat attributes{};
    if(file.descriptor < 0 || fstat(file.descriptor, &attributes) != 0 || !S_ISREG(attributes.st_mode) ||
       attributes.st_size < 0 || static_cast<std::uint64_t>(attributes.st_size) > maximum_bytes) {
        throw std::runtime_error("Expected a readable regular file within the size limit: " + path.string());
    }
    std::string data(static_cast<std::size_t>(attributes.st_size), '\0');
    std::size_t offset = 0;
    while(offset < data.size()) {
        const auto count = read(file.descriptor, data.data() + offset, data.size() - offset);
        if(count < 0 && errno == EINTR) {
            continue;
        }
        if(count <= 0) {
            throw std::runtime_error("File changed or failed while reading: " + path.string());
        }
        offset += static_cast<std::size_t>(count);
    }
    char extra;
    ssize_t count;
    do {
        count = read(file.descriptor, &extra, 1);
    } while(count < 0 && errno == EINTR);
    if(count != 0) {
        throw std::runtime_error("File changed or failed while reading: " + path.string());
    }
    return data;
}
} // namespace ngm
