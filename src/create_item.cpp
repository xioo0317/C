#include "server/actions.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

namespace app {

using nlohmann::json;

void handle_create_item(const httplib::Request& req, httplib::Response& res) {
    std::cout << "[action] create_item" << std::endl;

    const json payload = json::parse(req.body, nullptr, false);
    const std::string payload_str = payload.is_object() ? payload.dump() : "{}";

    std::ofstream log_file("/data/adb/local_api/123", std::ios::app);
    if (log_file.is_open()) {
        log_file << "[create_item] executed successfully, payload=" << payload_str << std::endl;
    } else {
        std::cerr << "[create_item] failed to open log file" << std::endl;
    }

    res.status = 200;
    res.set_content(json{{"created", true}}.dump(), "application/json");
}

} // namespace app
