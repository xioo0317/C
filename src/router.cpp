#include "server/router.hpp"
#include "server/actions.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>

using json = nlohmann::json;

namespace app {

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
        // Parse the request body with nlohmann/json (no exceptions).
        const json payload = json::parse(req.body, /*cb=*/nullptr, /*allow_exceptions=*/false);

        if (!payload.is_object()) {
            res.status = 400;
            res.set_content(json{{"error", "request body must be a JSON object"}}.dump(),
                            "application/json");
            return;
        }

        const auto action_field = payload.find("action");
        if (action_field == payload.end() || !action_field->is_string()
                || action_field->get<std::string>().empty()) {
            res.status = 400;
            res.set_content(json{{"error", "missing or invalid 'action' field"}}.dump(),
                            "application/json");
            return;
        }

        const std::string action = action_field->get<std::string>();
        std::cout << "[POST /api/v1/execute] action=" << action
                  << " body=" << req.body << std::endl;

        // Table-driven dispatch: one hash lookup, no if-else chain.
        const ActionHandler handler = find_action(action);
        if (!handler) {
            res.status = 400;
            res.set_content(json{{"error", "unknown action"}, {"action", action}}.dump(),
                            "application/json");
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
            res.set_content(json{{"error", "not found"}}.dump(), "application/json");
        }
    });
}

} // namespace app
