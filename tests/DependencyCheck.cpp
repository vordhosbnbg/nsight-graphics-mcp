#include "Check.hpp"
#include "ngm/Version.hpp"

#include <fastmcpp/exceptions.hpp>
#include <fastmcpp/server/server.hpp>
#include <volk.h>

#include <string>

int main() {
    return ngm::check::run([] {
        using ngm::check::require;
        fastmcpp::server::Server server("dependency-check", std::string(ngm::project_version()));
        server.route("echo", [](const fastmcpp::Json& request) { return request; });
        const auto request = fastmcpp::Json::parse(R"({"text":"source-built","number":7})");
        require(server.handle("echo", request) == request, "local fastmcpp/JSON round trip");
        bool missing_route = false;
        try {
            server.handle("missing", request);
        } catch(const fastmcpp::NotFoundError&) {
            missing_route = true;
        }
        require(missing_route, "fastmcpp reports a missing route");
        // Exercise the static loader without opening libvulkan or using a GPU.
        require(volkGetInstanceVersion() == 0, "uninitialized volk has no Vulkan version");
        require(volkGetLoadedInstance() == VK_NULL_HANDLE, "no Vulkan instance was created");
    });
}
