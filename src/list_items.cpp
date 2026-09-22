#include "server/actions.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

namespace app {

using nlohmann::json;

void handle_list_items(const httplib::Request& req, httplib::Response& res) {
    std::cout << "[action] list_items" << std::endl;

    const json payload = json::parse(req.body, nullptr, false);
    const std::string payload_str = payload.is_object() ? payload.dump() : "{}";

    std::ofstream log_file("/data/adb/local_api/123", std::ios::app);
    if (log_file.is_open()) {
        log_file << "[list_items] executed successfully, payload=" << payload_str << std::endl;
    } else {
        std::cerr << "[list_items] failed to open log file" << std::endl;
    }

    res.status = 200;
    res.set_content(json{{"items", json::array()}}.dump(), "application/json");
}

} // namespace app
