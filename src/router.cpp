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

    // ── POST /out — action dispatch (CRUD) ──────────────────────
    // Body: {"action": "list_items" | "create_item" | ...}
    svr.Post("/out", [](const httplib::Request& req, httplib::Response& res) {
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
        std::cout << "[POST /out] action=" << action << std::endl;

        const Handler handler = find_action(action);
        if (!handler) {
            res.status = 400;
            res.set_content(json{{"error", "unknown action"}, {"action", action}}.dump(),
                            "application/json");
            return;
        }

        handler(req, res);
    });

    // ── POST /in — utility dispatch ─────────────────────────────
    // Body: {"command": "ping" | "status" | "echo" | "time"}
    svr.Post("/in", [](const httplib::Request& req, httplib::Response& res) {
        const json payload = json::parse(req.body, nullptr, false);

        if (!payload.is_object()) {
            res.status = 400;
            res.set_content(json{{"error", "request body must be a JSON object"}}.dump(),
                            "application/json");
            return;
        }

        const auto cmd_field = payload.find("command");
        if (cmd_field == payload.end() || !cmd_field->is_string()
                || cmd_field->get<std::string>().empty()) {
            res.status = 400;
            res.set_content(json{{"error", "missing or invalid 'command' field"}}.dump(),
                            "application/json");
            return;
        }

        const std::string command = cmd_field->get<std::string>();
        std::cout << "[POST /in] command=" << command << std::endl;

        const Handler handler = find_util(command);
        if (!handler) {
            res.status = 400;
            res.set_content(json{{"error", "unknown command"}, {"command", command}}.dump(),
                            "application/json");
            return;
        }

        handler(req, res);
    });

    // ── Error handler ───────────────────────────────────────────
    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        if (res.status == 404) {
            res.set_content(json{{"error", "not found"}}.dump(), "application/json");
        }
    });
}

} // namespace app
