#pragma once

// Included by the explicit C++ hardware runner. This independent, fixture-only
// oracle uses debug names/API witnesses and literal export audit, never product
// parser output to establish the expected descriptor identities or offsets.
void verify_descriptors_mcp(const fs::path& server, const fs::path& run) {
    const auto captured = load(run / "report.json");
    Json report{{"status", "running"},
                {"project_version", ngm::project_version()},
                {"scope", "Recorded descriptor bindings/offsets versus private successful API submissions; no "
                          "descriptor contents or GPU access observation"},
                {"server_sha256", ngm::sha256_file(server)},
                {"harness_sha256", ngm::sha256_file("/proc/self/exe")},
                {"passes", Json::array()},
                {"export_audit", Json::array()}};
    fs::copy_file(__FILE__, run / "DescriptorStateIntegration.hpp", fs::copy_options::overwrite_existing);
    fs::copy_file(fs::path(__FILE__).parent_path() / "DrawStateIntegration.cpp", run / "DescriptorQueryIntegration.cpp",
                  fs::copy_options::overwrite_existing);
    std::vector<std::map<std::string, std::string>> names, pipeline_layouts;
    for(const auto& source : captured.at("cases")) {
        const auto id = source.at("submission").at("artifact_id").get<std::string>();
        const auto project = run / "store/bundles" / id / source.at("index").at("project_directory").get<std::string>();
        const auto setup = ngm::read_regular_file(project / "FrameSetup00.cpp", 4U * 1024U * 1024U);
        const auto resources = ngm::read_regular_file(project / "Resources00.cpp", 4U * 1024U * 1024U);
        std::map<std::string, std::string> named, layouts;
        const std::string handle_marker = "/* objectHandle = */ uint64_t(", name_marker = "/* pObjectName = */ \"";
        for(std::size_t at = 0; (at = setup.find(handle_marker, at)) != std::string::npos;) {
            const auto first = at + handle_marker.size(), end = setup.find(')', first);
            const auto label = setup.find(name_marker, end), scope_end = setup.find('}', end);
            require(end != std::string::npos && label < scope_end, "observed literal object-name aggregate");
            const auto value = label + name_marker.size(), last = setup.find('"', value);
            require(last < scope_end, "bounded literal fixture debug name");
            const auto symbol = setup.substr(first, end - first);
            if(!symbol.starts_with("VkShaderModule_uid_"))
                require(named.emplace(setup.substr(value, last - value), symbol).second,
                        "unique non-shader fixture object name in exported API inputs");
            at = scope_end + 1;
        }
        const std::string marker = "// Create VkPipeline_uid_", layout_marker = "/* layout = */ ";
        for(std::size_t at = 0; (at = resources.find(marker, at)) != std::string::npos;) {
            const auto first = at + std::string("// Create ").size(), end = resources.find('\n', first);
            const auto next = resources.find("// Create ", end), value = resources.find(layout_marker, end);
            require(value < next, "literal pipeline-layout input in observed pipeline creation");
            const auto start = value + layout_marker.size(), last = resources.find(',', start);
            layouts.emplace(resources.substr(first, end - first), resources.substr(start, last - start));
            at = end;
        }
        require(named.contains("fixture.descriptors.set.3") && named.contains("fixture.descriptors.layout.2") &&
                    layouts.size() == 3,
                "all fixture descriptor/pipeline API names are retained");
        names.push_back(named);
        pipeline_layouts.push_back(layouts);
        report["export_audit"].push_back({{"capture_id", id},
                                          {"named_symbols", named},
                                          {"pipeline_layouts", layouts},
                                          {"setup_sha256", ngm::sha256_file(project / "FrameSetup00.cpp")},
                                          {"resources_sha256", ngm::sha256_file(project / "Resources00.cpp")}});
    }
    save(run / "descriptor-query-report.json", report);
    std::ofstream transcript(run / "descriptor-query-transcript.ndjson");
    transcript.exceptions(std::ios::failbit | std::ios::badbit);
    Json original;
    for(unsigned pass = 0; pass < 2; ++pass) {
        ngm::check::mcp::Client client(server, "/usr/bin:/bin", {"--artifact-root", (run / "store").string()}, false,
                                       {}, {30s, 30s});
        int sequence = 1;
        const auto request = [&](const char* method, const Json& params) {
            transcript << Json{{"pass", pass},
                               {"direction", "request"},
                               {"id", sequence},
                               {"method", method},
                               {"params", params}}
                              .dump()
                       << '\n';
            const auto response = client.request(sequence++, method, params);
            transcript << Json{{"pass", pass}, {"direction", "response"}, {"message", response}}.dump() << '\n';
            transcript.flush();
            require(response.contains("result"), "descriptor MCP protocol result");
            return response.at("result");
        };
        const auto call = [&](const char* tool, const Json& args) {
            const auto result = request("tools/call", {{"name", tool}, {"arguments", args}});
            require(!result.value("isError", false) && result.contains("structuredContent"),
                    "descriptor MCP call: " + result.dump());
            require(Json::parse(result.at("content")[0].at("text").get<std::string>()) ==
                        result.at("structuredContent"),
                    "descriptor text/structured parity");
            return result.at("structuredContent");
        };
        const auto initialized = request(
            "initialize",
            {{"protocolVersion", "2025-06-18"},
             {"capabilities", Json::object()},
             {"clientInfo", {{"name", "ngm-descriptor-state-integration"}, {"version", ngm::project_version()}}}});
        require(initialized.at("serverInfo").at("version") == ngm::project_version(),
                "current descriptor server identity");
        client.send({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
        if(pass == 0)
            report["tools"] = request("tools/list", Json::object());
        Json cases = Json::array();
        std::set<std::string> refs;
        for(std::size_t case_at = 0; case_at < captured.at("cases").size(); ++case_at) {
            const auto& source = captured.at("cases")[case_at];
            const auto id = source.at("submission").at("artifact_id");
            Json pages = Json::array(), executions = Json::array(), selected = Json::array();
            for(std::size_t offset = 0;;) {
                const auto page =
                    call("capture_draw_executions", {{"capture_id", id}, {"offset", offset}, {"limit", 2}});
                require(page.at("unsupported_total") == 0, "qualified fresh descriptor recording grammar");
                pages.push_back(page);
                for(const auto& row : page.at("executions"))
                    executions.push_back(row);
                if(page.at("next_offset").is_null())
                    break;
                offset = page.at("next_offset").get<std::size_t>();
            }
            require(executions.size() == (case_at == 0 ? 5 : 10),
                    "five draws per submission with distinct reused occurrences");
            const auto& witness = source.at("witness");
            const auto layout_symbol = [&](const Json& handle) {
                const auto& layouts = witness.at("descriptor_layouts");
                for(std::size_t n = 0; n < layouts.size(); ++n)
                    if(layouts[n].at("handle") == handle)
                        return names[case_at].at("fixture.descriptors.layout." + std::to_string(n));
                throw std::runtime_error("missing independent descriptor layout API handle");
            };
            const auto binding_layout_symbol = [&](const Json& handle) {
                for(const auto& pipeline : witness.at("pipelines"))
                    if(pipeline.at("layout_handle") == handle)
                        return pipeline_layouts[case_at].at(names[case_at].at(pipeline.at("label")));
                throw std::runtime_error("missing independent pipeline layout API handle");
            };
            for(const auto& execution : executions) {
                const auto ref = execution.at("execution_ref");
                require(refs.insert(ref.get<std::string>()).second, "unique descriptor occurrence reference");
                const auto usage = call("artifact_usage", Json::object());
                const auto value = call("capture_draw_state", {{"capture_id", id}, {"execution_ref", ref}});
                require(call("artifact_usage", Json::object()).at("completed_bytes") == usage.at("completed_bytes"),
                        "descriptor query creates no resource-read artifacts");
                require(value.at("execution").at("selection") == execution.at("selection") &&
                            value.at("captured_gpu_execution_correspondence") == false &&
                            value.at("full_effective_state_coverage") == false &&
                            value.at("schema_profile").get<std::string>().ends_with("-recorded-state-v3"),
                        "explicit recorded selection scope");
                const auto& submissions = witness.at("submissions");
                const auto submitted = std::find_if(submissions.begin(), submissions.end(), [&](const auto& row) {
                    return row.at("label") == execution.at("labels")[0] &&
                           row.at("submission_occurrence") == execution.at("selection").at("submission_index");
                });
                require(submitted != submissions.end(),
                        "private successful submission/occurrence matches generated labels");
                const auto& draws = submitted->at("descriptor_draws");
                const auto expected = std::find_if(draws.begin(), draws.end(), [&](const auto& row) {
                    return row.at("label") == execution.at("labels").back();
                });
                require(expected != draws.end(), "unique private draw witness");
                const auto& pipelines = witness.at("pipelines");
                const auto pipeline = std::find_if(pipelines.begin(), pipelines.end(), [&](const auto& row) {
                    return row.at("pipeline_handle") == expected->at("pipeline_handle");
                });
                require(pipeline != pipelines.end() && pipeline->at("layout_handle") == expected->at("layout_handle"),
                        "independent pipeline/layout creation and bind handles");
                const auto& state = value.at("execution").at("state");
                for(const auto* key : {"pipeline", "shaders", "depth_test", "depth_write", "depth_compare", "cull_mode",
                                       "front_face", "viewports", "scissors"})
                    require(state.at(key).at("status") == "qualified_recorded_state",
                            "available independent fixture field");
                require(state.at("depth_test").at("value") == (pipeline->at("depthTestEnable") == 1) &&
                            state.at("depth_write").at("value") == (pipeline->at("depthWriteEnable") == 1) &&
                            state.at("depth_compare").at("value") == "VK_COMPARE_OP_LESS" &&
                            pipeline->at("depthCompareOp") == 1 &&
                            state.at("cull_mode").at("value") == "VK_CULL_MODE_NONE" && pipeline->at("cullMode") == 0 &&
                            state.at("front_face").at("value") == "VK_FRONT_FACE_COUNTER_CLOCKWISE" &&
                            pipeline->at("frontFace") == 0 &&
                            state.at("viewports").at("value") == Json::array({pipeline->at("viewport")}) &&
                            state.at("scissors").at("value") == Json::array({pipeline->at("scissor")}),
                        "all available raster values match private pipeline API structures");
                require(state.at("shaders").at("value").size() == pipeline->at("stages").size(),
                        "all shader stages are retained");
                for(const auto& stage : state.at("shaders").at("value")) {
                    require(stage.at("stage") == "VK_SHADER_STAGE_VERTEX_BIT" ||
                                stage.at("stage") == "VK_SHADER_STAGE_FRAGMENT_BIT",
                            "expected fixture shader stage");
                    const auto numeric_stage = stage.at("stage") == "VK_SHADER_STAGE_VERTEX_BIT" ? 1u : 16u;
                    const auto& stages = pipeline->at("stages");
                    const auto api = std::find_if(stages.begin(), stages.end(),
                                                  [&](const auto& row) { return row.at("stage") == numeric_stage; });
                    require(api != stages.end() && stage.at("declared_bytes") == api->at("bytes") &&
                                stage.at("entry_point_expression") == api->at("entry_point").dump() &&
                                api->at("flags") == 0 && stage.at("serialized_byte_read").at("status") == "not_read" &&
                                stage.at("serialized_byte_read").at("sha256").is_null(),
                            "shader declarations match private API stage, entry and module size without claiming byte "
                            "reads");
                }
                require(state.at("pipeline").at("value").at("symbol") == names[case_at].at(pipeline->at("label")) &&
                            state.at("pipeline").at("value").at("layout") ==
                                binding_layout_symbol(expected->at("layout_handle")),
                        "exported pipeline identity matches independently named API creation");
                const auto &field = state.at("descriptors"), coverage = field.at("coverage");
                require(coverage.at("sets").size() == 2 && coverage.at("contents").at("status") == "unavailable" &&
                            coverage.at("contents").at("value").is_null() &&
                            state.at("uniform_material_values").at("status") == "unavailable" &&
                            state.at("push_constants").at("status") == "not_applicable",
                        "contents/values remain explicitly unavailable after partial descriptor writes");
                bool complete = true;
                for(std::size_t n = 0; n < 2; ++n) {
                    const auto &expected_set = expected->at("expected_sets")[n], row = coverage.at("sets")[n];
                    require(row.at("pipeline_set_layout") == layout_symbol(pipeline->at("descriptor_set_layouts")[n]),
                            "declared pipeline set layout identity");
                    Json expected_bindings;
                    for(const auto& layout : witness.at("descriptor_layouts"))
                        if(layout.at("handle") == pipeline->at("descriptor_set_layouts")[n])
                            expected_bindings = layout.at("bindings");
                    require(expected_bindings.is_array() && expected_bindings.size() == 3 &&
                                row.at("bindings").size() == 3,
                            "all three independent API binding definitions are present");
                    std::sort(expected_bindings.begin(), expected_bindings.end(),
                              [](const auto& a, const auto& b) { return a.at("binding") < b.at("binding"); });
                    for(std::size_t b = 0; b < expected_bindings.size(); ++b) {
                        const auto &definition = row.at("bindings")[b], api = expected_bindings[b];
                        require(definition.at("binding") == api.at("binding") &&
                                    definition.at("descriptor_count") == api.at("descriptor_count") &&
                                    definition.at("stage_mask") == api.at("stage_mask") &&
                                    definition.at("descriptor_type") ==
                                        (api.at("descriptor_type") == 8 ? "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC"
                                                                        : "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC"),
                                "layout definitions match actual API structures");
                    }
                    if(!expected_set.at("available").get<bool>()) {
                        require(row.at("status") == "unavailable" && row.at("descriptor_set").is_null() &&
                                    row.at("dynamic_offsets").empty(),
                                "disturbed higher set does not retain stale values");
                        complete = false;
                        continue;
                    }
                    require(row.at("status") == "qualified_recorded_state",
                            "expected recorded descriptor bind: " + row.dump());
                    const auto& allocations = witness.at("descriptor_allocations");
                    const auto allocation =
                        std::find_if(allocations.begin(), allocations.end(), [&](const auto& entry) {
                            return entry.at("handle") == expected_set.at("descriptor_handle");
                        });
                    require(allocation != allocations.end(), "independent descriptor allocation handle");
                    const auto allocation_index = static_cast<std::size_t>(allocation - allocations.begin());
                    require(row.at("descriptor_set") ==
                                    names[case_at].at("fixture.descriptors.set." + std::to_string(allocation_index)) &&
                                row.at("allocated_set_layout") == layout_symbol(allocation->at("layout_handle")) &&
                                row.at("binding_layout") ==
                                    binding_layout_symbol(expected_set.at("binding_layout_handle")),
                            "allocated and bound layout/set identities match private API inputs");
                    const auto& actual_bind =
                        submitted->at("descriptor_binds").at(expected_set.at("bind_index").get<std::size_t>());
                    const std::array<unsigned, 4> bindings{0, 2, 2, 9}, elements{0, 0, 1, 0};
                    require(row.at("dynamic_offsets").size() == 4, "all dynamic array elements are mapped");
                    for(std::size_t i = 0; i < 4; ++i) {
                        const auto& offset = row.at("dynamic_offsets")[i];
                        require(offset.at("binding") == bindings[i] && offset.at("array_element") == elements[i] &&
                                    offset.at("offset") == expected_set.at("dynamic_offsets")[i] &&
                                    offset.at("source_index") ==
                                        (n - actual_bind.at("first_set").get<std::size_t>()) * 4 + i,
                                "independent set/binding/element dynamic offset oracle");
                    }
                }
                require(coverage.at("bindings_complete") == complete &&
                            field.at("status") == (complete ? "qualified_recorded_state" : "unavailable") &&
                            (complete ? field.at("value").size() == 2 : field.at("value").is_null()),
                        "complete/partial aggregate coverage is explicit");
                selected.push_back(value);
            }
            require(call("artifact_info", {{"artifact_id", id}}).at("pinned") == true, "source pin survives restart");
            cases.push_back({{"capture_id", id}, {"pages", pages}, {"selected", selected}});
        }
        if(pass == 0)
            original = cases;
        else
            require(cases == original, "descriptor references, fields and provenance survive server restart");
        report["passes"].push_back({{"pass", pass}, {"cases", cases}});
        save(run / "descriptor-query-report.json", report);
    }
    report["status"] = "pass";
    save(run / "descriptor-query-report.json", report);
}
