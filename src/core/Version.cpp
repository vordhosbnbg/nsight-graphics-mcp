#include "ngm/Version.hpp"

#include "ngm/BuildVersion.hpp"

namespace ngm {
std::string_view project_version() noexcept {
    return NGM_PROJECT_VERSION;
}
} // namespace ngm
