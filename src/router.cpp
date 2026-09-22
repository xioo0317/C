#include "server/router.hpp"
#include "server/actions.hpp"
#include "server/handlers.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>

using json = nlohmann::json;

namespace app {

void register_routes(httplib::Server& svr) {

    // ── Unified POST endpoint (plain JSON response) ─────────────
    svr.Post("/api/v1/execute", [](const httplib::Request& req, httplib::Response& res) {
        const json payload = json::parse(req.body, nullptr, false);

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

        const ActionHandler handler = find_action(action);
        if (!handler) {
            res.status = 400;
            res.set_content(json{{"error", "unknown action"}, {"action", action}}.dump(),
                            "application/json");
            return;
        }

        handler(req, res);
    });

    // ── Utility endpoints (src/handlers.cpp) ────────────────────
    svr.Get("/api/v1/ping",   handle_ping);
    svr.Get("/api/v1/status", handle_status);
    svr.Post("/api/v1/echo",  handle_echo);
    svr.Get("/api/v1/time",   handle_time);

    // ── Error handler ───────────────────────────────────────────
    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        if (res.status == 404) {
            res.set_content(json{{"error", "not found"}}.dump(), "application/json");
        }
    });
}

} // namespace app
