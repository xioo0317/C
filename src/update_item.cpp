#include "server/actions.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

namespace app {

using nlohmann::json;

void handle_update_item(const std::string& body, httplib::DataSink& sink) {
    std::cout << "[action] update_item" << std::endl;

    // Parse the request payload with nlohmann/json (validated upstream).
    const json payload = json::parse(body, /*cb=*/nullptr, /*allow_exceptions=*/false);
    const std::string payload_str = payload.is_object() ? payload.dump() : "{}";

    // 1. Stream the start event
    sse_send(sink, sse_event("开始执行指令"));

    // 2. Write test log to verify API execution
    std::ofstream log_file("/data/adb/local_api/123", std::ios::app);
    if (log_file.is_open()) {
        log_file << "[update_item] executed successfully, payload=" << payload_str << std::endl;
        sse_send(sink, json{{"step", "write_log"}, {"status", "ok"}}.dump());
    } else {
        std::cerr << "[update_item] failed to open log file" << std::endl;
        sse_send(sink, json{{"step", "write_log"}, {"status", "skipped"}}.dump());
    }

    // 3. Stream the result, then [DONE]
    sse_send(sink, json{{"result", json{{"updated", true}}}, {"status", "completed"}}.dump());
    sse_done(sink);
}

} // namespace app
