#include "server/router.hpp"
#include "server/actions.hpp"

#include <httplib.h>
#include <functional>
#include <iostream>
#include <string>

namespace app {

// Simple JSON string field extractor
static std::string extract_json_string(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(":", pos);
    if (pos == std::string::npos) return "";
    pos = json.find("\"", pos);
    if (pos == std::string::npos) return "";
    auto end = json.find("\"", pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

ActionHandler get_action_handler(const std::string& action_name) {
    if (action_name == "list_items") return handle_list_items;
    if (action_name == "create_item") return handle_create_item;
    if (action_name == "update_item") return handle_update_item;
    if (action_name == "delete_item") return handle_delete_item;
    return nullptr;
}

void register_routes(httplib::Server& svr) {

    // ── Unified POST endpoint (SSE streaming response) ─────────
    // Request:  POST /api/v1/execute
    // Body:     {"action": "<action_name>"}
    // Response: text/event-stream
    //   data: {"message":"开始执行指令"}
    //   data: {"step":...}
    //   data: {"result":...,"status":"completed"}
    //   data: [DONE]
    // ────────────────────────────────────────────────────────────
    svr.Post("/api/v1/execute", [](const httplib::Request& req, httplib::Response& res) {
        std::string action = extract_json_string(req.body, "action");

        if (action.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"missing or invalid 'action' field"})", "application/json");
            return;
        }

        std::cout << "[POST /api/v1/execute] action=" << action
                  << " body=" << req.body << std::endl;

        ActionHandler handler = get_action_handler(action);
        if (!handler) {
            res.status = 400;
            res.set_content(R"({"error":"unknown action","action":")" + action + "\"}", "application/json");
            return;
        }

        res.status = 200;
        res.set_header("Cache-Control", "no-cache");
        res.set_header("X-Accel-Buffering", "no");
        res.set_chunked_content_provider(
            "text/event-stream",
            [handler, body = req.body](size_t /*offset*/, httplib::DataSink& sink) {
                handler(body, sink); // handler ends with data: [DONE] + sink.done()
                return true;
            });
    });

    // ── Error handler ─────────────────────────────────────────
    // Only fills a body for unmatched routes; keeps the status set
    // by route handlers (e.g. 400) untouched.
    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        if (res.status == 404) {
            res.set_content(R"({"error":"not found"})", "application/json");
        }
    });
}

} // namespace app
