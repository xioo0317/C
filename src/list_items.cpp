#include "server/actions.hpp"

#include <fstream>
#include <iostream>

namespace app {

void handle_list_items(const std::string& body, httplib::DataSink& sink) {
    std::cout << "[action] list_items" << std::endl;

    // 1. Stream the start event
    sse_send(sink, sse_event("开始执行指令"));

    // 2. Write test log to verify API execution
    std::ofstream log_file("/data/adb/local_api/123", std::ios::app);
    if (log_file.is_open()) {
        log_file << "[list_items] executed successfully" << std::endl;
        sse_send(sink, R"({"step":"write_log","status":"ok"})");
    } else {
        std::cerr << "[list_items] failed to open log file" << std::endl;
        sse_send(sink, R"({"step":"write_log","status":"skipped"})");
    }

    // 3. Stream the result, then [DONE]
    sse_send(sink, R"({"result":{"items":[]},"status":"completed"})");
    sse_done(sink);
}

} // namespace app
