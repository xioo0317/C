// SPDX-License-Identifier: GPL-3.0-or-later
//
// tools.cpp — Tool function implementations.
//
// Each tool is called by router.cpp via execute(action, params).
// Detection is one tool; others are placeholders for future expansion.

#include "tools.hpp"
#include "detector.hpp"
#include "version.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <string>

#include <unistd.h>
#include <sys/stat.h>

using json = nlohmann::json;

namespace tools {

// ===========================================================================
// Dispatcher
// ===========================================================================

std::string execute(const std::string& action, const json& params) {
    if (action == "detect")  return tool_detect(params);
    if (action == "version") return tool_version(params);
    if (action == "debug")   return tool_debug(params);

    // Placeholder tools
    if (action == "sysinfo") return tool_sysinfo(params);
    if (action == "modules") return tool_modules(params);
    if (action == "config")  return tool_config(params);

    json err = {
        {"status", "error"},
        {"error", "unknown action: " + action},
        {"available_actions", list_actions()}
    };
    return err.dump();
}

json list_actions() {
    return json::array({
        "detect", "version", "debug",
        "sysinfo", "modules", "config"
    });
}

// ===========================================================================
// Tool: detect — Root detection (KernelSU / APatch / Magisk / SusFS)
// ===========================================================================

std::string tool_detect(const json&) {
    ksu_detector::Detector detector;
    auto result = detector.run_all();
    std::string detect_json = ksu_detector::result_to_json_string(result);

    // Write result to file
    mkdir(local_api::OUTPUT_DIR, 0755);
    std::ofstream out(local_api::OUTPUT_FILE, std::ios::trunc);

    json reply;
    if (out.is_open()) {
        out << detect_json;
        out.close();
        if (out.good()) {
            reply = {{"status", "ok"}, {"result_file", local_api::OUTPUT_FILE}};
        } else {
            reply = {{"status", "error"}, {"error", "failed to write result file"}};
        }
    } else {
        reply = {{"status", "error"}, {"error", "failed to write result file"}};
    }

    // Embed the detection result inline as well
    reply["result"] = json::parse(detect_json);
    return reply.dump();
}

// ===========================================================================
// Tool: version — Server version + API version
// ===========================================================================

std::string tool_version(const json&) {
    json j;
    j["status"] = "ok";
    j["server_version"] = local_api::SERVER_VERSION;
    j["api_version"] = local_api::API_VERSION;
    j["app_name"] = local_api::APP_NAME;
    return j.dump();
}

// ===========================================================================
// Tool: debug — Tool info, versions, dry-run output
// ===========================================================================

std::string tool_debug(const json&) {
    json j;
    j["status"] = "ok";
    j["server_version"] = local_api::SERVER_VERSION;
    j["api_version"] = local_api::API_VERSION;
    j["output_dir"] = local_api::OUTPUT_DIR;
    j["output_file"] = local_api::OUTPUT_FILE;
    j["pid"] = getpid();

    // List registered tools
    j["tools"] = list_actions();

    // Dry-run: execute detection and include result
    ksu_detector::Detector detector;
    auto result = detector.run_all();
    j["detect_dry_run"] = json::parse(ksu_detector::result_to_json_string(result));

    // Tool status
    json ts;
    ts["detect"]  = {{"status", "ready"}, {"description", "Root detection (KSU/APatch/Magisk/SusFS)"}};
    ts["version"] = {{"status", "ready"}, {"description", "Server version info"}};
    ts["debug"]   = {{"status", "ready"}, {"description", "Debug info + dry-run"}};
    ts["sysinfo"] = {{"status", "placeholder"}, {"description", "System information (not implemented)"}};
    ts["modules"] = {{"status", "placeholder"}, {"description", "Module management (not implemented)"}};
    ts["config"]  = {{"status", "placeholder"}, {"description", "Configuration (not implemented)"}};
    j["tool_status"] = ts;

    return j.dump();
}

// ===========================================================================
// Placeholder tools — implement when needed
// ===========================================================================

std::string tool_sysinfo(const json&) {
    json reply = {
        {"status", "ok"},
        {"message", "not implemented yet"},
        {"hint", "will return device model, Android version, kernel info, etc."}
    };
    return reply.dump();
}

std::string tool_modules(const json&) {
    json reply = {
        {"status", "ok"},
        {"message", "not implemented yet"},
        {"hint", "will list installed kernel modules, enable/disable"}
    };
    return reply.dump();
}

std::string tool_config(const json&) {
    json reply = {
        {"status", "ok"},
        {"message", "not implemented yet"},
        {"hint", "will read/write server configuration"}
    };
    return reply.dump();
}

} // namespace tools
