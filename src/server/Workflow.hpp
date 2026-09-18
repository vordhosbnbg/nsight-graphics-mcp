#pragma once

#include "Server.hpp"
#include "ngm/CaptureService.hpp"

#include <fastmcpp/tools/manager.hpp>
#include <memory>

namespace ngm {
fastmcpp::Json implemented_tool_names();

class WorkflowTools {
public:
    explicit WorkflowTools(ServerOptions options);
    void register_tools(fastmcpp::tools::ToolManager& tools);
    bool shutdown();

private:
    CaptureService& service();
    ServerOptions options_;
    std::unique_ptr<CaptureService> service_;
};
} // namespace ngm
